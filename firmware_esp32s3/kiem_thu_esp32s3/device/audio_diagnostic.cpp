#include <Arduino.h>
#include <atomic>
#include <cstring>
#include "WiFi.h"
#include "esp_bt.h"
#include "esp_task_wdt.h"
#include "audio_i2s.h"
#include "audio_stream.h"
#include "audio_diagnostic_signal.h"

struct DiagnosticResult {
    int percent;
    int frames;
    int tone_frames;
    int error;
    bool stopped;
    bool cleanup_ok;
};

static QueueHandle_t requests;
static QueueHandle_t results;
static TaskHandle_t worker_handle;
static std::atomic<bool> busy{false};
static std::atomic<bool> stop_requested{false};
static std::atomic<bool> worker_ready{false};
static int32_t rx_words[AUDIO_CHUNK * 2];
static int32_t tx_words[AUDIO_CHUNK * 2];

static void diagnostic_worker(void *) {
    if (esp_task_wdt_add(nullptr) != ESP_OK) {
        vTaskDelete(nullptr);
        return;
    }
    worker_ready.store(true);
    for (;;) {
        esp_task_wdt_reset();
        int percent;
        if (xQueueReceive(requests, &percent, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
        }
        DiagnosticResult result = {};
        result.percent = percent;
        result.cleanup_ok = true;
        result.error = audio_i2s_begin();
        bool installed = result.error == ESP_OK;
        for (int frame = 0; installed && frame < DIAGNOSTIC_TOTAL_SAMPLES;
             frame += AUDIO_CHUNK) {
            esp_task_wdt_reset();
            if (stop_requested.load()) {
                result.stopped = true;
                break;
            }
            result.error = audio_i2s_read(rx_words);
            if (result.error != ESP_OK) {
                break;
            }
            for (int offset = 0; offset < AUDIO_CHUNK; ++offset) {
                bool clipped;
                float sample = diagnostic_tone_sample(frame + offset, percent);
                int16_t pcm = audio_encode_pcm(sample, 1.0f, &clipped);
                if (clipped) {
                    result.error = ESP_ERR_INVALID_ARG;
                    break;
                }
                int32_t word = static_cast<int32_t>(pcm) * 65536;
                tx_words[2 * offset] = word;
                tx_words[2 * offset + 1] = word;
            }
            if (result.error != ESP_OK) {
                break;
            }
            result.error = audio_i2s_write(tx_words);
            if (result.error != ESP_OK) {
                break;
            }
            result.frames += AUDIO_CHUNK;
            int tone_remaining = DIAGNOSTIC_TONE_SAMPLES - frame;
            if (tone_remaining > 0) {
                result.tone_frames += tone_remaining < AUDIO_CHUNK ? tone_remaining : AUDIO_CHUNK;
            }
            if (!audio_i2s_events_ok()) {
                result.error = ESP_FAIL;
                break;
            }
        }
        if (installed) {
            memset(tx_words, 0, sizeof(tx_words));
            for (int chunk = 0; chunk <= AUDIO_DMA_COUNT; ++chunk) {
                esp_task_wdt_reset();
                if (audio_i2s_read(rx_words) != ESP_OK || audio_i2s_write(tx_words) != ESP_OK) {
                    result.cleanup_ok = false;
                    break;
                }
                if (!audio_i2s_events_ok()) {
                    result.cleanup_ok = false;
                }
            }
            audio_i2s_end();
        }
        xQueueOverwrite(results, &result);
    }
}

static void command_received(const char *command) {
    if (!strcmp(command, "stop")) {
        stop_requested.store(true);
        Serial.println("ACK stop");
    } else if (!strcmp(command, "status")) {
        Serial.printf("DIAG state=%s ready=%d fs=%d source=synthetic sd=external gpio10=unused\n",
                      busy.load() ? "BUSY" : "IDLE", worker_ready.load(), DSP_FS);
    } else {
        int percent = diagnostic_tone_percent(command);
        if (!percent || !worker_ready.load()) {
            Serial.println("REJECT use: tone 1 | tone 3 | tone 5 | tone 15 | tone 25 | stop | status");
            return;
        }
        bool expected = false;
        if (!busy.compare_exchange_strong(expected, true)) {
            Serial.println("REJECT busy");
            return;
        }
        stop_requested.store(false);
        Serial.printf("ACK tone percent=%d hz=1000 tone_frames=%d total_frames=%d\n",
                      percent, DIAGNOSTIC_TONE_SAMPLES, DIAGNOSTIC_TOTAL_SAMPLES);
        if (xQueueSend(requests, &percent, 0) != pdTRUE) {
            busy.store(false);
            Serial.println("FAULT request_queue");
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(400);
    setCpuFrequencyMhz(240);
    WiFi.persistent(false);
    WiFi.mode(WIFI_OFF);
    btStop();
    pinMode(AUDIO_PIN_AMP_MUTE, INPUT);
    requests = xQueueCreate(1, sizeof(int));
    results = xQueueCreate(1, sizeof(DiagnosticResult));
    if (!requests || !results ||
        xTaskCreatePinnedToCore(diagnostic_worker, "audio_diag", 6144, nullptr, 4,
                                &worker_handle, 0) != pdPASS) {
        Serial.println("FAULT diagnostic_init");
        return;
    }
    Serial.println("==== AUDIO DIAGNOSTIC v1: no DSP, no autoplay ====");
#ifdef AUDIO_DIAGNOSTIC_TX_ONLY
    Serial.println("BACKEND tx_only direct_amp_pins WS=7 BCLK=8 DIN=9 no_mic no_clock_fanout");
#else
    Serial.println("BACKEND full_duplex shared_clock_fanout");
#endif
    Serial.println("SD is external; GPIO10 unused. Commands: status | tone 1 | tone 3 | tone 5 | tone 15 | tone 25 | stop");
}

void loop() {
    static char command[48];
    static size_t length;
    static bool overflow;
    for (int index = 0; index < 64 && Serial.available(); ++index) {
        char character = static_cast<char>(Serial.read());
        if (character == '\n' || character == '\r') {
            if (length && !overflow) {
                command[length] = '\0';
                command_received(command);
            } else if (overflow) {
                Serial.println("REJECT command_too_long");
            }
            length = 0;
            overflow = false;
        } else if (!overflow && length + 1 < sizeof(command)) {
            command[length++] = character;
        } else {
            overflow = true;
        }
    }
    DiagnosticResult result;
    if (results && xQueueReceive(results, &result, 0) == pdTRUE) {
        busy.store(false);
        Serial.printf("DONE tone percent=%d frames=%d tone_frames=%d error=%d stopped=%d cleanup_ok=%d state=IDLE\n",
                      result.percent, result.frames, result.tone_frames, result.error,
                      result.stopped, result.cleanup_ok);
    }
    delay(10);
}
