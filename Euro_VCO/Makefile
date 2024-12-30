# Project Name
TARGET = Euro_VCO
USE_DAISYSP_LGPL = 1
APP_TYPE = BOOT_SRAM
# Sources
CPP_SOURCES = Euro_VCO.cpp \
dsp/fm_voice.cpp \
dsp/part.cpp \
dsp/resonator.cpp \
dsp/string.cpp \
dsp/string_synth_part.cpp \
resources.cpp \

CC_SOURCES += $(STMLIB_DIR)/dsp/units.cc \
$(STMLIB_DIR)/utils/random.cc \

# Library Locations
LIBDAISY_DIR = ../../libDaisy/
DAISYSP_DIR = ../../DaisySP/
STMLIB_DIR = ../../stmlib/

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

C_INCLUDES += \
-I. \
-Iresources \
-I../..

C_INCLUDES += -Wno-unused-local-typedefs

# compiler flags
LDFLAGS += -u _printf_float
OPT = -Os
