# Host-Tests für den plattformunabhängigen Kern (src/core), ohne ESP32.
#   make -C firmware/timer/test
CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined
CORE = ../src/core
SRCS = test_core.c $(CORE)/settings.c $(CORE)/shot_string.c $(CORE)/timer_core.c

.PHONY: test clean
test: test_core
	./test_core

test_core: $(SRCS) $(wildcard $(CORE)/*.h)
	$(CC) $(CFLAGS) -I$(CORE) -o $@ $(SRCS) -lm

clean:
	rm -f test_core
