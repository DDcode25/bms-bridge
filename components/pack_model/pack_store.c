#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "pack_model.h"

static SemaphoreHandle_t s_lock;
static pack_module_t s_modules[PACK_MAX_MODULES];
static uint8_t s_n;
static pack_cfg_t s_cfg;

void pack_store_init(uint8_t n_modules, const pack_cfg_t *cfg)
{
    s_lock = xSemaphoreCreateMutex();
    configASSERT(s_lock);
    s_n = n_modules > PACK_MAX_MODULES ? PACK_MAX_MODULES : n_modules;
    s_cfg = *cfg;
    memset(s_modules, 0, sizeof(s_modules));
}

void pack_store_put(uint8_t idx, const pack_module_t *m)
{
    if (idx >= s_n) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_modules[idx] = *m;
    xSemaphoreGive(s_lock);
}

void pack_store_get(uint8_t idx, pack_module_t *out)
{
    memset(out, 0, sizeof(*out));
    if (idx >= s_n) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_modules[idx];
    xSemaphoreGive(s_lock);
}

void pack_store_summary(int64_t now_us, pack_summary_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    pack_aggregate(s_modules, s_n, now_us, &s_cfg, out);
    xSemaphoreGive(s_lock);
}
