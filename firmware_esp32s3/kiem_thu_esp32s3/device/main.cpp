#include <Arduino.h>
#include "esp_heap_caps.h"
#include "esp_chip_info.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "WiFi.h"
#include "esp_bt.h"

extern "C" {
#include "wavelet_dsp.h"
#include "golden_impulse64.h"
}

static const int kPinMicWs = 4;
static const int kPinMicSck = 5;
static const int kPinMicSd = 6;
static const int kPinAmpLrck = 7;
static const int kPinAmpBclk = 8;
static const int kPinAmpDin = 9;

static DspWorkspace *g_ws = nullptr;
static SemaphoreHandle_t g_dspDone;

static float maxabs_diff(const float *a, const float *b, int n) {
    float m = 0.0f;
    for (int i = 0; i < n; ++i) {
        float d = fabsf(a[i] - b[i]);
        if (d > m) {
            m = d;
        }
    }
    return m;
}

static void radio_off() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_OFF);
    btStop();
}

static void print_mem(const char *tag) {
    Serial.printf("[%s] heap=%u  largest=%u  psram_free=%u  psram_size=%u\n",
                  tag,
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                  (unsigned)ESP.getFreePsram(),
                  (unsigned)ESP.getPsramSize());
}

static void dsp_task(void *arg) {
    (void)arg;
    float x[64];
    float ca[64];
    float cd[64];
    float rec[64];
    memset(x, 0, sizeof(x));
    x[0] = 1.0f;
    uint32_t t0 = millis();
    dwt_sym_coif5(x, 64, ca, cd);
    idwt_sym_coif5(ca, cd, 46, 64, rec);
    float e_dwt = maxabs_diff(ca, kImpulse64Approx, 46);
    float e_idwt = maxabs_diff(rec, x, 64);
    uint32_t dt_small = millis() - t0;

    float *block = (float *)heap_caps_malloc(DSP_BLOCK * sizeof(float), MALLOC_CAP_SPIRAM);
    float *out = (float *)heap_caps_malloc(DSP_BLOCK * sizeof(float), MALLOC_CAP_SPIRAM);
    float Pn[DSP_NBAND];
    if (!block || !out || !g_ws) {
        Serial.println("DSP alloc failed");
        vTaskDelete(nullptr);
        return;
    }
    for (int i = 0; i < DSP_BLOCK; ++i) {
        block[i] = 0.0f;
    }
    block[0] = 1.0f;
    t0 = millis();
    estimate_pn_frozen(block, DSP_BLOCK, DSP_NOISE_SAMPLES, Pn, g_ws);
    denoise_block_l5(block, DSP_BLOCK, Pn, out, g_ws);
    uint32_t dt_block = millis() - t0;

    Serial.printf("core_dsp=%d  dwt64_err=%.3e  idwt64_err=%.3e  t_small=%u ms\n",
                  xPortGetCoreID(), e_dwt, e_idwt, (unsigned)dt_small);
    Serial.printf("block1s  t=%u ms  deadline=1000 ms  %s\n",
                  (unsigned)dt_block, dt_block < 800 ? "OK" : "SLOW");
    Serial.printf("Pn:");
    for (int i = 0; i < DSP_NBAND; ++i) {
        Serial.printf(" %.3e", Pn[i]);
    }
    Serial.println();
    print_mem("after_dsp");
    xSemaphoreGive(g_dspDone);
    vTaskDelete(nullptr);
}

void setup() {
    Serial.begin(115200);
    delay(400);
    setCpuFrequencyMhz(240);
    radio_off();

    esp_chip_info_t info;
    esp_chip_info(&info);
    Serial.println("==== WaveletDSP bring-up ====");
    Serial.printf("chip=ESP32-S3 rev=%d cores=%d  cpu=%u MHz\n",
                  (int)info.revision, (int)info.cores, (unsigned)getCpuFrequencyMhz());
    Serial.printf("psram_size=%u  flash=%u\n",
                  (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFlashChipSize());
    Serial.printf("setup_core=%d  loop_core=%d\n", xPortGetCoreID(), xPortGetCoreID());
    Serial.printf("I2S pins UNVERIFIED mic WS=%d SCK=%d SD=%d  amp LRC=%d BCLK=%d DIN=%d\n",
                  kPinMicWs, kPinMicSck, kPinMicSd, kPinAmpLrck, kPinAmpBclk, kPinAmpDin);
    print_mem("boot");

    g_ws = (DspWorkspace *)heap_caps_malloc(sizeof(DspWorkspace), MALLOC_CAP_SPIRAM);
    g_dspDone = xSemaphoreCreateBinary();
    Serial.printf("workspace_bytes=%u  ptr=%p\n",
                  (unsigned)sizeof(DspWorkspace), (void *)g_ws);
    if (!g_ws || !g_dspDone) {
        Serial.println("PSRAM workspace alloc failed — stop");
        return;
    }

    /* Core 1: DSP. Arduino loop also on core 1; this task has a 32 KB stack. */
    BaseType_t ok = xTaskCreatePinnedToCore(
        dsp_task, "dsp", 32768, nullptr, 5, nullptr, 1);
    if (ok != pdPASS) {
        Serial.println("dsp task create failed");
    }
}

void loop() {
    static bool printed = false;
    if (!printed && g_dspDone && xSemaphoreTake(g_dspDone, 0) == pdTRUE) {
        Serial.println("bring-up done. I2S not started (pins unverified).");
        printed = true;
    }
    delay(200);
}
