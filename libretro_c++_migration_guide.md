# LIBRETRO MIGRATION GUIDE

**Author:** Tom7
**Date:** 2025-12-15  
**Clearance:** STRICTLY CONFIDENTIAL // AGENTS ONLY  
**Target Architecture:** C++23 (The Future)  
**Dependencies:** `libretro-common` (Submodule), `libretro-core` (Dynamic)

-----

## 0\. THE MANIFESTO

We are undertaking a critical transformation of the `tasbot` codebase. The current implementation relies on a deprecated, statically linked fork of FCEUX (`fceu/`) that fundamentally limits our ability to generalize. It is filled with pre-standard C++ idioms, global state, and unsafe memory practices. The existing `Makefile` is a crime scene.

**The Mission:**

1.  **Modernize:** Refactor the codebase to adhere to strict **C++23** standards.
2.  **Decouple:** Replace the internal `fceu` emulation core with a modular **Libretro Frontend** implementation.
3.  **Optimize:** Implement a bifurcated build system (Debug vs. Maximum Performance) that squeezes every cycle out of the CPU, mirroring the logic found in the legacy `configure.ac` but implemented in modern CMake.

**Rules of Engagement:**

  * **Zero-Trust:** Do not assume legacy headers or types are correct. Verify everything against the standard.
  * **Tooling is Mandatory:** `clang-tidy` and `clang-format` are required at every step. Use `-Wall -Wextra -Wpedantic -Werror`.
  * **Memory Safety:** Manual `new` and `delete` are forbidden. Use `std::unique_ptr`, `std::shared_ptr`, and container semantics.
  * **Type Safety:** C-style casts `(type)val` are forbidden. Use `static_cast`, `reinterpret_cast`, or `std::bit_cast`.
  * **Exploration:** You have access to the `libretro-common` submodule. Use it. Do not reinvent headers that exist there.
  * **Codebase Specifics:** Respect the inline comments in `tasbot/playfun.cc` and the directive in `tasbot/TODO`.

-----

## 1\. PHASE ZERO: C++23 MODERNIZATION

**Priority:** CRITICAL. This refactoring must occur *before* the Libretro migration to ensure a stable foundation.

### 1.1 The Great Replacement List

We are systematically replacing archaic C idioms with robust C++23 features.

| Legacy Artifact | C++23 Replacement | Rationale |
| :--- | :--- | :--- |
| `printf`, `fprintf` | `std::print`, `std::println` | Type-safe, faster, unicode support. Requires `<print>`. |
| `sprintf` | `std::format` | Prevents buffer overflows. Returns `std::string`. |
| `typedef` | `using` | Improved readability and template compatibility. |
| `NULL` | `nullptr` | Strict pointer type safety. |
| `new T[]` | `std::vector<T>` or `std::array` | RAII compliance. Prevents memory leaks. |
| `void*` + `size_t` | `std::span<T>` | Bounds-checked memory views (debug mode). |
| `#define CONST 10` | `constexpr int kConst = 10;` | Scoped, typed constants. |
| `__FILE__` macros | `std::source_location` | Standardized introspection. |
| `unsigned char` | `std::uint8_t` | Explicit bit-width guarantees. |

### 1.2 Agent Tasks (Source Audit)

  - [ ] **CC-Lib Updates:** Audit `cc-lib/` headers (e.g., `cc-lib/base/basictypes.h`). Remove deprecated type definitions like `int32`, `uint32` and replace them with `<cstdint>` standards (`std::int32_t`, `std::uint32_t`).
  - [ ] **Constants:** Convert all `#define` constants in `tasbot/` files (specifically `playfun.cc` and `tasbot.h`) to `constexpr`.
  - [ ] **Type Hygiene:** Execute a global search-and-replace to enforce `using` over `typedef`.
      * *Constraint:* Ensure `uint8`, `uint16`, `uint32` are replaced with `std::uint8_t`, etc.
  - [ ] **Header Modernization:** Replace old include guards (`#ifndef __FILE_H_`) with `#pragma once` in all header files.
  - [ ] **I/O Refactor:**
      * Replace `fprintf(stderr, ...)` with `std::println(std::cerr, "...", args)`.
  - [ ] **Memory Management:**
      * Audit `tasbot/learnfun.cc` for `new vector`. Replace with `std::unique_ptr` or stack allocation.
      * Audit `tasbot/playfun.cc` for raw pointer usage in `Future` structs.
  - [ ] **Loops:** Refactor index-based loops to Range-based for loops (`for (auto&& item : vec)`) or `std::views` where applicable.

-----

## 2\. PHASE ONE: THE BUILD SYSTEM (CMAKE REVOLUTION)

The existing `Makefile` is deprecated. We will establish a robust **CMake** build environment that supports distinct **Debug** and **Maximum Performance** configurations, translating the logic from the legacy `configure.ac` into modern CMake logic.

### 2.1 Directives

1.  **Standard:** Enforce `CMAKE_CXX_STANDARD 23`.
2.  **Dependencies:** Use `FetchContent` or `find_package` for:
      * **SDL2:** Required for input/video (Replace SDL 1.2).
      * **Protobuf:** For `marionet.proto`.
      * **ZLIB / LibPNG:** For state compression and screenshots.
      * **Libretro Common:** Link as an interface library.

### 2.2 Build Configurations

We need two distinct modes. A "Release" build is not enough. We need a "Ludicrous Speed" mode.

#### A. Debug Mode (`-DBuildType=Debug`)

  * **Flags:** `-O0 -g3 -ggdb -fno-omit-frame-pointer`
  * **Sanitizers:** Must enable AddressSanitizer (`-fsanitize=address`) and UndefinedBehaviorSanitizer (`-fsanitize=undefined`).
  * **Defines:** `-DDEBUG=1`

#### B. Maximum Performance Mode (`-DBuildType=Performance`)

  * **Optimization:** `-Ofast` (Note: breaks IEEE float compliance, acceptable for TAS heuristics).
  * **Architecture:** `-march=native` (Optimize for the build machine's CPU).
  * **Link-Time Optimization (LTO):** Enable ThinLTO (`-flto=thin` on Clang) or standard LTO (`-flto` on GCC).
  * **Graphite/Polly:** Enable loop optimizations if available.
      * *GCC:* `-fgraphite-identity -floop-nest-optimize`
      * *Clang:* `-mllvm -polly -mllvm -polly-vectorizer=stripmine`
  * **Vectorization:** Ensure `-ftree-vectorize` / `-fvectorize` is active.
  * **Alignment:** Force function alignment `-falign-functions=32`.

### 2.3 Static Analysis (`.clang-tidy`)

We must enforce code quality automatedly. Create a `.clang-tidy` file in the root:

```yaml
Checks: >
  -*,
  bugprone-*,
  performance-*,
  readability-identifier-naming,
  modernize-use-nullptr,
  modernize-use-override,
  modernize-use-auto,
  modernize-loop-convert,
  cppcoreguidelines-init-variables,
  misc-unused-*,
WarningsAsErrors: 'bugprone-*,performance-*'
CheckOptions:
  - key: performance-unnecessary-value-param.AllowedTypes
    value: 'std::vector;std::string'
```

### 2.4 Agent Tasks

  - [ ] Initialize `CMakeLists.txt` in the root directory.
  - [ ] Implement configuration switches for `Debug` vs `Performance`.
  - [ ] Add custom commands to run `clang-tidy` as part of the build.
  - [ ] Verify `libretro-common` inclusion directories are correct.

-----

## 3\. PHASE TWO: THE LIBRETRO WRAPPER

We are implementing a Libretro **Frontend**. This requires a translation layer to communicate with Libretro **Cores** (dynamic libraries).

### 3.1 Wrapper Architecture (`libretro-wrapper.h`)

Create a strongly-typed C++ interface to wrap the C API.

```cpp
#include <expected>
#include <span>
#include <string_view>
#include "libretro.h" // From libretro-common

class LibretroWrapper {
public:
    // Core Lifecycle
    static std::expected<void, std::string> Initialize(std::string_view corePath, std::string_view romPath);
    static void Shutdown();

    // Execution
    static void RunFrame();
    
    // State Management
    static void Serialize(std::span<uint8_t> buffer);
    static void Unserialize(std::span<const uint8_t> buffer);
    static size_t GetSerializeSize();
    
    // Input
    static void SetInput(int port, int device, int index, int id, int16_t value);
};
```

### 3.2 Dynamic Loading Abstraction

Do not use platform-specific loading calls (like `dlopen`) directly in the logic code.

  - Create `tasbot/dynlib.h` to abstract `dlopen` (POSIX) and `LoadLibrary` (Windows).
  - Ensure the core exports `retro_init`, `retro_run`, etc., are correctly mapped.

-----

## 4\. PHASE THREE: THE CORE TRANSPLANT

**Target:** `tasbot/emulator.cc` & `tasbot/emulator.h`

We must remove the static linking of `fceu` and replace it with calls to our `LibretroWrapper`.

### 4.1 Input Mapping

FCEU uses a packed bitmask (`RLDUTSBA`). Libretro uses discrete IDs (e.g., `RETRO_DEVICE_ID_JOYPAD_A`).

  * **Directive:** Do not hardcode mappings. Create a `constexpr` map or switch statement to translate FCEU bitmasks to Libretro calls.

### 4.2 Memory Access

  * **Old:** `extern uint8 RAM[0x800];` (Direct array access)
  * **New:** `retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);`
  * **Warning:** The pointer returned by Libretro *may* change between `retro_load_game` calls. Always fetch the pointer before reading/writing in a frame loop. Do not cache it globally.

### 4.3 Save State Logic

The original `tasbot` uses a custom gzip-compressed state format with delta compression ("basis vectors").

  * **Libretro:** Provides a raw binary blob via `retro_serialize`.
  * **Migration:**
    1.  Allocate a buffer of size `retro_serialize_size()`.
    2.  Call `retro_serialize` into this buffer.
    3.  Pass this raw buffer to the existing `BasisUtil` to maintain the delta-compression optimization.

-----

## 5\. PHASE FOUR: LOGIC REPAIR & VALIDATION

The provided codebase contains historical logic bugs. These must be addressed during the port.

### 5.1 The Comparator Bug (`playfun.cc`)

**Context:** The function `TakeBestAmong` manages a list of "Futures" (input sequences). It is supposed to maintain a high-quality population by discarding the lowest-scoring futures.

**The Logic:** In `playfun.cc`, lines \~1228-1234:

```cpp
double worst_total = futuretotals[0];
int worst_idx = 0;
for (int i = 1; i < futures->size(); i++) {
  if (futuretotals[i] < worst_total) {
    worst_total = futuretotals[i];
    worst_idx = i;
  }
}
```

**Audit:** This loop currently finds the *minimum* score. If the intent is to drop the *worst* (lowest) score, this logic is correct. However, if previous versions inverted this (`>`) to erroneously drop the *best* score, ensure that regression does not occur.
**Fix:** Use `std::ranges::min_element` to make the intent explicit and eliminate manual loop errors.

### 5.2 Score Calculation Precision

**Location:** `ParallelStep` in `playfun.cc`.
**Issue:** `futures_score` is a `double`, but `futuretotals` vector operations might implicitly cast to `int` in older code.
**Directive:** Ensure all score accumulators explicitly use `double`. Verify that `auto` type deduction resolves to `double` (initialize variables with `0.0`, not `0`).

### 5.3 Frame Timing

**Issue:** FCEU assumes a strict NTSC 60Hz. Libretro cores may vary (PAL, etc.).
**Directive:** Use `retro_get_system_av_info` to retrieve the core's reported timing. Do not hardcode 60.0. Use this timing data if implementing video export or strict synchronization.

### 5.4 TODO Integration

**Directive:** Scan `tasbot/TODO`.

  * **GPU Parallelization:** Implement the "massively parallelize on GPU" idea using Vulkan Compute or CUDA if feasible, or stick to CPU threading via `std::jthread`.
  * **Adaptive Futures:** Implement "When futures are bad in general, shorten them and have more of them." This requires dynamic resizing of the `futures` vector based on score trends.

-----

## 6\. FINAL EXECUTION CHECKLIST

Before marking the migration as complete, perform the following validation:

1.  **Build (Debug):** `cmake -DCMAKE_BUILD_TYPE=Debug .. && make` (Must pass with 0 warnings).
2.  **Build (Perf):** `cmake -DCMAKE_BUILD_TYPE=Performance .. && make` (Verify `-O3`/`-Ofast` flags).
3.  **Test:** Run `ctest`. All unit tests must pass.
4.  **Sanitization:** Run `playfun` (Debug build) with `-fsanitize=address,undefined`.
5.  **Verification:**
      * **Config:** `config.txt`
      * **ROM:** `smb.nes`
      * **Input:** `smb-walk.fm2`
      * **Execution:** The system should load the Core, load the ROM, replay the inputs, and begin searching for optimizations without crashing.

**Agent Note:**
The goal is code clarity and raw speed. Use `std::views` for data pipelines. Use `constexpr` for lookups. Make the logic visible and the architecture modular.

```
```