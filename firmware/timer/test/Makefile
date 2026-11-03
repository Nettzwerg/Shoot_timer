# Host-Tests für den plattformunabhängigen Kern (src/core), ohne ESP32.
#   make -C firmware/timer/test
CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined
CORE = ../src/core
UI = ../src/ui
INC = -I$(CORE) -I$(UI) -I../src/hal
CORE_SRCS = $(CORE)/settings.c $(CORE)/shot_string.c $(CORE)/timer_core.c
UI_SRCS = $(UI)/ui.c $(UI)/gfx.c $(wildcard $(UI)/fonts/*.c)
HEADERS = $(wildcard $(CORE)/*.h $(UI)/*.h $(UI)/fonts/*.h) ui_fixture.h
SCREENS_DIR = ../../../docs/ui-screens

.PHONY: test screens clean
test: test_core test_ui
	./test_core
	./test_ui

test_core: test_core.c $(CORE_SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(INC) -o $@ test_core.c $(CORE_SRCS) -lm

test_ui: test_ui.c $(CORE_SRCS) $(UI_SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(INC) -o $@ test_ui.c $(CORE_SRCS) $(UI_SRCS) -lm

# Screenshots aller Bildschirme als PNG (braucht Python mit Pillow)
screens: screens.c $(CORE_SRCS) $(UI_SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(INC) -o screens screens.c $(CORE_SRCS) $(UI_SRCS) -lm
	mkdir -p $(SCREENS_DIR)
	./screens $(SCREENS_DIR)
	python3 -c "import glob, os; from PIL import Image; [ (Image.open(p).resize((640, 480), Image.NEAREST).save(p[:-4] + '.png'), os.remove(p)) for p in glob.glob('$(SCREENS_DIR)/*.ppm') ]"

clean:
	rm -f test_core test_ui screens
