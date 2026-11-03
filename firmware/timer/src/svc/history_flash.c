#include "history_flash.h"

#include "esp_partition.h"

#define SECTOR 4096
#define PART_SUBTYPE 0x40       // eigener Daten-Untertyp, siehe partitions.csv

_Static_assert(HIST_REC_SIZE <= SECTOR, "Datensatz muss in einen Sektor passen");

static const esp_partition_t *s_part;

static bool part_read(void *ctx, uint16_t slot, uint8_t *buf)
{
    return esp_partition_read(s_part, (size_t)slot * SECTOR, buf, HIST_REC_SIZE) == ESP_OK;
}

static bool part_write(void *ctx, uint16_t slot, const uint8_t *buf)
{
    size_t off = (size_t)slot * SECTOR;
    return esp_partition_erase_range(s_part, off, SECTOR) == ESP_OK &&
           esp_partition_write(s_part, off, buf, HIST_REC_SIZE) == ESP_OK;
}

static bool part_erase_all(void *ctx)
{
    return esp_partition_erase_range(s_part, 0, (size_t)HIST_SLOTS * SECTOR) == ESP_OK;
}

bool history_flash_io(hist_io_t *io)
{
    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, PART_SUBTYPE, "history");
    if (s_part == NULL || s_part->size < (size_t)HIST_SLOTS * SECTOR) {
        return false;
    }
    io->read = part_read;
    io->write = part_write;
    io->erase_all = part_erase_all;
    io->ctx = NULL;
    return true;
}
