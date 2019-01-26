//
// Author: Valerio Bertone: valerio.bertone@cern.ch
//

#include "NangaParbat/chisquare.h"

#include <math.h>
#include <iostream>
#include <apfel/apfelxx.h>

// Davies-Webber-Stirling Parameterisation derived from the
// "Parameterisation" mother class
class DWS: public NangaParbat::Parameterisation
{
public:
  DWS(): Parameterisation{2, std::vector<double>(2, 1.)} { };

  double Evaluate(double const& x, double const& b, double const& zeta, int const& ifunc) const
  {
    if (ifunc < 0 || ifunc >= this->_nfuncs)
      throw std::runtime_error("[DWS::Evaluate]: function index out of range");

    const double g1  = this->_pars[0];
    const double g2  = this->_pars[1];
    const double Q02 = 3;

    return exp( - ( g1 + g2 * log(zeta / Q02) ) * pow(b, 2) );
  };
};

//_________________________________________________________________________________
// Main program
int main()
{
  // Allocate "Parameterisation" derived object
  DWS NPFunc{};

  // Datafile
  const NangaParbat::DataHandler DHand{"CDF_Run_I", YAML::LoadFile("../data/HEPData-ins505738-v1-yaml/Table1.yaml")};

  // Convolution table
  const NangaParbat::ConvolutionTable CTable{YAML::LoadFile("../tables/CDF_Run_I.yaml")};

  // Define "ChiSquare" object
  NangaParbat::ChiSquare chi2{NPFunc};

  // Append dataset to the chi2 object
  chi2.AddBlock(std::make_pair(DHand, CTable));

  // Compute chi2
  std::cout << "chi2 = " << chi2() << std::endl;

  // Performance test
  apfel::Timer t;
  const int n = 100;
  for (int i = 0; i < n; i++)
      chi2();
  std::cout << "Evaluating chi2 " << n << " times... ";
  t.stop(true);

  return 0;
}
