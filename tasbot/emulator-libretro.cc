// emulator-libretro.cc - Libretro-based emulator implementation
// Provides same interface as FCEU-based emulator for drop-in replacement

#include "emulator-libretro.h"
#include "libretro-wrapper.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <format>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <zlib.h>

#include "../cc-lib/city/city.h"

// Default NES core - look for it in standard locations
static constexpr const char* DEFAULT_CORE_PATHS[] = {
  "/tmp/fceumm_libretro.so",
  "/usr/lib/libretro/fceumm_libretro.so",
  "/usr/local/lib/libretro/fceumm_libretro.so",
  "~/.config/retroarch/cores/fceumm_libretro.so",
  "./fceumm_libretro.so",
};

// Singleton wrapper instance
static std::unique_ptr<LibretroWrapper> g_wrapper;
static bool g_initialized = false;
static std::string g_core_name;
static std::string g_core_version;

// Cached frame/audio data from last StepFull
static std::vector<std::uint8_t> g_frame_rgba;
static std::vector<std::int16_t> g_audio_samples;

// State cache (same algorithm as FCEU version)
struct StateCache {
  using Key = std::pair<std::uint8_t, const std::vector<std::uint8_t>*>;
  using Value = std::pair<std::uint64_t, std::vector<std::uint8_t>*>;

  struct HashFunction {
    size_t operator()(const Key& k) const {
      return CityHash64WithSeed(
        reinterpret_cast<const char*>(k.second->data()),
        k.second->size(), k.first);
    }
  };

  struct KeyEquals {
    bool operator()(const Key& l, const Key& r) const {
      return l.first == r.first && *l.second == *r.second;
    }
  };

  std::unordered_map<Key, Value, HashFunction, KeyEquals> table;
  std::uint64_t limit = 0;
  std::uint64_t count = 0;
  std::uint64_t next_seq = 0;
  std::uint64_t slop = 10000;
  std::uint64_t hits = 0;
  std::uint64_t misses = 0;

  void Resize(std::uint64_t ll, std::uint64_t ss) {
    for (auto& [k, v] : table) {
      delete k.second;
      delete v.second;
    }
    table.clear();
    limit = ll;
    slop = ss;
    next_seq = count = 0;
  }

  void Remember(std::uint8_t input, const std::vector<std::uint8_t>& start,
                const std::vector<std::uint8_t>& result) {
    auto* start_copy = new std::vector<std::uint8_t>(start);
    auto* result_copy = new std::vector<std::uint8_t>(result);
    table.emplace(std::make_pair(input, start_copy),
                  std::make_pair(next_seq++, result_copy));
    count++;
    MaybeGC();
  }

  std::vector<std::uint8_t>* GetKnown(std::uint8_t input,
                                       const std::vector<std::uint8_t>& start) {
    auto it = table.find(std::make_pair(input, &start));
    if (it == table.end()) {
      misses++;
      return nullptr;
    }
    hits++;
    it->second.first = next_seq++;
    return it->second.second;
  }

  void MaybeGC() {
    if (count <= limit + slop) return;
    
    std::vector<std::uint64_t> seqs;
    seqs.reserve(count);
    for (const auto& [k, v] : table) {
      seqs.push_back(v.first);
    }
    std::sort(seqs.begin(), seqs.end());
    
    auto num_remove = count - limit;
    auto min_seq = seqs[num_remove];

    for (auto it = table.begin(); it != table.end();) {
      if (it->second.first < min_seq) {
        delete it->first.second;
        delete it->second.second;
        it = table.erase(it);
        count--;
      } else {
        ++it;
      }
    }
  }

  void PrintStats() const {
    std::cout << std::format("Cache: {}/{}, seq {}, {} hits, {} misses\n",
                             count, limit, next_seq, hits, misses);
  }
};

static std::unique_ptr<StateCache> g_cache;

// Find default NES core
static std::string FindDefaultCore() {
  for (const char* path : DEFAULT_CORE_PATHS) {
    std::string p = path;
    if (p[0] == '~') {
      if (const char* home = getenv("HOME")) {
        p = std::string(home) + p.substr(1);
      }
    }
    std::ifstream f(p);
    if (f.good()) return p;
  }
  return "";
}

bool EmulatorLibretro::Initialize(const std::string& core_path, const std::string& rom_path) {
  if (g_initialized) {
    std::cerr << "Already initialized\n";
    return false;
  }

  g_wrapper = std::make_unique<LibretroWrapper>();
  g_cache = std::make_unique<StateCache>();

  auto err = g_wrapper->LoadCore(core_path);
  if (err != LibretroError::OK) {
    std::cerr << std::format("Failed to load core: {}\n", core_path);
    g_wrapper.reset();
    return false;
  }

  auto info = g_wrapper->GetCoreInfo();
  if (info) {
    g_core_name = info->library_name;
    g_core_version = info->library_version;
    std::cerr << std::format("Loaded core: {} v{}\n", g_core_name, g_core_version);
  }

  err = g_wrapper->LoadROM(rom_path);
  if (err != LibretroError::OK) {
    std::cerr << std::format("Failed to load ROM: {}\n", rom_path);
    g_wrapper.reset();
    return false;
  }

  // Set up callbacks to capture video/audio
  g_wrapper->SetVideoCallback([](const FrameBuffer& fb) {
    // Convert from core's pixel format to RGBA
    // FCEUmm typically uses RGB565 or XRGB8888
    g_frame_rgba.resize(256 * 256 * 4, 0);
    
    // NES is 256x240, we use 256x256 for compatibility with FCEU version
    for (unsigned y = 0; y < std::min(fb.height, 256u); y++) {
      for (unsigned x = 0; x < std::min(fb.width, 256u); x++) {
        // Assuming XRGB8888 format (most common)
        size_t src_offset = y * fb.pitch + x * 4;
        size_t dst_offset = y * 256 * 4 + x * 4;
        
        if (src_offset + 3 < fb.data.size()) {
          g_frame_rgba[dst_offset + 0] = fb.data[src_offset + 2]; // R
          g_frame_rgba[dst_offset + 1] = fb.data[src_offset + 1]; // G
          g_frame_rgba[dst_offset + 2] = fb.data[src_offset + 0]; // B
          g_frame_rgba[dst_offset + 3] = 0xFF;                     // A
        }
      }
    }
  });

  g_wrapper->SetAudioCallback([](const AudioBuffer& ab) {
    // Libretro provides stereo, convert to mono
    g_audio_samples.clear();
    g_audio_samples.reserve(ab.frames);
    for (size_t i = 0; i < ab.samples.size(); i += 2) {
      std::int32_t mono = (static_cast<std::int32_t>(ab.samples[i]) +
                          static_cast<std::int32_t>(ab.samples[i + 1])) / 2;
      g_audio_samples.push_back(static_cast<std::int16_t>(mono));
    }
  });

  g_initialized = true;
  return true;
}

bool EmulatorLibretro::Initialize(const std::string& rom_path) {
  std::string core = FindDefaultCore();
  if (core.empty()) {
    std::cerr << "No default NES core found. Provide core path explicitly.\n";
    return false;
  }
  return Initialize(core, rom_path);
}

void EmulatorLibretro::Shutdown() {
  g_wrapper.reset();
  g_cache.reset();
  g_frame_rgba.clear();
  g_audio_samples.clear();
  g_initialized = false;
}

void EmulatorLibretro::Step(std::uint8_t inputs) {
  if (!g_wrapper) return;
  g_wrapper->SetInput(0, inputs);
  g_wrapper->Run();
}

void EmulatorLibretro::StepFull(std::uint8_t inputs) {
  // Same as Step - callbacks capture video/audio automatically
  Step(inputs);
}

void EmulatorLibretro::GetMemory(std::vector<std::uint8_t>* mem) {
  if (!g_wrapper) {
    mem->clear();
    return;
  }
  auto ram = g_wrapper->GetRAM();
  mem->assign(ram.begin(), ram.end());
}

void EmulatorLibretro::GetImage(std::vector<std::uint8_t>* rgba) {
  *rgba = g_frame_rgba;
}

void EmulatorLibretro::GetSound(std::vector<std::int16_t>* wav) {
  *wav = g_audio_samples;
}

std::uint64_t EmulatorLibretro::RamChecksum() {
  if (!g_wrapper) return 0;
  
  auto ram = g_wrapper->GetRAM();
  if (ram.empty()) return 0;
  
  return CityHash64(reinterpret_cast<const char*>(ram.data()), ram.size());
}

void EmulatorLibretro::SaveUncompressed(std::vector<std::uint8_t>* out) {
  if (!g_wrapper) {
    out->clear();
    return;
  }
  out->resize(g_wrapper->GetStateSize());
  g_wrapper->SaveState(*out);
}

void EmulatorLibretro::LoadUncompressed(std::vector<std::uint8_t>* in) {
  if (!g_wrapper || in->empty()) return;
  g_wrapper->LoadState(*in);
}

void EmulatorLibretro::GetBasis(std::vector<std::uint8_t>* out) {
  SaveUncompressed(out);
}

// Compressed save/load (same algorithm as FCEU version)
void EmulatorLibretro::Save(std::vector<std::uint8_t>* out) {
  SaveEx(out, nullptr);
}

void EmulatorLibretro::Load(std::vector<std::uint8_t>* in) {
  LoadEx(in, nullptr);
}

void EmulatorLibretro::SaveEx(std::vector<std::uint8_t>* out, 
                               const std::vector<std::uint8_t>* basis) {
  std::vector<std::uint8_t> raw;
  SaveUncompressed(&raw);

  // XOR with basis for better compression
  size_t blen = basis ? std::min(basis->size(), raw.size()) : 0;
  for (size_t i = 0; i < blen; i++) {
    raw[i] -= (*basis)[i];
  }

  // Compress
  uLongf comprlen = compressBound(raw.size());
  out->resize(4 + comprlen);
  
  if (compress2(&(*out)[4], &comprlen, raw.data(), raw.size(),
                Z_DEFAULT_COMPRESSION) != Z_OK) {
    std::cerr << "Compression failed\n";
    abort();
  }

  // Store uncompressed size in header
  *reinterpret_cast<std::uint32_t*>(out->data()) = raw.size();
  out->resize(4 + comprlen);
}

void EmulatorLibretro::LoadEx(std::vector<std::uint8_t>* in,
                               const std::vector<std::uint8_t>* basis) {
  if (in->size() < 4) return;

  std::uint32_t uncomprlen = *reinterpret_cast<std::uint32_t*>(in->data());
  std::vector<std::uint8_t> uncompressed(uncomprlen);

  uLongf actual_len = uncomprlen;
  if (uncompress(uncompressed.data(), &actual_len,
                 &(*in)[4], in->size() - 4) != Z_OK) {
    std::cerr << "Decompression failed\n";
    abort();
  }
  uncompressed.resize(actual_len);

  // XOR with basis
  size_t blen = basis ? std::min(basis->size(), uncompressed.size()) : 0;
  for (size_t i = 0; i < blen; i++) {
    uncompressed[i] += (*basis)[i];
  }

  LoadUncompressed(&uncompressed);
}

// Cache management
void EmulatorLibretro::ResetCache(std::uint64_t numstates, std::uint64_t slop) {
  if (g_cache) {
    g_cache->Resize(numstates, slop);
  }
}

void EmulatorLibretro::CachingStep(std::uint8_t input) {
  if (!g_cache) {
    Step(input);
    return;
  }

  std::vector<std::uint8_t> start;
  SaveUncompressed(&start);
  
  if (auto* cached = g_cache->GetKnown(input, start)) {
    LoadUncompressed(cached);
  } else {
    Step(input);
    std::vector<std::uint8_t> result;
    SaveUncompressed(&result);
    g_cache->Remember(input, start, result);
  }
}

void EmulatorLibretro::PrintCacheStats() {
  if (g_cache) {
    g_cache->PrintStats();
  }
}

const std::string& EmulatorLibretro::GetCoreName() {
  return g_core_name;
}

const std::string& EmulatorLibretro::GetCoreVersion() {
  return g_core_version;
}
