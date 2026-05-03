# Makefile — firmware build for the DrumMachine.
#
# Per task 09 / SPECIFICATION.md §Build Configuration:
#   - arm-none-eabi-g++, -std=gnu++20, -Ofast, -Wall -Wextra -Werror
#   - APP_TYPE = BOOT_SRAM (matches the existing GuitarPedal firmware so the
#     same Daisy bootloader can flash both via DFU)
#   - Links against the prebuilt libdaisy.a / libdaisysp.a from
#     ../GuitarPedal/dependencies/. Run `bash ../GuitarPedal/ci/build_libs.sh`
#     once to produce them.
#
# Output: build/DrumMachine.{bin,elf}.
# Flash:  `make program-dfu` (Daisy must be in DFU mode; the libDaisy core
#         Makefile defines this target — see ../GuitarPedal/dependencies/
#         libDaisy/core/Makefile).

# Project name — also the basename of the .bin / .elf produced under build/.
TARGET = DrumMachine

CPP_STANDARD = -std=gnu++20

# ---- Sources ----------------------------------------------------------------
# Firmware build includes every src/**/*.cpp (the inverse of Makefile.test,
# which excludes the Daisy* hardware seam files). Listed explicitly rather
# than wildcard'd so adding a new source is a deliberate, reviewable edit.
CPP_SOURCES  = src/main.cpp
CPP_SOURCES += src/Config.cpp
CPP_SOURCES += src/DrumMachine.cpp
CPP_SOURCES += src/DrumPad.cpp
CPP_SOURCES += src/Mixer.cpp
CPP_SOURCES += src/controls/DaisyButton.cpp
CPP_SOURCES += src/controls/DaisyLed.cpp
CPP_SOURCES += src/instruments/BassDrum.cpp
CPP_SOURCES += src/instruments/HiHat.cpp
CPP_SOURCES += src/instruments/Resonator.cpp
CPP_SOURCES += src/instruments/Snare.cpp
CPP_SOURCES += src/randomization/XorShiftRng.cpp
CPP_SOURCES += src/util/LedTrigger.cpp

# Project headers.
C_INCLUDES = -Isrc

# ---- Compiler options -------------------------------------------------------
OPT = -Ofast

# Promote warnings to errors for the firmware build (matches host test build).
# DaisySP / libDaisy headers are pulled in via system include paths set by the
# core Makefile, so their internal warnings won't trip -Werror.
CFLAGS += -Wextra -Werror

# Boot mode: SRAM-resident app launched by the Daisy bootloader. Without this,
# the binary lands at the wrong address and the bootloader won't run it.
APP_TYPE = BOOT_SRAM

# ---- Library locations ------------------------------------------------------
# Reuse the GuitarPedal-side submodules; no duplication. `build_libs.sh`
# leaves the prebuilt static libraries inside their respective dependency
# trees, which the libDaisy core Makefile already knows how to find.
DAISYSP_DIR  ?= ../GuitarPedal/dependencies/DaisySP
LIBDAISY_DIR ?= ../GuitarPedal/dependencies/libDaisy

# ---- Pull in the libDaisy core Makefile -------------------------------------
# It defines the arm-none-eabi toolchain, MCU/FPU flags, BOOT_SRAM linker
# config, .elf/.bin/.hex targets, the openocd `program` target, and the
# `program-dfu` target we use for flashing.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
