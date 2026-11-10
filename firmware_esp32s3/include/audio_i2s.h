#ifndef AUDIO_I2S_H
#define AUDIO_I2S_H

#include <stdint.h>
#include "esp_err.h"

esp_err_t audio_i2s_begin();
void audio_i2s_end();
esp_err_t audio_i2s_read(int32_t *stereo);
esp_err_t audio_i2s_write(const int32_t *stereo);
bool audio_i2s_events_ok();

#endif
