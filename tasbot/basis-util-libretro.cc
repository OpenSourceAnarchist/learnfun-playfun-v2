// basis-util-libretro.cc - Basis computation using Libretro emulator

#include "basis-util.h"

#include <format>
#include <iostream>

#include "emulator-libretro.h"
#include "../cc-lib/util.h"

vector<uint8> BasisUtil::LoadOrComputeBasis(const vector<uint8>& inputs,
                                            int frame,
                                            const string& basisfile) {
  if (Util::ExistsFile(basisfile)) {
    std::cerr << std::format("Loading basis file {}.\n", basisfile);
    return Util::ReadFileBytes(basisfile);
  }

  std::cerr << std::format("Computing basis file {}.\n", basisfile);
  vector<uint8> start;
  EmulatorLibretro::Save(&start);
  
  for (size_t i = 0; i < static_cast<size_t>(frame) && i < inputs.size(); i++) {
    EmulatorLibretro::Step(inputs[i]);
  }
  
  vector<uint8> basis;
  EmulatorLibretro::GetBasis(&basis);
  
  if (!Util::WriteFileBytes(basisfile, basis)) {
    std::cerr << std::format("Couldn't write to {}\n", basisfile);
    abort();
  }
  std::cerr << "Written.\n";

  // Rewind
  EmulatorLibretro::Load(&start);
  return basis;
}
