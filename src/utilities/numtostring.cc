//
// Author: Valerio Bertone: valerio.bertone@cern.ch
//

#include "NangaParbat/numtostring.h"
#include <iostream>

namespace NangaParbat
{
  //_________________________________________________________________________________
  std::string num_to_string(int const& i, int const& len)
  {
    std::string str = std::to_string(i);
    const int bf = len - str.size();
    if (bf < 0)
      std::cout << "[num_to_string]: The input integer exceeds the string length." << std::endl;
    for (int j = 0; j < bf; j++)
      str = "0" + str;
    return str;
  }
}
