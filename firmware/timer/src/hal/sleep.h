// Tiefschlaf: Peripherie stromlos schalten und mit START (GPIO 14) wieder aufwecken.
#pragma once

#include <stdbool.h>

// Beim Start aufrufen: hebt das Halten der Pins aus dem Tiefschlaf auf. true = durch Taste geweckt.
bool sleep_after_boot(void);

// Schaltet Display, Analogteil und Beeper ab und geht in den Tiefschlaf. Kehrt nicht zurück.
void sleep_enter(void) __attribute__((noreturn));
