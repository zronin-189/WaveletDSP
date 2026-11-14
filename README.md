# WaveletDSP — MATLAB & Simulink

Nhánh lưu các file `.m` và `.slx` của dự án, giữ nguyên nội dung và thư mục:

- `matlab_base/`: lọc WAV ngoại tuyến và thử xử lý theo khối.
- `simulink_dsp/`: mô hình `mo_hinh_dwt_wiener.slx`, công cụ dựng/chạy mô hình.
- `matlab_simulink_dsp-1.0.0/`: mã filter-bank tham khảo, giữ license gốc.

Tại thư mục repo trong MATLAB:

```matlab
addpath('matlab_base', 'simulink_dsp');
[y, wav, metrics] = khu_nhieu_am_thanh('duong_dan/noisy.wav', '', 0.25);
```

Đoạn đầu 0,25 giây phải chỉ có nhiễu. `khu_nhieu_am_thanh.m` hiện dùng
**coif5/L7/double**, khác firmware **coif5/L5/float32**. WAV xuất vào `results/`.
Dùng `chay_mo_hinh_simulink(inputFile, cleanFile)` để truyền đường dẫn thực
cho mô hình; không bắt buộc chạy `startup.m` để dùng hàm lọc WAV.

Cần MATLAB cùng các toolbox tương ứng: Wavelet Toolbox, Signal Processing
Toolbox; mô hình còn dùng Simulink và DSP System Toolbox. Chưa chạy lại MATLAB
hoặc mô phỏng trong đợt sắp xếp này; kiểm tra toolbox theo bản MATLAB của bạn.

Dataset `data_DSP_speech/` và kết quả WAV không được đưa lên nhánh. Một số script
hoặc tham số mặc định trong mô hình còn trỏ tới dataset đó; cần cung cấp WAV
của bạn hoặc chỉnh đường dẫn trước khi chạy. Các file `.slx` được sao chép nguyên
vẹn, không sửa mô hình. Firmware nằm ở nhánh `main` và `debug`.
