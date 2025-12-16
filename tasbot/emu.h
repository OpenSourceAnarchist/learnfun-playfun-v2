// emu.h - Emulator abstraction layer
// Allows switching between FCEU and Libretro backends at compile time
// Use -DUSE_LIBRETRO to select the Libretro backend

#ifndef __EMU_H
#define __EMU_H

#ifdef USE_LIBRETRO
  #include "emulator-libretro.h"
  using Emu = EmulatorLibretro;
  #define EMU_BACKEND "Libretro"
#else
  #include "emulator.h"
  using Emu = Emulator;
  #define EMU_BACKEND "FCEU"
#endif

#endif // __EMU_H
