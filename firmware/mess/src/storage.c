#include "storage.h"

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define BASE        "/s"
#define LABEL       "storage"
#define CHUNK_PAIRS 256

typedef struct {
    uint32_t trigger_idx;
    uint32_t start_idx;
    uint32_t pairs;
} cap_header_t;

static SemaphoreHandle_t s_lock;
static FILE *s_log;
static bool s_ok;
static uint32_t s_session;
static uint32_t s_cap_no;

static uint32_t next_session(void)
{
    uint32_t max = 0;
    DIR *d = opendir(BASE);
    if (d == NULL) {
        return 1;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        unsigned n;
        if (sscanf(e->d_name, "l%u.txt", &n) == 1 && n > max) {
            max = n;
        }
    }
    closedir(d);
    return max + 1;
}

static bool open_session(void)
{
    char path[32];
    s_session = next_session();
    s_cap_no = 0;
    snprintf(path, sizeof(path), BASE "/l%03lu.txt", (unsigned long)s_session);
    s_log = fopen(path, "w");
    return s_log != NULL;
}

bool store_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    esp_vfs_spiffs_conf_t conf = {
        .base_path = BASE,
        .partition_label = LABEL,
        .max_files = 4,
        .format_if_mount_failed = true,   // beim allerersten Start: Formatieren dauert bis zu 1 min
    };
    if (esp_vfs_spiffs_register(&conf) != ESP_OK) {
        return false;
    }
    s_ok = open_session();
    return s_ok;
}

bool store_ok(void)
{
    return s_ok;
}

uint32_t store_session(void)
{
    return s_session;
}

void store_line(const char *fmt, ...)
{
    if (!s_ok) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(s_log, fmt, ap);
    va_end(ap);
    fputc('\n', s_log);
    fflush(s_log);
    fsync(fileno(s_log));   // bei Stromausfall (Powerbank ab) bleibt alles bis hier erhalten
    xSemaphoreGive(s_lock);
}

void store_capture(uint32_t trigger_idx, uint32_t start_idx, const uint16_t *data, uint32_t pairs)
{
    if (!s_ok) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    char path[32];
    snprintf(path, sizeof(path), BASE "/c%03lu_%03lu.bin", (unsigned long)s_session,
             (unsigned long)++s_cap_no);
    FILE *f = fopen(path, "wb");
    if (f != NULL) {
        cap_header_t h = { .trigger_idx = trigger_idx, .start_idx = start_idx, .pairs = pairs };
        fwrite(&h, sizeof(h), 1, f);
        fwrite(data, sizeof(uint16_t) * 2, pairs, f);
        fclose(f);
    }
    xSemaphoreGive(s_lock);
}

void store_info(void)
{
    size_t total = 0, used = 0;
    esp_spiffs_info(LABEL, &total, &used);
    printf("# Speicher %s: Sitzung %lu, belegt %u von %u kB\n", s_ok ? "ok" : "FEHLER",
           (unsigned long)s_session, (unsigned)(used / 1024), (unsigned)(total / 1024));
}

static void dump_log(const char *name, const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return;
    }
    printf("FILE_BEGIN,%s\n", name);
    char line[160];
    while (fgets(line, sizeof(line), f) != NULL) {
        fputs(line, stdout);
    }
    printf("FILE_END\n");
    fclose(f);
}

static void dump_capture(const char *name, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return;
    }
    cap_header_t h;
    if (fread(&h, sizeof(h), 1, f) == 1) {
        printf("CAP_BEGIN,%lu,%lu,%s\n", (unsigned long)h.trigger_idx, (unsigned long)h.pairs, name);
        static uint16_t buf[CHUNK_PAIRS * 2];
        uint32_t idx = h.start_idx;
        size_t n;
        while ((n = fread(buf, sizeof(uint16_t) * 2, CHUNK_PAIRS, f)) > 0) {
            for (size_t k = 0; k < n; k++) {
                printf("%lu,%u,%u\n", (unsigned long)idx++, buf[k * 2], buf[k * 2 + 1]);
            }
        }
        printf("CAP_END\n");
    }
    fclose(f);
}

void store_dump(void)
{
    if (!s_ok) {
        printf("DUMP_END\n");
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    DIR *d = opendir(BASE);
    struct dirent *e;
    while (d != NULL && (e = readdir(d)) != NULL) {
        char path[300];
        snprintf(path, sizeof(path), BASE "/%s", e->d_name);
        if (e->d_name[0] == 'l') {
            dump_log(e->d_name, path);
        } else if (e->d_name[0] == 'c') {
            dump_capture(e->d_name, path);
        }
    }
    if (d != NULL) {
        closedir(d);
    }
    printf("DUMP_END\n");
    xSemaphoreGive(s_lock);
}

void store_erase(void)
{
    if (!s_ok) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    fclose(s_log);
    DIR *d = opendir(BASE);
    struct dirent *e;
    while (d != NULL && (e = readdir(d)) != NULL) {
        char path[300];
        snprintf(path, sizeof(path), BASE "/%s", e->d_name);
        unlink(path);
    }
    if (d != NULL) {
        closedir(d);
    }
    s_ok = open_session();
    xSemaphoreGive(s_lock);
}
