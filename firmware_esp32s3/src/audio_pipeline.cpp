#include "audio_pipeline.h"
#include "audio_config.h"
#include "audio_i2s.h"
#include "audio_stream.h"

#include <Arduino.h>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"

enum class State { Idle, Starting, Calibrating, Buffering, Running, Stopping, Fault };
enum class Fault {
    None, Memory, Task, Watchdog, I2sInit, Read, Write, Dma, InputQueue,
    OutputQueue, Underrun, Sequence, Calibration, DspDeadline, NonFinite, Handshake
};
enum class Command { Start, Raw };

struct Frame {
    int slot;
    uint32_t sequence;
};

static const char *state_names[] = {
    "IDLE", "STARTING", "CALIBRATING", "BUFFERING", "RUNNING", "STOPPING", "FAULT"
};
static const char *fault_names[] = {
    "none", "memory", "task", "watchdog", "i2s_init", "rx_timeout", "tx_timeout",
    "dma_overflow", "input_queue", "output_queue", "underrun", "sequence",
    "noise_profile", "dsp_deadline", "nonfinite", "dsp_handshake"
};
static std::atomic<State> state{State::Idle};
static std::atomic<Fault> fault{Fault::None};
static std::atomic<bool> stop_requested{false};
static std::atomic<bool> raw_mode{false};
static std::atomic<uint32_t> volume_percent{AUDIO_BOOT_VOLUME_PERCENT};
static std::atomic<uint32_t> processed{0}, dsp_ms{0}, dsp_max_ms{0};
static std::atomic<uint32_t> underruns{0}, overruns{0}, dma_errors{0}, clipped{0};
static std::atomic<uint32_t> rx_peak_ppm{0}, io_heartbeat{0}, dsp_heartbeat{0};
static QueueHandle_t input_free, input_ready, output_free, output_ready;
static QueueHandle_t commands, dsp_done;
static QueueHandle_t profiles;
static TaskHandle_t io_handle, dsp_handle;
static DspWorkspace *workspace;
static float *inputs[AUDIO_INPUT_SLOTS], *outputs[AUDIO_OUTPUT_SLOTS], *tail;
static float noise_powers[DSP_NBAND];
#if AUDIO_TEST_CAPTURE
static float *noise_snapshot;
static std::atomic<bool> noise_snapshot_ready{false};
static constexpr uint32_t TRACE_FRAMES = 8 * DSP_HOP;
static int16_t *filter_trace;
static std::atomic<uint32_t> trace_frames{0};
static uint32_t trace_volume;
#endif
static int32_t rx_words[AUDIO_CHUNK * 2], tx_words[AUDIO_CHUNK * 2];
static std::atomic<bool> ready{false};
static portMUX_TYPE mute_lock = portMUX_INITIALIZER_UNLOCKED;

static_assert(DSP_HOP % (1 << DSP_LEVEL) == 0, "DWT hop must preserve phase");
static_assert(DSP_HOP > DSP_OVERLAP, "Only adjacent frames may overlap");
static_assert(AUDIO_CHUNK * 2 * sizeof(int32_t) <= 4092, "DMA descriptor too large");
static_assert(AUDIO_BOOT_VOLUME_PERCENT <= AUDIO_MAX_VOLUME * 100.0f, "Boot volume exceeds limit");

static void fail(Fault reason) {
    portENTER_CRITICAL(&mute_lock);
    Fault expected = Fault::None;
    fault.compare_exchange_strong(expected, reason);
    if (AUDIO_EXTERNAL_NPN_MUTE) {
        digitalWrite(AUDIO_PIN_AMP_MUTE, HIGH);
    }
    portEXIT_CRITICAL(&mute_lock);
}

static void mute(bool enabled) {
    portENTER_CRITICAL(&mute_lock);
    if (AUDIO_EXTERNAL_NPN_MUTE) {
        digitalWrite(AUDIO_PIN_AMP_MUTE,
                     enabled || fault.load() != Fault::None || stop_requested.load() ? HIGH : LOW);
    }
    portEXIT_CRITICAL(&mute_lock);
}

static void dsp_task(void *) {
    const bool watchdog = esp_task_wdt_add(nullptr) == ESP_OK;
    if (!watchdog) {
        fail(Fault::Watchdog);
        ready.store(false);
    }
    bool has_tail = false;
    for (;;) {
        if (watchdog) {
            esp_task_wdt_reset();
        }
        dsp_heartbeat.store(millis());
        Frame frame;
        if (xQueueReceive(input_ready, &frame, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
        }
        if (frame.slot < 0) {
            has_tail = false;
            bool done = true;
            if (xQueueSend(dsp_done, &done, 0) != pdTRUE) {
                fail(Fault::Handshake);
            }
            continue;
        }
        const int64_t began = esp_timer_get_time();
        int output_slot = -1;
        if (fault.load() == Fault::None && !stop_requested.load()) {
            if (xQueueReceive(output_free, &output_slot, 0) != pdTRUE) {
                fail(Fault::OutputQueue);
            } else {
                const float *input = inputs[frame.slot];
                float *output = outputs[output_slot];
                if (frame.sequence == 0) {
                    has_tail = false;
                    if (!raw_mode.load()) {
                        estimate_pn_frozen(input, DSP_BLOCK, DSP_NOISE_SAMPLES,
                                           noise_powers, workspace);
                        if (!audio_noise_valid(input, DSP_NOISE_SAMPLES, noise_powers)) {
                            fail(Fault::Calibration);
                        } else {
#if AUDIO_TEST_CAPTURE
                            memcpy(noise_snapshot, input, DSP_NOISE_SAMPLES * sizeof(float));
                            noise_snapshot_ready.store(true);
#endif
                            if (xQueueSend(profiles, noise_powers, 0) != pdTRUE) {
                                fail(Fault::Calibration);
                            }
                        }
                    }
                }
                if (fault.load() == Fault::None) {
                    if (raw_mode.load()) {
                        memcpy(output, input, DSP_BLOCK * sizeof(float));
                    } else {
                        denoise_block_l5(input, DSP_BLOCK, noise_powers, output, workspace);
                    }
                    for (int index = 0; index < DSP_BLOCK; ++index) {
                        if (!std::isfinite(output[index])) {
                            fail(Fault::NonFinite);
                            break;
                        }
                    }
                    audio_join_frame(tail, &has_tail, output, DSP_BLOCK);
#if AUDIO_TEST_CAPTURE
                    if (!raw_mode.load() && frame.sequence < 8 && fault.load() == Fault::None) {
                        const uint32_t base = frame.sequence * DSP_HOP;
                        for (int i = 0; i < DSP_HOP; ++i) {
                            bool limited;
                            filter_trace[2 * (base + i)] = audio_encode_raw_pcm(input[i], trace_volume * 0.01f, &limited);
                            filter_trace[2 * (base + i) + 1] = audio_encode_raw_pcm(output[i], trace_volume * 0.01f, &limited);
                        }
                        trace_frames.store(base + DSP_HOP);
                    }
#endif
                    uint32_t elapsed = static_cast<uint32_t>((esp_timer_get_time() - began) / 1000);
                    dsp_ms.store(elapsed);
                    if (elapsed > dsp_max_ms.load()) {
                        dsp_max_ms.store(elapsed);
                    }
                    if (elapsed >= DSP_HOP * 1000 / DSP_FS) {
                        fail(Fault::DspDeadline);
                    }
                    if (fault.load() == Fault::None && !stop_requested.load()) {
                        Frame result{output_slot, frame.sequence};
                        if (xQueueSend(output_ready, &result, 0) != pdTRUE) {
                            fail(Fault::OutputQueue);
                        } else {
                            output_slot = -1;
                            processed.fetch_add(1);
                            if (frame.sequence == 0) {
                                state.store(State::Buffering);
                            }
                        }
                    }
                }
            }
        }
        if (output_slot >= 0 && xQueueSend(output_free, &output_slot, 0) != pdTRUE) {
            fail(Fault::OutputQueue);
        }
        if (xQueueSend(input_free, &frame.slot, 0) != pdTRUE) {
            fail(Fault::InputQueue);
        }
        vTaskDelay(1);
    }
}

static bool reset_pools() {
    xQueueReset(input_free);
    xQueueReset(input_ready);
    xQueueReset(output_free);
    xQueueReset(output_ready);
    xQueueReset(dsp_done);
    xQueueReset(profiles);
    for (int index = 0; index < AUDIO_INPUT_SLOTS; ++index) {
        if (xQueueSend(input_free, &index, 0) != pdTRUE) {
            return false;
        }
    }
    for (int index = 0; index < AUDIO_OUTPUT_SLOTS; ++index) {
        if (xQueueSend(output_free, &index, 0) != pdTRUE) {
            return false;
        }
    }
    return true;
}

static void run_audio() {
    if (!reset_pools()) {
        fail(Fault::InputQueue);
        return;
    }
    int input_slot = -1;
    if (xQueueReceive(input_free, &input_slot, 0) != pdTRUE) {
        fail(Fault::InputQueue);
        return;
    }
    state.store(raw_mode.load() ? State::Buffering : State::Calibrating);
    if (audio_i2s_begin() != ESP_OK) {
        fail(Fault::I2sInit);
        return;
    }
    uint32_t sequence = 0, output_sequence = 0;
    uint64_t stream_samples = 0;
    int settled = 0, filled = 0, output_slot = -1, output_pos = 0;
    const int settle_samples = raw_mode.load() ? AUDIO_RAW_SETTLE_SAMPLES : AUDIO_SETTLE_SAMPLES;
    bool playing = false;
    while (!stop_requested.load() && fault.load() == Fault::None) {
        esp_task_wdt_reset();
        io_heartbeat.store(millis());
        if (audio_i2s_read(rx_words) != ESP_OK) {
            fail(Fault::Read);
            break;
        }
        memset(tx_words, 0, sizeof(tx_words));
        float peak = 0.0f;
        for (int index = 0; index < AUDIO_CHUNK; ++index) {
            if (settled < settle_samples) {
                ++settled;
                continue;
            }
            float sample = audio_decode_mic(rx_words[2 * index + AUDIO_MIC_SLOT]);
            peak = fmaxf(peak, fabsf(sample));
            inputs[input_slot][filled++] = sample;
            if (filled == DSP_BLOCK) {
                int next_slot;
                if (xQueueReceive(input_free, &next_slot, 0) != pdTRUE) {
                    overruns.fetch_add(1);
                    fail(Fault::InputQueue);
                    break;
                }
                memcpy(inputs[next_slot], inputs[input_slot] + DSP_HOP,
                       DSP_OVERLAP * sizeof(float));
                Frame frame{input_slot, sequence++};
                if (xQueueSend(input_ready, &frame, 0) != pdTRUE) {
                    overruns.fetch_add(1);
                    fail(Fault::InputQueue);
                    break;
                }
                input_slot = next_slot;
                filled = DSP_OVERLAP;
            }
            if (stream_samples >= AUDIO_DELAY_SAMPLES) {
                if (output_slot < 0) {
                    Frame frame;
                    if (xQueueReceive(output_ready, &frame, 0) != pdTRUE) {
                        underruns.fetch_add(1);
                        fail(Fault::Underrun);
                        break;
                    }
                    if (frame.sequence != output_sequence++) {
                        fail(Fault::Sequence);
                        break;
                    }
                    output_slot = frame.slot;
                    output_pos = 0;
                }
                bool saturated;
                float ramp = fminf(1.0f, static_cast<float>(stream_samples - AUDIO_DELAY_SAMPLES) / 160.0f);
                float output_sample = outputs[output_slot][output_pos++];
                float output_volume = ramp * volume_percent.load() * 0.01f;
                int16_t pcm = audio_encode_raw_pcm(output_sample, output_volume, &saturated);
                if (saturated) {
                    clipped.fetch_add(1);
                }
                int32_t word = static_cast<int32_t>(pcm) * 65536;
                tx_words[2 * index] = word;
                tx_words[2 * index + 1] = word;
                playing = true;
                if (output_pos == DSP_HOP) {
                    if (xQueueSend(output_free, &output_slot, 0) != pdTRUE) {
                        fail(Fault::OutputQueue);
                        break;
                    }
                    output_slot = -1;
                }
            }
            ++stream_samples;
        }
        rx_peak_ppm.store(static_cast<uint32_t>(peak * 1000000.0f));
        if (fault.load() != Fault::None || stop_requested.load()) {
            break;
        }
        if (audio_i2s_write(tx_words) != ESP_OK) {
            fail(Fault::Write);
            break;
        }
        if (!audio_i2s_events_ok()) {
            dma_errors.fetch_add(1);
            fail(Fault::Dma);
            break;
        }
        if (playing) {
            mute(false);
            state.store(State::Running);
        }
    }
    mute(true);
    state.store(State::Stopping);
    audio_i2s_end();
    Frame end{-1, 0};
    bool done = false;
    if (xQueueSend(input_ready, &end, pdMS_TO_TICKS(2000)) != pdTRUE ||
        xQueueReceive(dsp_done, &done, pdMS_TO_TICKS(2000)) != pdTRUE) {
        fail(Fault::Handshake);
        ready = false;
    }
}

static void io_task(void *) {
    const bool watchdog = esp_task_wdt_add(nullptr) == ESP_OK;
    if (!watchdog) {
        fail(Fault::Watchdog);
        state.store(State::Fault);
        ready.store(false);
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    for (;;) {
        esp_task_wdt_reset();
        io_heartbeat.store(millis());
        Command command;
        if (xQueueReceive(commands, &command, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
        }
        if (!ready || fault.load() == Fault::Watchdog) {
            continue;
        }
        raw_mode.store(command == Command::Raw);
#if AUDIO_TEST_CAPTURE
        noise_snapshot_ready.store(false);
        trace_frames.store(0);
        trace_volume = volume_percent.load();
#endif
        fault.store(Fault::None);
        processed.store(0);
        dsp_ms.store(0);
        dsp_max_ms.store(0);
        underruns.store(0);
        overruns.store(0);
        dma_errors.store(0);
        clipped.store(0);
        state.store(State::Starting);
        mute(true);
        run_audio();
        mute(true);
        state.store(fault.load() == Fault::None ? State::Idle : State::Fault);
    }
}

void audio_pipeline_init() {
    if (AUDIO_EXTERNAL_NPN_MUTE) {
        digitalWrite(AUDIO_PIN_AMP_MUTE, HIGH);
        pinMode(AUDIO_PIN_AMP_MUTE, OUTPUT);
    } else {
        pinMode(AUDIO_PIN_AMP_MUTE, INPUT);
    }
    workspace = static_cast<DspWorkspace *>(heap_caps_calloc(1, sizeof(DspWorkspace), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    tail = static_cast<float *>(heap_caps_calloc(DSP_OVERLAP, sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    bool allocated = workspace && tail;
#if AUDIO_TEST_CAPTURE
    noise_snapshot = static_cast<float *>(heap_caps_calloc(DSP_NOISE_SAMPLES, sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    filter_trace = static_cast<int16_t *>(heap_caps_calloc(TRACE_FRAMES * 2, sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    allocated = allocated && noise_snapshot && filter_trace;
#endif
    for (int index = 0; index < AUDIO_INPUT_SLOTS; ++index) {
        inputs[index] = static_cast<float *>(heap_caps_calloc(DSP_BLOCK, sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        allocated = allocated && inputs[index];
    }
    for (int index = 0; index < AUDIO_OUTPUT_SLOTS; ++index) {
        outputs[index] = static_cast<float *>(heap_caps_calloc(DSP_BLOCK, sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        allocated = allocated && outputs[index];
    }
    input_free = xQueueCreate(AUDIO_INPUT_SLOTS, sizeof(int));
    input_ready = xQueueCreate(AUDIO_INPUT_SLOTS + 1, sizeof(Frame));
    output_free = xQueueCreate(AUDIO_OUTPUT_SLOTS, sizeof(int));
    output_ready = xQueueCreate(AUDIO_OUTPUT_SLOTS, sizeof(Frame));
    commands = xQueueCreate(1, sizeof(Command));
    dsp_done = xQueueCreate(1, sizeof(bool));
    profiles = xQueueCreate(1, sizeof(noise_powers));
    if (!allocated || !input_free || !input_ready || !output_free || !output_ready || !commands || !dsp_done || !profiles) {
        fail(Fault::Memory);
        state.store(State::Fault);
        return;
    }
    ready = true;
    if (xTaskCreatePinnedToCore(dsp_task, "audio_dsp", 6144, nullptr, 2, &dsp_handle, 1) != pdPASS ||
        xTaskCreatePinnedToCore(io_task, "audio_io", 6144, nullptr, 4, &io_handle, 0) != pdPASS) {
        ready = false;
        fail(Fault::Task);
        state.store(State::Fault);
    }
    Serial.printf("PSRAM audio=%u bytes; internal_free=%u PSRAM_free=%u\n",
                  static_cast<unsigned>(
#if AUDIO_TEST_CAPTURE
                      TRACE_FRAMES * 2 * sizeof(int16_t) + DSP_NOISE_SAMPLES * sizeof(float) +
#endif
                      sizeof(DspWorkspace) + DSP_OVERLAP * sizeof(float) +
                      (AUDIO_INPUT_SLOTS + AUDIO_OUTPUT_SLOTS) * DSP_BLOCK * sizeof(float)),
                  heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    Serial.printf("Autostart=%d test_capture=%d boot_volume=%u%%\n",
                  AUDIO_AUTOSTART, AUDIO_TEST_CAPTURE, AUDIO_BOOT_VOLUME_PERCENT);
#if AUDIO_TEST_CAPTURE
    Serial.println("Commands: start | raw | stop | status | noise_dump | filter_dump | volume 0..25 (percent)");
#else
    Serial.println("Commands: start | raw | stop | status | volume 0..25 (percent)");
#endif
    Serial.println("start: stay quiet for 1.25 s (1 s settling + 0.25 s noise); profile frozen until stop/start.");
}

#if AUDIO_TEST_CAPTURE
static void dump_noise_snapshot() {
    if (state.load() != State::Idle || !noise_snapshot_ready.load()) {
        Serial.println("Rejected: stop a valid filtered session before noise_dump.");
        return;
    }
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(noise_snapshot);
    const size_t size = DSP_NOISE_SAMPLES * sizeof(float);
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        hash = (hash ^ bytes[i]) * 16777619u;
    }
    Serial.printf("NOISE_DATA fs=%d samples=%d bytes=%u fnv=%u settle_samples=%d\n",
                  DSP_FS, DSP_NOISE_SAMPLES, static_cast<unsigned>(size), hash, AUDIO_SETTLE_SAMPLES);
    Serial.print("NOISE_PN");
    for (int band = 0; band < DSP_NBAND; ++band) {
        Serial.printf(" %.9e", noise_powers[band]);
    }
    Serial.println();
    size_t offset = 0;
    const uint32_t began = millis();
    while (offset < size && millis() - began < 5000) {
        int available = Serial.availableForWrite();
        if (available > 0) {
            size_t count = size - offset;
            if (count > 64) count = 64;
            if (count > static_cast<size_t>(available)) count = available;
            offset += Serial.write(bytes + offset, count);
        }
        delay(1);
    }
    Serial.println(offset == size ? "NOISE_END state=IDLE" : "NOISE_ERROR dump_timeout");
}
#endif

static void print_status() {
    Serial.printf("state=%s fault=%s mode=%s volume=%u%% blocks=%u dsp=%u max=%u ms "
                  "underrun=%u overrun=%u dma=%u clip=%u peak_ppm=%u\n",
                  state_names[static_cast<int>(state.load())], fault_names[static_cast<int>(fault.load())],
                  raw_mode.load() ? "raw" : "coif5_L5", volume_percent.load(), processed.load(),
                  dsp_ms.load(), dsp_max_ms.load(), underruns.load(), overruns.load(),
                  dma_errors.load(), clipped.load(), rx_peak_ppm.load());
    Serial.printf("stack_free_bytes io=%u dsp=%u; heartbeat_age_ms io=%u dsp=%u\n",
                  io_handle ? uxTaskGetStackHighWaterMark(io_handle) : 0,
                  dsp_handle ? uxTaskGetStackHighWaterMark(dsp_handle) : 0,
                  millis() - io_heartbeat.load(), millis() - dsp_heartbeat.load());
}

#if AUDIO_TEST_CAPTURE
static void dump_filter_trace() {
    const uint32_t count = trace_frames.load();
    if (state.load() != State::Idle || count != TRACE_FRAMES) {
        Serial.println("Rejected: stop a filtered session with eight complete trace hops first.");
        return;
    }
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(filter_trace);
    const uint32_t size = count * 2 * sizeof(int16_t);
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    Serial.printf("FILTER_DATA fs=%d frames=%u bytes=%u fnv=%u volume=%u gain=8\n", DSP_FS, count, size, hash, trace_volume);
    uint32_t offset = 0;
    const uint32_t began = millis();
    while (offset < size && millis() - began < 60000) {
        int available = Serial.availableForWrite();
        if (available > 0) {
            uint32_t n = size - offset;
            if (n > 64) n = 64;
            if (n > static_cast<uint32_t>(available)) n = available;
            offset += Serial.write(bytes + offset, n);
        }
        delay(1);
    }
    Serial.println(offset == size ? "FILTER_END state=IDLE" : "FILTER_ERROR timeout");
}
#endif

void audio_pipeline_command(const char *command) {
    if (!strcmp(command, "stop")) {
        stop_requested.store(true);
        mute(true);
    } else if (!strcmp(command, "status")) {
        print_status();
#if AUDIO_TEST_CAPTURE
    } else if (!strcmp(command, "noise_dump")) {
        dump_noise_snapshot();
    } else if (!strcmp(command, "filter_dump")) {
        dump_filter_trace();
#endif
    } else if (!strncmp(command, "volume ", 7)) {
        char *end;
        unsigned long percent = strtoul(command + 7, &end, 10);
        if (end == command + 7 || *end || percent > static_cast<unsigned>(AUDIO_MAX_VOLUME * 100.0f)) {
            Serial.println("Rejected: volume must be an integer 0..25.");
        } else {
            volume_percent.store(percent);
        }
    } else if (!strcmp(command, "start") || !strcmp(command, "raw")) {
        State current = state.load();
        if (!ready || (current != State::Idle && current != State::Fault)) {
            Serial.println("Rejected: busy or restart required.");
            return;
        }
        stop_requested.store(false);
        state.store(State::Starting);
        Command next = !strcmp(command, "raw") ? Command::Raw : Command::Start;
        if (xQueueSend(commands, &next, 0) != pdTRUE) {
            state.store(current);
            Serial.println("Rejected: command pending.");
        }
    } else {
        Serial.println("Unknown command. Use start/raw/stop/status/volume 0..25.");
    }
}

void audio_pipeline_poll() {
    static uint32_t last_report;
    float profile[DSP_NBAND];
    if (profiles && xQueueReceive(profiles, profile, 0) == pdTRUE) {
        Serial.print("Pn frozen:");
        for (int index = 0; index < DSP_NBAND; ++index) {
            Serial.printf(" %.6e", profile[index]);
        }
        Serial.println();
    }
    State current = state.load();
    if ((current == State::Running || current == State::Calibrating || current == State::Buffering) &&
        millis() - io_heartbeat.load() > 1000) {
        fail(Fault::Read);
    }
    if (fault.load() != Fault::None) {
        mute(true);
    }
    if (millis() - last_report >= 2000) {
        last_report = millis();
        print_status();
    }
}
