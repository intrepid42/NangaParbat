//
// Author: Valerio Bertone: valerio.bertone@cern.ch
//

#include "NangaParbat/computetables.h"
#include "NangaParbat/utilities.h"

// LHAPDF
#include <LHAPDF/LHAPDF.h>

// APFEL++
#include <apfel/apfelxx.h>

namespace NangaParbat
{
  //_________________________________________________________________________________
  void ComputeTables(std::string const& configfile, std::string const& datasetfile)
  {
    // Read configuration parameters with YAML
    const YAML::Node config = YAML::LoadFile(configfile);

    // Open LHAPDF set
    LHAPDF::PDF* distpdf = LHAPDF::mkPDF(config["pdfset"]["name"].as<std::string>(), config["pdfset"]["member"].as<int>());

    // Heavy-quark thresholds
    std::vector<double> Thresholds;
    for (auto const& v : distpdf->flavors())
      if (v > 0 && v < 7)
	Thresholds.push_back(distpdf->quarkThreshold(v));

    // Alpha_s
    const auto Alphas = [&] (double const& mu) -> double{ return distpdf->alphasQ(mu); };

    // Set verbosity level of APFEL++
    apfel::SetVerbosityLevel(config["verbosity"].as<int>());

    // x-space grid
    std::vector<apfel::SubGrid> vsg;
    for (auto const& sg : config["xgridpdf"])
      vsg.push_back({sg[0].as<int>(), sg[1].as<double>(), sg[2].as<int>()});
    const apfel::Grid g{vsg};

    // Rotate PDF set into the QCD evolution basis
    const auto RotPDFs = [=] (double const& x, double const& mu) -> std::map<int,double>{ return apfel::PhysToQCDEv(distpdf->xfxQ(x,mu)); };

    // Construct set of distributions as a function of the scale to be
    // tabulated
    const auto EvolvedPDFs = [=,&g] (double const& mu) -> apfel::Set<apfel::Distribution>{
      return apfel::Set<apfel::Distribution>{apfel::EvolutionBasisQCD{apfel::NF(mu, Thresholds)}, DistributionMap(g, RotPDFs, mu)};
    };

    // Tabulate collinear PDFs
    const apfel::TabulateObject<apfel::Set<apfel::Distribution>> TabPDFs{EvolvedPDFs, 100, distpdf->qMin(), distpdf->qMax(), 3, Thresholds};
    const auto CollPDFs = [&] (double const& mu) -> apfel::Set<apfel::Distribution> { return TabPDFs.Evaluate(mu); };

    // Initialize TMD objects
    const auto TmdObj = apfel::InitializeTmdObjects(g, Thresholds);

    // Build evolved TMD PDFs
    const auto EvTMDPDFs = BuildTmdPDFs(TmdObj, CollPDFs, Alphas, config["PerturbativeOrder"].as<int>(), config["TMDscales"]["Ci"].as<double>());

    // Ogata-quadrature object of degree one (required to compute the
    // primitive in qT of the cross section)
    apfel::OgataQuadrature OgataObj{1};

    // Alpha_em
    const apfel::AlphaQED alphaem{config["alphaem"]["aref"].as<double>(), config["alphaem"]["Qref"].as<double>(), Thresholds, {0, 0, 1.777}, 0};

    // Read kinematics from the 'Data' folder
    for (auto const& ds : YAML::LoadFile(datasetfile))
      for (auto const& dist : ds.second)
	{
	  // Timer
	  apfel::Timer t;

	  // Open HEPData datafile name
	  const YAML::Node df = YAML::LoadFile("../Data/" + ds.first.as<std::string>() + "/" + dist.as<std::string>());

	  // C.M.E.
	  double Vs = - 101;

	  // Rapidity interval
	  double ymin = - 101;
	  double ymax = - 101;

	  // Invariant mass interval
	  double Qmin = - 101;
	  double Qmax = - 101;

	  // Transverse momentum bin bounds
	  std::vector<double> qTv;

	  // Read kinematics from the HEPdata yaml file
	  RetrieveKinematics(df, Vs, Qmin, Qmax, ymin, ymax, qTv);

	  // Unscaled coordinates and weights of the degree-one Ogata
	  // quadrature.
	  std::vector<double> z1 = OgataObj.GetCoordinates();
	  std::vector<double> w1 = OgataObj.GetWeights();

	  // Retrieve relevant parameters from the configuration file
	  const double Cf            = config["TMDscales"]["Cf"].as<double>();
	  const int    nOgata        = std::min(config["nOgata"].as<int>(), (int) z1.size());
	  const int    nQ            = config["Qgrid"]["nQ"].as<int>();
	  const int    InterDegreeQ  = config["Qgrid"]["InterDegreeQ"].as<int>();
	  const int    nxi           = config["xigrid"]["nxi"].as<int>();
	  const int    InterDegreexi = config["xigrid"]["InterDegreexi"].as<int>();
	  const double eps           = config["integration_accuracy"].as<double>();

	  // Construct QGrid-like grids for the integration in Q and y
	  const std::vector<double> Qg  = GenerateQGrid(nQ, Qmin, Qmax);
	  const std::vector<double> xig = GenerateQGrid(nxi, exp(ymin), exp(ymax));
	  const apfel::QGrid<double> Qgrid {Qg,  InterDegreeQ};
	  const apfel::QGrid<double> xigrid{xig, InterDegreexi};

	  // Open output file
	  const std::string outfile = "../Tables/" + ds.first.as<std::string>() + "_" + dist.as<std::string>();
	  std::ofstream fout(outfile);

	  // Emit through YAML
	  YAML::Emitter emitter;
	  emitter.SetFloatPrecision(8);
	  emitter.SetDoublePrecision(8);

	  // Write kinematics
	  emitter << YAML::BeginMap;
	  emitter << YAML::Comment("Kinematics and grid information");
	  emitter << YAML::Key << "CME" << YAML::Value << Vs;
	  emitter << YAML::Key << "qT_bounds" << YAML::Value << YAML::Flow << qTv;
	  emitter << YAML::Key << "Ogata_coordinates" << YAML::Value << YAML::Flow
		  << std::vector<double>(z1.begin(), z1.begin() + std::min(config["nOgata"].as<int>(), (int) z1.size()));
	  emitter << YAML::Key << "Qgrid" << YAML::Value << YAML::Flow << std::vector<double>(Qg.begin(), Qg.end() - 1);
	  emitter << YAML::Key << "xigrid" << YAML::Value << YAML::Flow << std::vector<double>(xig.begin(), xig.end() - 1);
	  emitter << YAML::EndMap;
	  emitter << YAML::Newline;
	  emitter << YAML::BeginMap;
	  emitter << YAML::Comment("Weights");
	  // Loop over the qT bin bounds
	  for (auto const& qT : qTv)
	    {
	      // Container of the weights
	      std::vector<std::vector<std::vector<double>>> W(nOgata, std::vector<std::vector<double>>(nQ, std::vector<double>(nxi)));
	      // Loop over the Ogata-quadrature points
	      for (int n = 0; n < nOgata; n++)
		{
		  // Get impact parameter 'b' and 'b*'
		  const double b  = z1[n] / qT;
		  const double bs = bstar(b, config["bstar"]["bmax"].as<double>());

		  // Tabulate TMDs in Q
		  const auto EvolvedTMDPDFs = [&] (double const& Q) -> apfel::Set<apfel::Distribution>{ return EvTMDPDFs(bs, Cf * Q, Q * Q); };
		  const apfel::TabulateObject<apfel::Set<apfel::Distribution>> TabEvolvedTMDPDFs{EvolvedTMDPDFs, Qg, InterDegreeQ};

		  // Loop over the grid in Q
		  //std::vector<std::vector<double>> W(nQ, std::vector<double>(nxi));
		  for (int tau = 0; tau < nQ; tau++)
		    // Loop over the grid in xi
		    for (int alpha = 0; alpha < nxi; alpha++)
		      {
			// Function to be integrated in Q
			const auto Qintegrand = [&] (double const& Q) -> double
			  {
			    // Renormalisation scale
			    const double muf = Cf * Q;

			    // Number of active flavours at 'Q'
			    const int nf = apfel::NF(muf, Thresholds);

			    // EW charges
			    const std::vector<double> Bq = apfel::ElectroWeakCharges(Q, true);

			    // Get Evolved TMD PDFs and rotate them into the physical basis
			    const std::map<int,apfel::Distribution> xF = QCDEvToPhys(TabEvolvedTMDPDFs.Evaluate(Q).GetObjects());

			    // Function to be integrated in xi
			    const auto xiintegrand = [&] (double const& xi) -> double
			    {
			      // Compute 'x1' and 'x2'
			      const double x1 = Q * xi / Vs;
			      const double x2 = Q / xi / Vs;

			      // Get interpolating function in Q
			      const double Ixi = xigrid.Interpolant(0, alpha, xi);

			      // Combine TMDs through the EW charges
			      double integrand = 0;
			      for (int i = 1; i <= nf; i++)
				integrand += Bq[i-1] * ( xF.at(i).Evaluate(x1) * xF.at(-i).Evaluate(x2) + xF.at(-i).Evaluate(x1) * xF.at(i).Evaluate(x2) );

			      // Return xi integrand
			      return Ixi * integrand / xi;
			    };

			    // Get integration bounds and fixed points in xi
			    const int iximin = std::max(alpha - InterDegreexi, 0);
			    const int iximax = std::min(alpha + 1, nxi);
			    std::vector<double> FixPtsxi(xig.begin() + iximin + 1, xig.begin() + std::max(iximax - 1, iximin + 1));

			    // Perform the integral in xi
			    const apfel::Integrator xiIntObj{xiintegrand};
			    const double xiintegral = xiIntObj.integrate(xig[iximin], xig[iximax], FixPtsxi, eps);

			    // Get interpolating function in Q
			    const double IQ = Qgrid.Interpolant(0, tau, Q);

			    // Electromagnetic coupling squared
			    const double aem2 = pow((config["alphaem"]["run"].as<bool>() ? alphaem.Evaluate(Q) : config["alphaem"]["ref"].as<double>()), 2);

			    // Compute hard factor
			    const double hcs = apfel::HardFactorDY(config["PerturbativeOrder"].as<int>(), Alphas(muf), nf, Cf);

			    // Return Q integrand
			    return IQ * aem2 * hcs * xiintegral / pow(Q, 3);
			  };

			// Get integration bounds and fixed points in Q
			const int iQmin = std::max(tau - InterDegreeQ, 0);
			const int iQmax = std::min(tau + 1, nQ);
			std::vector<double> FixPtsQ(Qg.begin() + iQmin + 1, Qg.begin() + std::max(iQmax - 1, iQmin + 1));

			// Perform the integral in Q
			const apfel::Integrator QIntObj{Qintegrand};
			const double Qintegral = QIntObj.integrate(Qg[iQmin], Qg[iQmax], FixPtsQ, eps);

			// Compute weight
			W[n][tau][alpha] = apfel::ConvFact * 8 * M_PI * w1[n] * Qintegral / 9;
		      }
		}
	      emitter << YAML::Key << qT;
	      emitter << YAML::Value << YAML::Flow << W;
	    }
	  emitter << YAML::EndMap;

	  // Dump emitter to file
	  fout << emitter.c_str();
	  fout.close();
	  t.stop();
	}
  }
}