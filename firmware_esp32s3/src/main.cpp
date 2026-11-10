#include <Arduino.h>
#include "WiFi.h"
#include "esp_bt.h"

#include "audio_config.h"
#include "audio_pipeline.h"

void setup() {
    Serial.begin(115200);
    delay(400);
    setCpuFrequencyMhz(240);
    WiFi.persistent(false);
    WiFi.mode(WIFI_OFF);
    btStop();

    Serial.println("==== WaveletDSP wired audio ====");
    Serial.printf("DSP config: coif5 L%d, Fs=%d Hz, block=%d, float32\n",
                  DSP_LEVEL, DSP_FS, DSP_BLOCK);
    Serial.printf("GPIO mic WS=%d SCK=%d SD=%d; amp LRC=%d BCLK=%d DIN=%d mute=%d\n",
                  AUDIO_PIN_MIC_WS, AUDIO_PIN_MIC_SCK, AUDIO_PIN_MIC_SD,
                  AUDIO_PIN_AMP_LRC, AUDIO_PIN_AMP_BCLK, AUDIO_PIN_AMP_DIN, AUDIO_PIN_AMP_MUTE);
    Serial.printf("Amp external_NPN_mute=%d; %s\n", AUDIO_EXTERNAL_NPN_MUTE,
                  AUDIO_EXTERNAL_NPN_MUTE ? "GPIO10 active HIGH shutdown" :
                  "SD wired to 3V3; GPIO10 unused; stop uses PCM silence/I2S shutdown");
    audio_pipeline_init();
    Serial.printf("Raw/filtered playback gain=%.0fx, limiter before volume; Wiener algorithm unchanged.\n",
                  AUDIO_RAW_PLAYBACK_GAIN);
#if AUDIO_AUTOSTART
    Serial.println("Autostart: stay quiet during mic settling and noise calibration.");
    audio_pipeline_command("start");
#endif
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
                audio_pipeline_command(command);
            }
            length = 0;
            overflow = false;
        } else if (length + 1 < sizeof(command) && !overflow) {
            command[length++] = character;
        } else {
            overflow = true;
        }
    }
    audio_pipeline_poll();
    delay(10);
}
