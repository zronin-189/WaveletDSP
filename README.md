# WaveletDSP — main

Firmware **ESP32-S3 + INMP441 + MAX98357A + loa**, xử lý và phát liên tục.
Sau cấp nguồn tự ổn định mic, lấy mẫu nền rồi lọc; **không ghi cặp WAV, không tự
dừng sau 10 giây**, không cần máy tính gửi `start`. Thuật toán DSP giống nhánh
`debug`; khác chế độ khởi động và bỏ phần ghi/truyền dữ liệu kiểm thử.

## Đấu nối

Dùng ESP32-S3 N16R8 như cấu hình đã thử; kiểm tra nhãn GPIO trên board cụ thể.

- INMP441: VDD→3V3, GND→GND, L/R→GND; WS→GPIO4, SCK→GPIO5, SD→GPIO6.
- MAX98357A: LRC→GPIO7, BCLK→GPIO8, DIN→GPIO9; GAIN→GND, **SD→3V3 cố định**,
  GND→GND chung, **Vin→5V**. GPIO10 không điều khiển SD.
- Loa **4 Ω / 3 W** nối giữa hai cực OUT của MAX98357A; không nối một cực loa
  về GND. Mic chỉ dùng 3,3 V; nguồn ampli 5 V phải đủ dòng, GND nối chung.

## Build và chạy

Mở `firmware_esp32s3` trong VS Code/PlatformIO, chạy `pio run`, chọn đúng port
rồi Upload. Chỉ một file firmware `src/main.cpp`; `platformio.ini` giữ cấu hình
board. Sau bật nguồn giữ yên nền trong thời gian ổn định và lấy mẫu nền. Khi
Serial báo `RUNNING`, nói vào mic; loa phát tín hiệu sau khoảng đệm xử lý.
Volume mặc định **20%**, gain phát **8×**, limiter đặt trước volume.

Serial 115200 baud, Newline: `status`, `stop`, `start`, `raw`, `volume 0..25`.
`start` lấy lại nền; `raw` kiểm tra đường truyền không lọc. Không có lệnh dump
hay công cụ xuất WAV trên nhánh này. Khi lỗi, luồng dừng và I2S được đóng;
SD ampli vẫn nối cố định 3V3, không có điều khiển mute bằng GPIO.

## Cấu hình chung

Arduino-ESP32 **2.0.17** (ESP-IDF 4.4.7), PlatformIO `espressif32@7.1.3`;
ESP32-S3 N16R8, flash 16 MB và OPI PSRAM 8 MB. Code chặn phiên bản IDF khác
vì có cấu hình thanh ghi I2S dành riêng cho bản đã kiểm thử.

DSP: **16 kHz, coif5, level 5, float32**; cửa sổ **2.048 mẫu**, overlap
**512 mẫu**, hop **1.536 mẫu**. Giữ Wiener theo dải, cửa sổ công suất 64 ms,
làm mượt gain 40 ms và gain tối thiểu 0,05. Mẫu nền riêng **4.000 mẫu**.

Sau `start`, bỏ 5 giây mẫu đầu rồi lấy nền 0,25 giây. Tiếp đó xử lý liên tục;
đệm đầu ra đặt **4.096 mẫu = 256 ms**, tính từ đầu luồng sau lấy nền.
Đây là độ trễ cấu hình, **chưa phải độ trễ mic–loa đo thực tế**.

Toàn bộ firmware nằm trong `firmware_esp32s3/src/main.cpp`, có chú thích tiếng Việt.
Không cần file `.h` riêng. Muốn dùng Arduino IDE, chép toàn bộ nội dung file vào
sketch, chọn đúng ESP32-S3, flash 16 MB và **OPI PSRAM**; bản này đã build bằng
PlatformIO, chưa kiểm chứng riêng bằng Arduino IDE. Hai task: I/O nhân 0 ưu tiên 4,
DSP nhân 1 ưu tiên 2; bộ đệm lớn nằm trong PSRAM.

## Trạng thái kiểm chứng

Build và DSP đã kiểm tra bằng phần mềm; phiên bản `main` đầy đủ sau đợt sắp xếp
này **chưa chạy kiểm thử với ampli/loa**. Các lượt mic-only không thay thế kiểm
thử âm thanh đầu ra thực tế. Chưa đo độ trễ âm học. Dùng nhánh `debug` để ghi
WAV và so chất lượng; [`MATLAB&SIM`](https://github.com/zronin-189/WaveletDSP/tree/MATLAB%26SIM)
giữ toàn bộ mã MATLAB/Simulink hiện có.
