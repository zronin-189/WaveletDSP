# Firststep — mic → loa, không lọc

Nhánh `dev` chứa bản Arduino cơ bản để kiểm tra đường âm thanh:
**INMP441 → ESP32-S3 → MAX98357A → loa**. Cấp nguồn là chạy liên tục;
không wavelet, lấy nền, chờ một giây hay xuất WAV.

Firmware thu một đoạn **128 mẫu tại 16 kHz**, lấy mẫu PCM 24-bit từ slot
mic, nhân **gain 4×**, bão hòa trong dải PCM rồi phát cả hai kênh TX.
Không lọc hoặc chỉnh volume riêng. Phép nhân dùng int64 để tránh tràn số;
bão hòa có thể gây méo nếu tín hiệu đầu vào quá lớn. Một đoạn dài **8 ms**,
nhưng vẫn có bộ đệm DMA và thời gian chuyển đổi của phần cứng: chưa đo độ trễ
mic–loa và không khẳng định tổng trễ chỉ 8 ms. DMA giữ 8 buffer mỗi chiều.

## Đấu nối

- INMP441: VDD→3V3, GND→GND, L/R→GND, WS→GPIO4, SCK→GPIO5, SD→GPIO6.
- MAX98357A: Vin→5V, GND→GND chung, SD→3V3, GAIN→GND;
  LRC→GPIO7, BCLK→GPIO8, DIN→GPIO9. GPIO10 không dùng.
- Loa 4 Ω / 3 W: nối giữa OUT+ và OUT− của ampli; không nối cực loa về GND.

Mic chỉ dùng 3,3 V; ampli dùng nguồn 5 V đủ dòng. Pin map giữ theo hệ đã dùng
trong dự án; kiểm tra nhãn GPIO trên board của bạn trước khi đấu. Giữ loa xa
mic để hạn chế hồi tiếp âm thanh.

## Build và nạp

Mở `firmware_esp32s3/` bằng VS Code/PlatformIO, build bằng `pio run`. Chọn đúng
port board rồi Upload. Serial 115200 chỉ báo RUNNING hoặc STOP khi có lỗi.
Không cần gửi `start`; không có các lệnh thu WAV hay xử lý DSP.

Một file `firmware_esp32s3/src/main.cpp` chứa toàn bộ firmware. Có thể chép nội
dung vào sketch Arduino IDE, dùng **esp32 by Espressif Systems**, core **2.x (IDF 4.4)** hoặc
**3.x (IDF 5.x)**,
ESP32-S3 N16R8, flash 16 MB, OPI PSRAM 8 MB. Cấu hình PlatformIO được pin tại
`espressif32@7.1.3` (core 2.0.17). Arduino IDE không còn bắt buộc đúng bản vá này.

Trong Boards Manager cài gói **esp32 by Espressif Systems**. Chọn board theo
phần cứng: **ESP32S3 Dev Module** cho module/devkit S3 thông thường, hoặc
**ESP32-S3-USB-OTG** nếu dùng đúng board USB-OTG của Espressif. Tên board không
phải phiên bản core; xem phiên bản gói trong Boards Manager. Đặt flash/PSRAM và
USB theo board thực tế, không mặc định mọi board S3 đều là N16R8.

Nếu core 3.x báo cảnh báo driver I2S `deprecated`, đó là cảnh báo API cũ,
không phải lỗi khóa phiên bản. Không dùng đồng thời thư viện `ESP_I2S.h`/driver
I2S mới trong sketch này. Khi gặp lỗi tiếp theo, gửi toàn bộ thông báo compile
và phiên bản core; không chỉ bỏ các dòng `#error` khác.

Mỗi lần đọc/ghi có timeout; khi timeout, thiếu byte hoặc DMA lỗi, firmware đóng
I2S và đưa tín hiệu ampli về thấp. SD ampli vẫn nối cố định 3V3. Reset để thử
lại; không tự lặp khởi động khi phần cứng lỗi.

## Kiểm chứng

Bản này được build bằng PlatformIO và kiểm tra trên máy tính phép nhân gain, bão hòa và
chép stereo. Bản gain 4× đã nạp vào ESP32-S3, xác minh hash và thấy RUNNING
trong lượt theo dõi 8 giây không có STOP/panic. Chất lượng nghe do người dùng kiểm tra.
Bản sửa tương thích được build trên core 2.0.17; API và thanh ghi đã đối chiếu
với nguồn chính thức core 3.3.4 / IDF 5.5, **chưa build hoặc nghe thử trên core
3.x tại máy này**. Driver legacy chưa được thay bằng driver mới.
Các nhánh `main`, `debug` giữ thuật toán lọc; `MATLAB&SIM` giữ MATLAB/Simulink.
