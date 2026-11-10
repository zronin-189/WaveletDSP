# Ví dụ Wavelet một chiều được giữ lại

Bộ DSP ban đầu đã được rút gọn để phục vụ đề tài tiếng nói. Các file bên dưới là tài liệu học nguyên lý, không phải pipeline đang dùng để demo.

- `ds10_04_03_DWTFB.m`: ví dụ DWT/IDWT nhiều mức, gọi hai helper bên dưới.
- `DWT_filterbank.m`: ngân hàng lọc phân tích và tổng hợp.
- `DWT_dec.m`: tái tạo riêng approximation/detail để quan sát từng dải.
- `dyadic_filterbank.m`: soft threshold với vector ngưỡng riêng theo dải; cần kiểm tra độ trễ khi tái sử dụng.
- `license.txt`: giữ nguyên giấy phép và ghi công mã gốc.

Chạy ví dụ ở thư mục này với Wavelet Toolbox và Signal Processing Toolbox. Script ví dụ có `clear, clf`; nên chạy riêng, tránh làm mất biến đang dùng trong Workspace. Không thay pipeline MODWT bằng các ví dụ này mà chưa kiểm tra căn chỉnh và chiều dài.

Code ứng dụng chính là `../matlab_base/khu_nhieu_am_thanh.m`, sử dụng MODWT `coif5`, level 7; không phụ thuộc vào thư mục tham khảo này.

Các ví dụ ảnh, DoA, Fourier, Kalman, bộ lọc khác và README nguyên bản được cất ở `../.trash/don_dep_20261024/matlab_simulink_dsp-1.0.0/`. Chúng không nằm trên đường chạy ứng dụng hiện tại.
