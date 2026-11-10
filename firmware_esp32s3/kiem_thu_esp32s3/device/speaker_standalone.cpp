#include <Arduino.h>
#include <driver/i2s.h>
#include "speaker_test_signal.h"

// TX-only stereo16 adaptation of the user's MAX98357A sample.
static constexpr i2s_port_t PORT = I2S_NUM_0;
static bool installed = false;
static uint32_t frames = 0, started = 0;
static QueueHandle_t events = nullptr;
static int16_t pcm[128 * 2];

static void finish(const char *reason, esp_err_t error = ESP_OK)
{
    esp_err_t cleanup = ESP_OK;
    if (installed)
    {
        // PCM zero is not physical SD shutdown on the user's module.
        esp_err_t zero = i2s_zero_dma_buffer(PORT);
        cleanup = i2s_driver_uninstall(PORT);
        if (cleanup == ESP_OK) cleanup = zero;
        installed = false;
        events = nullptr;
    }
    for (int pin : {7, 8, 9})
    {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
    Serial.printf("DONE speaker reason=%s frames=%lu error=%d cleanup=%d state=IDLE\n",
                  reason, (unsigned long)frames, error, cleanup);
}

static void beginTest()
{
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate = SPEAKER_FS;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 8;
    cfg.dma_buf_len = 128;
    cfg.tx_desc_auto_clear = true;
    cfg.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT;
    cfg.bits_per_chan = I2S_BITS_PER_CHAN_16BIT;
    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = 8;
    pins.ws_io_num = 7;
    pins.data_out_num = 9;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    frames = 0;
    esp_err_t err = i2s_driver_install(PORT, &cfg, 32, &events);
    installed = err == ESP_OK;
    if (err == ESP_OK) err = i2s_set_pin(PORT, &pins);
    if (err == ESP_OK) err = i2s_zero_dma_buffer(PORT);
    if (err != ESP_OK)
    {
        finish("init", err);
        return;
    }
    started = millis();
    Serial.printf("ACK speaker fs=%d bits=16 hz=1000 percent=25 tone_frames=%d total_frames=%d WS=7 BCLK=8 DIN=9\n",
                  SPEAKER_FS, SPEAKER_TONE_FRAMES, SPEAKER_TOTAL_FRAMES);
}

void setup()
{
    Serial.begin(115200);
    Serial.setTimeout(20);
    delay(500);
    Serial.println("SPEAKER_STANDALONE state=IDLE commands=tone|stop|status no_autoplay");
}

void loop()
{
    if (Serial.available())
    {
        char cmd[32] = {};
        size_t n = Serial.readBytesUntil('\n', cmd, sizeof(cmd) - 1);
        if (n && cmd[n - 1] == '\r') cmd[n - 1] = 0;
        if (!strcmp(cmd, "tone") && !installed) beginTest();
        else if (!strcmp(cmd, "stop")) finish("stop");
        else if (!strcmp(cmd, "status"))
            Serial.printf("SPEAKER_STANDALONE state=%s frames=%lu\n", installed ? "RUNNING" : "IDLE", (unsigned long)frames);
        else Serial.println("REJECT use: tone|stop|status (tone only when IDLE)");
    }
    if (!installed)
    {
        delay(2);
        return;
    }
    if (millis() - started > 6000)
    {
        finish("deadline", ESP_ERR_TIMEOUT);
        return;
    }
    i2s_event_t event;
    for (int i = 0; i < 32 && xQueueReceive(events, &event, 0); ++i)
    {
        if (event.type == I2S_EVENT_DMA_ERROR || event.type == I2S_EVENT_TX_Q_OVF)
        {
            finish("dma_or_overflow", ESP_FAIL);
            return;
        }
    }
    const uint32_t count = min((uint32_t)128, (uint32_t)SPEAKER_TOTAL_FRAMES - frames);
    for (uint32_t i = 0; i < count; ++i)
    {
        const int16_t value = speaker_test_sample(frames + i);
        pcm[2 * i] = pcm[2 * i + 1] = value;
    }
    const size_t requested = count * 2 * sizeof(int16_t);
    size_t bytes = 0;
    esp_err_t err = i2s_write(PORT, pcm, requested, &bytes, pdMS_TO_TICKS(100));
    if (err != ESP_OK || bytes != requested)
    {
        finish("write_or_short", err == ESP_OK ? ESP_FAIL : err);
        return;
    }
    frames += count;
    if (frames == SPEAKER_TOTAL_FRAMES) finish("complete");
    delay(1);
}
