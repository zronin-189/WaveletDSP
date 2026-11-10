#include "audio_i2s.h"
#include "audio_config.h"

#include <Arduino.h>
#include "driver/i2s.h"
#include "esp32-hal-matrix.h"
#include "esp_idf_version.h"
#include "soc/gpio_sig_map.h"
#include "soc/i2s_struct.h"

#if !defined(CONFIG_IDF_TARGET_ESP32S3) || ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(4, 4, 7)
#error Review the stereo RX workaround before changing target or ESP-IDF version
#endif

static QueueHandle_t events;
static bool installed;

esp_err_t audio_i2s_begin() {
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX);
    config.sample_rate = DSP_FS;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = AUDIO_DMA_COUNT;
    config.dma_buf_len = AUDIO_CHUNK;
    config.tx_desc_auto_clear = true;
    config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    config.bits_per_chan = I2S_BITS_PER_CHAN_32BIT;
    esp_err_t result = i2s_driver_install(I2S_NUM_0, &config, 64, &events);
    if (result != ESP_OK) {
        return result;
    }
    installed = true;
    result = i2s_stop(I2S_NUM_0);
    if (result != ESP_OK) {
        audio_i2s_end();
        return result;
    }
    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.ws_io_num = AUDIO_PIN_MIC_WS;
    pins.bck_io_num = AUDIO_PIN_MIC_SCK;
    pins.data_in_num = AUDIO_PIN_MIC_SD;
    pins.data_out_num = AUDIO_PIN_AMP_DIN;
    result = i2s_set_pin(I2S_NUM_0, &pins);
    if (result == ESP_OK) {
        pinMode(AUDIO_PIN_AMP_LRC, OUTPUT);
        pinMode(AUDIO_PIN_AMP_BCLK, OUTPUT);
        pinMatrixOutAttach(AUDIO_PIN_AMP_LRC, I2S0O_WS_OUT_IDX, false, false);
        pinMatrixOutAttach(AUDIO_PIN_AMP_BCLK, I2S0O_BCK_OUT_IDX, false, false);
        I2S0.rx_conf.rx_mono = 0;
        I2S0.rx_conf.rx_mono_fst_vld = 0;
        I2S0.tx_conf.tx_ws_idle_pol = 0;
        I2S0.rx_conf.rx_ws_idle_pol = 0;
        xQueueReset(events);
        result = i2s_start(I2S_NUM_0);
    }
    if (result != ESP_OK) {
        audio_i2s_end();
    }
    return result;
}

void audio_i2s_end() {
    if (installed) {
        i2s_driver_uninstall(I2S_NUM_0);
        installed = false;
        events = nullptr;
        pinMatrixOutDetach(AUDIO_PIN_AMP_LRC, false, false);
        pinMatrixOutDetach(AUDIO_PIN_AMP_BCLK, false, false);
        digitalWrite(AUDIO_PIN_AMP_LRC, LOW);
        digitalWrite(AUDIO_PIN_AMP_BCLK, LOW);
    }
}

esp_err_t audio_i2s_read(int32_t *stereo) {
    size_t received = 0;
    const size_t wanted = AUDIO_CHUNK * 2 * sizeof(int32_t);
    esp_err_t result = i2s_read(I2S_NUM_0, stereo, wanted, &received,
                              pdMS_TO_TICKS(AUDIO_IO_TIMEOUT_MS));
    return result == ESP_OK && received != wanted ? ESP_ERR_TIMEOUT : result;
}

esp_err_t audio_i2s_write(const int32_t *stereo) {
    size_t sent = 0;
    const size_t wanted = AUDIO_CHUNK * 2 * sizeof(int32_t);
    esp_err_t result = i2s_write(I2S_NUM_0, stereo, wanted, &sent,
                               pdMS_TO_TICKS(AUDIO_IO_TIMEOUT_MS));
    return result == ESP_OK && sent != wanted ? ESP_ERR_TIMEOUT : result;
}

bool audio_i2s_events_ok() {
    i2s_event_t event;
    bool okay = true;
    for (int index = 0; index < 64 && xQueueReceive(events, &event, 0) == pdTRUE; ++index) {
        if (event.type == I2S_EVENT_DMA_ERROR || event.type == I2S_EVENT_RX_Q_OVF ||
            event.type == I2S_EVENT_TX_Q_OVF) {
            okay = false;
        }
    }
    return okay;
}
