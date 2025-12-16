AM_CXXFLAGS = -pthread $(PROTOBUF_CFLAGS) $(SDL_CFLAGS) $(SDL_NET_CFLAGS) \
              $(ZLIB_CFLAGS) $(LIBPNG_CFLAGS) \
              -DMARIONET=1 -I../cc-lib -I../cc-lib/city \
              -DAM_DATADIR=\"$(pkgdatadir)\"
AM_LDFLAGS = -pthread $(PROTOBUF_LIBS) $(SDL_LIBS) $(SDL_NET_LIBS) \
             $(ZLIB_LIBS) $(LIBPNG_LIBS)

# All binaries are now Libretro-based (fceu fully removed)
bin_PROGRAMS = learnfun playfun
check_PROGRAMS = libretro_wrapper_test emulator_libretro_test objective_test weighted_objectives_test

# Protobuf
%.pb.cc %.pb.h: %.proto
	$(PROTOC) --proto_path=. --cpp_out=. $^
dist_noinst_DATA = marionet.proto
MARIONETSOURCES = marionet.pb.cc marionet.pb.h
MOSTLYCLEANFILES = $(MARIONETSOURCES)
BUILT_SOURCES = marionet.pb.h

# Libretro wrapper (dynamically loads cores)
LIBRETRO_SOURCES = libretro-wrapper.cc libretro-wrapper.h
LIBRETRO_EMU_SOURCES = $(LIBRETRO_SOURCES) emulator-libretro.cc emulator-libretro.h

NETWORKINGSOURCES = netutil.cc netutil.h
PNGSAVESOURCES = pngsave.cc pngsave.h

# Core tasbot sources (no fceu dependency)
TASBOT_CORE_SOURCES = simplefm2.cc simplefm2.h \
                      objective.cc objective.h \
                      weighted-objectives.cc weighted-objectives.h \
                      motifs.cc motifs.h \
                      util.cc util.h \
                      tasbot.h game.h motifs-style.h wave.h

# Libretro-based emulator sources
TASBOT_LIBRETRO_SOURCES = $(LIBRETRO_EMU_SOURCES) \
                          $(TASBOT_CORE_SOURCES) \
                          basis-util-libretro.cc

# Main binaries (Libretro-based)
learnfun_SOURCES = $(TASBOT_LIBRETRO_SOURCES) learnfun-libretro.cc
learnfun_LDADD = ../cc-lib/libcclib.la -ldl -lz

playfun_SOURCES = $(TASBOT_LIBRETRO_SOURCES) $(NETWORKINGSOURCES) playfun-nobacktrack-libretro.cc
nodist_playfun_SOURCES = $(MARIONETSOURCES)
playfun_LDADD = ../cc-lib/libcclib.la -ldl -lz

# Tests (no fceu)
libretro_wrapper_test_SOURCES = $(LIBRETRO_SOURCES) libretro-wrapper_test.cc
libretro_wrapper_test_LDADD = -ldl

emulator_libretro_test_SOURCES = $(LIBRETRO_EMU_SOURCES) emulator-libretro_test.cc
emulator_libretro_test_LDADD = ../cc-lib/libcclib.la -ldl -lz

objective_test_SOURCES = $(TASBOT_CORE_SOURCES) objective_test.cc
objective_test_LDADD = ../cc-lib/libcclib.la

weighted_objectives_test_SOURCES = $(TASBOT_CORE_SOURCES) weighted-objectives_test.cc
weighted_objectives_test_LDADD = ../cc-lib/libcclib.la

TESTS = $(check_PROGRAMS)
