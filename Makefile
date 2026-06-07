TARGET := airdomergds
BUILD_DIR := build
SRC := src/main.cpp src/dyn_sdl.cpp
RGDS_AUDIO_HEADER := src/generated_audio_rgds.h
RGDS_ASSET_FILES := \
	assets/rgds/level_1.argb \
	assets/rgds/level_2.argb \
	assets/rgds/level_3.argb \
	assets/rgds/level_4.argb \
	assets/rgds/level_5.argb \
	assets/rgds/level_6.argb \
	assets/rgds/level_1_death.argb \
	assets/rgds/level_2_death.argb \
	assets/rgds/level_3_death.argb \
	assets/rgds/level_4_death.argb \
	assets/rgds/level_5_death.argb \
	assets/rgds/level_6_death.argb \
	assets/rgds/title_top.argb \
	assets/rgds/title_bottom.argb \
	assets/rgds/hud_easy.argb \
	assets/rgds/hud_normal.argb \
	assets/rgds/hud_hard.argb
LOCAL_SDL_CONFIG := $(CURDIR)/.deps/sdl2/bin/sdl2-config
SDL_CONFIG ?= $(if $(wildcard $(LOCAL_SDL_CONFIG)),$(LOCAL_SDL_CONFIG),sdl2-config)
SDL_CFLAGS ?= $(shell $(SDL_CONFIG) --cflags 2>/dev/null)
ZIG ?= $(CURDIR)/.deps/zig/zig
RGDS_TARGET ?= aarch64-linux-gnu.2.31
RGDS_CXXFLAGS ?= -O2 -s -fno-exceptions -fno-rtti -nostdlib++
RGDS_HOST ?=
RGDS_PORTS_DIR ?= /mnt/mmc/Ports

CPPFLAGS += $(SDL_CFLAGS)

.DEFAULT_GOAL := all

all: rgds

check-sdl:
	@if [ -z "$(strip $(SDL_CFLAGS))" ]; then \
		echo "SDL2 headers were not found through sdl2-config. Install SDL2 headers or pass SDL_CFLAGS manually."; \
		exit 1; \
	fi

$(BUILD_DIR)/$(TARGET)-rgds-aarch64: $(SRC) $(RGDS_AUDIO_HEADER) | check-zig check-sdl $(BUILD_DIR)
	$(ZIG) c++ -target $(RGDS_TARGET) $(CPPFLAGS) -DAIRDOME_DYNAMIC_SDL -std=c++17 -Wall -Wextra -Wpedantic $(RGDS_CXXFLAGS) $(SRC) -o $@ -ldl

rgds: $(BUILD_DIR)/$(TARGET)-rgds-aarch64

check-rgds-assets:
	@for file in $(RGDS_ASSET_FILES); do \
		if [ ! -f "$$file" ]; then \
			echo "Missing generated RG DS asset: $$file"; \
			echo "Restore the checked-in assets/rgds files before packaging."; \
			exit 1; \
		fi; \
	done

check-zig:
	@if [ ! -x "$(ZIG)" ]; then \
		echo "Zig was not found at $(ZIG)."; \
		exit 1; \
	fi

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

package-rgds: rgds check-rgds-assets
	rm -rf dist/rgds
	mkdir -p dist/rgds/AirdomeRGDS
	cp ports/AirdomeRGDS.sh dist/rgds/AirdomeRGDS.sh
	cp $(BUILD_DIR)/$(TARGET)-rgds-aarch64 dist/rgds/AirdomeRGDS/$(TARGET)
	cp ports/rgds_volume_helper.py dist/rgds/AirdomeRGDS/rgds_volume_helper.py
	cp -R assets dist/rgds/AirdomeRGDS/assets
	find dist/rgds -name .DS_Store -delete
	chmod +x dist/rgds/AirdomeRGDS.sh dist/rgds/AirdomeRGDS/$(TARGET) dist/rgds/AirdomeRGDS/rgds_volume_helper.py
	cd dist && COPYFILE_DISABLE=1 tar -czf airdomergds-rgds-aarch64.tar.gz rgds

upload-rgds: package-rgds
	@if [ -z "$(strip $(RGDS_HOST))" ]; then \
		echo "Set RGDS_HOST, for example: make upload-rgds RGDS_HOST=root@192.168.1.51"; \
		exit 1; \
	fi
	scp -r dist/rgds/AirdomeRGDS dist/rgds/AirdomeRGDS.sh $(RGDS_HOST):$(RGDS_PORTS_DIR)/

.PHONY: all check-sdl check-zig check-rgds-assets rgds clean package-rgds upload-rgds
