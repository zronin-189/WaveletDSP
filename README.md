# Firststep — mic → loa, không lọc

Nhánh `dev` chứa bản Arduino cơ bản để kiểm tra đường âm thanh:
**INMP441 → ESP32-S3 → MAX98357A → loa**. Cấp nguồn là chạy liên tục;
không wavelet, lấy nền, chờ một giây hay xuất WAV.

Firmware thu một đoạn **128 mẫu tại 16 kHz**, chép nguyên word 32-bit của slot
mic sang cả hai kênh TX rồi gửi ngay. **Gain 1**, không limiter, không volume
số; vì thế có thể nghe nhỏ hơn bản xử lý trước đây. Một đoạn dài **8 ms**,
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
dung vào sketch Arduino IDE, dùng **Arduino-ESP32 2.0.17 / IDF 4.4.7**,
ESP32-S3 N16R8, flash 16 MB, OPI PSRAM 8 MB. Cấu hình PlatformIO được pin tại
`espressif32@7.1.3`; không tự chuyển sang Arduino core 3.x.

Mỗi lần đọc/ghi có timeout; khi timeout, thiếu byte hoặc DMA lỗi, firmware đóng
I2S và đưa tín hiệu ampli về thấp. SD ampli vẫn nối cố định 3V3. Reset để thử
lại; không tự lặp khởi động khi phần cứng lỗi.

## Kiểm chứng

Bản này được build bằng PlatformIO và kiểm tra trên máy tính việc giữ nguyên
mẫu khi chép slot mic sang stereo. Chưa nạp hoặc nghe thử trên board trong đợt
này; cần kiểm thử phần cứng trước khi kết luận âm lượng và độ liền mạch.
Các nhánh `main`, `debug` giữ thuật toán lọc; `MATLAB&SIM` giữ MATLAB/Simulink.
