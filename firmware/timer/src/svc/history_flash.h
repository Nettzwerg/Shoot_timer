// Ein-/Ausgabe der Historie auf der Flash-Partition "history" (je Datensatz ein 4-KB-Sektor).
#pragma once

#include <stdbool.h>

#include "history.h"

// Liefert die IO-Funktionen; false, wenn die Partition fehlt (falsche Partitionstabelle).
bool history_flash_io(hist_io_t *io);
