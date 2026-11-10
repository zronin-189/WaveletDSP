#include <Arduino.h>
#include <cstring>
#include "audio_i2s.h"
#include "audio_config.h"
#include "driver/i2s.h"

static bool installed;
static QueueHandle_t events;

esp_err_t audio_i2s_begin() {
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
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
    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.ws_io_num = AUDIO_PIN_AMP_LRC;
    pins.bck_io_num = AUDIO_PIN_AMP_BCLK;
    pins.data_out_num = AUDIO_PIN_AMP_DIN;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    if (result == ESP_OK) {
        result = i2s_set_pin(I2S_NUM_0, &pins);
    }
    if (result == ESP_OK) {
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
        pinMode(AUDIO_PIN_AMP_LRC, OUTPUT);
        pinMode(AUDIO_PIN_AMP_BCLK, OUTPUT);
        pinMode(AUDIO_PIN_AMP_DIN, OUTPUT);
        digitalWrite(AUDIO_PIN_AMP_LRC, LOW);
        digitalWrite(AUDIO_PIN_AMP_BCLK, LOW);
        digitalWrite(AUDIO_PIN_AMP_DIN, LOW);
    }
}

esp_err_t audio_i2s_read(int32_t *stereo) {
    memset(stereo, 0, AUDIO_CHUNK * 2 * sizeof(int32_t));
    return installed ? ESP_OK : ESP_ERR_INVALID_STATE;
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
        if (event.type == I2S_EVENT_DMA_ERROR || event.type == I2S_EVENT_TX_Q_OVF) {
            okay = false;
        }
    }
    return okay;
}
