// emulator-libretro_test.cc - Test for LibretroEmulator
// Build: make emulator_libretro_test

#include "emulator-libretro.h"
#include <cassert>
#include <format>
#include <iostream>
#include <string>
#include <vector>

constexpr const char* CORE_PATH = "/tmp/fceumm_libretro.so";
constexpr const char* ROM_PATH = "smb.nes";

void test_basic_init() {
  bool ok = EmulatorLibretro::Initialize(CORE_PATH, ROM_PATH);
  if (!ok) {
    std::cerr << "Failed to initialize - make sure core and ROM exist\n";
    return;
  }
  
  std::cout << std::format("Core: {} v{}\n", 
    EmulatorLibretro::GetCoreName(),
    EmulatorLibretro::GetCoreVersion());
  
  std::cout << "PASS: test_basic_init\n";
}

void test_get_memory() {
  std::vector<std::uint8_t> mem;
  EmulatorLibretro::GetMemory(&mem);
  
  std::cout << std::format("RAM size: {} bytes\n", mem.size());
  assert(mem.size() == 2048);  // NES RAM is 2KB
  
  std::cout << "PASS: test_get_memory\n";
}

void test_step() {
  // Run some frames with no input
  for (int i = 0; i < 60; i++) {
    EmulatorLibretro::Step(0);
  }
  
  // Get RAM checksum
  auto csum = EmulatorLibretro::RamChecksum();
  std::cout << std::format("RAM checksum after 60 frames: {:016x}\n", csum);
  
  // Run more frames with some input
  for (int i = 0; i < 60; i++) {
    EmulatorLibretro::Step(0x80); // Right button
  }
  
  auto csum2 = EmulatorLibretro::RamChecksum();
  std::cout << std::format("RAM checksum after 120 frames: {:016x}\n", csum2);
  
  // Checksums should differ (game state changed)
  assert(csum != csum2);
  
  std::cout << "PASS: test_step\n";
}

void test_save_load() {
  // Get current state
  std::vector<std::uint8_t> state1;
  EmulatorLibretro::Save(&state1);
  std::cout << std::format("Compressed state size: {} bytes\n", state1.size());
  
  // Get RAM before
  std::vector<std::uint8_t> ram_before;
  EmulatorLibretro::GetMemory(&ram_before);
  
  // Run more frames
  for (int i = 0; i < 100; i++) {
    EmulatorLibretro::Step(0);
  }
  
  // Get RAM after (should differ)
  std::vector<std::uint8_t> ram_after;
  EmulatorLibretro::GetMemory(&ram_after);
  assert(ram_before != ram_after);
  
  // Load state
  EmulatorLibretro::Load(&state1);
  
  // Get RAM restored
  std::vector<std::uint8_t> ram_restored;
  EmulatorLibretro::GetMemory(&ram_restored);
  
  // Should match original
  assert(ram_before == ram_restored);
  
  std::cout << "PASS: test_save_load\n";
}

void test_save_uncompressed() {
  std::vector<std::uint8_t> uncomp;
  EmulatorLibretro::SaveUncompressed(&uncomp);
  std::cout << std::format("Uncompressed state size: {} bytes\n", uncomp.size());
  
  // Run some frames
  for (int i = 0; i < 10; i++) {
    EmulatorLibretro::Step(0x01); // A button
  }
  
  // Restore
  EmulatorLibretro::LoadUncompressed(&uncomp);
  
  // Verify
  std::vector<std::uint8_t> check;
  EmulatorLibretro::SaveUncompressed(&check);
  assert(uncomp == check);
  
  std::cout << "PASS: test_save_uncompressed\n";
}

void test_stepfull_and_image() {
  EmulatorLibretro::StepFull(0);
  
  std::vector<std::uint8_t> rgba;
  EmulatorLibretro::GetImage(&rgba);
  
  std::cout << std::format("Image size: {} bytes (expected {})\n", 
    rgba.size(), 256 * 256 * 4);
  assert(rgba.size() == 256 * 256 * 4);
  
  // Check for non-black pixels (game should be rendering something)
  int nonblack = 0;
  for (size_t i = 0; i < rgba.size(); i += 4) {
    if (rgba[i] != 0 || rgba[i+1] != 0 || rgba[i+2] != 0) {
      nonblack++;
    }
  }
  std::cout << std::format("Non-black pixels: {}\n", nonblack);
  assert(nonblack > 0);
  
  std::cout << "PASS: test_stepfull_and_image\n";
}

void test_audio() {
  // Run several frames to generate audio
  for (int i = 0; i < 10; i++) {
    EmulatorLibretro::StepFull(0);
  }
  
  std::vector<std::int16_t> wav;
  EmulatorLibretro::GetSound(&wav);
  
  std::cout << std::format("Audio samples: {}\n", wav.size());
  assert(!wav.empty());
  
  std::cout << "PASS: test_audio\n";
}

void test_cache() {
  EmulatorLibretro::ResetCache(1000, 100);
  
  // Run some caching steps
  std::vector<std::uint8_t> start_state;
  EmulatorLibretro::SaveUncompressed(&start_state);
  
  for (int i = 0; i < 100; i++) {
    EmulatorLibretro::CachingStep(0);
  }
  
  // Restore to start
  EmulatorLibretro::LoadUncompressed(&start_state);
  
  // Run same steps again - should hit cache
  for (int i = 0; i < 100; i++) {
    EmulatorLibretro::CachingStep(0);
  }
  
  EmulatorLibretro::PrintCacheStats();
  
  std::cout << "PASS: test_cache\n";
}

void test_shutdown() {
  EmulatorLibretro::Shutdown();
  std::cout << "PASS: test_shutdown\n";
}

int main(int argc, char* argv[]) {
  std::cout << "=== EmulatorLibretro Tests ===\n\n";
  
  const char* core = (argc > 1) ? argv[1] : CORE_PATH;
  const char* rom = (argc > 2) ? argv[2] : ROM_PATH;
  
  // Initialize first
  if (!EmulatorLibretro::Initialize(core, rom)) {
    std::cerr << "Usage: " << argv[0] << " [core.so] [rom.nes]\n";
    return 1;
  }
  
  std::cout << "Initialized successfully\n\n";
  
  test_get_memory();
  test_step();
  test_save_load();
  test_save_uncompressed();
  test_stepfull_and_image();
  test_audio();
  test_cache();
  test_shutdown();
  
  std::cout << "\n=== All tests passed ===\n";
  return 0;
}
