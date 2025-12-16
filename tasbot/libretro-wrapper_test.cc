// libretro-wrapper_test.cc - Test for Libretro wrapper
// Build: clang++ -std=c++23 -o libretro_test libretro-wrapper_test.cc libretro-wrapper.cc -ldl

#include "libretro-wrapper.h"
#include <cassert>
#include <cstdio>
#include <format>
#include <iostream>

void test_no_core() {
  LibretroWrapper wrapper;
  assert(!wrapper.IsCoreLoaded());
  assert(!wrapper.IsROMLoaded());
  assert(wrapper.GetCoreInfo() == std::nullopt);
  assert(wrapper.GetAVInfo() == std::nullopt);
  assert(wrapper.GetRAMSize() == 0);
  assert(wrapper.GetStateSize() == 0);
  std::cout << "PASS: test_no_core\n";
}

void test_bad_core_path() {
  LibretroWrapper wrapper;
  auto err = wrapper.LoadCore("/nonexistent/path.so");
  assert(err == LibretroError::CoreLoadFailed);
  assert(!wrapper.IsCoreLoaded());
  std::cout << "PASS: test_bad_core_path\n";
}

void test_move_semantics() {
  LibretroWrapper w1;
  LibretroWrapper w2 = std::move(w1);
  LibretroWrapper w3;
  w3 = std::move(w2);
  assert(!w3.IsCoreLoaded());
  std::cout << "PASS: test_move_semantics\n";
}

void test_input_conversion() {
  // Test LibretroUtil functions
  using namespace LibretroUtil;
  
  constexpr std::uint8_t mask = 0xFF; // All buttons pressed
  
  assert(IsPressed(mask, JOYPAD_A));
  assert(IsPressed(mask, JOYPAD_B));
  assert(IsPressed(mask, JOYPAD_SELECT));
  assert(IsPressed(mask, JOYPAD_START));
  assert(IsPressed(mask, JOYPAD_UP));
  assert(IsPressed(mask, JOYPAD_DOWN));
  assert(IsPressed(mask, JOYPAD_LEFT));
  assert(IsPressed(mask, JOYPAD_RIGHT));
  
  constexpr std::uint8_t empty = 0;
  assert(!IsPressed(empty, JOYPAD_A));
  assert(!IsPressed(empty, JOYPAD_B));
  
  constexpr std::uint8_t just_a = LibretroInput::A;
  assert(IsPressed(just_a, JOYPAD_A));
  assert(!IsPressed(just_a, JOYPAD_B));
  
  std::cout << "PASS: test_input_conversion\n";
}

#ifdef TEST_WITH_CORE
// These tests require a working NES core (e.g., FCEUmm or Nestopia)
// Run with: ./libretro_test /path/to/fceumm_libretro.so /path/to/game.nes

void test_core_load(const char* core_path) {
  LibretroWrapper wrapper;
  auto err = wrapper.LoadCore(core_path);
  if (err != LibretroError::OK) {
    std::cerr << "Failed to load core (expected if no core available)\n";
    return;
  }
  
  assert(wrapper.IsCoreLoaded());
  auto info = wrapper.GetCoreInfo();
  assert(info.has_value());
  std::cout << std::format("Core: {} v{}\n", info->library_name, info->library_version);
  std::cout << std::format("Extensions: {}\n", info->valid_extensions);
  std::cout << "PASS: test_core_load\n";
}

void test_rom_load(const char* core_path, const char* rom_path) {
  LibretroWrapper wrapper;
  auto err = wrapper.LoadCore(core_path);
  if (err != LibretroError::OK) {
    std::cerr << "Skipping ROM test (no core)\n";
    return;
  }
  
  err = wrapper.LoadROM(rom_path);
  if (err != LibretroError::OK) {
    std::cerr << "Failed to load ROM\n";
    return;
  }
  
  assert(wrapper.IsROMLoaded());
  auto av = wrapper.GetAVInfo();
  assert(av.has_value());
  std::cout << std::format("Resolution: {}x{}\n", av->base_width, av->base_height);
  std::cout << std::format("FPS: {:.2f}, Sample Rate: {:.0f}\n", av->fps, av->sample_rate);
  
  auto ram = wrapper.GetRAM();
  std::cout << std::format("RAM size: {} bytes\n", ram.size());
  
  size_t state_size = wrapper.GetStateSize();
  std::cout << std::format("State size: {} bytes\n", state_size);
  
  std::cout << "PASS: test_rom_load\n";
}

void test_run_frame(const char* core_path, const char* rom_path) {
  LibretroWrapper wrapper;
  if (wrapper.LoadCore(core_path) != LibretroError::OK) return;
  if (wrapper.LoadROM(rom_path) != LibretroError::OK) return;
  
  int frame_count = 0;
  wrapper.SetVideoCallback([&frame_count](const FrameBuffer& fb) {
    ++frame_count;
  });
  
  // Run 60 frames
  for (int i = 0; i < 60; ++i) {
    wrapper.Run();
  }
  
  std::cout << std::format("Ran {} frames\n", frame_count);
  assert(frame_count >= 59); // Allow for frame skipping
  std::cout << "PASS: test_run_frame\n";
}

void test_save_state(const char* core_path, const char* rom_path) {
  LibretroWrapper wrapper;
  if (wrapper.LoadCore(core_path) != LibretroError::OK) return;
  if (wrapper.LoadROM(rom_path) != LibretroError::OK) return;
  
  // Run some frames
  for (int i = 0; i < 100; ++i) {
    wrapper.Run();
  }
  
  // Save state
  size_t size = wrapper.GetStateSize();
  std::vector<std::uint8_t> state(size);
  assert(wrapper.SaveState(state));
  
  // Get RAM snapshot
  auto ram = wrapper.GetRAM();
  std::vector<std::uint8_t> ram_before(ram.begin(), ram.end());
  
  // Run more frames
  for (int i = 0; i < 100; ++i) {
    wrapper.Run();
  }
  
  // RAM should have changed
  ram = wrapper.GetRAM();
  bool ram_changed = false;
  for (size_t i = 0; i < ram.size() && i < ram_before.size(); ++i) {
    if (ram[i] != ram_before[i]) {
      ram_changed = true;
      break;
    }
  }
  assert(ram_changed);
  
  // Load state
  assert(wrapper.LoadState(state));
  
  // RAM should match saved state
  ram = wrapper.GetRAM();
  bool ram_matches = true;
  for (size_t i = 0; i < ram.size() && i < ram_before.size(); ++i) {
    if (ram[i] != ram_before[i]) {
      ram_matches = false;
      break;
    }
  }
  assert(ram_matches);
  
  std::cout << "PASS: test_save_state\n";
}
#endif

int main(int argc, char* argv[]) {
  std::cout << "=== LibretroWrapper Tests ===\n\n";
  
  // Basic tests that don't need a core
  test_no_core();
  test_bad_core_path();
  test_move_semantics();
  test_input_conversion();
  
#ifdef TEST_WITH_CORE
  if (argc >= 3) {
    std::cout << "\n=== Tests with Core ===\n";
    test_core_load(argv[1]);
    test_rom_load(argv[1], argv[2]);
    test_run_frame(argv[1], argv[2]);
    test_save_state(argv[1], argv[2]);
  } else {
    std::cout << "\nSkipping core tests (pass core.so and rom.nes as args)\n";
  }
#endif
  
  std::cout << "\n=== All basic tests passed ===\n";
  return 0;
}
