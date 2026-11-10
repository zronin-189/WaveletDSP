# Kiểm thử WaveletDSP trên PC và ESP32-S3

Thư mục này chỉ chứa kiểm thử, không phải firmware thu mic/phát loa.

## Lượt lọc thực tế đã nghe và WAV đối chiếu

**Cậpnhậtgiảmtrễ:** lịchpháthiện20000mẫu/1.25s,thay2s,trongkhiDWT/Wiener
vàcấutrúckhốigiữnguyên. Lượt20svolume20 khôngunderrun/overrun/DMA/limiter,
boardIDLEvolume0;loglatency1250_hardware trongthưmụcdữliệu. Kiểmthử
`host/latency_equivalence.py` soPCMthật từcùngvector/Pnởconfig2s/1.25s:
16000mẫugiốnghệt. Lịchđệmkhácvớitrễvậtlý,1ssettlingởđầuphiênkhôngphảitrễ
liêntụccủamỗimẫu. 1.25slàmứcthấpnhấtđãthử,khôngbảođảmtốiưu tuyệtđối.

[verified: results/du_lieu_kiem_thu/loc_buoc3.log/.json và người dùng nghe]
Wienercoif5/L5/float32,16kHz,1sổnđịnh+0.25slấynền,frozenPn,gain8×/volume20%.
20s,21khối,DSPmax248ms,underrun/overrun/DMA/clip0,cuốiIDLEvolume0. Người dùng
xác nhận nền giảm,lời nói rõ;chưađịnhlượngSNR hoặctrễvậtlý.

`results/loc_buoc3_truoc.wav` và `results/loc_buoc3_sau.wav` cùng111616mẫu,
6.976s/16kmono16,cùngmic/gain/volume,khôngnormalize. CặpPCMđượcESP32 lưu ngay
trước/sauDSP (saunốioverlap),khôngphảighiâmngoàiloa;bỏtrễbuffer2s/fadephátđầu.
`host/filter_listen_test.py` chạy20svàtảicặpPCMsaustop,kiểmtrabytes/checksum.
Firmware`filter_dump` chỉIDLE,trace8hopcốđịnhPSRAM446464byte,khôngstreamUART
khiđangthu/phát. Log/JSON/PCMthô ởresults/du_lieu_kiem_thu/loc_buoc3.*.

## Cấu trúc

- `vectors/`: các CSV/WAV và metadata xuất từ MATLAB, giữ nguyên dữ liệu gốc.
- `include/golden_impulse64.h`: đáp án MATLAB cho phép thử xung.
- `host/host_test.c`: đối chiếu DSP C với vector chuẩn trên PC.
- `host/stream_test.c`: PCM, profile lỗi, bounds DSP, nối overlap đúng timestamp.
- `device/main.cpp`: chương trình bring-up/benchmark Arduino từng nằm trong firmware chính.
- `device/dsp_source.c`: biên dịch lại chính `firmware_esp32s3/src/wavelet_dsp.c`, không sao chép thuật toán.
- `platformio.ini`: kế thừa cấu hình board Arduino ở firmware chính; không tự duy trì bản cấu hình phần cứng khác.

## Kiểm thử trên PC

Chạy từ thư mục gốc WaveletDSP:

```bash
mkdir -p .agent-tmp/separate-esp32-tests
gcc -O2 -std=c11 -I firmware_esp32s3/include -I firmware_esp32s3/kiem_thu_esp32s3/include \
    firmware_esp32s3/src/wavelet_dsp.c firmware_esp32s3/kiem_thu_esp32s3/host/host_test.c \
    -lm -o .agent-tmp/separate-esp32-tests/host_test
.agent-tmp/separate-esp32-tests/host_test firmware_esp32s3/kiem_thu_esp32s3/vectors
```

Host test kiểm tra xung, ramp, Pn và đầu ra **khối đầu tiên** của speech_001 white 10 dB.
Không kiểm tra toàn bộ bản ghi, nối khối hoặc âm thanh từ mic.
Binary `host/host_test` cũ được giữ lại nhưng không dùng làm bằng chứng kiểm thử mới;
hãy biên dịch lại bằng lệnh trên.

## Test streaming độc lập phần cứng

Chạy từ workspace root, toàn bộ fixture/test vẫn nằm trong thư mục kiểm thử này:

```bash
mkdir -p .agent-tmp/live-audio
gcc -O2 -std=c11 -Wall -Wextra -Werror -I firmware_esp32s3/include \
    firmware_esp32s3/src/wavelet_dsp.c firmware_esp32s3/src/audio_stream.c \
    firmware_esp32s3/kiem_thu_esp32s3/host/stream_test.c -lm \
    -o .agent-tmp/live-audio/stream_test
.agent-tmp/live-audio/stream_test
gcc -O1 -g -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I firmware_esp32s3/include \
    firmware_esp32s3/src/wavelet_dsp.c firmware_esp32s3/src/audio_stream.c \
    firmware_esp32s3/kiem_thu_esp32s3/host/stream_test.c -lm \
    -o .agent-tmp/live-audio/stream_sanitize
ASAN_OPTIONS=detect_leaks=1 .agent-tmp/live-audio/stream_sanitize
```

[verified: results/du_lieu_kiem_thu/live_audio_stream.log và live_audio_sanitizer.log] Kiểm tra
167424mẫu qua12khối overlapping đúng cùng timestamp, maxerr1.490e-08. Chưa phải
chứng minh nối khối Wiener không click trên mic thật. Profile toàn0/NaN/clipping
bị reject; silence và input/length/Pn không hợp lệ có xử lý giới hạn. API DWT/IDWT
cấp thấp vẫn chỉ dùng với kích thước và bộ đệm hợp lệ như host_test; firmware
đi qua API denoise/calibration có guard, không nhận length ngoài từ Serial.

[verified: results/du_lieu_kiem_thu/live_audio_golden.log] Sau tối ưu sliding mean và Pn float32:
khối0 speech001 white10dB maxerr2.235e-08, Pn maxerr1.455e-11 so với CSV MATLAB.
Không tái sinh/thay vector. Sai số khớp golden không phải SNR sau lọc của mic.

## Kiểm thử phần cứng qua serial có giới hạn thời gian

Chỉ chạy sau khi xác nhận đúng board và đấu nguồn/mic/ampli/mạch mute. Script
kiểm tra VID/PID/serial và đường by-id, chờ boot, xác nhận IDLE/volume0 trước
khi chạy. Không flash/reset qua esptool; mở serial có thể làm board tự reset.
Kết thúc hoặc bắt lỗi luôn gửi stop/volume0 và ghi kết quả JSON cạnh log.

```bash
timeout 40s /home/alberttochiro/.platformio/penv/bin/python -u \
    firmware_esp32s3/kiem_thu_esp32s3/host/hardware_test.py \
    raw --seconds 20 --volume 0 --log results/du_lieu_kiem_thu/live_audio_hardware_raw_muted.log
timeout 50s /home/alberttochiro/.platformio/penv/bin/python -u \
    firmware_esp32s3/kiem_thu_esp32s3/host/hardware_test.py \
    start --seconds 30 --volume 0 --log results/du_lieu_kiem_thu/live_audio_hardware_denoise_muted.log
```

Script giới hạn volume0..15%, thời gian5..300s; volume0 là PCM0, không phải đo
SD mute bằng đồng hồ. Không thể chứng minh loa có phát bằng serial. Để thử loa,
đặt xa mic và dùng volume2 trước, ví dụ `raw --seconds 60 --volume 2`; thông báo
người nghe trước khi bắt đầu. `start` cần yên lặng đoạn lấy nền0.35s đầu.

[verified: live_audio_hardware_raw_muted.log/.json] Raw20s hoàn tất23khối,
không lỗi DMA/underrun/overrun; có peak khác0. [verified:
live_audio_hardware_denoise_muted.log/.json] Lọc30s hoàn tất35khối, Pn6band frozen,
DSP195ms thường và max210ms, dưới hop872ms; các counter lỗi0; cuối test IDLE/volume0.
[unverified] Không có dữ liệu mic dump để xác minh hình sóng/slot bằng MATLAB;
chưa đánh giá noise profile có chứa giọng nói, chất lượng nghe hoặc latency thật.

## Build benchmark Arduino

Bản diagnostic bíp riêng dùng env `esp32-s3-audio-diagnostic`. Build từ workspace:

```bash
pio run -d firmware_esp32s3/kiem_thu_esp32s3 -e esp32-s3-audio-diagnostic
```

Firmware diagnostic boot IDLE, GPIO10 INPUT (không điều khiển SD), không tự phát.
Serial115200: `status`, `tone 1`, `tone 3`, `tone 5`, `tone 15`, `tone 25`, `stop`. Mỗi lệnh tone phát
sin1000Hz trong2s (theo yêu cầu mới), fade10ms ở hai đầu, rồi2s PCM0. Các mức là biên độđỉnh
phần trăm fullscale trực tiếp, không nhân volume của firmware chính. Mức15 thêm
theo yêu cầu rõ của người dùng; mức25 cũng được thêm theo yêu cầu tiếp theo. Chỉ thử1
trước, chờ người nghe xác nhận rồi mới tăng; không có autoescalate.

Sau khi xác minh và nạp application diagnostic, hosttool chạy một lượt cóACK,
kiểm tra từchốitone30, framecount/error/cleanup và finally stop/IDLE:

```bash
timeout 20s /home/alberttochiro/.platformio/penv/bin/python -u \
    firmware_esp32s3/kiem_thu_esp32s3/host/diagnostic_tone.py \
    --percent 1 --log results/audio_no_output_tone1.log
```

Driver/PCM được reuse từ production qua hai file sourcewrapper. Diagnostic không
chạy DWT; benchmark env chỉ build main.cpp+dsp_source.c để tránh hai setup/loop.
SD đang hở theo người dùng: stop ghiPCM0 rồi dừng I2S, không phải hardwaremute.
Chưa triển khai capture mic ở bước này; phải hoàn tất kiểm tra bíp trước.

[verified: results/du_lieu_kiem_thu/audio_no_output_signal_test.log và audio_no_output_host_regression.log]
Hosttest kiểm tra1kHz/8000mẫu, biên/ramp/PCM, lệnh sai; ASAN/UBSAN PASS. GoldenDSP
và167424mẫu overlap PASS. Build diagnostic/main/benchmark đềuSUCCESS; các log
audio_no_output_*_build.log có kết quả. Firmware/source production có backup tại
đường ghi trong results/du_lieu_kiem_thu/audio_no_output_backup.log. Sau khi quyềnUSB hoạt động
trở lại, diagnostic đã nạp app0 hash verified, giữbootloader/partition/NVS/eFuse.
[verified: results/du_lieu_kiem_thu/audio_no_output_tone15.log/.json] Đã thử đúng một bíp15%,
1kHz/8000mẫu=0.5s, tổng32000frame kểcảPCM0; error0/cleanup_ok1, cuốiIDLE.
Người dùng báo không nghe bíp15%. [verified: results/du_lieu_kiem_thu/audio_no_output_tone25.log/.json]
Đã phát1bíp25% trên cùngbackendfull-duplex để chỉ thayđổi biênđộ;8000toneframes,
32000totalframes,error0,cleanup_ok1,cuốiIDLE. Người dùng cũng báo không nghe lượt này.

Env `esp32-s3-audio-tx-only` thêm backend riêng `device/audio_tx_only.cpp`, chỉTX
và i2s_set_pin trực tiếpGPIO7/8/9, khôngmicRX/fanout/registerworkaround. Biếnthể
đãbuild/nạp và thửmộtlượt25%2s sau khi cảhai lượtfull-duplex khôngnghe.
[verified: results/du_lieu_kiem_thu/audio_no_output_tx25_2s_tone.log/.json] BootxácnhậnbackendTX-only,
32000tonemframes/64000total,error0/cleanup_ok1/cuốiIDLE. Chờphảnhồi nghe;
không phải fixđãxácminh và không thaydriverproduction. Hostcount/sanitizer cho2s
PASS trong audio_no_output_signal25_2s_test.log; app0hashverified trong
audio_no_output_tx25_2s_flash.log. BoardhiệnchạyTX-onlydiagnostic.

Chạy từ thư mục gốc WaveletDSP:

```bash
pio run -d firmware_esp32s3/kiem_thu_esp32s3 -e esp32-s3-dsp-test
```

Lệnh chỉ build, không nạp. Bản benchmark đo một lần xử lý gồm ước lượng Pn và
khử nhiễu trên khối xung tổng hợp; đó không phải đo deadline đường thu/phát I2S.
Benchmark giữ nguyên cấu hình task/bộ nhớ trước khi tách, chưa bật I2S.
Ảnh hưởng của I2S, jitter, underrun và độ trễ đầu-cuối chưa được kiểm chứng ở đây.
Muốn nạp/đo trên board cần bước riêng với cổng và phần cứng được xác nhận.

## Test mic và loa độc lập theo code mẫu (2026-11-10)

Hai env mới không liên kết pipeline/DWT hoặc driver full-duplex:

- `esp32-s3-mic-standalone`: `device/mic_standalone.cpp`, RX-only stereo32,
  16 kHz, WS4/BCLK5/SD6. Boot IDLE; `start` thu tối đa 10 giây, `stop` dừng.
  Mỗi 100 ms in RMS/peak/DC/zero/clipping riêng slot0/slot1, theo đơn vị PCM24.
  Double chỉ dùng tích lũy thống kê test; không thay float32 của DSP chính.
  Chưa gán tên left/right cho thứ tự slot cho đến khi kiểm chứng trên board.
  Bổ sung `capture`: thu4giây stereo32 thô vàoPSRAM(64000frame,512000byte),
  dừngI2S rồi báo CAPTURE_READY. `dump` gửi dữ liệu sau khi dừngthu, kèmFNV-1a;
  không đẩy từngmẫu quaUART khi đangthu. Có timeoutthu và chuyển dữ liệu.
  Ghi RX_CONFIG để phân biệt cấu hình yêu cầu và trạng thái phần cứng thực tế.
- `esp32-s3-speaker-standalone`: `device/speaker_standalone.cpp`, TX-only
  stereo16, hiện dùng 44.1 kHz theo mẫu trong lượt so sánh được người dùng duyệt.
  LRC7/BCLK8/DIN9; không điều khiển SD. Boot IDLE; `tone` phát đúng một bíp
  1 kHz/25% đỉnh PCM/2 giây, fade 10 ms, sau đó 2 giây PCM0 rồi IDLE.
  Không phát giai điệu lặp như mẫu. `stop` dừng; PCM0 không phải shutdown SD.
  `include/speaker_test_signal.h` tạo sin theo 44.1 kHz; không dùng bảng sin
  16 mẫu của diagnostic 16 kHz. Tổng 88200 frame có tiếng + 88200 frame PCM0.
  Host runner kiểm tra ACK 44.1 kHz và DONE 176400 frame; các log 16 kHz cũ
  được giữ lại làm bằng chứng của lượt trước, không phản ánh image hiện tại.

**Cấu hình loa đã có tiếng — 2026-11-10:** MAX98357A Vin→5V, GND→GND chung,
LRC→GPIO7,BCLK→GPIO8,DIN→GPIO9,GAIN→GND,**SD→3V3**; loa4Ω3W nối haiOUT.
Không có NPN, GPIO10 không nối module. [verified: người dùng xác nhận nghe và
results/du_lieu_kiem_thu/speaker_44100_sd3v3_trial.log] Giữ nguyên image44.1kHz/stereo16,
phát một bíp1kHz25%2s;88200tone/176400totalframes,error0,cleanup0,cuốiIDLE.
Lượt trước cùng image nhưngSDđểhở không nghe. Chỉ thay kết nốiSD trước lượt
nghe được; không tăng âm lượng, không nạp lại. Khuyến nghị SDđểhở cũ không áp
dụng cho bộ phần cứng này. Chưa đo lại điện ápSD sau nối, chưa thử16kHz vớiSD3V3.
Ưu tiên cấu hình này khi đối chiếu các ghi chú lịch sử không nghe bên dưới.

Downloads giữ nguyên. Pin trong mẫu đã được đổi về dây đang đấu do người dùng
xác nhận; chưa có sơ đồ riêng của module để xác minh độc lập. Không nạp nguyên
mẫu loa: GPIO7 của mẫu là SD nhưng GPIO7 đang nối LRC trong hệ thống này.

Build từ gốc dự án, không nạp:

```bash
pio run -d firmware_esp32s3/kiem_thu_esp32s3 -e esp32-s3-mic-standalone -e esp32-s3-speaker-standalone
```

Sau khi đúng image đã được nạp, dùng Python của PlatformIO đã có pyserial:

```bash
/home/alberttochiro/.platformio/penv/bin/python firmware_esp32s3/kiem_thu_esp32s3/host/standalone_audio.py mic-status --log results/mic_status_trial.log
/home/alberttochiro/.platformio/penv/bin/python firmware_esp32s3/kiem_thu_esp32s3/host/standalone_audio.py mic --log results/mic_speech_trial.log
/home/alberttochiro/.platformio/penv/bin/python firmware_esp32s3/kiem_thu_esp32s3/host/mic_capture.py --prefix results/mic_capture_trial --wait-for-enter
/home/alberttochiro/.platformio/penv/bin/python firmware_esp32s3/kiem_thu_esp32s3/host/standalone_audio.py speaker --log results/speaker_trial.log
```

Chỉ chạy một lệnh mỗi lần; tool kiểm tra danh tính USB và loại firmware, không
nạp tự động, không ghi đè log cũ. Lượt mic cần phối hợp: im lặng rồi nói ở cùng
khoảng cách. `start` chỉ lưu thống kê; `capture`/host mic_capture.py lưu raw,
WAVPCM16 riêng từngslot,JSON thống kê và bảnslot0_listen.wav đã trừDC/tăngmức
để nghe kiểm tra. Bản listen không phản ánh nguyên mức đầu vào; raw và WAVPCM
gốc được giữ lại. Thử1giây đầu imlặng,3giây sau nói. Không chạy loa/DWT/AGC.
Lượt loa cần người dùng xác nhận nghe; protocol PASS không chứng minh có tiếng.
Không ghép lại trước khi từng phần đạt. Nạp app-only sau khi nhận dạng MCU,
không dùng upload mặc định để tránh ghi bootloader/partition ngoài phạm vi.

[verified: results/du_lieu_kiem_thu/isolated_audio_build.log] Cả hai env build SUCCESS.
[verified: results/du_lieu_kiem_thu/isolated_mic_speech_trial.log] Mic thu đủ 160000 frame,
error0/cleanup0/IDLE; người dùng xác nhận đã im lặng rồi nói. Slot0 có dữ liệu
nhưng DC lớn, slot1 toàn 0. Chưa xác nhận giải mã và chất lượng lời nói đúng.
[verified: results/du_lieu_kiem_thu/isolated_speaker_tone_trial.log và phản hồi người dùng]
Loa đã chạy một lượt stereo16 bíp25%/2s,64000frame kể cả PCM0,error0,cleanup0;
người dùng vẫn không nghe thấy. Không coi telemetry PASS là loa đạt.
Board sau lượt này chạy **speaker-standalone, IDLE**, không phải firmware chính.
Chưa ghép lại; cần kiểm tra tín hiệu điện tại ampli và đường giải mã mic.

[verified: results/du_lieu_kiem_thu/speaker_44100_build.log và speaker_44100_flash.log]
Lượt so sánh 44.1 kHz đã build/nạp application hash verified.
[verified: results/du_lieu_kiem_thu/speaker_44100_signal_test.log và speaker_44100_tone_trial.log]
Host signal test ASAN/UBSAN PASS; board đã chạy một bíp1kHz25%2s,
88200frame có tiếng/176400frame tổng,error0,cleanup0,cuốiIDLE. Chờ người dùng
xác nhận nghe; chưa coi đây là sửa lỗi loa. Board hiện dùng bản44.1kHz.

## Quan hệ với firmware chính

### Kết quả thu mic đã nghe — 2026-11-10

[verified: results/du_lieu_kiem_thu/mic_capture_baseline.log/.json và người dùng nghe WAV]
RX-only16kHz/stereo32,WS4/BCLK5/SD6;64000frame/512000byte thu vàoRAM,checksum
FNV3870743862khớp,error0,cleanup0,cuốiIDLE. RX_CONFIGmono1/first1/ws_idle0/
msb_shift1; chưa sửa thanh ghi RX. Slot0 giải mãsigned24 cảâm/dương,khôngclip;
slot1toàn0. Người dùng xác nhận lời nói nghe đúng nhưng córènền. Vì thế mic
thu/giải mã lời nói **đạt**,chất lượngnền chưađạt xácminh; chưa chạy bộlọc.
File gốc `results/du_lieu_kiem_thu/mic_capture_baseline.raw`,hai WAVslot0/slot1PCM và bản
`mic_capture_baseline_slot0_listen.wav`(trừDC/chuẩnhóamức) được giữ đểđốichiếu.
Không suy ra nguồn nhiễu hoặc yêu cầu sửa phầncứng chỉ từ bảnnghe này.
Sau lượt mic này board chạy **mic-standalone capture-enabled, IDLE**.
Trạng thái mới hơn sau khi ghép nằm ở ghi chú bên dưới.

### Ghép mic–loa raw đã nghe — 2026-11-10

**Cập nhật bước1:** gainraw hiện8×, thời gian ổn địnhmic1giây riêng chế độraw;
volume thử20%. [verified: results/du_lieu_kiem_thu/gain8_volume20_hardware.json]
20giây,22khối,limiter0,underrun/overrun/DMA0,cuốiIDLEvolume0. Người dùng xác nhận
đủ nghe vàliềnmạch. Các sốgain16×/clip13107 bên dưới là lượt trước bước1.
Wiener/calibration không thay đổi. Bufferdelay2s bắt đầu sau đoạn ổn định1s.

**Quy ước file kết quả:** WAV ngay `results/`;log/JSON/raw trong
`results/du_lieu_kiem_thu/`. Modulehost`result_paths.py` dùng chung bởi các runner
để không đưa log trở lại thưmụcWAV.108filecũ đã chuyển,hash giữnguyên; mapping
trong `results/du_lieu_kiem_thu/danh_sach_di_chuyen.json`.

`host/export_gain_wav.py` dựng bộ ba `gain_01_mic_goc.wav`,
`gain_02_truoc_16x_volume20.wav`, `gain_03_sau_8x_volume20.wav` từ cùngrawmiccũ
bằng Cencoder thật, khôngnormalize,khôngDCremoval. Đầubản16× có0.1sPCM0;
đầubản8× có1sPCM0 để môphỏngsettling,vẫngiữ cùngtimeline4s. Cảhai bỏbufferdelay2s.
WAV là môphỏng phần gain/PCM từ cùnginput,không phải bản thu mới hoặc âmloa.
Metadata `gain_wav_comparison.json` trong thưmụcdữliệu ghi đầyđủtham số.
Hoststreamtest/sanitizer PASS vàproductionbuildSUCCESS trong log`gain8_*`.

[verified: results/du_lieu_kiem_thu/mic_speaker_reference_tone.log/.json + người dùng nghe]
Full-duplex16kHz/slot32/fanout phát bíp25%2s thành công vớiSD3V3.
Lượt raw15% trước đó không nghe lời nói; dữ liệu mic thô có mức nhỏ.

[verified: results/du_lieu_kiem_thu/mic_speaker_raw_gain16_volume20.log/.json + người dùng nghe]
Firmware chính chạy raw20giây,volume20%,gain phát cố định16× và giới hạn trước
volume. Người dùng xác nhận **nghe được, đúng giọng và liền mạch**.23khối,
underrun/overrun/DMA0,faultnone;cuốiIDLEvolume0. Có13107mẫu bị giới hạn biên,
chưa tối ưu gain/chất lượng nền. Bộ lọc Wiener chưa bật. Delay2giây là cấu hình,
chưa đo vật lý. Hoststreamtest ASAN/UBSAN PASS bao gồmgain/sign/NaN/volume cap,
log mic_speaker_raw_gain_test.log; build/flash hashverified trong log cùngprefix.

Board hiện là **firmware chính, IDLE/volume0**, không phải bản test mic/loa riêng.
Các env riêng giữ nguyên để đối chiếu. SD vẫn3V3, GPIO10INPUT không điều khiển
ampli. Host hardware_test.py chấp nhận volume0..20; ví dụ một lượt có hạn:

```bash
/home/alberttochiro/.platformio/penv/bin/python firmware_esp32s3/kiem_thu_esp32s3/host/hardware_test.py raw --seconds 20 --volume 20 --log results/raw20_new_trial.log
```

Chạy khi chủ động thử, đặt loa xa mic; tool stop/volume0 khi kết thúc. Volume20
là mức số, không phải công suất loa20%. `clip` raw tính cả tác động limiter.

Firmware chính ở thư mục cha `firmware_esp32s3/` không include đáp án hoặc đọc CSV từ đây.
Hệ số `coif5_filters.h` vẫn ở firmware chính vì cần cho DWT/IDWT.
CSV/WAV không được đưa vào image firmware bởi cấu hình build này.
