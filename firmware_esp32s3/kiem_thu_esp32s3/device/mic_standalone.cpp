#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>
#include "soc/i2s_struct.h"

// RX-only adaptation of the user's INMP441 sample; keep existing wiring.
static constexpr i2s_port_t PORT = I2S_NUM_0;
static constexpr uint32_t FS = 16000;
static bool installed = false;
static uint32_t frames = 0, started = 0, window = 0;
static QueueHandle_t events = nullptr;
static int32_t samples[256 * 2];
static constexpr uint32_t CAPTURE_FRAMES = 4 * FS;
static int32_t *recording = nullptr;
static bool capturing = false, captureReady = false, dumping = false;
static uint32_t dumpOffset = 0, dumpStarted = 0, checksum = 2166136261u;

struct Meter
{
    double sum = 0, squares = 0;
    uint32_t peak = 0, zeros = 0, clips = 0;
};
static Meter meter[2];

static void finish(const char *reason, esp_err_t error = ESP_OK)
{
    esp_err_t cleanup = ESP_OK;
    if (installed)
    {
        cleanup = i2s_driver_uninstall(PORT);
        installed = false;
        events = nullptr;
    }
    captureReady = capturing && !strcmp(reason, "complete") && error == ESP_OK && cleanup == ESP_OK;
    capturing = false;
    dumping = false;
    Serial.printf("DONE mic reason=%s frames=%lu error=%d cleanup=%d state=IDLE\n",
                  reason, (unsigned long)frames, error, cleanup);
    if (captureReady)
    {
        checksum = 2166136261u;
        const uint8_t *data = (const uint8_t *)recording;
        for (uint32_t i = 0; i < CAPTURE_FRAMES * 8; ++i)
            checksum = (checksum ^ data[i]) * 16777619u;
        Serial.printf("CAPTURE_READY frames=%lu bytes=%lu fnv=%lu\n",
                      (unsigned long)CAPTURE_FRAMES, (unsigned long)(CAPTURE_FRAMES * 8), (unsigned long)checksum);
    }
}

static void beginTest()
{
    captureReady = false;
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    cfg.sample_rate = FS;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 8;
    cfg.dma_buf_len = 256;
    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = 5;
    pins.ws_io_num = 4;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num = 6;
    frames = window = 0;
    meter[0] = Meter{};
    meter[1] = Meter{};
    esp_err_t err = i2s_driver_install(PORT, &cfg, 32, &events);
    installed = err == ESP_OK;
    if (err == ESP_OK) err = i2s_set_pin(PORT, &pins);
    if (err != ESP_OK)
    {
        finish("init", err);
        return;
    }
    started = millis();
    Serial.printf("RX_CONFIG mono=%u first=%u ws_idle=%u msb_shift=%u\n",
                  I2S0.rx_conf.rx_mono, I2S0.rx_conf.rx_mono_fst_vld,
                  I2S0.rx_conf.rx_ws_idle_pol, I2S0.rx_conf1.rx_msb_shift);
    Serial.println("ACK mic fs=16000 bits=32 slots=2 max_frames=160000 WS=4 BCLK=5 SD=6");
}

void setup()
{
    Serial.begin(115200);
    Serial.setTimeout(20);
    delay(500);
    Serial.println("MIC_STANDALONE state=IDLE commands=start|capture|dump|stop|status max_seconds=10");
}

void loop()
{
    if (Serial.available())
    {
        char cmd[32] = {};
        size_t n = Serial.readBytesUntil('\n', cmd, sizeof(cmd) - 1);
        if (n && cmd[n - 1] == '\r') cmd[n - 1] = 0;
        if (!strcmp(cmd, "start") && !installed && !dumping) beginTest();
        else if (!strcmp(cmd, "capture") && !installed && !dumping)
        {
            if (!recording) recording = (int32_t *)ps_malloc(CAPTURE_FRAMES * 8);
            if (!recording)
            {
                Serial.println("ERROR capture allocation failed");
                return;
            }
            captureReady = false;
            capturing = true;
            beginTest();
        }
        else if (!strcmp(cmd, "dump") && captureReady && !installed && !dumping)
        {
            Serial.printf("DATA bytes=%lu fnv=%lu\n", (unsigned long)(CAPTURE_FRAMES * 8), (unsigned long)checksum);
            dumpOffset = 0;
            dumpStarted = millis();
            dumping = true;
        }
        else if (!strcmp(cmd, "stop")) finish("stop");
        else if (!strcmp(cmd, "status"))
            Serial.printf("MIC_STANDALONE state=%s frames=%lu\n", installed ? "RUNNING" : "IDLE", (unsigned long)frames);
        else Serial.println("REJECT use: start|capture|dump|stop|status");
    }
    if (dumping)
    {
        if (millis() - dumpStarted > 60000)
        {
            dumping = false;
            Serial.println("ERROR dump deadline");
            return;
        }
        int available = Serial.availableForWrite();
        if (available > 0)
        {
            const uint32_t count = min((uint32_t)64, min((uint32_t)available, CAPTURE_FRAMES * 8 - dumpOffset));
            dumpOffset += Serial.write((const uint8_t *)recording + dumpOffset, count);
            if (dumpOffset == CAPTURE_FRAMES * 8)
            {
                dumping = false;
                Serial.println("END state=IDLE");
            }
        }
        delay(1);
        return;
    }
    if (!installed)
    {
        delay(2);
        return;
    }
    if (millis() - started > 12000)
    {
        finish("deadline", ESP_ERR_TIMEOUT);
        return;
    }
    i2s_event_t event;
    for (int i = 0; i < 32 && xQueueReceive(events, &event, 0); ++i)
    {
        if (event.type == I2S_EVENT_DMA_ERROR || event.type == I2S_EVENT_RX_Q_OVF)
        {
            finish("dma_or_overflow", ESP_FAIL);
            return;
        }
    }
    size_t bytes = 0;
    const uint32_t limit = capturing ? CAPTURE_FRAMES : FS * 10;
    const uint32_t count = min((uint32_t)256, limit - frames);
    esp_err_t err = i2s_read(PORT, samples, count * 2 * sizeof(int32_t), &bytes, pdMS_TO_TICKS(100));
    if (err != ESP_OK || bytes == 0 || bytes % (2 * sizeof(int32_t)))
    {
        finish("read", err == ESP_OK ? ESP_FAIL : err);
        return;
    }
    if (capturing)
    {
        memcpy(recording + frames * 2, samples, bytes);
        frames += bytes / 8;
        if (frames == limit) finish("complete");
        delay(1);
        return;
    }
    for (size_t i = 0; i < bytes / sizeof(int32_t); i += 2)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            const int32_t value = samples[i + ch] >> 8;
            const uint32_t magnitude = value < 0 ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
            Meter &m = meter[ch];
            m.sum += value;
            m.squares += (double)value * value;
            if (magnitude > m.peak) m.peak = magnitude;
            m.zeros += value == 0;
            m.clips += magnitude >= 8388607;
        }
        ++frames;
        if (++window == 1600)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                const Meter &m = meter[ch];
                Serial.printf("MIC frame=%lu slot=%d n=%lu rms=%.1f peak=%lu dc=%.1f zeros=%lu clips=%lu\n",
                              (unsigned long)frames, ch, (unsigned long)window, sqrt(m.squares / window),
                              (unsigned long)m.peak, m.sum / window, (unsigned long)m.zeros, (unsigned long)m.clips);
                meter[ch] = Meter{};
            }
            window = 0;
        }
    }
    if (frames == limit) finish("complete");
    delay(1);
}
