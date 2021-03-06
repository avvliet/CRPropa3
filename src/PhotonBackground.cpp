#include "crpropa/PhotonBackground.h"
#include "crpropa/Units.h"
#include "crpropa/Random.h"

#include <fstream>
#include <stdexcept>
#include <limits>
#include <cmath>

namespace crpropa {

TabularPhotonField::TabularPhotonField(std::string fieldName, bool isRedshiftDependent) {
	this->fieldName = fieldName;
	this->isRedshiftDependent = isRedshiftDependent;

	readPhotonEnergy(getDataPath("") + "Scaling/" + this->fieldName + "_photonEnergy.txt");
	readPhotonDensity(getDataPath("") + "Scaling/" + this->fieldName + "_photonDensity.txt");
	if (this->isRedshiftDependent)
		readRedshift(getDataPath("") + "Scaling/" + this->fieldName + "_redshift.txt");

	checkInputData();

	if (this->isRedshiftDependent)
		initRedshiftScaling();
}

double TabularPhotonField::getPhotonDensity(double ePhoton, double z) const {
	if (this->isRedshiftDependent) {
		return interpolate2d(ePhoton, z, this->photonEnergies, this->redshifts, this->photonDensity);
	} else {
		return interpolate(ePhoton, this->photonEnergies, this->photonDensity);
	}
}

double TabularPhotonField::getRedshiftScaling(double z) const {
	if (this->isRedshiftDependent) {
		if (z > this->redshifts.back()) {
			return 0.;
		} else if (z < this->redshifts.front()) {
			return 1.;
		} else {
			return interpolate(z, this->redshifts, this->redshiftScalings);
		}
	} else {
		return 1.;
	}
}

void TabularPhotonField::readPhotonEnergy(std::string filePath) {
	std::ifstream infile(filePath.c_str());
	if (!infile.good())
		throw std::runtime_error("TabularPhotonField::readPhotonEnergy: could not open " + filePath);

	std::string line;
	while (std::getline(infile, line)) {
		if (line.size() > 0)
			this->photonEnergies.push_back(std::stod(line));
	}
	infile.close();
}

void TabularPhotonField::readPhotonDensity(std::string filePath) {
	std::ifstream infile(filePath.c_str());
	if (!infile.good())
		throw std::runtime_error("TabularPhotonField::readPhotonDensity: could not open " + filePath);

	std::string line;
	while (std::getline(infile, line)) {
		if (line.size() > 0)
			this->photonDensity.push_back(std::stod(line));
	}
	infile.close();
}

void TabularPhotonField::readRedshift(std::string filePath) {
	std::ifstream infile(filePath.c_str());
	if (!infile.good())
		throw std::runtime_error("TabularPhotonField::initRedshift: could not open " + filePath);

	std::string line;
	while (std::getline(infile, line)) {
		if (line.size() > 0)
			this->redshifts.push_back(std::stod(line));
	}
	infile.close();
}

void TabularPhotonField::initRedshiftScaling() {
	double n0 = 0.;
	for (int i = 0; i < this->redshifts.size(); ++i) {
		double z = this->redshifts[i];
		double n = 0.;
		for (int j = 0; j < this->photonEnergies.size(); ++j) {
			double e = this->photonEnergies[j];
			if (z == 0.)
				n0 += getPhotonDensity(e, z);
			n += getPhotonDensity(e, z);
		}
		this->redshiftScalings.push_back(n / n0);
	}
}

void TabularPhotonField::checkInputData() const {
	if (this->isRedshiftDependent) {
		if (this->photonDensity.size() != this->photonEnergies.size() * this-> redshifts.size())
			throw std::runtime_error("TabularPhotonField::checkInputData: length of photon density input is unequal to length of photon energy input times length of redshift input");
	} else {
		if (this->photonEnergies.size() != this->photonDensity.size())
			throw std::runtime_error("TabularPhotonField::checkInputData: length of photon energy input is unequal to length of photon density input");
	}

	for (int i = 0; i < this->photonEnergies.size(); ++i) {
		double ePrevious = 0.;
		double e = this->photonEnergies[i];
		if (e <= 0.)
			throw std::runtime_error("TabularPhotonField::checkInputData: a value in the photon energy input is not positive");
		if (e <= ePrevious)
			throw std::runtime_error("TabularPhotonField::checkInputData: photon energy values are not strictly increasing");
		ePrevious = e;
	}

	for (int i = 0; i < this->photonDensity.size(); ++i) {
		if (this->photonDensity[i] < 0.)
			throw std::runtime_error("TabularPhotonField::checkInputData: a value in the photon density input is negative");
	}

	if (this->isRedshiftDependent) {
		if (this->redshifts[0] != 0.)
			throw std::runtime_error("TabularPhotonField::checkInputData: redshift input must start with zero");

		for (int i = 0; i < this->redshifts.size(); ++i) {
			double zPrevious = -1.;
			double z = this->redshifts[i];
			if (z < 0.)
				throw std::runtime_error("TabularPhotonField::checkInputData: a value in the redshift input is negative");
			if (z <= zPrevious)
				throw std::runtime_error("TabularPhotonField::checkInputData: redshift values are not strictly increasing");
			zPrevious = z;
		}

		for (int i = 0; i < this->redshiftScalings.size(); ++i) {
			double scalingFactor = this->redshiftScalings[i];
			if (scalingFactor <= 0.)
				throw std::runtime_error("TabularPhotonField::checkInputData: initRedshiftScaling has created a non-positive scaling factor");
		}
	}
}

BlackbodyPhotonField::BlackbodyPhotonField(std::string fieldName, double blackbodyTemperature) {
	this->fieldName = fieldName;
	this->blackbodyTemperature = blackbodyTemperature;
}

double BlackbodyPhotonField::getPhotonDensity(double ePhoton, double z) const {
	return 8 * M_PI * pow_integer<3>(ePhoton / (h_planck * c_light)) / std::expm1(ePhoton / (k_boltzmann * this->blackbodyTemperature));
}

PhotonFieldSampling::PhotonFieldSampling() {
	bgFlag = 0;
}

PhotonFieldSampling::PhotonFieldSampling(int flag) {
	if (flag != 1 && flag != 2)
		throw std::runtime_error("error: incorrect background flag. Must be 1 (CMB) or 2 (IRB_Kneiske04).");
	bgFlag = flag;
}

double PhotonFieldSampling::sample_eps(bool onProton, double E_in, double z_in) const {
	if (bgFlag == 0)
		throw std::runtime_error("error: select photon field first: 1 (CMB) or 2 (IRB_Kneiske04)");

	const double mass = onProton? 0.93827 : 0.93947;  // Gev/c^2
	const double P_in = sqrt(E_in * E_in - mass * mass);  // GeV/c

	double eps = 0.;
	if (bgFlag == 1) {
		// CMB
		const double tbb = 2.73 * (1. + z_in);
		const double epsMin = (1.1646 - mass * mass) / 2. / (E_in + P_in) * 1.e9;
		const double epsMax = 0.007 * tbb;
		if (epsMin > epsMax) {
			std::cout << "sample_eps (CMB): CMF energy is below threshold for nucleon energy " << E_in << " GeV !" << std::endl;
			return 0.;
		}

		const double cnorm = gaussInt([this, onProton, E_in, z_in](double e) { return this->prob_eps(e, onProton, E_in, z_in); }, epsMin, epsMax);
		const double epskt = 8.619e-5 * tbb;
		const double epspmax = (3.e-3 * std::pow(E_in * epskt * 1.e-9, -0.97) + 0.047) / 3.9e2 * tbb;
		const double pmaxc = prob_eps(epspmax, onProton, E_in, z_in) / cnorm;
		const double facpmax = 1.6;
		const double pMax = facpmax * pmaxc;

		// sample eps between epsMin ... epsMax
		double peps = 0.;
		Random &random = Random::instance();
		do {
			eps = epsMin + random.rand() * (epsMax - epsMin);
			peps = prob_eps(eps, onProton, E_in, z_in) / cnorm;
		} while (random.rand() * pMax > peps);
	}

	if (bgFlag == 2) {
		// IRB_Kneiske04     
		const double epsMin = std::max(0.00395, 1.e9 * (1.1646 - mass * mass) / 2. / (E_in + P_in));  // eV
		const double epsMax = 12.2;  // eV
		if (epsMin > epsMax) {
			std::cout << "sample_eps (IRB): CMF energy is below threshold for nucleon energy " << E_in << " GeV !" << std::endl;
			return 0.;
		}
		const int i_max = static_cast<int>(10. * std::log(epsMax / epsMin)) + 1;
		const double de = std::log(epsMax / epsMin) / i_max;
		double rmax = 0.;
		double eps_dum = 0.;
		double dum = 0.;
		for (int i = 0; i < i_max; ++i) {
			eps_dum = epsMin * std::exp(i * de);
			dum = eps_dum * eps_dum * getPhotonDensity(eps_dum, z_in);
			if (dum > rmax)
				rmax = dum;
		}
		const double beta = 4.;
		const double e1 = std::pow(epsMin, 1. - beta);
		const double e2 = std::pow(epsMax, 1. - beta);
		bool keepTrying = true;
		int i_rep = 0;
		Random &random = Random::instance();
		do {
			if (i_rep >= 100000) {
				keepTrying = false;
				return 0.;
			}
			eps = std::pow(random.rand() * (e1 - e2) + e2, 1./(1. - beta));
			i_rep++;
			if (random.rand() < eps * eps * getPhotonDensity(eps, z_in) / rmax)
				keepTrying = false;
		} while (keepTrying);
	}
	return eps * eV;
}

double PhotonFieldSampling::prob_eps(double eps, bool onProton, double E_in, double z_in) const {
	const double mass = onProton? 0.93827 : 0.93947;  // Gev/c^2
	double gamma = E_in / mass;
	double beta = std::sqrt(1. - 1. / gamma / gamma);
	double photonDensity = getPhotonDensity(eps, z_in);
	
	if (photonDensity != 0.) {
		double sMin = 1.1646;  // [GeV], head-on collision
		double sMax = std::max(sMin, mass * mass + 2. * eps / 1.e9 * E_in * (1. + beta));
		double sintegr = gaussInt([this, onProton](double s) { return this->functs(s, onProton); }, sMin, sMax);
		return photonDensity / eps / eps * sintegr / 8. / beta / E_in / E_in * 1.e18 * 1.e6;
	}

	return 0;
}

double PhotonFieldSampling::getPhotonDensity(double eps, double z_in) const {
	if (bgFlag == 1) {
		// CMB
		return 1.318e13 * eps * eps / (std::exp(eps / (8.619e-5 * 2.73)) - 1.);
	}

	if (bgFlag == 2) {
		// IR background from Primack et al. (1999) 
		const double ZMAX_IR = 5.;
		if (z_in > ZMAX_IR)
			return 0.;
		const double X = 1.2398 * (1. + z_in) / eps;
		if (X > 500.)
			return 0.;

		static const double XData[15] = {-1.0, -0.75, -0.5, -0.25, 0.0, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 2.25, 2.5};  // log10(eV)
		static const double YData[15] = {-0.214401, 0.349313, 0.720354, 0.890389, 1.16042, 1.24692, 1.06525, 0.668659, 0.536312, 0.595859, 0.457456, 0.623521, 1.20208, 1.33657, 1.04461};  // log10(nW/m^2/sr)
		
		// interpolate
		if (std::log10(X) <= XData[0])
			return 0.;
		if (std::log10(X) >= XData[14]) {
			double result = (YData[14] - YData[13]) / (XData[14] - XData[13]) * (std::log10(X) - XData[13]) + YData[13];
			return std::pow(10., result);
		}
		int index = 1;
		do {
			index++;
		} while (XData[index] < std::log10(X));
		double result = (YData[index] - YData[index - 1]) / (XData[index] - XData[index - 1]) * (std::log10(X) - XData[index - 1]) + YData[index - 1];
		result = std::pow(10., result);

		const double fluxConversion = 3.82182e3;  // conversion from nW/cm^3/sr to eV/cm^3
		return result * std::pow(1. + z_in, 4.) / (eps * eps) / fluxConversion;
	}
}

double PhotonFieldSampling::crossection(double x, bool onProton) const {
	const double mass = onProton? 0.93827 : 0.93947;  // Gev/c^2
	const double sth = 1.1646;  // GeV^2
	const double s = mass * mass + 2. * mass * x;
	if (s < sth)
		return 0.;
	double cross_res = 0.;
	double cross_dir = 0.;
	double cross_dir1 = 0.;
	double cross_dir2 = 0.;
	double sig_res[9];

	// first half of array: 9x proton resonance data | second half of array 9x neutron resonance data
	static const double AMRES[18] = {1.231, 1.440, 1.515, 1.525, 1.675, 1.680, 1.690, 1.895, 1.950, 1.231, 1.440, 1.515, 1.525, 1.675, 1.675, 1.690, 1.895, 1.950};
	static const double BGAMMA[18] = {5.6, 0.5, 4.6, 2.5, 1.0, 2.1, 2.0, 0.2, 1.0, 6.1, 0.3, 4.0, 2.5, 0.0, 0.2, 2.0, 0.2, 1.0};
	static const double WIDTH[18] = {0.11, 0.35, 0.11, 0.1, 0.16, 0.125, 0.29, 0.35, 0.3, 0.11, 0.35, 0.11, 0.1, 0.16, 0.150, 0.29, 0.35, 0.3};
	static const double RATIOJ[18] = {1., 0.5, 1., 0.5, 0.5, 1.5, 1., 1.5, 2., 1., 0.5, 1., 0.5, 0.5, 1.5, 1., 1.5, 2.};
	static const double AM2[2] = {0.882792, 0.880351};

	const int idx = onProton? 0 : 9;
	double SIG0[9];
	for (int i = 0; i < 9; ++i) {
		SIG0[i] = 4.893089117 / AM2[int(onProton)] * RATIOJ[i + idx] * BGAMMA[i + idx];
	}
	if (x <= 10.) {
		cross_res = breitwigner(SIG0[0], WIDTH[0 + idx], AMRES[0 + idx], x, onProton) * Ef(x, 0.152, 0.17);
		sig_res[0] = cross_res;
		for (int i = 1; i < 9; ++i) {
			sig_res[i] = breitwigner(SIG0[i], WIDTH[i + idx], AMRES[i + idx], x, onProton) * Ef(x, 0.15, 0.38);
			cross_res += sig_res[i];
		}
		// direct channel
		if ((x > 0.1) && (x < 0.6)) {
			cross_dir1 = 92.7 * Pl(x, 0.152, 0.25, 2.0)  // single pion production
					   + 40. * std::exp(-(x - 0.29) * (x - 0.29) / 0.002)
					   - 15. * std::exp(-(x - 0.37) * (x - 0.37) / 0.002);
		} else {
			cross_dir1 = 92.7 * Pl(x, 0.152, 0.25, 2.0);  // single pion production
		}
		cross_dir2 = 37.7 * Pl(x, 0.4, 0.6, 2.0);  // double pion production
		cross_dir = cross_dir1 + cross_dir2;
	}
	// fragmentation 2:
	double cross_frag2 = onProton? 80.3 : 60.2;
	cross_frag2 *= Ef(x, 0.5, 0.1) * std::pow(s, -0.34);
	// multipion production/fragmentation 1 cross section
	double cs_multidiff = 0.;
	double cs_multi = 0.;
	double cross_diffr1 = 0.;
	double cross_diffr2 = 0.;
	double cross_diffr = 0.;
	if (x > 0.85) {
		double ss1 = (x - 0.85) / 0.69;
		double ss2 = onProton? 29.3 : 26.4;
		ss2 *= std::pow(s, -0.34) + 59.3 * std::pow(s, 0.095);
		cs_multidiff = (1. - std::exp(-ss1)) * ss2;
		cs_multi = 0.89 * cs_multidiff;
		// diffractive scattering:
		cross_diffr1 = 0.099 * cs_multidiff;
		cross_diffr2 = 0.011 * cs_multidiff;
		cross_diffr = 0.11 * cs_multidiff;
		// **************************************
		ss1 = std::pow(x - 0.85, 0.75) / 0.64;
		ss2 = 74.1 * std::pow(x, -0.44) + 62. * std::pow(s, 0.08);
		double cs_tmp = 0.96 * (1. - std::exp(-ss1)) * ss2;
		cross_diffr1 = 0.14 * cs_tmp;
		cross_diffr2 = 0.013 * cs_tmp;
		double cs_delta = cross_frag2 - (cross_diffr1 + cross_diffr2 - cross_diffr);
		if (cs_delta < 0.) {
			cross_frag2 = 0.;
			cs_multi += cs_delta;
		} else {
			cross_frag2 = cs_delta;
		}
		cross_diffr = cross_diffr1 + cross_diffr2;
		cs_multidiff = cs_multi + cross_diffr;
	// in the original SOPHIA code, here is a switch for the return argument.
	// Here, only one case (compare in SOPHIA: NDIR=3) is needed.
	}
	return cross_res + cross_dir + cs_multidiff + cross_frag2;
}

double PhotonFieldSampling::Pl(double x, double xth, double xmax, double alpha) const {
	if (xth > x)
		return 0.;
	const double a = alpha * xmax / xth;
	const double prod1 = std::pow((x - xth) / (xmax - xth), a - alpha);
	const double prod2 = std::pow(x / xmax, -a);
	return prod1 * prod2;
}

double PhotonFieldSampling::Ef(double x, double th, double w) const {
	const double wth = w + th;
	if (x <= th) {
		return 0.;
	} else if ((x > th) && (x < wth)) {
		return (x - th) / w;
	} else if (x >= wth) {
		return 1.;
	} else {
		throw std::runtime_error("error in function Ef");
	}
}

double PhotonFieldSampling::breitwigner(double sigma_0, double Gamma, double DMM, double epsPrime, bool onProton) const {
	const double mass = onProton? 0.93827 : 0.93947;  // Gev/c^2
	const double s = mass * mass + 2. * mass * epsPrime;
	const double gam2s = Gamma * Gamma * s;
	return sigma_0 * (s / epsPrime / epsPrime) * gam2s / ((s - DMM * DMM) * (s - DMM * DMM) + gam2s);
}

double PhotonFieldSampling::functs(double s, bool onProton) const {
	const double mass = onProton? 0.93827 : 0.93947;  // Gev/c^2
	const double factor = s - mass * mass;
	const double epsPrime = factor / 2. / mass;
	const double sigma_pg = crossection(epsPrime, onProton);
	return factor * sigma_pg;
}

} // namespace crpropa
