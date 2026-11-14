# WaveletDSP — debug

Nhánh kiểm thử **ESP32-S3 + INMP441**, không cần MAX98357A hay loa. Ghi cặp
WAV trước/sau lọc để nghe và đối chiếu với MATLAB. Firmware vẫn bật I2S RX+TX
để giữ cấu hình clock đã dùng; chưa có loa thì không đo được độ trễ âm học.

## File và đấu nối

- `firmware_esp32s3/src/main.cpp`: một file firmware Arduino.
- `firmware_esp32s3/platformio.ini`: cấu hình build/nạp.
- `firmware_esp32s3/download_wav.py`: công cụ chạy trên máy tính; không nạp vào MCU.

INMP441: **VDD→3V3**, GND→GND, L/R→GND, WS→GPIO4, SCK→GPIO5, SD→GPIO6.
GPIO7/8/9 vẫn có tín hiệu I2S TX; GPIO10 không dùng. Không cấp 5 V cho mic.

## Build và thử WAV

Mở thư mục `firmware_esp32s3` bằng VS Code/PlatformIO. Chạy `pio run` để build;
chọn đúng cổng board trước khi Upload. Bản debug khởi động ở `IDLE`.

Để tải âm thanh, tạo môi trường Python tại thư mục gốc repo:

```sh
python3 -m venv firmware_esp32s3/.venv
firmware_esp32s3/.venv/bin/python -m pip install -r firmware_esp32s3/requirements.txt
firmware_esp32s3/.venv/bin/python firmware_esp32s3/download_wav.py --port /dev/ttyACM0 --name lan1 --record
```

Đóng Serial Monitor trước khi chạy; thay port bằng cổng board thực tế. Giữ im
lặng lúc lấy nền, bật nhạc/giọng nói khi terminal báo **RUNNING**. Có thể thêm
`--notify` trên Linux nếu đã có `notify-send` để hiện thông báo ngay lúc thu.
Script giữ một kết nối Serial xuyên suốt vì mở lại cổng có thể reset board,
làm mất bản thu RAM. Firmware ghi 10 giây rồi tự dừng; script tải và kiểm FNV.

WAV nằm trong `results/`, log/JSON trong `results/du_lieu_kiem_thu/`; không đưa
bản ghi lên Git. Mỗi lượt dùng tên mới để tránh ghi đè. WAV trước lọc là PCM16,
**gain 1, chưa gain phát/limiter/volume**; vẫn có lượng tử hóa từ mẫu mic.
WAV sau lọc có gain 8×, limiter rồi volume 20%; cần cân mức khi so nghe hai bản.
MATLAB nên dùng WAV trước lọc và đoạn đầu đúng là chỉ có nền.

Lệnh Serial 115200 baud, kết thúc dòng Newline: `start`, `raw`, `stop`, `status`,
`volume 0..25`. `pins`, `raw_dump`, `noise_dump`, `filter_dump` là chẩn đoán;
không chạy `pins` trong lượt đánh giá âm thanh vì có thể ảnh hưởng deadline.
Chỉ dùng `raw_dump` khi đã dừng. Khi lỗi `noise_profile`, kiểm tra đầu vào;
không ép bỏ kiểm tra để lấy WAV lỗi.

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

Bản 2.048 trước khi trình bày lại đã thu 10 giây trên ESP32-S3, DSP báo tối đa
49 ms và không underrun/overrun/DMA; người dùng đánh giá nghe tốt. Đợt sắp xếp
nhánh này kiểm tra lại build, tính tương đương code và DSP trên máy tính;
không nạp/thu lại phần cứng. Nhánh `main` dành cho hệ mic–ampli–loa;
[`MATLAB&SIM`](https://github.com/zronin-189/WaveletDSP/tree/MATLAB%26SIM) giữ MATLAB/Simulink.
