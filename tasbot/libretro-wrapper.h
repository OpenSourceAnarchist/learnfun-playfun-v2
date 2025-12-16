// libretro-wrapper.h - C++23 wrapper for Libretro cores
// Provides a type-safe interface to dynamically loaded Libretro cores
// Part of the tasbot migration from static fceu to modular Libretro

#ifndef LIBRETRO_WRAPPER_H
#define LIBRETRO_WRAPPER_H

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <memory>

// Forward declare to avoid including heavy libretro.h everywhere
struct retro_game_info;
struct retro_system_info;
struct retro_system_av_info;

// Input bitmask matching FCEU format: RLDUTSBA
// Right=0x80, Left=0x40, Down=0x20, Up=0x10, Start=0x08, Select=0x04, B=0x02, A=0x01
namespace LibretroInput {
  constexpr std::uint8_t A      = 0x01;
  constexpr std::uint8_t B      = 0x02;
  constexpr std::uint8_t SELECT = 0x04;
  constexpr std::uint8_t START  = 0x08;
  constexpr std::uint8_t UP     = 0x10;
  constexpr std::uint8_t DOWN   = 0x20;
  constexpr std::uint8_t LEFT   = 0x40;
  constexpr std::uint8_t RIGHT  = 0x80;
}

// Result type for operations that can fail
enum class LibretroError {
  OK,
  CoreNotLoaded,
  CoreLoadFailed,
  ROMLoadFailed,
  SerializationFailed,
  MemoryAccessFailed,
  InvalidState,
};

// Video frame data
struct FrameBuffer {
  std::span<const std::uint8_t> data;
  unsigned width;
  unsigned height;
  unsigned pitch;
};

// Audio sample batch
struct AudioBuffer {
  std::span<const std::int16_t> samples;
  size_t frames;
};

// Core information
struct CoreInfo {
  std::string library_name;
  std::string library_version;
  std::string valid_extensions;
  bool need_fullpath;
  bool block_extract;
};

// AV (Audio/Video) information
struct AVInfo {
  unsigned base_width;
  unsigned base_height;
  unsigned max_width;
  unsigned max_height;
  double aspect_ratio;
  double fps;
  double sample_rate;
};

class LibretroWrapper {
public:
  LibretroWrapper();
  ~LibretroWrapper();
  
  // Disable copy, allow move
  LibretroWrapper(const LibretroWrapper&) = delete;
  LibretroWrapper& operator=(const LibretroWrapper&) = delete;
  LibretroWrapper(LibretroWrapper&&) noexcept;
  LibretroWrapper& operator=(LibretroWrapper&&) noexcept;

  // Core lifecycle
  [[nodiscard]] LibretroError LoadCore(std::string_view core_path);
  void UnloadCore();
  [[nodiscard]] bool IsCoreLoaded() const;
  
  // ROM lifecycle
  [[nodiscard]] LibretroError LoadROM(std::string_view rom_path);
  void UnloadROM();
  [[nodiscard]] bool IsROMLoaded() const;
  
  // Core information
  [[nodiscard]] std::optional<CoreInfo> GetCoreInfo() const;
  [[nodiscard]] std::optional<AVInfo> GetAVInfo() const;
  
  // Emulation control
  void Reset();
  void Run();
  
  // Input (FCEU-compatible bitmask: RLDUTSBA)
  void SetInput(unsigned port, std::uint8_t input);
  
  // Memory access (NES RAM is typically 0x800 bytes)
  [[nodiscard]] std::span<std::uint8_t> GetRAM();
  [[nodiscard]] std::span<const std::uint8_t> GetRAM() const;
  [[nodiscard]] size_t GetRAMSize() const;
  
  // Save states
  [[nodiscard]] size_t GetStateSize() const;
  [[nodiscard]] bool SaveState(std::span<std::uint8_t> buffer);
  [[nodiscard]] bool LoadState(std::span<const std::uint8_t> buffer);
  
  // Callbacks (set these before calling Run)
  using VideoCallback = std::function<void(const FrameBuffer&)>;
  using AudioCallback = std::function<void(const AudioBuffer&)>;
  
  void SetVideoCallback(VideoCallback cb);
  void SetAudioCallback(AudioCallback cb);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Utility: Convert FCEU input mask to Libretro discrete inputs
namespace LibretroUtil {
  // FCEU uses RLDUTSBA, Libretro uses discrete button IDs
  // This provides conversion helpers
  
  // Libretro joypad button IDs (from libretro.h RETRO_DEVICE_ID_JOYPAD_*)
  enum JoypadButton : unsigned {
    JOYPAD_B = 0,
    JOYPAD_Y = 1,
    JOYPAD_SELECT = 2,
    JOYPAD_START = 3,
    JOYPAD_UP = 4,
    JOYPAD_DOWN = 5,
    JOYPAD_LEFT = 6,
    JOYPAD_RIGHT = 7,
    JOYPAD_A = 8,
    JOYPAD_X = 9,
  };
  
  // Convert FCEU bitmask to individual button state
  constexpr bool IsPressed(std::uint8_t fceu_mask, JoypadButton btn) {
    switch (btn) {
      case JOYPAD_A:      return (fceu_mask & LibretroInput::A) != 0;
      case JOYPAD_B:      return (fceu_mask & LibretroInput::B) != 0;
      case JOYPAD_SELECT: return (fceu_mask & LibretroInput::SELECT) != 0;
      case JOYPAD_START:  return (fceu_mask & LibretroInput::START) != 0;
      case JOYPAD_UP:     return (fceu_mask & LibretroInput::UP) != 0;
      case JOYPAD_DOWN:   return (fceu_mask & LibretroInput::DOWN) != 0;
      case JOYPAD_LEFT:   return (fceu_mask & LibretroInput::LEFT) != 0;
      case JOYPAD_RIGHT:  return (fceu_mask & LibretroInput::RIGHT) != 0;
      default:            return false;
    }
  }
}

#endif // LIBRETRO_WRAPPER_H
