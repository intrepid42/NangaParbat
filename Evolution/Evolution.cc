//
// APFEL++ 2017
//
// Authors: Valerio Bertone: valerio.bertone@cern.ch
//          Stefano Carrazza: stefano.carrazza@cern.ch
//

#include <apfel/dglapbuilder.h>
#include <apfel/grid.h>
#include <apfel/timer.h>
#include <apfel/tools.h>
#include <apfel/alphaqcd.h>
#include <apfel/tabulateobject.h>
#include <apfel/tmdbuilder.h>
#include <apfel/rotations.h>

#include <cmath>
#include <map>
#include <iomanip>

using namespace apfel;
using namespace std;

// LH Toy PDFs
double xupv(double const& x)  { return 5.107200 * pow(x,0.8) * pow((1-x),3); }
double xdnv(double const& x)  { return 3.064320 * pow(x,0.8) * pow((1-x),4); }
double xglu(double const& x)  { return 1.7 * pow(x,-0.1) * pow((1-x),5); }
double xdbar(double const& x) { return 0.1939875 * pow(x,-0.1) * pow((1-x),6); }
double xubar(double const& x) { return xdbar(x) * (1-x); }
double xsbar(double const& x) { return 0.2 * ( xdbar(x) + xubar(x) ); }
map<int,double> LHToyPDFs(double const& x, double const&)
{
  // Call all functions once.
  const double upv  = xupv (x);
  const double dnv  = xdnv (x);
  const double glu  = xglu (x);
  const double dbar = xdbar(x);
  const double ubar = xubar(x);
  const double sbar = xsbar(x);

  // Construct QCD evolution basis conbinations.
  double const Gluon   = glu;
  double const Singlet = dnv + 2 * dbar + upv + 2 * ubar + 2 * sbar;
  double const T3      = upv + 2 * ubar - dnv - 2 * dbar;
  double const T8      = upv + 2 * ubar + dnv + 2 * dbar - 4 * sbar;
  double const Valence = upv + dnv;
  double const V3      = upv - dnv;

  // Fill in map in the QCD evolution basis.
  map<int,double> QCDEvMap;
  QCDEvMap.insert({0 , Gluon});
  QCDEvMap.insert({1 , Singlet});
  QCDEvMap.insert({2 , Valence});
  QCDEvMap.insert({3 , T3});
  QCDEvMap.insert({4 , V3});
  QCDEvMap.insert({5 , T8});
  QCDEvMap.insert({6 , Valence});
  QCDEvMap.insert({7 , Singlet});
  QCDEvMap.insert({8 , Valence});
  QCDEvMap.insert({9 , Singlet});
  QCDEvMap.insert({10, Valence});
  QCDEvMap.insert({11, Singlet});
  QCDEvMap.insert({12, Valence});

  return QCDEvMap;
}

int main()
{
  // =================================================================
  // Parameters
  // =================================================================
  // x-space grid.
  const Grid g{{SubGrid{100,1e-5,3}, SubGrid{60,1e-1,3}, SubGrid{50,6e-1,3}, SubGrid{50,8e-1,3}}};

  // Initial scale for the collinear PDF evolution.
  const double mui = sqrt(2);

  // Vectors of masses and thresholds.
  const vector<double> Masses = {0, 0, 0, sqrt(2), 4.5, 175};
  const vector<double> Thresholds = Masses;

  // Perturbative order.
  const int PerturbativeOrder = 2;

  // =================================================================
  // Alpha_s
  // =================================================================
  // Running coupling
  const double AlphaQCDRef = 0.35;
  const double MuAlphaQCDRef = sqrt(2);
  AlphaQCD a{AlphaQCDRef, MuAlphaQCDRef, Masses, PerturbativeOrder};
  const TabulateObject<double> TabAlphas{a, 100, 1, 1000001, 3};
  const auto Alphas = [&] (double const& mu) -> double{ return TabAlphas.Evaluate(mu); };

  // =================================================================
  // Collinear PDFs
  // =================================================================
  // Initialize DGLAP objects.
  const auto DglapObj = InitializeDglapObjectsQCD(g, Masses, Thresholds);

  // Construct the DGLAP evolution.
  auto EvolvedPDFs = BuildDglap(DglapObj, LHToyPDFs, mui, PerturbativeOrder, Alphas);

  // Tabulate PDFs
  const TabulateObject<Set<Distribution>> CollPDFs{*EvolvedPDFs, 50, 1, 1000000, 3};

  // =================================================================
  // TMD PDFs
  // =================================================================
  // Initialize TMD objects.
  const auto TmdObj = InitializeTmdObjects(g, Thresholds);

  // Function that returns the scale inital scale mu and the
  // intermediate scale "mu0" as functions of the impact parameter b.
  const auto Mu0b = [] (double const& b) -> double
    {
      const double C0   = 2 * exp(- emc);
      const double bmax = 1;
      return C0 * sqrt( 1 + pow(b/bmax,2) ) / b;
    };
  const auto Mub  = [Mu0b] (double const& b) -> double
    {
      const double Q0 = 1;
      return Mu0b(b);// + Q0;
    };

  // Non-perturbative TMD function.
  const auto fNP = [] (double const& x, double const& b) -> double
    {
      const double Lq = 10;
      const double L1 = 0.2;
      const double fact = Lq * x * b;
      return exp( - fact * b / sqrt( 1 + pow( fact / L1, 2) ) );
    };

  // Get evolved TMDs (this assumes the zeta-prescription).
  const auto EvolvedTMDs = BuildTmdPDFs(TmdObj, DglapObj, CollPDFs, fNP, Mu0b, Mub, PerturbativeOrder, Alphas);

  // Test values.
  const double Vs    = 8000;
  const double Q     = 91.2;
  const double y     = 2.5;
  const double qT    = 5;
  const double x1    = Q * exp(-y) / Vs;
  const double x2    = Q * exp(y) / Vs;
  const double muf   = Q;
  const double zetaf = Q * Q;

  Timer ttot;
  ttot.start();

  const TabulateObject<Set<Distribution>> TabulatedTMDs{[&] (double const& b)->Set<Distribution>{ return EvolvedTMDs(b, muf, zetaf); }, 100, 6e-6, 100, 3, Thresholds, 1e-7};

  // Now construct the integrand of the Fourier (Hankel) tranform,
  // that is the TMD luminosity.
  const auto TMDLumi = [=] (double const& b) -> double
    {
      // Get map of the TMDs in "x1" and "x2" and rotate them into the
      // physical basis.
      map<int,double> TMD1 = QCDEvToPhys(TabulatedTMDs.EvaluateMapxQ(x1, b));
      map<int,double> TMD2 = QCDEvToPhys(TabulatedTMDs.EvaluateMapxQ(x2, b));

      // Construct the combination of TMDs weighted with the charges.
      double lumi = 0;
      for (int i = 1; i <= 6; i++)
	lumi += QCh2[i-1] * ( TMD1[i] * TMD2[-i] + TMD1[-i] * TMD2[i] );

      // Now multuply it by the bessel function j0.
      lumi *= b * j0(b*qT) / 2; 

      return lumi;
    };

  cout << scientific;

  // Performance test.
  Timer t;

  // Zero's of the Bessel function j0.
  const vector<double> j0Zeros{
      2.4048255576957730, 5.5200781102863115, 8.653727912911013,
      11.791534439014281, 14.930917708487787, 18.071063967910924,
      21.211636629879260, 24.352471530749302, 27.493479132040253,
      30.634606468431976, 33.775820213573570, 36.917098353664045,
      40.058425764628240, 43.199791713176730, 46.341188371661815,
      49.482609897397815, 52.624051841115000, 55.765510755019980,
      58.906983926080940, 62.048469190227166, 65.189964800206870,
      68.331469329856800, 71.472981603593740, 74.614500643701830,
      77.756025630388050, 80.897555871137630, 84.039090776938200,
      87.180629843641160, 90.322172637210490, 93.463718781944780,
      96.605267950996260, 99.746819858680600, 102.88837425419480,
      106.02993091645162, 109.17148964980538, 112.31305028049490,
      115.45461265366694, 118.59617663087253, 121.73774208795096,
      124.87930891323295, 128.02087700600833, 131.16244627521390,
      134.30401663830546, 137.44558802028428, 140.58716035285428,
      143.72873357368974, 146.87030762579664, 150.01188245695477,
      153.15345801922788, 156.29503426853353, 159.43661116426316,
      162.57818866894667, 165.71976674795502, 168.86134536923583,
      172.00292450307820, 175.14450412190274, 178.28608420007376,
      181.42766471373105, 184.56924564063870, 187.71082696004936,
      190.85240865258152, 193.99399070010912, 197.13557308566140,
      200.27715579333240};

  // Attempt an integration
  const Integrator integrand{TMDLumi};
  cout << "Integration... ";
  t.start();
  const double TMDLumqT = integrand.integrate(0.000006, 50, j0Zeros, eps7);
  t.stop();

  // Drell-Yan hard cross section
  const auto HardCrossSectionDY = [Alphas,Thresholds] (int const& pt, double const& Qh, double const& mu)->double
    {
      // Compute log anf its powers.
      const double lQ2  = 2 * log( mu / Qh );
      const double lQ22 = lQ2 * lQ2;
      const double lQ23 = lQ22 * lQ2;
      const double lQ24 = lQ23 * lQ2;

      // Compute coupling and its powers.
      const double as  = Alphas(mu) / FourPi;
      const double as2 = as * as;

      // Now compute hard cross section accordi to the perturnative
      // order.
      double hxs = 1;
      if (pt > 0)
	hxs += 2 * as * CF * ( - lQ22 - 3 * lQ2 - 8 + 7 * Pi2 / 6 );
      if (pt > 1)
	hxs += 2 * as2 * CF *
	  ( CF * ( lQ24 + 6 * lQ23 + ( 25 - 7 * Pi2 / 3 ) * lQ22 + ( 93. / 2. - 5 * Pi2 - 24 * zeta3 ) * lQ2 +
		   511. / 8. - 83 * Pi2 / 6 - 30 * zeta3 + 67 * Pi2 * Pi2 / 60 ) +
	    CA * ( - 11 * lQ23 / 9 + ( - 233 / 18 + Pi2 / 3 ) * lQ22 + ( - 2545. / 54. + 22 * Pi2 / 9 + 26 * zeta3 ) * lQ2 +
		   - 51157. / 648. + 1061 * Pi2 / 108 + 313 * zeta3 / 9 - 4 * Pi2 * Pi2 / 45 ) +
	    TR * NF(mu, Thresholds) * ( 4 * lQ23 / 9 + 38 * lQ22 / 9 + ( 418. / 27. - 8 * Pi2 / 9 ) * lQ2 +
					4085. / 162. - 91 * Pi2 / 27 + 4 * zeta3 / 9 ) );

      return hxs;
    };

  const double aslphaem = 1. / 127.;
  const double prefactor = 4 * M_PI * pow(aslphaem,2) / 9 / Vs / Vs / Q / Q;

  cout << "Cross section = " << prefactor * HardCrossSectionDY(PerturbativeOrder, Q, muf) * TMDLumqT << endl;

  cout << "Total computation time per point... ";
  ttot.stop();

  EvolvedTMDs(1, 10, 100);

  return 0;
}

