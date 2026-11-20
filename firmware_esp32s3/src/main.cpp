// Firststep: INMP441 -> ESP32-S3 -> MAX98357A, không lọc hay thay đổi gain.
// Arduino-ESP32 2.0.17 / ESP-IDF 4.4.7; một file dùng cho PlatformIO/Arduino IDE.
#include <Arduino.h>
#include <driver/i2s.h>
#include <esp32-hal-matrix.h>
#include <esp_idf_version.h>
#include <soc/gpio_sig_map.h>
#include <soc/i2s_struct.h>

#if !defined(CONFIG_IDF_TARGET_ESP32S3) || ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(4, 4, 7)
#error Firststep requires ESP32-S3 and Arduino-ESP32 2.0.17 (IDF 4.4.7)
#endif

// Mic: nguồn 3V3, L/R nối GND. Ampli: Vin 5V, SD nối 3V3, GAIN nối GND.
static constexpr int MIC_WS = 4;
static constexpr int MIC_SCK = 5;
static constexpr int MIC_SD = 6;
static constexpr int AMP_LRC = 7;
static constexpr int AMP_BCLK = 8;
static constexpr int AMP_DIN = 9;
static constexpr int MIC_SLOT = 0;
static constexpr int SAMPLE_RATE = 16000;
static constexpr int CHUNK_FRAMES = 128;
static constexpr int DMA_BUFFERS = 8;
static constexpr int IO_TIMEOUT_MS = 100;
static constexpr size_t CHUNK_BYTES = CHUNK_FRAMES * 2 * sizeof(int32_t);
static_assert(CHUNK_BYTES <= 4092, "DMA descriptor too large");

static int32_t samples[CHUNK_FRAMES * 2];
static QueueHandle_t events = nullptr;
static bool installed = false;
static bool running = false;

// Giữ nguyên từng word 32-bit từ slot mic; phát cùng tín hiệu ở cả hai kênh.
static void copy_mic_to_stereo(int32_t *words, size_t frames)
{
    for (size_t frame = 0; frame < frames; ++frame)
    {
        const int32_t sample = words[2 * frame + MIC_SLOT];
        words[2 * frame] = sample;
        words[2 * frame + 1] = sample;
    }
}

static void stop_audio(esp_err_t reason)
{
    running = false;
    esp_err_t cleanup = ESP_OK;
    if (installed)
    {
        // Đóng cả RX/TX và bỏ clock ngoài; SD ampli vẫn nối cố định 3V3.
        cleanup = i2s_driver_uninstall(I2S_NUM_0);
        installed = false;
        events = nullptr;
    }
    pinMatrixOutDetach(AMP_LRC, false, false);
    pinMatrixOutDetach(AMP_BCLK, false, false);
    pinMatrixOutDetach(AMP_DIN, false, false);
    pinMode(AMP_LRC, OUTPUT);
    pinMode(AMP_BCLK, OUTPUT);
    pinMode(AMP_DIN, OUTPUT);
    digitalWrite(AMP_LRC, LOW);
    digitalWrite(AMP_BCLK, LOW);
    digitalWrite(AMP_DIN, LOW);
    Serial.printf("STOP error=%d cleanup=%d; reset board to retry\n", reason, cleanup);
}

static esp_err_t begin_audio()
{
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX);
    config.sample_rate = SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = DMA_BUFFERS;
    config.dma_buf_len = CHUNK_FRAMES;
    config.tx_desc_auto_clear = true;
    config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    config.bits_per_chan = I2S_BITS_PER_CHAN_32BIT;
    esp_err_t result = i2s_driver_install(I2S_NUM_0, &config, 64, &events);
    if (result != ESP_OK)
    {
        return result;
    }
    installed = true;
    result = i2s_stop(I2S_NUM_0);
    if (result != ESP_OK)
    {
        return result;
    }
    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.ws_io_num = MIC_WS;
    pins.bck_io_num = MIC_SCK;
    pins.data_in_num = MIC_SD;
    pins.data_out_num = AMP_DIN;
    result = i2s_set_pin(I2S_NUM_0, &pins);
    if (result != ESP_OK)
    {
        return result;
    }
    pinMode(AMP_LRC, OUTPUT);
    pinMode(AMP_BCLK, OUTPUT);
    pinMatrixOutAttach(AMP_LRC, I2S0O_WS_OUT_IDX, false, false);
    pinMatrixOutAttach(AMP_BCLK, I2S0O_BCK_OUT_IDX, false, false);
    // Giữ workaround stereo RX đã dùng trên S3 / IDF 4.4.7.
    I2S0.rx_conf.rx_mono = 0;
    I2S0.rx_conf.rx_mono_fst_vld = 0;
    I2S0.tx_conf.tx_ws_idle_pol = 0;
    I2S0.rx_conf.rx_ws_idle_pol = 0;
    result = i2s_zero_dma_buffer(I2S_NUM_0);
    if (result != ESP_OK)
    {
        return result;
    }
    xQueueReset(events);
    return i2s_start(I2S_NUM_0);
}

void setup()
{
    Serial.begin(115200);
    esp_err_t result = begin_audio();
    if (result != ESP_OK)
    {
        stop_audio(result);
        return;
    }
    running = true;
    Serial.println("RUNNING firststep fs=16000 chunk=128 gain=1; no filter/calibration/WAV");
}

void loop()
{
    if (!running)
    {
        delay(20);
        return;
    }
    size_t received = 0;
    esp_err_t result = i2s_read(I2S_NUM_0, samples, CHUNK_BYTES, &received,
                               pdMS_TO_TICKS(IO_TIMEOUT_MS));
    if (result != ESP_OK || received != CHUNK_BYTES)
    {
        stop_audio(result == ESP_OK ? ESP_ERR_TIMEOUT : result);
        return;
    }
    copy_mic_to_stereo(samples, CHUNK_FRAMES);
    size_t sent = 0;
    result = i2s_write(I2S_NUM_0, samples, received, &sent, pdMS_TO_TICKS(IO_TIMEOUT_MS));
    if (result != ESP_OK || sent != received)
    {
        stop_audio(result == ESP_OK ? ESP_ERR_TIMEOUT : result);
        return;
    }
    i2s_event_t event;
    for (int count = 0; count < 64 && xQueueReceive(events, &event, 0) == pdTRUE; ++count)
    {
        if (event.type == I2S_EVENT_DMA_ERROR || event.type == I2S_EVENT_RX_Q_OVF ||
            event.type == I2S_EVENT_TX_Q_OVF)
        {
            stop_audio(ESP_FAIL);
            return;
        }
    }
    // Trả CPU cho RTOS/watchdog; không chờ tích lũy khung DSP.
    delay(1);
}
