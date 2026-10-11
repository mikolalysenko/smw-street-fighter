CC ?= cc
PYTHON ?= python3
SDL_CONFIG ?= sdl2-config
CPPFLAGS += -Isrc
CFLAGS ?= -O2 -g
WARNINGS = -Wall -Wextra -Wpedantic
SDL_CFLAGS = $(shell $(SDL_CONFIG) --cflags)
SDL_LIBS = $(shell $(SDL_CONFIG) --libs)
CORE = src/fight.c
MEDIA = src/draw.c src/audio.c src/assets.c

.PHONY: all dojo run tv test sanitize smoke capture smw test-smw test-play assets original-assets package clean
all: smw
dojo: build/world-warrior-dojo
build:
	mkdir -p build
build/world-warrior-dojo: $(CORE) $(MEDIA) src/dojo.c src/fight.h src/draw.h src/audio.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -std=c11 $(SDL_CFLAGS) $(CORE) $(MEDIA) src/dojo.c -o $@ $(SDL_LIBS) -lm
run: smw
	./scripts/play-smw.sh
tv: smw
	./scripts/play-smw.sh --fullscreen
build/test-fight: $(CORE) tests/test_fight.c src/fight.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -Werror -std=c11 $(CORE) tests/test_fight.c -o $@ -lm
build/test-assets: src/assets.c src/assets.h tests/test_assets.c src/draw.h src/fight.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -Werror -std=c11 src/assets.c tests/test_assets.c -o $@
test: build/test-fight build/test-assets
	./build/test-fight
	./build/test-assets
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py' -v
sanitize: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARNINGS) -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer $(CORE) tests/test_fight.c -o build/test-fight-sanitize -lm
	./build/test-fight-sanitize
	$(CC) $(CPPFLAGS) -O1 -g $(WARNINGS) -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer src/assets.c tests/test_assets.c -o build/test-assets-sanitize
	./build/test-assets-sanitize
smoke: dojo
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/world-warrior-dojo --smoke 600
capture: dojo
	mkdir -p build/screenshots
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/world-warrior-dojo --capture build/screenshots
smw:
	$(PYTHON) scripts/prepare_smw.py
	$(MAKE) -C build/smw smw CC="$(CC)" CFLAGS="-O2 -g -fno-strict-aliasing $(SDL_CFLAGS) -I. -DSYSTEM_VOLUME_MIXER_AVAILABLE=0"
assets:
	$(PYTHON) scripts/import_smw.py "$(ROM)"
build/asset-tools/bin/python3: requirements-assets.txt | build
	$(PYTHON) -m venv build/asset-tools
	build/asset-tools/bin/pip install -r requirements-assets.txt
original-assets: build/asset-tools/bin/python3
	$(PYTHON) scripts/bootstrap_originals.py
	build/asset-tools/bin/python3 scripts/import_community_sf2.py
package: smw
	$(PYTHON) scripts/package_game.py
test-smw: smw
	$(PYTHON) scripts/test_smw_bridge.py
test-play: smw
	$(PYTHON) scripts/test_smw_play.py
clean:
	rm -rf build
