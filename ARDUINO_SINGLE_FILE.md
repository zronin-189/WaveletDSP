# Firmware Arduino trong một file

Nhánh `dev` có hai dự án độc lập:

- `arduino_main/src/main.cpp`: tự chạy sau cấp nguồn, volume 20%, không ghi mẫu kiểm thử.
- `arduino_debug/src/main.cpp`: chờ lệnh Serial, volume ban đầu 5%, có ghi cặp PCM trước/sau lọc.

Mỗi thư mục chỉ cần `platformio.ini` và `src/main.cpp` để build. Không cần
copy các file trong `firmware_esp32s3/include/` hoặc `firmware_esp32s3/src/`.
Các phần cấu hình, hệ số coif5, DSP, I2S, task và `setup()`/`loop()` nằm
trong cùng file, được ngăn bằng dòng chú thích. Sửa ngay `main.cpp` của bản
muốn dùng. Hai bản chỉ khác ba cấu hình: `AUDIO_TEST_CAPTURE`,
`AUDIO_AUTOSTART`, `AUDIO_BOOT_VOLUME_PERCENT`.

## VS Code và PlatformIO

Mở **một trong hai thư mục** `arduino_main` hoặc `arduino_debug` bằng VS Code
có PlatformIO IDE, rồi chọn Build. Hoặc chạy từ gốc repo:

```sh
pio run -d arduino_main
pio run -d arduino_debug
```

Chọn đúng cổng trong PlatformIO Devices trước khi Upload. Cấu hình không
gắn cứng cổng USB của máy tác giả. Không nạp cả hai dự án vào cùng board
cùng lúc: board chỉ chạy bản được nạp gần nhất.

## Copy sang Arduino IDE

1. Cài board package **esp32 by Espressif Systems, phiên bản 2.0.17** trong
   Boards Manager. Mã đang dùng API I2S của ESP-IDF 4.4.7; không dùng core
   ESP32 3.x cho bản này.
2. Tạo sketch mới, xóa nội dung mặc định rồi copy **toàn bộ** một file
   `src/main.cpp` vào tab `.ino`. Không cần tab `.h`/`.cpp` khác.
3. Chọn **ESP32S3 Dev Module**, CPU **240MHz**, Flash Size **16MB**, Flash Mode
   **QIO 80MHz**, PSRAM **OPI PSRAM**, USB Mode **Hardware CDC and JTAG**,
   USB CDC On Boot **Disabled**. Chọn Partition Scheme **16M Flash
   (3MB APP/9.9MB FATFS)** khi nạp bằng Arduino IDE.
4. Chọn đúng port, Verify rồi Upload. Serial Monitor dùng **115200 baud**.

Partition Scheme trên Arduino IDE khác `default_16MB.csv` của PlatformIO;
dùng cùng một công cụ khi muốn giữ bố cục phân vùng đã có. Không bật
Erase All Flash. `main.cpp` có khai báo hàm rõ ràng và đặt tối ưu `O2`
cho mã ứng dụng để không phụ thuộc mặc định `-Os` của Arduino IDE.

Đây là firmware cho **ESP32-S3 N16R8**, không phải Arduino Uno/Nano AVR.

## Phần cứng và sử dụng

Giữ cách nối đã thử trên bộ phần cứng hiện tại:

- INMP441: VDD→3V3, GND→GND, L/R→GND, WS→GPIO4, SCK→GPIO5, SD→GPIO6.
- MAX98357A: LRC→GPIO7, BCLK→GPIO8, DIN→GPIO9, GAIN→GND, SD→3V3,
  Vin→5V, GND chung. GPIO10 không dùng.
- Loa 4Ω/3W nối giữa OUT+ và OUT−, không nối đầu loa xuống GND.

Cả hai giữ coif5/L5, 16 kHz, float32, gain 8×, đệm danh định 1,25 giây.
Giữ im lặng trong 1 giây ổn định mic và 0,25 giây lấy nền tính từ lúc I2S
bắt đầu; sau đó nói vào mic. Pn giữ cố định trong phiên. `main` tự bắt
đầu; `debug` dùng Serial `volume 20` rồi `start`. Có thể dùng `stop`,
`status`, `volume 0..25`, hoặc `raw` để chẩn đoán. SD không điều khiển
bằng GPIO.

`debug` ghi cặp PCM trong tám hop đầu, không ghi liên tục vô hạn. Sau
`stop` và trạng thái IDLE, dùng công cụ host để tải cặp PCM thành WAV:

```sh
python firmware_esp32s3/kiem_thu_esp32s3/host/filter_listen_test.py --name thu_moi
```

Chạy lệnh từ gốc repo với Python có pyserial, sau khi đã nạp bản debug.
Công cụ hiện nhận diện USB serial của board tác giả; nếu dùng board khác,
cần sửa bộ lọc VID/PID/serial trong công cụ host. Điều này không ảnh hưởng
việc chạy firmware hoặc gõ lệnh Serial trực tiếp. WAV nằm tại `results/`,
log/metadata nằm trong `results/du_lieu_kiem_thu/`; không đưa bản thu lên Git.

## Phạm vi kiểm chứng

[verified: PlatformIO build và kiểm thử trên PC] Hai `main.cpp` và hai bản
copy `.ino` đều được build qua Arduino framework trong PlatformIO.
Phần DSP được trích trực tiếp từ file đã gom để chạy golden-vector và
stream regression với AddressSanitizer/UBSan.

[unverified] Chưa chạy Verify trực tiếp bằng Arduino IDE/Arduino CLI với
core ESP32, và chưa nạp bản một file lên phần cứng. Máy kiểm tra chỉ có
core AVR trong Arduino CLI; không dùng core AVR để xác nhận firmware ESP32.
Các kết quả nghe của bản nhiều file không tự thay thế kiểm thử phần cứng
cho bản một file này.
