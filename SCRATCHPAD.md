# MIGRATION SCRATCHPAD - C++23 Modernization & Libretro Port

**Created:** 2025-12-15
**Status:** IN PROGRESS

---

## 🎯 MISSION CRITICAL OBJECTIVES

1. **Phase 0:** C++23 Modernization (BEFORE Libretro)
2. **Phase 1:** Build System (Autotools already exists; verify C++23)
3. **Phase 2:** Libretro Wrapper abstraction
4. **Phase 3:** Core Transplant (replace fceu with Libretro)
5. **Phase 4:** Logic Repair & Validation

---

## 📋 CURRENT STATE ANALYSIS

### Clang 18 C++23 Feature Support:
- ✅ std::format
- ✅ std::span
- ✅ std::source_location
- ✅ std::uint8_t (cstdint)
- ❌ std::print (not available - use format + cout)
- ❌ std::expected (not available - use optional or manual)

### Files to Audit (cc-lib + tasbot only, NOT fceu):
- [ ] cc-lib/*.cc, *.h
- [ ] tasbot/*.cc, *.h (excluding fceu/)
- [ ] tasbot/emulator.cc/h (CRITICAL - fceu interface)

### Tom7 TODO Items (from tasbot/TODO):
- DEBUG flag in fceu/types.h - check if enabled
- Approximate 6502 for GPU parallelization (future)
- Algorithm improvements (futures, motifs, backtracking)
- Various game suggestions

### Build System Status:
- configure.ac exists with C++23 support
- autogen.sh present
- Makefile.am files present

### Test Files Available:
- tasbot/smb.nes (ROM)
- tasbot/smb-walk.fm2 (movie)
- tasbot/config.txt (game smb, movie smb-walk.fm2)

---

## 🔧 MIGRATION CHECKLIST

### C++23 Replacements (from guide):
| Legacy | Modern | Status |
|--------|--------|--------|
| printf/fprintf | std::print/println | TODO |
| sprintf | std::format | TODO |
| typedef | using | TODO |
| NULL | nullptr | TODO |
| new T[] | std::vector/array | TODO |
| void* + size_t | std::span<T> | TODO |
| #define CONST | constexpr | TODO |
| __FILE__ macros | std::source_location | TODO |
| unsigned char | std::uint8_t | TODO |

---

## 🐛 BUGS & ISSUES FOUND

(To be populated during audit)

---

## 💡 HYPOTHESES & NOTES

- fceu is deeply integrated via tasbot/emulator.cc
- Need to understand Emulator class interface first
- libretro-common submodule available for reference

---

## 🔄 PROGRESS LOG

### Session 1 (2025-12-15)
- Created scratchpad
- Updated configure.ac for C++23 support
- Downloaded latest ax_cxx_compile_stdcxx.m4 with C++23 support
- Successfully built with clang-18 and C++23

### Phase 0 Progress - C++23 Modernization
**Fixed in cc-lib/util.cc:**
- ✅ Fixed critical memory bug: `delete fname` → modern `std::string` (no alloc!)
- ✅ Replaced sprintf with std::to_string and std::format
- ✅ Added `<format>` include
- ✅ Fixed sign comparison warnings using range-based for loops

**Fixed in cc-lib/rle.cc:**
- ✅ Fixed sign comparison warnings (int → size_t)
- ✅ Updated printf format specifiers (%d → %zu)

**Build Status:**
- ✅ All binaries built successfully: learnfun, playfun, scopefun, pinviz

**Remaining cc-lib warnings (third-party code, low priority):**
- base/logging.h - const qualifier (harmless)
- wavesave.cc - sign comparison
- base/stringprintf.cc - sign comparison
- base/casts.h - unused typedefs (Google style)
- city/city.cc - sign comparison

**Next Steps:**
- [x] Test learnfun execution - Works but crashes at cleanup (fceu bug)
- [ ] Test playfun execution
- [ ] Plan Libretro wrapper (Phase 2)

### Pre-existing fceu Bugs Found (from sanitizers):
1. `fceu/cart.cpp:106` - Array index out of bounds (-2048)
2. `fceu/cart.cpp:112` - Array index out of bounds (-1024)  
3. `objective.cc:22` - Shift exponent too large (64 bits)
4. `munmap_chunk(): invalid pointer` - Double-free/use-after-free at shutdown

These bugs confirm why we need to migrate to Libretro.

