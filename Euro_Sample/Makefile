# Project Name
TARGET = Euro_Sample
#APP_TYPE = BOOT_SRAM
#APP_TYPE = BOOT_QSPI

# Sources
CPP_SOURCES = Euro_Sample.cpp

# Optimize for space
OPT = -Os

# Library Locations
LIBDAISY_DIR = ../../libDaisy/
DAISYSP_DIR = ../../DaisySP/

# Includes FatFS source files within project.
USE_FATFS = 1

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
