// emulator-libretro.h - Libretro-based emulator backend
// Drop-in replacement for FCEU-based emulator
// Provides same static interface as original Emulator class

#ifndef __EMULATOR_LIBRETRO_H
#define __EMULATOR_LIBRETRO_H

#include <cstdint>
#include <string>
#include <vector>

struct EmulatorLibretro {
  // Initialize with core and ROM paths
  // core_path: path to libretro core .so file (e.g., fceumm_libretro.so)
  // rom_path: path to ROM file (e.g., smb.nes)
  static bool Initialize(const std::string& core_path, const std::string& rom_path);
  
  // Alternative: initialize with just ROM, using default NES core
  static bool Initialize(const std::string& rom_path);
  
  static void Shutdown();

  // Save/Load state (compressed)
  static void Save(std::vector<std::uint8_t>* out);
  static void Load(std::vector<std::uint8_t>* in);

  // Make one emulator step with the given input.
  // Bits from MSB to LSB are RLDUTSBA (same as FCEU)
  static void Step(std::uint8_t inputs);

  // Copy the 0x800 bytes of RAM
  static void GetMemory(std::vector<std::uint8_t>* mem);

  // Step with video/audio processing
  static void StepFull(std::uint8_t inputs);

  // Get image - 256x256 RGBA (same as FCEU output format)
  static void GetImage(std::vector<std::uint8_t>* rgba);

  // Get sound - signed 16-bit mono samples
  static void GetSound(std::vector<std::int16_t>* wav);

  // RAM checksum (for caching)
  static std::uint64_t RamChecksum();

  // Cache management (same API as FCEU version)
  static void ResetCache(std::uint64_t numstates, std::uint64_t slop = 10000ULL);
  static void CachingStep(std::uint8_t input);
  static void PrintCacheStats();

  // Uncompressed save/load (for caching)
  static void GetBasis(std::vector<std::uint8_t>* out);
  static void SaveUncompressed(std::vector<std::uint8_t>* out);
  static void LoadUncompressed(std::vector<std::uint8_t>* in);

  // Extended save/load with basis (for compression)
  static void SaveEx(std::vector<std::uint8_t>* out, const std::vector<std::uint8_t>* basis);
  static void LoadEx(std::vector<std::uint8_t>* in, const std::vector<std::uint8_t>* basis);

  // Get core and ROM info
  static const std::string& GetCoreName();
  static const std::string& GetCoreVersion();
};

#endif // __EMULATOR_LIBRETRO_H
