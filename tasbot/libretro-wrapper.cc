// libretro-wrapper.cc - C++23 implementation of Libretro wrapper
// Dynamically loads Libretro cores and provides type-safe C++ interface

#include "libretro-wrapper.h"

#include <cstring>
#include <dlfcn.h>
#include <format>
#include <iostream>
#include <fstream>

// Prevent redefinition of RETRO_API for dynamic loading
#define RETRO_IMPORT_SYMBOLS
#include "../libretro-common/include/libretro.h"

// Function pointer types for dynamically loaded core functions
using retro_init_t = void (*)();
using retro_deinit_t = void (*)();
using retro_api_version_t = unsigned (*)();
using retro_get_system_info_t = void (*)(struct retro_system_info*);
using retro_get_system_av_info_t = void (*)(struct retro_system_av_info*);
using retro_set_environment_t = void (*)(retro_environment_t);
using retro_set_video_refresh_t = void (*)(retro_video_refresh_t);
using retro_set_audio_sample_t = void (*)(retro_audio_sample_t);
using retro_set_audio_sample_batch_t = void (*)(retro_audio_sample_batch_t);
using retro_set_input_poll_t = void (*)(retro_input_poll_t);
using retro_set_input_state_t = void (*)(retro_input_state_t);
using retro_set_controller_port_device_t = void (*)(unsigned, unsigned);
using retro_reset_t = void (*)();
using retro_run_t = void (*)();
using retro_serialize_size_t = size_t (*)();
using retro_serialize_t = bool (*)(void*, size_t);
using retro_unserialize_t = bool (*)(const void*, size_t);
using retro_load_game_t = bool (*)(const struct retro_game_info*);
using retro_unload_game_t = void (*)();
using retro_get_memory_data_t = void* (*)(unsigned);
using retro_get_memory_size_t = size_t (*)(unsigned);

// Implementation struct - defined in anonymous namespace for internal linkage
// but aliased via LibretroWrapper::Impl for external interface
namespace {

struct ImplData {
  void* core_handle = nullptr;
  bool rom_loaded = false;
  
  // Function pointers
  retro_init_t fn_init = nullptr;
  retro_deinit_t fn_deinit = nullptr;
  retro_api_version_t fn_api_version = nullptr;
  retro_get_system_info_t fn_get_system_info = nullptr;
  retro_get_system_av_info_t fn_get_system_av_info = nullptr;
  retro_set_environment_t fn_set_environment = nullptr;
  retro_set_video_refresh_t fn_set_video_refresh = nullptr;
  retro_set_audio_sample_t fn_set_audio_sample = nullptr;
  retro_set_audio_sample_batch_t fn_set_audio_sample_batch = nullptr;
  retro_set_input_poll_t fn_set_input_poll = nullptr;
  retro_set_input_state_t fn_set_input_state = nullptr;
  retro_set_controller_port_device_t fn_set_controller_port_device = nullptr;
  retro_reset_t fn_reset = nullptr;
  retro_run_t fn_run = nullptr;
  retro_serialize_size_t fn_serialize_size = nullptr;
  retro_serialize_t fn_serialize = nullptr;
  retro_unserialize_t fn_unserialize = nullptr;
  retro_load_game_t fn_load_game = nullptr;
  retro_unload_game_t fn_unload_game = nullptr;
  retro_get_memory_data_t fn_get_memory_data = nullptr;
  retro_get_memory_size_t fn_get_memory_size = nullptr;
  
  // Cached info
  retro_system_info sys_info{};
  retro_system_av_info av_info{};
  
  // Current input state per port (FCEU bitmask format)
  std::uint8_t input_state[2] = {0, 0};
  
  // Current frame data (owned by core, valid until next retro_run)
  const void* frame_data = nullptr;
  unsigned frame_width = 0;
  unsigned frame_height = 0;
  size_t frame_pitch = 0;
  
  // Audio buffer (accumulated during retro_run)
  std::vector<std::int16_t> audio_buffer;
  
  // User callbacks
  LibretroWrapper::VideoCallback video_cb;
  LibretroWrapper::AudioCallback audio_cb;
  
  template<typename T>
  bool LoadSymbol(T& ptr, const char* name) {
    ptr = reinterpret_cast<T>(dlsym(core_handle, name));
    return ptr != nullptr;
  }
  
  bool LoadAllSymbols() {
    return LoadSymbol(fn_init, "retro_init") &&
           LoadSymbol(fn_deinit, "retro_deinit") &&
           LoadSymbol(fn_api_version, "retro_api_version") &&
           LoadSymbol(fn_get_system_info, "retro_get_system_info") &&
           LoadSymbol(fn_get_system_av_info, "retro_get_system_av_info") &&
           LoadSymbol(fn_set_environment, "retro_set_environment") &&
           LoadSymbol(fn_set_video_refresh, "retro_set_video_refresh") &&
           LoadSymbol(fn_set_audio_sample, "retro_set_audio_sample") &&
           LoadSymbol(fn_set_audio_sample_batch, "retro_set_audio_sample_batch") &&
           LoadSymbol(fn_set_input_poll, "retro_set_input_poll") &&
           LoadSymbol(fn_set_input_state, "retro_set_input_state") &&
           LoadSymbol(fn_set_controller_port_device, "retro_set_controller_port_device") &&
           LoadSymbol(fn_reset, "retro_reset") &&
           LoadSymbol(fn_run, "retro_run") &&
           LoadSymbol(fn_serialize_size, "retro_serialize_size") &&
           LoadSymbol(fn_serialize, "retro_serialize") &&
           LoadSymbol(fn_unserialize, "retro_unserialize") &&
           LoadSymbol(fn_load_game, "retro_load_game") &&
           LoadSymbol(fn_unload_game, "retro_unload_game") &&
           LoadSymbol(fn_get_memory_data, "retro_get_memory_data") &&
           LoadSymbol(fn_get_memory_size, "retro_get_memory_size");
  }
};

// Thread-local pointer for callback trampolines
// This is safe because Libretro cores are not thread-safe anyway
thread_local ImplData* g_current_impl = nullptr;

// Null log function for headless operation
static void NullLogFunc(retro_log_level, const char*, ...) {}

// Static callback trampolines
bool EnvironmentCallback(unsigned cmd, void* data) {
  if (!g_current_impl) return false;
  
  switch (cmd) {
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
      auto* log = static_cast<retro_log_callback*>(data);
      log->log = NullLogFunc;
      return true;
    }
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
      *static_cast<bool*>(data) = true;
      return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
      auto fmt = *static_cast<retro_pixel_format*>(data);
      return fmt == RETRO_PIXEL_FORMAT_0RGB1555 ||
             fmt == RETRO_PIXEL_FORMAT_XRGB8888 ||
             fmt == RETRO_PIXEL_FORMAT_RGB565;
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
      *static_cast<const char**>(data) = nullptr;
      return false;
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_VARIABLES:
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
      return true;
    default:
      return false;
  }
}

void VideoRefreshCallback(const void* data, unsigned width, unsigned height, size_t pitch) {
  if (!g_current_impl) return;
  g_current_impl->frame_data = data;
  g_current_impl->frame_width = width;
  g_current_impl->frame_height = height;
  g_current_impl->frame_pitch = pitch;
  
  if (data && g_current_impl->video_cb) {
    FrameBuffer fb{
      .data = std::span(static_cast<const std::uint8_t*>(data), height * pitch),
      .width = width,
      .height = height,
      .pitch = static_cast<unsigned>(pitch)
    };
    g_current_impl->video_cb(fb);
  }
}

void AudioSampleCallback(std::int16_t left, std::int16_t right) {
  if (!g_current_impl) return;
  g_current_impl->audio_buffer.push_back(left);
  g_current_impl->audio_buffer.push_back(right);
}

size_t AudioSampleBatchCallback(const std::int16_t* data, size_t frames) {
  if (!g_current_impl) return frames;
  g_current_impl->audio_buffer.insert(
    g_current_impl->audio_buffer.end(),
    data, data + frames * 2
  );
  return frames;
}

void InputPollCallback() {}

std::int16_t InputStateCallback(unsigned port, unsigned device, unsigned index, unsigned id) {
  if (!g_current_impl || port >= 2) return 0;
  if (device != RETRO_DEVICE_JOYPAD) return 0;
  if (index != 0) return 0;
  
  std::uint8_t mask = g_current_impl->input_state[port];
  
  switch (id) {
    case RETRO_DEVICE_ID_JOYPAD_A:      return (mask & LibretroInput::A) ? 1 : 0;
    case RETRO_DEVICE_ID_JOYPAD_B:      return (mask & LibretroInput::B) ? 1 : 0;
    case RETRO_DEVICE_ID_JOYPAD_SELECT: return (mask & LibretroInput::SELECT) ? 1 : 0;
    case RETRO_DEVICE_ID_JOYPAD_START:  return (mask & LibretroInput::START) ? 1 : 0;
    case RETRO_DEVICE_ID_JOYPAD_UP:     return (mask & LibretroInput::UP) ? 1 : 0;
    case RETRO_DEVICE_ID_JOYPAD_DOWN:   return (mask & LibretroInput::DOWN) ? 1 : 0;
    case RETRO_DEVICE_ID_JOYPAD_LEFT:   return (mask & LibretroInput::LEFT) ? 1 : 0;
    case RETRO_DEVICE_ID_JOYPAD_RIGHT:  return (mask & LibretroInput::RIGHT) ? 1 : 0;
    case RETRO_DEVICE_ID_JOYPAD_MASK: {
      std::int16_t result = 0;
      if (mask & LibretroInput::B)      result |= (1 << RETRO_DEVICE_ID_JOYPAD_B);
      if (mask & LibretroInput::A)      result |= (1 << RETRO_DEVICE_ID_JOYPAD_A);
      if (mask & LibretroInput::SELECT) result |= (1 << RETRO_DEVICE_ID_JOYPAD_SELECT);
      if (mask & LibretroInput::START)  result |= (1 << RETRO_DEVICE_ID_JOYPAD_START);
      if (mask & LibretroInput::UP)     result |= (1 << RETRO_DEVICE_ID_JOYPAD_UP);
      if (mask & LibretroInput::DOWN)   result |= (1 << RETRO_DEVICE_ID_JOYPAD_DOWN);
      if (mask & LibretroInput::LEFT)   result |= (1 << RETRO_DEVICE_ID_JOYPAD_LEFT);
      if (mask & LibretroInput::RIGHT)  result |= (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT);
      return result;
    }
    default: return 0;
  }
}

} // anonymous namespace

// Alias for PIMPL
struct LibretroWrapper::Impl : ImplData {};

// Constructor/Destructor
LibretroWrapper::LibretroWrapper() : impl_(std::make_unique<Impl>()) {}

LibretroWrapper::~LibretroWrapper() {
  if (impl_) {
    UnloadROM();
    UnloadCore();
  }
}

LibretroWrapper::LibretroWrapper(LibretroWrapper&&) noexcept = default;
LibretroWrapper& LibretroWrapper::operator=(LibretroWrapper&&) noexcept = default;

// Core lifecycle
LibretroError LibretroWrapper::LoadCore(std::string_view core_path) {
  UnloadCore();
  
  std::string path_str(core_path);
  impl_->core_handle = dlopen(path_str.c_str(), RTLD_LAZY);
  if (!impl_->core_handle) {
    std::cerr << std::format("dlopen failed: {}\n", dlerror());
    return LibretroError::CoreLoadFailed;
  }
  
  if (!impl_->LoadAllSymbols()) {
    std::cerr << "Failed to load required symbols from core\n";
    dlclose(impl_->core_handle);
    impl_->core_handle = nullptr;
    return LibretroError::CoreLoadFailed;
  }
  
  unsigned version = impl_->fn_api_version();
  if (version != RETRO_API_VERSION) {
    std::cerr << std::format("API version mismatch: {} vs {}\n", version, RETRO_API_VERSION);
    dlclose(impl_->core_handle);
    impl_->core_handle = nullptr;
    return LibretroError::CoreLoadFailed;
  }
  
  g_current_impl = impl_.get();
  impl_->fn_set_environment(EnvironmentCallback);
  impl_->fn_set_video_refresh(VideoRefreshCallback);
  impl_->fn_set_audio_sample(AudioSampleCallback);
  impl_->fn_set_audio_sample_batch(AudioSampleBatchCallback);
  impl_->fn_set_input_poll(InputPollCallback);
  impl_->fn_set_input_state(InputStateCallback);
  
  impl_->fn_init();
  impl_->fn_get_system_info(&impl_->sys_info);
  
  return LibretroError::OK;
}

void LibretroWrapper::UnloadCore() {
  if (!impl_->core_handle) return;
  
  UnloadROM();
  
  if (impl_->fn_deinit) {
    g_current_impl = impl_.get();
    impl_->fn_deinit();
  }
  
  dlclose(impl_->core_handle);
  impl_->core_handle = nullptr;
  g_current_impl = nullptr;
}

bool LibretroWrapper::IsCoreLoaded() const {
  return impl_->core_handle != nullptr;
}

// ROM lifecycle
LibretroError LibretroWrapper::LoadROM(std::string_view rom_path) {
  if (!impl_->core_handle) return LibretroError::CoreNotLoaded;
  
  UnloadROM();
  
  std::string path_str(rom_path);
  
  std::ifstream file(path_str, std::ios::binary | std::ios::ate);
  if (!file) {
    std::cerr << std::format("Failed to open ROM: {}\n", path_str);
    return LibretroError::ROMLoadFailed;
  }
  
  auto size = file.tellg();
  file.seekg(0);
  std::vector<std::uint8_t> rom_data(static_cast<size_t>(size));
  file.read(reinterpret_cast<char*>(rom_data.data()), size);
  
  retro_game_info info{};
  info.path = path_str.c_str();
  info.data = rom_data.data();
  info.size = rom_data.size();
  info.meta = nullptr;
  
  g_current_impl = impl_.get();
  
  if (!impl_->fn_load_game(&info)) {
    std::cerr << "Core rejected ROM\n";
    return LibretroError::ROMLoadFailed;
  }
  
  impl_->rom_loaded = true;
  impl_->fn_get_system_av_info(&impl_->av_info);
  impl_->fn_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
  impl_->fn_set_controller_port_device(1, RETRO_DEVICE_JOYPAD);
  
  return LibretroError::OK;
}

void LibretroWrapper::UnloadROM() {
  if (!impl_->core_handle || !impl_->rom_loaded) return;
  
  g_current_impl = impl_.get();
  impl_->fn_unload_game();
  impl_->rom_loaded = false;
}

bool LibretroWrapper::IsROMLoaded() const {
  return impl_->rom_loaded;
}

// Core information
std::optional<CoreInfo> LibretroWrapper::GetCoreInfo() const {
  if (!impl_->core_handle) return std::nullopt;
  
  return CoreInfo{
    .library_name = impl_->sys_info.library_name ? impl_->sys_info.library_name : "",
    .library_version = impl_->sys_info.library_version ? impl_->sys_info.library_version : "",
    .valid_extensions = impl_->sys_info.valid_extensions ? impl_->sys_info.valid_extensions : "",
    .need_fullpath = impl_->sys_info.need_fullpath,
    .block_extract = impl_->sys_info.block_extract,
  };
}

std::optional<AVInfo> LibretroWrapper::GetAVInfo() const {
  if (!impl_->rom_loaded) return std::nullopt;
  
  return AVInfo{
    .base_width = impl_->av_info.geometry.base_width,
    .base_height = impl_->av_info.geometry.base_height,
    .max_width = impl_->av_info.geometry.max_width,
    .max_height = impl_->av_info.geometry.max_height,
    .aspect_ratio = impl_->av_info.geometry.aspect_ratio,
    .fps = impl_->av_info.timing.fps,
    .sample_rate = impl_->av_info.timing.sample_rate,
  };
}

// Emulation control
void LibretroWrapper::Reset() {
  if (!impl_->rom_loaded) return;
  g_current_impl = impl_.get();
  impl_->fn_reset();
}

void LibretroWrapper::Run() {
  if (!impl_->rom_loaded) return;
  
  g_current_impl = impl_.get();
  impl_->audio_buffer.clear();
  impl_->fn_run();
  
  if (impl_->audio_cb && !impl_->audio_buffer.empty()) {
    AudioBuffer ab{
      .samples = std::span(impl_->audio_buffer),
      .frames = impl_->audio_buffer.size() / 2
    };
    impl_->audio_cb(ab);
  }
}

// Input
void LibretroWrapper::SetInput(unsigned port, std::uint8_t input) {
  if (port < 2) {
    impl_->input_state[port] = input;
  }
}

// Memory access
std::span<std::uint8_t> LibretroWrapper::GetRAM() {
  if (!impl_->rom_loaded) return {};
  
  auto* data = static_cast<std::uint8_t*>(
    impl_->fn_get_memory_data(RETRO_MEMORY_SYSTEM_RAM)
  );
  size_t size = impl_->fn_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
  
  if (!data || size == 0) return {};
  return std::span(data, size);
}

std::span<const std::uint8_t> LibretroWrapper::GetRAM() const {
  if (!impl_->rom_loaded) return {};
  
  auto* data = static_cast<const std::uint8_t*>(
    impl_->fn_get_memory_data(RETRO_MEMORY_SYSTEM_RAM)
  );
  size_t size = impl_->fn_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
  
  if (!data || size == 0) return {};
  return std::span(data, size);
}

size_t LibretroWrapper::GetRAMSize() const {
  if (!impl_->rom_loaded) return 0;
  return impl_->fn_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
}

// Save states
size_t LibretroWrapper::GetStateSize() const {
  if (!impl_->rom_loaded) return 0;
  return impl_->fn_serialize_size();
}

bool LibretroWrapper::SaveState(std::span<std::uint8_t> buffer) {
  if (!impl_->rom_loaded) return false;
  if (buffer.size() < GetStateSize()) return false;
  
  g_current_impl = impl_.get();
  return impl_->fn_serialize(buffer.data(), buffer.size());
}

bool LibretroWrapper::LoadState(std::span<const std::uint8_t> buffer) {
  if (!impl_->rom_loaded) return false;
  
  g_current_impl = impl_.get();
  return impl_->fn_unserialize(buffer.data(), buffer.size());
}

// Callback setters
void LibretroWrapper::SetVideoCallback(VideoCallback cb) {
  impl_->video_cb = std::move(cb);
}

void LibretroWrapper::SetAudioCallback(AudioCallback cb) {
  impl_->audio_cb = std::move(cb);
}
