# Building learnfun/playfun

This document describes how to build `learnfun` and `playfun` with various configurations, ranging from debug builds to maximum performance optimizations.

-----

## Prerequisites

### Ubuntu/Debian

```bash
# 1. Update index and enable community/multiverse repositories
sudo apt update
sudo apt install -y software-properties-common
sudo add-apt-repository -y universe
sudo add-apt-repository -y multiverse
sudo apt update && sudo apt upgrade -y

# 2. Install dependencies
sudo apt install -y \
  build-essential autoconf automake libtool pkg-config \
  libsdl1.2-dev libsdl-net1.2-dev \
  zlib1g-dev libpng-dev \
  libprotobuf-dev protobuf-compiler \
  isl-devel libasan8 libubsan1 \
  retroarch
```

-----

## Quick Start

| Build Type | Command | Description |
| :--- | :--- | :--- |
| **Default** | `./autogen.sh && ./configure && make -j$(nproc)` | C++23, standard optimizations. |
| **Debug** | `./autogen.sh && ./configure --enable-debug && make -j$(nproc)` | `-O0`, Sanitizers enabled. |
| **Max Perf** | `./autogen.sh && ./configure --enable-performance --with-opt-level=fast --with-lto=yes && make -j$(nproc)` | Highest optimization, LTO enabled. |

-----

## Configuration Options

### 1\. General Settings

| Flag | Values | Default | Description |
| :--- | :--- | :--- | :--- |
| `--with-compiler` | `auto`, `gcc`, `clang`, `icx` | `auto` | Selects the compiler suite (GNU, LLVM, or Intel). |
| `--with-cxx-standard`| `11`, `14`, `17`, `20`, `23` | `23` | **Must use `=`**. Sets C++ standard version. |
| `--disable-native` | N/A | *Enabled* | Disables `-march=native`. Use for portable binaries. |

### 2\. Build Modes

  * **Debug Mode** (`--enable-debug`):

      * Optimization: `-O0 -g3`
      * Sanitizers: Address (`-fsanitize=address`), Undefined Behavior (`-fsanitize=undefined`)
      * Defines: `DEBUG=1`
      * *Note: Mutually exclusive with Performance Mode.*

  * **Performance Mode** (`--enable-performance`):

      * Enables highest opt-level allowed.
      * Enables advanced compiler optimizations (Graphite/Polly).
      * Enables vectorization reporting.
      * Defines: `NDEBUG`

### 3\. Optimization Tuning

| Flag | Values | Description |
| :--- | :--- | :--- |
| `--with-opt-level` | `2` (Default), `3`, `fast` | `2` is standard `-O2`. `fast` enables `-Ofast` (non-IEEE math). |
| `--with-lto` | `no` (Default), `yes`, `thin`, `full` | Link-Time Optimization. `thin` is Clang only (faster link). |
| `--with-align-functions` | `no`, `32`, `64` | Aligns functions to byte boundaries. Default `32` for perf builds. |

-----

## Advanced Compiler Optimizations

When using `--enable-performance`, the build system automatically tests and enables the following flags if supported.

### GCC Optimizations (Requires `libisl-dev`)

| Flag | Description |
| :--- | :--- |
| `-fivopts` | Induction variable optimizations |
| `-fmodulo-sched` | Modulo scheduling for loops |
| `-fgraphite-identity` | Enable Graphite loop optimizations |
| `-floop-nest-optimize` | Optimize nested loops (Graphite) |
| `-ftree-vectorize` | Auto-vectorization |

### Clang Optimizations

| Flag | Description |
| :--- | :--- |
| `-mllvm -polly` | Enable Polly polyhedral optimizer |
| `-mllvm -polly-vectorizer=stripmine` | Polly vectorization |
| `-fslp-vectorize` | Enable superword-level parallelism |
| `-fvectorize` | Enable vectorization |

-----

## Use Case Examples

### Development (Bug Hunting)

Best for finding bugs with sanitizers and step-through debugging.

```bash
./configure --enable-debug --with-cxx-standard=23
```

### Balanced / Portable

Best for regular use and distribution to other machines.

```bash
./configure --with-opt-level=3 --with-cxx-standard=23 --disable-native
```

### Maximum Performance (Production)

Best for benchmarks and dedicated hardware.

```bash
./configure \
  --enable-performance \
  --with-opt-level=fast \
  --with-lto=yes \
  --with-cxx-standard=23
```

### Cross-Compilation / Legacy

Best for maximum compatibility.

```bash
./configure \
  --with-opt-level=3 \
  --disable-native \
  --with-cxx-standard=17
```

-----

## Static Analysis & Environment Variables

### Running `clang-tidy`

```bash
# 1. Install prerequisites
sudo apt-get install bear clang-tidy

# 2. Generate compilation database
./autogen.sh && ./configure
bear -- make

# 3. Run analysis
clang-tidy -p . tasbot/*.cc
```

### Environment Variables

You can override specific settings or versions using standard environment variables:

```bash
# Specific Compiler Versions
CC=clang-18 CXX=clang++-18 ./configure

# Intel oneAPI
CC=icx CXX=icpx ./configure

# Injecting Flags
CXXFLAGS="-march=haswell" ./configure --enable-performance
```

-----

## Troubleshooting

| Issue | Solution |
| :--- | :--- |
| **"C++23 is not supported"** | Update GCC to 13+ / Clang to 17+, or use `--with-cxx-standard=20`. |
| **LTO errors (Clang)** | Switch LTO mode: `./configure --with-lto=full` or disable it. |

### CI/CD

This project uses **GitHub Actions**. See `.github/workflows/` for:

  * **CI - Debug & Lint**: Sanitizers and clang-tidy.
  * **CI - Performance Build**: Optimization testing.
