# firmware_esp32s3 — Arduino / PlatformIO

Target: ESP32-S3 N16R8, Arduino framework, Wi-Fi/BT off.

## Chế độ nhánh main

`AUDIO_TEST_CAPTURE=0`, `AUDIO_AUTOSTART=1`, volume khởi động 20%, gain 8×.
Sau cấp nguồn, firmware tự yêu cầu chạy lọc sau khi khởi tạo task/buffer;
không chờ kết nối Serial hay lệnh máy tính. Giữ im lặng trong 1 giây ổn
định mic và 0,25 giây lấy nền tính từ lúc I2S bắt đầu, sau đó có thể nói.
Pn cố định cho đến khi stop/start hoặc cấp nguồn lại. Nếu nền thay đổi,
cần lấy lại mẫu nền khi không nói.

Mic → coif5/L5 Wiener → loa chạy liên tục. Bộ đệm danh định 1,25 giây
được giữ; khoảng ổn định mic ban đầu tính riêng. Đây không phải số đo
trễ âm học và chưa giảm thêm độ trễ trong lần tách nhánh này.

Không cấp phát/ghi cặp PCM hoặc snapshot nhiễu kiểm thử, không có lệnh
`filter_dump`/`noise_dump`, không tạo WAV. Vẫn có Serial `status`, `stop`,
`start`, `raw`, `volume 0..25` để chẩn đoán khi cần; hoạt động bình thường
không cần PC. Khi phát hiện lỗi, giữ cơ chế dừng I2S hiện có; không tự
khởi động lại/lấy nền trong lúc người dùng đang nói.

SD MAX98357A nối 3V3, GPIO10 không dùng. Mic WS4/SCK5/SD6;
ampli LRC7/BCLK8/DIN9, Vin 5V, GAIN GND, GND chung; loa 4Ω/3W nối OUT+/OUT−.
Không đổi phần cứng so với bản đã nghe đạt. Nạp firmware là thao tác riêng;
chuyển Git branch không thay chương trình đang nằm trên board.

Build: `pio run -d firmware_esp32s3`. Bản tách nhánh mới đã build trên PC;
chưa nạp/kiểm thử tự chạy trên board. Các kết quả lịch sử bên dưới thuộc
những lượt trước khi tách nhánh. Để thu cặp WAV, dùng nhánh `debug`; bản đó
chờ lệnh và ghi tám hop đầu, sau stop mới tải cặp PCM về PC.

**Dọn file 2026-11-10:** các file tạm, cache bài test và log chẩn đoán cũ
đã được cách ly tại `.trash/don_dep_20261110/` tính từ gốc dự án.
Đường dẫn lịch sử bên dưới nếu không còn tại chỗ có thể tra trong
`.agent/cleanup-manifest.json`. Mã nguồn, công cụ kiểm thử, build chính,
WAV hữu ích và kết quả xác nhận hiện tại vẫn được giữ.

**Giảm trễ, giữ thuật toán:** [verified: audio_config.h và loglatency1250_hardware]
Lịch phát hiện là20000mẫu/1.25s thay32000mẫu/2s,giảm0.75s. Giữblock16000,
overlap2048,hop13952,coif5L5/float32,giaithuậtWiener/gain8/volume20 vàSD3V3.
Hostso cùnginput/Pn choPCMgiốnghệt16000mẫu(SHA256 trong
results/du_lieu_kiem_thu/latency_pcm_equivalence.json). Lượtboard20s khônglỗi
IO/limiter,22khối,DSPmax248ms,cuốiIDLEvolume0. Đâylàlịchđệm,khôngphảiphépđo
trễâmhọc:1ssettling chỉởđầuphiên,dođólầnphátđầukhoảng2.25s+sai sốI2S.
Người dùng xác nhận chất lượng giữ nguyên,nghe liền mạch ở mức đệm mới.
1.25s làmứcthấpnhấtđãthửđạt hiện tại,khôngbảođảm tốiưu tuyệtđối. Với khối1s,
phảiđợikhốiđầy rồiDSP;DSPmaxđãquan sát248ms khônglàgiớihạnthờigianbảođảm.
Cácghichú2s/32000mẫu bên dưới làcấuhìnhlịchsử. KhôngđiềukhiểnSD.

**Kiểm tra ổn định5phút:** [verified: results/du_lieu_kiem_thu/loc_5phut_tong_hop.json]
ChạyWiener300s,gain8/volume20:344khối,khôngreset,lỗiDMA/underrun/overrun0,
Pnchỉtính1lần. DSPmax248ms<872ms/khối;stackcònít nhấtIO5036/DSP5236byte.
Có1659mẫutácđộnglimiter,khôngcoilàkhôngclipping. KếtthúcIDLEvolume0.
Chưađoheaptheothờigian,trễvậtlý hoặcđộbềnvượtlượt5phút. KhôngthuWAV5phút;
WAVso sánh trước/sauởbước3 vẫnđượcgiữnguyên. [verified: người dùng] Nghe suốt
lượt5phút ổn,rõ,liềnmạch;khôngbáobấtthường vềhú,bụp hoặc nóngampli.

**Bước3 lọc đã nghe:** [verified: results/du_lieu_kiem_thu/loc_buoc3.log/.json
và người dùng] Wienercoif5L5/16kHz/float32,1ssettle+0.25snoise,frozenPn,
gainphát8×/volume20% giốngraw. Người dùng xác nhận nền giảm,lời nói vẫn rõ.
20s,21khối,DSPmax248ms,underrun/overrun/DMA/limiter0;boardIDLEvolume0.
Gain8× nayáp cảrawvàlọc;ghi chúchưa bậtWiener bên dưới làlịch sửtrước bước3.
WAVPCM thật từboard: `results/loc_buoc3_truoc.wav` và `results/loc_buoc3_sau.wav`,
cùng111616mẫu/6.976s/16kmono16,cùnggain8/volume20,khôngnormalize. Bảnlọc sau
nốioverlap; bỏbufferdelay2s/fadephátđầu ởcảhai. Khôngphải ghiâmngoàiloa.
`filter_dump` chỉsaustopIDLE;metadata/log trongresults/du_lieu_kiem_thu/loc_buoc3.*.
ChưađoSNR/trễvậtlý hoặcchạydàihạn.

**Bước2 lấy nền:** [verified: results/du_lieu_kiem_thu/nhieu_buoc2.log/.json]
Đã bỏ1sổnđịnhmic,lấy4000mẫu/0.25s thực,tính6Pn cốđịnh,đốichiếuC trênPCkhớp;
người dùng xác nhận không nói/chạmmic. Phiên8s chạyWiener ởvolume0,khônglỗiIO,
cuốiIDLEvolume0. Chưa thử nghe đầu raWiener. `noise_dump` saustop xuấtđúng
mẫunền; rawgain8 vẫn chỉápraw. Tài liệu cũ nói0.1s/0.35s calibration bên dưới
đã được thay bằng1ssettling+0.25snoise=1.25s;giữimlặng trongkhoảngđó.
Mẫu cóDClớn,chưa xác địnhnguồn hoặc tínhổnđịnh. Filepreview lặp2s tạođiểmnối
mỗi0.25s,cóthểgâybụp4Hz;khôngcoi đó làlỗimic. Bảnnghekhônglặp:
`results/nhieu_buoc2_nghe_khong_lap.wav`(trừDC/fade đểnghe);mẫugốc/Pnkhôngđổi.

**Cập nhật 2026-11-10: loa đã phát được với bản test độc lập sau khi nối SD của
MAX98357A vào 3V3. Mic đã thu WAV16kHz và người dùng xác nhận lời nói đúng,
còn rè nền. Đã ghép mic–loa16kHz ở chế độ raw: người dùng xác nhận tiếng nói
đúng giọng, liền mạch với volume20%. Sau bước1, gain phát raw hiện là8× và bỏ
qua1giây đầu ổn địnhmic; người dùng xác nhận đủ nghe, liền mạch. Board chạy firmware
chính, IDLE/volume0 sau lượt thử; chưa bật hoặc xác minh Wiener trên loa.**

[verified: results/du_lieu_kiem_thu/gain8_volume20_hardware.json + người dùng nghe]
Gain8×/volume20% chạy20giây,22khối,`clip=0`,underrun/overrun/DMA0,faultnone.
Không cam kết không clip với mọi mức nói/khoảng cách. Raw bỏ qua1giây đầu,
sau đó bộđệm danh định2giây; lần phát đầu khoảng3giây sau khiI2Sbắtđầu, chưa
đo độ trễ vật lý. Calibration/Wiener vẫn giữ nguyên, chưa bật trong lượt này.

File nghe đặt ngay trong `results/`: `gain_01_mic_goc.wav`,
`gain_02_truoc_16x_volume20.wav`, `gain_03_sau_8x_volume20.wav`.
Hai file so sánh được dựng bằng hàmC decode/encode của firmware từ cùng bản
mic đã thu trước đây, không tự chuẩn hóa/DCremoval. Khoảng đầu ổn địnhmic được
thay bằngPCM0 để giữ cùng timestamp; bỏ phần trễ bộđệm2giây ở cảhai file.
Đây không phải bản thu mới hoặc bản ghi âm vật lý từ loa. Metadata trong
`results/du_lieu_kiem_thu/gain_wav_comparison.json` ghi nguồn,hash vàtham số.

Từ bước này, log/JSON/raw đặt trong `results/du_lieu_kiem_thu/`, WAV giữ ngay
trong `results/`.108file cũ đã được di chuyển không đổiSHA256; mapping nằm ở
`results/du_lieu_kiem_thu/danh_sach_di_chuyen.json`. Hosttools tự chuyển đường
`--log results/ten.log` sang thư mục dữ liệu; `mic_capture.py --prefix results/ten`
giữWAV tạiresults và đặt sidecar vào thưmục dữliệu.

[verified: results/du_lieu_kiem_thu/mic_speaker_raw_gain16_volume20.log/.json + người dùng nghe]
Lượt cũ trước bước1: bypass20giây xử lý23khối,underrun/overrun/DMA đều0,faultnone. Gain16× chỉ
áp vào phát raw, giới hạn biên trước volume20%; không tự chuẩn hóa/AGC, không
thay đổi đường Wiener. `clip=13107` là số mẫu bộ giới hạn đã tác động; không
được gọi là bản không clipping/méo. Chưa đo trễ vật lý; cấu hình danh định2giây.
Để chạy lại qua Serial115200: `volume 20`, rồi `raw`; `stop` để dừng.
Không tự chạy khi reset. Muốn lặp lại kiểm thử có giới hạn, dùng hosttool.

[verified: results/du_lieu_kiem_thu/mic_capture_baseline.log/.json + người dùng nghe WAV]
INMP441 WS4/SCK5/SD6,16kHz/stereo32,thu4giây/64000frame/512000byte vàoRAM,
dừngI2S rồi chuyểnUART;checksumkhớp,error0,cleanup0. Slot0 giải mãsigned24 có
lời nói đúng, không clippingPCM;slot1toàn0. Có nhiễu/rè nền chưa xác định nguồn.
Bản nghe đã trừDC/chỉnhmức,raw vàWAVPCM gốc được giữ. Chưa chạyWiener/AGC.

[verified: results/du_lieu_kiem_thu/speaker_44100_sd3v3_trial.log + người dùng xác nhận nghe]
Bíp1kHz,25% đỉnh PCM,2giây;44.1kHz/stereo16,GPIO LRC7/BCLK8/DIN9.
Giữ nguyên firmware và mức phát so với lượt không nghe trước đó; người dùng
chỉ đổi SD từ để hở sang nối3V3. Không cần NPN trong cấu hình đã thử này.
**Với bộ phần cứng hiện tại, dùng SD→3V3, không tiếp tục hướng dẫn để hở SD.**
Đây là cấu hình đã có tiếng, chưa phải phép đo ngưỡng SD của chip/module.
Đã xác nhận bíp và lời nói phát được ở16kHz sau nốiSD3V3; không cần44.1kHz.

[verified: results/du_lieu_kiem_thu/live_audio_port_recheck.log, live_audio_flash_app0.log,
live_audio_boot_serial.log] Cổng hiện tại `/dev/ttyACM0`, driver `cdc_acm`, USB
VID:PID1a86:55d3, serial5CE5226325; đường by-id trong platformio.ini trỏ đúng
cổng này. Không có `/dev/ttyUSB0` khi kiểm tra. Esptool xác nhận ESP32-S3 rev0.2,
PSRAM8MB/flash16MB; application tại0x10000 được hash verified. Không ghi
bootloader/partition/NVS/eFuse, không erase toàn chip. Phép thử backup toàn flash
ở921600 bị timeout trước khi đọc; chỉ có backup prefix64KB và partition4KB tại
`.agent-tmp/live-audio/`, **không có backup đầy đủ application cũ**.

## Framework và giới hạn xác minh

- [verified: platformio.ini + SDK esp_idf_version.h] Giữ Arduino2.0.17, PlatformIO
  Espressif32 7.1.3, ESP-IDF4.4.7 đi kèm. I2S và task dùng API ESP-IDF trực tiếp.
  Không chuyển framework nên chưa tạo bản backup Arduino riêng.
- [inferred] Chuyển ESP-IDF thuần không tự làm phép DWT chạy nhanh hơn. Chỉ nên
  chuyển nếu phép đo I2S/deadline hoặc nhu cầu cấu hình SDK chứng minh cần thiết;
  khi chuyển sẽ backup nguyên bản Arduino trước.
- [unverified] Tên board PlatformIO mặc định là DevKitC-1-N8; cấu hình đã override
  flash16MB và qio_opi PSRAM cho mục tiêu N16R8. Build không xác minh được module
  thực tế. Bản firmware này dừng an toàn nếu cấp phát PSRAM thất bại.

## Đấu dây dự kiến — kiểm tra trước khi cấp nguồn

[verified: audio_config.h + xác nhận đấu nối của người dùng] Chân I2S theo phần
mềm; SD/nguồn theo cấu hình phần cứng hiện tại:

| Module | Chân | Nối |
| --- | --- | --- |
| INMP441 | VDD | 3V3 của ESP, không nối5V |
| INMP441 | GND | GND chung |
| INMP441 | L/R | GND, chọn kênh trái |
| INMP441 | WS | GPIO4 |
| INMP441 | SCK | GPIO5 |
| INMP441 | SD | GPIO6 |
| MAX98357A | LRC | GPIO7 |
| MAX98357A | BCLK | GPIO8 |
| MAX98357A | DIN | GPIO9 |
| MAX98357A | GAIN | GND |
| MAX98357A | SD | 3V3 của ESP; đã nghe bíp với cấu hình này; không nối GPIO10 |
| MAX98357A | GND | GND chung |
| MAX98357A | Vin | 5V ổn áp |
| Loa4Ω3W | Hai dây | Hai cực OUT+/OUT− của MAX98357A |

[verified: ConvertDocs ESP32_S3_DATASHEET_MD/02_2_pins.md và
03_3_boot_configurations.md] GPIO4–10 có ở mức chip, không thuộc các strapping
GPIO0/3/45/46. [unverified] Chưa có ảnh/schematic đúng board để xác minh chân
header và ngoại vi đã chiếm chân. Không khởi tạo Wire mặc định dùng GPIO8/9.

[assumed] Module ghi đúng INMP441/MAX98357A như người dùng nêu; pin map chưa phải
sơ đồ lắp đã được kiểm chứng. Với INMP441, mặc định đọc word trái thứ0 trong
stereo32 (`AUDIO_MIC_SLOT=0`). Nếu log báo noise_profile lỗi hoặc peak luôn0,
kiểm tra WS/L/R/slot bằng máy hiện sóng trước, không bỏ kiểm tra calibration.

### Nguồn, GAIN và loa

- [verified: hướng dẫn Adafruit MAX98357A, liên kết cuối tài liệu] MAX98357A dùng
  Vin5V với logic I2S3.3V; GAIN nốiGND là12dB trên breakout tham chiếu.
  Hai OUT là cầu BTL: **không nối bất kỳ cực loa nào xuống GND**, không cắm trực
  tiếp vào đầu vào âm thanh PC. Không lấy nguồn loa từ chân3V3 ESP.
- [inferred] Chọn nguồn5V/2A có bảo vệ là phương án dự phòng cho ESP+ampli;
  đây là định mức nguồn đề xuất, không phải dòng tiêu thụ đã đo. Chưa biết giới
  hạn diode/cổng5V của board: ưu tiên cấp ampli bằng nhánh5V riêng, GND chung,
  không đấu ngược5V ngoài vào USB khi chưa kiểm tra sơ đồ board.
- [assumed] Thêm tụ100nF sát VDD mic; sát Vin ampli dùng100nF và tụ điện phân
  470µF đúng cực, điện áp định mức≥10V, nếu module chưa đủ decoupling. Đi dây
  nguồn/loa ngắn, tách dây mic khỏi dòng loa; không chạm dây khi đang cấp nguồn.
- [verified: firmware] Volume mặc định5%, giới hạn25% trong phần mềm; không có
  tone tự phát. [unverified] Giới hạn này không đo/cam kết công suất loa hoặc
  mức SPL an toàn; 3W là công suất danh định loa, không phải mức phải luôn phát.

### SD hiện tại và mạch mute dự kiến

[verified: người dùng + lượt bíp2026-11-10] Hiện không lắp NPN, GPIO10 không
nối ampli; SD nối thẳng3V3. Loa phát được trong bài test độc lập. Mạch bên dưới
là phương án bổ sung chưa lắp/chưa kiểm chứng, **không phải điều kiện bắt buộc
để thử loa**. Khi SD nối3V3, tín hiệu muteGPIO10 trong firmware chính không thể
shutdown ampli; các từ "mute" trong mô tả pipeline không bảo đảm mute vật lý.
PCM0 hoặc dừng I2S không tương đương kéo SD xuống thấp.
[verified: audio_config.h/audio_pipeline.cpp] `AUDIO_EXTERNAL_NPN_MUTE=0`
mặc định để GPIO10INPUT, không phát lệnh mute ra chân này. Chỉ đổi sang1 khi
thực tế có mạch NPN như dưới. Điều khiển SD trực tiếp bằngGPIO cần logicHIGH
bật/LOWtắt khác phương ánNPN và chưa được triển khai trong cấu hình hiện tại.

[inferred] Dùng NPN open-collector để không đưa pull-up5V có thể có trên SD vào
GPIO ESP. Mạch dự kiến (cần kiểm tra đúng thứ tự E/B/C theo transistor thực tế):

```text
GPIO10 ---- 4.7kΩ ---- B  NPN (ví dụ 2N3904)
   |                   C ---------------- SD MAX98357A
  47kΩ                 E ---------------- GND chung
   |                                      |
  3V3                                    GND

3V3 ---------------- 100kΩ -------------- SD MAX98357A
```

- GPIO10 HIGH = NPN dẫn = kéo SD thấp = mute. GPIO10 LOW = enable; không đảo
  logic trong source nếu vẫn dùng mạch này. Pull-up47k giữ yêu cầu mute khi GPIO
  ở high-impedance và3V3 đã có; pull-up100k ở SD định mức enable cho module tham chiếu.
- [verified: Adafruit pinouts] SD<0.16V mới là shutdown; SD còn chọn kênh. Hướng
  dẫn tham chiếu có pull-up1M lênVin và pulldown100k nội; clone có thể khác.
  TX gửi cùng mono cho cả hai slot nên chọn left/right/average không đổi nội dung.
- [unverified] Mạch NPN/module clone chưa đo: khi chưa nối loa, cần đo SD<0.16V
  ở mute và SD nằm vùng enable khi nhả; nếu module nối SD trực tiếp Vin hoặc
  đo không đúng thì dừng, không đấu transistor/GPIO vào mạch chưa rõ.
- [verified: ConvertDocs/ESP32_S3_DATASHEET_MD/02_2_pins.md] GPIO10 có glitch lúc
  bật nguồn. [unverified] Không cam kết chống pop trước khi firmware chạy hoặc
  khi5V ampli có trước3V3. Chỉ cấp ampli sau khi ESP ổn định trong lần thử đầu;
  muốn đảm bảo bằng phần cứng cần thêm mạch power sequencing/mute phù hợp.

## RAM, RTOS và đường âm thanh

[verified: source + host stream_test]

| Phần | Bố trí hiện tại |
| --- | --- |
| I/O task | Core0, priority4, stack6144byte; cài ISR I2S trên core0 |
| DSP task | Core1, priority2, stack6144byte; không ghi Serial |
| Arduino loop | Nhận lệnh/log/giám sát; không chạy DSP, không giả định loop ởcore0 |
| PSRAM payload | 716316byte: workspace324124 + input/output3+3 khối + tail8192 |
| DMA payload | 16384byte SRAM nội, chưa gồm descriptor/queue/driver |
| Staging RX/TX | Hai mảng1024byte SRAM tĩnh; không DMA trực tiếp vào PSRAM |
| DSP | coif5/L5/float32, khối16000, overlap2048, hop13952mẫu =0.872s |
| I2S | I2S0 full-duplex, stereo32bit,16kHz, BCLK1024000Hz |

Hai cặp clock GPIO4/5 và7/8 là fanout của cùng I2S0 bằng GPIO matrix, **không
nối chập các GPIO output với nhau**. RX lấy kênh trái24bit, TX mã hóa PCM16 ở
16bit cao của slot32 rồi lặp cho trái/phải. Không cần MCLK ngoài.

[verified: audio_i2s.cpp + SDK4.4.7 i2s_hal.c] HAL phiên bản này ép RX mono trong
đường TDM của S3. Sau khi dừng I2S, code xóa `rx_mono/rx_mono_fst_vld`, đặt WS
low cho left rồi khởi động để nhận đủ hai word/frame. Đây là workaround có
guard đúng ESP32-S3/IDF4.4.7, phải xem lại nếu đổi SDK; không dùng API IDF5
`i2s_std.h` vì SDK đã cài không có API đó.

Crossfade chỉ trộn hai ước lượng của **cùng thời điểm**; phát hop13952 và giữ
tail2048 cho khối kế. Không sửa phần âm thanh đã phát. Test bypass kiểm tra
thứ tự167424mẫu; không thay cho kiểm tra click/artefact trên loa.

[verified: audio_config.h] Lịch phát đặt lùi32000mẫu = **2giây** so với luồng
được chấp nhận sau đoạn settling. **Không phải cam kết trễ đúng1giây**.
Sau `start` bỏ1600mẫu ổn định mic, lấy4000mẫu kế tiếp làm noise profile; xử lý
vẫn thu/phát liên tục. [unverified] Latency vật lý còn DMA, thời điểm command,
mic/ampli, nên phải đo thực tế. Không bù drift bằng bỏ/lặp mẫu.

Mỗi khối DSP phải xong trước hop872ms; quá hạn thì FAULT và mute, không âm thầm
phát lại khối cũ. Hai task có watchdog, heartbeat, I/O timeout100ms; lỗi DMA,
sequence/queue, profile không hợp lệ hoặc mất output cũng mute. Stop bỏ phần
đuôi đang đệm; start/raw mới luôn tạo luồng mới và xóa lịch cũ.

## Hiệu chuẩn và chạy thử sau khi được phép nạp

[verified: state machine] Boot ở IDLE, mute; chưa bật I2S. Serial115200, gửi
lệnh kết thúc bằng newline:

```text
status
raw
stop
start
volume 5
status
stop
```

1. Application đã nạp và dừngIDLE. Ngắt nguồn trước khi đấu dây; xác nhận
   header/nguồn/mạch SD trước khi cấp điện cho ampli hoặc gửi `raw`/`start`.
2. `raw`: mic→buffer→loa, bỏ Wiener để kiểm tra đấu dây. Bắt đầu volume thấp,
   để mic xa loa. Kiểm tra60s không `underrun/overrun/dma` và `peak_ppm` có phản ứng.
3. `stop`, sau đó `start`: **giữ im lặng, chỉ có tiếng nền trong1.25s đầu** kể từ
   khi bắt đầu I2S. 1s settling,0.25s lấy nền. Không nói trong đoạn đó.
4. Firmware in `Pn frozen:` với6 giá trị khi ước lượng xong; giữ chúng trong cả
   phiên, không tự cập nhật theo lời nói. Muốn lấy nền mới: `stop`, chờIDLE rồi
   `start`. `raw` không dùng profile.
5. Kiểm tra5phút: không FAULT, `max` DSP nên còn dư so với872ms, stack còn dư;
   ghi âm bên ngoài để đo độ trễ và nghe chỗ nối khối. Build không chứng minh
   những tiêu chí này.

[verified: audio_noise_valid] Reject đầu vào toàn0, profile không hữu hạn/không
có phương sai hoặc mẫu đạt0.99full-scale trong đoạn nền. **Không có VAD**: nói
trong calibration vẫn có thể bị nhận là noise, khiến lọc làm mất tiếng. Giữ
nguồn nhiễu nền giống lúc sử dụng, mic/nguồn/gain không đổi sau calibration.
[inferred] Nhiễu đổi theo thời gian sẽ làm frozen Pn kém phù hợp; không có
AEC/chống hú, nên âm loa lọt mic có thể tạo echo/hú dù không dùng mạng.

## Build — không upload

```
cd firmware_esp32s3
pio run -e esp32-s3-n16r8
```

## Phân tách mã chính và kiểm thử

- `src/main.cpp`: khởi động Arduino, tắt radio, nhận lệnh không chặn.
- `src/audio_i2s.cpp`: full-duplex I2S/DMA, clock fanout, kiểm tra timeout/event.
- `src/audio_pipeline.cpp`: ownership pool/queue, calibration, DSP, phát và mute.
- `src/audio_stream.c`: PCM, kiểm tra noise profile và crossfade dùng chung test.
- `src/wavelet_dsp.c`: thuật toán DWT/IDWT và Wiener dùng chung.
- `include/wavelet_dsp.h`: khai báo hàm, cấu hình và bộ đệm DSP.
- `include/coif5_filters.h`: hệ số bộ lọc cần thiết cho thuật toán, không phải dữ liệu kiểm thử.

Toàn bộ dữ liệu mẫu, đáp án MATLAB, host test và benchmark Arduino nằm trong
`kiem_thu_esp32s3/` ngay bên trong thư mục firmware. Xem README ở thư mục đó để chạy kiểm thử.
Firmware chính không phụ thuộc thư mục kiểm thử; bản kiểm thử dùng lại code DSP chính.

## Nguồn đã dùng

- Tài liệu cục bộ người dùng chỉ định:
  `/home/alberttochiro/Documents/antigravity/ConvertDocs/ESP_IDF_MD/0030_peripherals_i2s.md`,
  `0057_system_freertos.md`, `ESP32_S3_TRM_MD/28_i2s_controller_i2s.md`,
  `ESP32_S3_DATASHEET_MD/02_2_pins.md`, `03_3_boot_configurations.md`.
- SDK đã cài: `driver/i2s.h`, `hal/esp32s3/i2s_ll.h`, `soc/i2s_struct.h`,
  `freertos/task.h`, `esp_idf_version.h` trong Arduino package.
- [Espressif i2s driver4.4.7](https://github.com/espressif/esp-idf/blob/v4.4.7/components/driver/i2s.c)
  và [HAL4.4.7](https://github.com/espressif/esp-idf/blob/v4.4.7/components/hal/i2s_hal.c).
- [Adafruit MAX98357A pinouts](https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp/pinouts).
- [unverified] Không tải được bản datasheet INMP441/MAX98357A từ các URL nhà sản
  xuất thử trong phiên này; không coi hướng dẫn breakout thay cho schematic
  module clone thực tế.
