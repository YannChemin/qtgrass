# Convenience targets for qtgrass development and packaging.
# Usage:
#   make build          — CMake build (RelWithDebInfo) into ./build/
#   make build-debug    — CMake Debug build into ./build-debug/
#   make install        — install into GRASS_PREFIX/bin
#   make deb-nc         — build release .deb (unsigned)
#   make deb-debug      — build debug .deb with full symbols (unsigned)
#   make deb            — build release .deb (unsigned, no GPG required)
#   make clean          — remove build/, build-debug/, build-area/

GRASS_PREFIX ?= $(shell ls -d /usr/local/grass[0-9]* 2>/dev/null | sort -V | tail -1)
ifeq ($(GRASS_PREFIX),)
GRASS_PREFIX := /usr/local/grass86
endif

BUILD_DIR := build
NPROC     := $(shell nproc)

.PHONY: build build-debug install deb deb-nc deb-debug clean help

help:
	@echo "qtgrass build targets:"
	@echo "  make build        — configure + build (CMake RelWithDebInfo)"
	@echo "  make build-debug  — configure + build (CMake Debug, full symbols)"
	@echo "  make install      — install binary into \$$GRASS_PREFIX/bin"
	@echo "  make deb-nc       — build release .deb (unsigned)"
	@echo "  make deb-debug    — build debug .deb with full symbols (unsigned)"
	@echo "  make deb          — build release .deb (unsigned, no GPG required)"
	@echo "  make clean        — remove build/, build-debug/, build-area/"
	@echo ""
	@echo "GRASS_PREFIX = $(GRASS_PREFIX)"

build:
	@cmake -S . -B $(BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DGRASS_PREFIX=$(GRASS_PREFIX) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	@cmake --build $(BUILD_DIR) -j$(NPROC)

build-debug:
	@cmake -S . -B build-debug \
		-DCMAKE_BUILD_TYPE=Debug \
		-DGRASS_PREFIX=$(GRASS_PREFIX) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	@cmake --build build-debug -j$(NPROC)

install: build
	@cp $(BUILD_DIR)/src/qtgrass $(GRASS_PREFIX)/bin/qtgrass
	@echo "Installed: $(GRASS_PREFIX)/bin/qtgrass"

# Build unsigned release .deb (RelWithDebInfo, stripped — for normal use)
deb-nc:
	@mkdir -p ../build-area
	dpkg-buildpackage -us -uc -b

# Build unsigned debug .deb (Debug, full symbols — for crash investigation)
# The binary is ~10× larger but gives full GDB backtraces.
# Install with: sudo dpkg -i ../qtgrass_*_amd64.deb
deb-debug:
	@mkdir -p ../build-area
	CMAKE_BUILD_TYPE=Debug dpkg-buildpackage -us -uc -b

# Build release .deb (unsigned — no GPG key required)
deb:
	@mkdir -p ../build-area
	dpkg-buildpackage -us -uc -b

clean:
	rm -rf $(BUILD_DIR) build-debug ../build-area
