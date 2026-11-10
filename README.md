# WaveletDSP — Khử nhiễu tiếng nói bằng MATLAB & Simulink

## Firmware ESP32-S3: hai nhánh

**Nhánh hiện tại: `main`.** Sau cấp nguồn, firmware tự khởi động lọc ở
volume 20%: ổn định mic, lấy nền rồi phát mic → lọc → loa. Không cần máy
tính gửi lệnh, không ghi cặp kiểm thử hay tạo WAV. Nhánh `debug` giữ chế
độ khởi động bằng Serial và ghi cặp PCM để nghe so sánh, cải tiến.

Cả hai giữ coif5/L5, float32, 16 kHz, gain 8× và đệm danh định 1,25 giây.
Mức đệm này đã được nghe xác nhận ở firmware trước khi tách nhánh;
không phải phép đo trễ âm học. Không điều khiển SD: MAX98357A SD nối 3V3.
Xem [hướng dẫn firmware](firmware_esp32s3/README.md).

Build từ gốc repo bằng `pio run -d firmware_esp32s3`.
Không đưa bản thu giọng nói/kết quả cục bộ lên Git. `data_DSP_speech/` là dữ
liệu cục bộ không kèm repo; dùng WAV của bạn cho MATLAB/Simulink. Vector
để kiểm thử DSP C đã có trong `firmware_esp32s3/kiem_thu_esp32s3/vectors/`.

## 1. Chương trình chạy chính trên MATLAB (`matlab_base/khu_nhieu_am_thanh.m`)

Mở MATLAB tại thư mục gốc dự án, thêm hai thư mục mã vào path rồi chạy:

```matlab
addpath(fullfile(pwd, 'matlab_base'), fullfile(pwd, 'simulink_dsp'));
khu_nhieu_am_thanh
```

Chọn file noisy, rồi chọn clean cùng bản ghi nếu có; Cancel ở bước clean để bỏ qua đánh giá tham chiếu. Cũng có thể gọi trực tiếp:

```matlab
[am_thanh, tep_wav, bang] = khu_nhieu_am_thanh('noisy.wav', 'clean.wav', 0.25);
```

Không có clean thì truyền `''`. Hai file phải mono, cùng Fs, số mẫu và khớp thời gian. Đoạn đầu được chỉ định (mặc định 250 ms) phải chỉ có nhiễu. Cần Wavelet Toolbox và Signal Processing Toolbox.

Thuật toán chuẩn: DWT `coif5`, level 7, biên đối xứng (`Mode='sym'`); noise profile theo dải; gain Wiener cục bộ cho cả dải xấp xỉ và các dải chi tiết. Cửa sổ công suất 64 ms và làm mượt gain 40 ms; sàn gain là 0.05. Không có bước giảm đuôi nhiễu. WAV 24 bit có hậu tố `_dwt_coif5_L7_denoised`, lưu trong `results/`, tự thêm số khi trùng tên. Đồ thị gồm waveform/spectrogram, có ba hàng nếu có clean và hai hàng nếu không. Chỉ tính SNR/MSE/RMSE khi có clean đúng cặp. Chưa tự theo dõi nhiễu thay đổi theo thời gian. Cú pháp `Mode` đã được kiểm tra trên MATLAB R2026b.

---

## 2. Mô hình trực quan Simulink (`simulink_dsp/mo_hinh_dwt_wiener.slx`)

Mô hình dùng DWT/IDWT `coif5` level 7 và Wiener theo dải, **không có MATLAB Function block**. Không cần chạy file `.m` dự án trước khi mở `.slx` và bấm Run. Tuy nhiên, điều này **không đồng nghĩa toàn bộ model không có mã MATLAB nhúng**, và hiện chưa tương đương 100% thuật toán offline chuẩn.

- `Âm thanh có nhiễu`: chọn WAV một kênh. `Use_Clean=0`: chỉ cần tệp có nhiễu, không yêu cầu tên theo bộ dữ liệu và không đọc tệp sạch. Thuật toán vẫn lấy mẫu nhiễu từ chính đầu vào. `Âm thanh sạch` là nguồn tham chiếu tùy chọn; `Ghép kênh` là tên hiển thị của khối Matrix Concatenate, không thay đổi kiểu ghép dữ liệu.
- Nhấp đúp `Bộ lọc sóng con và Wiener` để chỉnh tham số qua lời nhắc tiếng Việt có dấu. Các khóa kỹ thuật tương ứng là `Noise_Sec`, `Noise_Start_Auto`, `Noise_Start`, `Win_Pow`, `Win_Smooth`, `Gain_Floor`, `Noise_Auto`, `Noise_Power_1` đến `Noise_Power_8`, `Use_Clean`, `Auto_Clean`, `Auto_Run`, `Auto_Frame`. Wavelet và level hiện cố định `coif5`, 7; các biến cũ trong `simulink_dsp/khoi_tao_mo_hinh_dwt.m` không điều khiển model này.
- Khi có âm thanh sạch để đánh giá: đặt `Use_Clean=1`, `Auto_Clean=0` và chọn đường dẫn **trực tiếp trong khối `Âm thanh sạch`**. `Clean_File` cũ trong mask được ẩn, không còn là nguồn ghi đè. Tham chiếu phải cùng Fs, số mẫu và khớp thời gian; không tham gia lọc. Chỉ bật `Auto_Clean=1` nếu muốn tự ghép `speech_NNN_*.wav`; đường dẫn đã chọn được giữ lại sau Run.
- DWT → tám dải → bình phương, trung bình FIR, gain Wiener, làm mượt gain, nhân hệ số → đồng bộ các dải → IDWT. FIR đồng bộ vẫn có chức năng trễ; thay tên khối không loại bỏ độ trễ toán học.
- `So sánh dạng sóng` nhận **tín hiệu có nhiễu đã đồng bộ–tín hiệu đã lọc** khi không có tham chiếu, hoặc **tín hiệu sạch đã đồng bộ–tín hiệu đã lọc** khi có tham chiếu. Spectrum Analyzer và công tắc nghe thử vẫn được bỏ; đường lọc không đổi.
- Bộ đo cộng năng lượng trên đúng toàn bộ mẫu hợp lệ: `SNR = 10*log10(sum(clean.^2)/sum((clean-signal).^2))`. Bỏ phần khởi động và padding khỏi phép đo; không cắt mất đuôi bản ghi, không ép SNR về 10 hay đổi biên độ noisy.
- Không có clean: các Display SNR In/Out, Delta SNR và RMSE nhận `NaN` (không khả dụng), không phải lỗi bộ lọc và không phải SNR bằng 0. Có clean: giá trị đang chạy là SNR tích lũy một phần; **giá trị cuối** mới so với SNR toàn file. Bản thu bất kỳ không mặc nhiên có SNR 10 dB.
- `Xuất âm thanh đã lọc` nhận thẳng nhánh đã lọc và xuất WAV 24-bit. Model chạy thêm để xả pipeline, rồi callback lấy đúng số mẫu nguồn sau độ trễ. File kết quả đã tồn tại được giữ, lần chạy mới có hậu tố thời gian.

### Nhãn tiếng Việt

Tên khối do dự án quản lý dùng tiếng Việt có dấu, dấu cách thay gạch dưới, viết hoa đầu câu (giữ tên riêng Wiener). Bên trong bộ lọc có `Phân rã sóng con` → `Dải con 1` đến `Dải con 8` → `Tái tạo sóng con`. Lời nhắc tham số và thông báo dự án cũng được dịch; công thức, tên biến mask/workspace và API không đổi.

[verified: `results/vietnamese_20261108_090549_151/verification.log`, `results/vietnamese_20261108_090826_841/verification.log`] MATLAB R2026b kiểm tra 216 tên khối dự án, gồm cả khối trong mặt nạ. WAV và toàn bộ chuỗi bốn chỉ số/bộ đếm khớp chính xác trước–sau đổi tên ở cả hai chế độ có/không tham chiếu. Cập nhật lặp lại và mô phỏng model đã lưu ngoài đường dẫn dự án đều đạt; thiết lập nguồn, tham số, thời gian và vị trí khối cấp ngoài được giữ. Không cần chạy tệp `.m` để dùng model đã lưu.

[unverified] Chưa quan sát giao diện Simulink bằng mắt. Hộp thoại, nhãn cổng tự sinh như `Audio`/`EOF`, chữ trên biểu tượng và nội dung bên trong khối liên kết thư viện MathWorks vẫn có thể là tiếng Anh; không sửa thư viện để tránh phá liên kết. Tên tệp âm thanh được giữ nguyên. Việc Việt hóa không khắc phục các giới hạn DSP và cảnh báo `non-tunable` đã nêu.

### Chỉ nhập số, không cần tính hệ số

1. Chọn WAV tại nguồn âm thanh có nhiễu; chọn nguồn sạch chỉ khi cần đánh giá tham chiếu.
2. Mở bộ lọc, nhập các số tại `Độ dài mẫu nhiễu (giây)`, `Thời điểm bắt đầu lấy mẫu nhiễu thủ công (giây)`, `Cửa sổ tính công suất (giây)`, `Cửa sổ làm mượt hệ số khuếch đại (giây)` và `Hệ số khuếch đại tối thiểu (từ 0 đến 1)` theo nhu cầu. Giữ các chế độ tự động hiện có nếu không cần nhập thủ công.
3. Mặc định để tự ước lượng công suất nhiễu. Khi tắt tự ước lượng, nhập tám ô `Công suất nhiễu dải 1 khi nhập tay` đến `Công suất nhiễu dải 8 khi nhập tay`, mỗi ô một số không âm. Trường vector cũ `Noise_Powers` được giữ ẩn để chuyển đổi dữ liệu cũ, không còn điều khiển chế độ nhập tay.
4. Chạy mô phỏng. Model tự sinh hệ số FIR và độ trễ bằng công thức cũ. Không cần nhập `b_pow_1`, `b_sm_1`, `b_sync_1`, `b_tree_1`, `Pn_1` hoặc tính `Total_Delay`.

Các khối FIR hiện có mặt nạ riêng chỉ hiển thị **số mẫu cửa sổ/số mẫu đồng bộ đã tính**. Ô này chỉ đọc vì phụ thuộc thiết lập bộ lọc và tần số lấy mẫu; chỉnh thời gian cửa sổ tại bộ lọc để cập nhật nhất quán các nhánh. Không có chế độ nhập tay từng độ trễ làm sai đồng bộ. Bên trong vẫn dùng các dãy hệ số FIR, không thay phép trung bình bằng phép nhân một số.

### Đổi tên khối tùy ý

Tên hiển thị không còn là khóa tìm khối khi mô phỏng. Callback khởi tạo/xuất WAV, mặt nạ và bộ chạy tùy chọn dùng `Tag` vai trò nội bộ và handle/cổng. Bạn có thể đổi tên khối nguồn, đích, bộ lọc, dải con và các khối dự án bên trong; không cần giữ các tên minh họa trong README. Cập nhật tại chỗ bằng `tao_mo_hinh_simulink('mo_hinh_dwt_wiener',true)` cũng giữ tên bạn đã đổi. Dựng mới toàn bộ không phải cập nhật tại chỗ.

Không xóa/thay `Tag` nội bộ hoặc nhân bản khối rồi giữ cùng `Tag`: model báo thiếu/trùng định danh thay vì chọn nhầm. Xóa khối, thay dây nối, chuyển khối sang model khác và sửa nội bộ thư viện không thuộc bảo đảm đổi tên. Tên file model vẫn là `mo_hinh_dwt_wiener.slx` đối với bộ chạy tiện ích.

[verified: `results/numeric_interface_20261108_121233_871/verification.log`] MATLAB R2026b: bốn ca đối chứng gồm có/không tham chiếu, cửa sổ 64/40 ms và 48/32 ms, nhiễu tự động/thủ công, nguồn 16/48 kHz. WAV, hệ số FIR, tám Pn, tổng độ trễ, bốn chuỗi chỉ số và bộ đếm khớp chính xác trước–sau. Đổi tên toàn bộ 216 khối dự án vẫn chạy cả hai chế độ; lưu/mở lại model đã đổi tên chạy ngoài project path; runner và cập nhật tại chỗ giữ tên đã đổi; thiếu/trùng định danh được chẩn đoán. Tên tiếng Việt và thiết lập trước thử nghiệm được phục hồi ở bản giao cuối. Chưa kiểm tra GUI, Fast Restart hoặc dựng mới toàn bộ từ đầu.

### Giữ thiết lập người dùng khi Run

Run không nạp lại giá trị mặc định của tệp đầu vào, tham số lọc hay mẫu nhiễu. Đường dẫn nhập trực tiếp ở `Âm thanh có nhiễu`/`Âm thanh sạch` có hiệu lực; chọn chế độ không tham chiếu không thay vĩnh viễn đường dẫn nguồn sạch. Các thiết lập tự động là lựa chọn riêng:

- `Auto_Run=0` (mới thêm, mặc định): giữ nguyên solver, FixedStep và StopTime bạn đặt. Bước solver phải chia hết chu kỳ lấy mẫu; StopTime phải hữu hạn và đủ xả pipeline. Model báo lỗi hướng dẫn nếu không phù hợp, không tự sửa. `Auto_Run=1`: bạn cho phép model tự tính bước/thời gian chạy theo input; các giá trị dẫn xuất này được cập nhật.
- `Auto_Frame=0` (mới thêm, mặc định): giữ nguyên frame đã chọn. Khi dùng clean, hai nguồn cần cùng SamplesPerFrame. `Auto_Frame=1`: cho phép đồng bộ frame Clean theo Noisy.
- `Auto_Clean=0`: dùng file Clean bạn chọn. `Auto_Clean=1`: cho phép tìm clean theo tên noisy trong bộ dữ liệu.
- Tên output cấu hình được giữ giữa các Run. Nếu trùng file đã có, chỉ **file thực tế của lần chạy** nhận hậu tố thời gian, không tích lũy hậu tố vào thiết lập. Command Window báo đường dẫn thực tế; wrapper đọc `Last_Output_File` trong model workspace.
- Bộ đo được gom thành ba nhóm `Mẫu hợp lệ` → `Tổng năng lượng` → `Tính các chỉ số`. Đây là giản lược sơ đồ, không thay năng lượng toàn bản ghi bằng trung bình SNR theo khung. Giữ mặt nạ mẫu hợp lệ, bộ đếm và NaN khi không có tham chiếu.

[verified: `results/user_settings_console.log`] Kiểm tra nhiều Run với 512 mẫu/frame, cửa sổ 48/32 ms, gain floor 0.08 và manual noise start: đường dẫn/param/solver thủ công giữ nguyên; WAV giống baseline, các trace chỉ số khớp trên cùng khoảng lấy mẫu. Những giá trị tự động chỉ đổi khi bật tùy chọn tương ứng. MATLAB có cảnh báo `non-tunable` khi callback khôi phục metadata đường dẫn trong StopFcn; kiểm tra đọc lại cấu hình và WAV qua các Run đều đạt. Chưa kiểm tra GUI/Fast Restart.

**Giới hạn còn lại:** `Pn` tự động được tính bằng `audioread`, `wavedec`, `var(...,1)` trong Mask Initialization từ noise profile của chính noisy; không cố định chung cho mọi đầu vào, nhưng chưa được dựng thành các block ước lượng nhiễu. InitFcn/StopFcn nhúng xử lý cấu hình, thời gian chạy và cắt latency khi xuất WAV. FIR streaming có điều kiện biên và khởi động khác `Mode='sym'`/`movmean` co cửa sổ ở biên; saturation điểm cũng khác chuẩn hóa chống clipping toàn bản ghi. Vì vậy không được gọi model này là bản sao thuật toán chuẩn 100% hoặc hoàn toàn không có code nhúng. Chuyển phần còn lại sang kiến trúc toàn bản ghi/native blocks cần một bước thiết kế tiếp theo.

### Cách chạy mô hình Simulink

**Bản thu điện thoại, không có clean:** chuyển M4A sang WAV mono bằng công cụ của bạn, mở `.slx`, chọn WAV ở `Âm thanh có nhiễu`, đặt `Use_Clean=0` rồi Run. Giữ thời gian/solver thủ công hoặc tự bật `Auto_Run=1` để tính đủ thời gian xuất file. Không cần chạy `.m` bên ngoài. Chỉnh `Noise_Sec` theo đoạn chỉ có tiếng ồn: nên bắt đầu ghi âm tiếng nền trước khi nói. Nếu đoạn này có lời nói, bộ lọc có thể làm mất lời. Noise profile phải đủ ít nhất 1024 mẫu và không vượt độ dài file. Xuất WAV giữ Fs và số mẫu; chưa hỗ trợ stereo tự động hay kiểm chứng giải mã M4A trực tiếp.

**Bỏ đoạn khởi động giả khi lấy mẫu nhiễu:** với `Noise_Auto=1`, mặc định `Noise_Start_Auto=1` tìm chuỗi mẫu đầu file có giá trị bằng nhau chính xác (bằng 0 hoặc khác 0). Nếu chuỗi kéo dài ít nhất 10 ms, noise profile bắt đầu ở mẫu thay đổi đầu tiên. Nếu không có chuỗi dài như vậy, vẫn lấy từ đầu file. Đây không phải phát hiện tiếng nói, không tìm một đoạn yên nhất và không bỏ qua đoạn khởi động tăng dần hoặc gần-hằng có dao động.

- `Noise_Sec`: độ dài mẫu nhiễu, mặc định 0.25 giây, tính từ thời điểm bắt đầu đã chọn.
- `Noise_Start_Auto=0`: dùng `Noise_Start` là thời điểm tuyệt đối từ đầu file, đơn vị giây; không cộng thêm thời gian tự bỏ đoạn hằng. Chỉ chọn một đoạn bạn biết là tiếng nền, không có lời.
- Chỉ loại đoạn khởi động khỏi **ước lượng Pn**; không cắt tín hiệu đầu vào, không đổi Fs, không xóa mẫu khỏi WAV đầu ra. DWT–Wiener và cách tính phương sai từng dải không đổi.
- Model báo lỗi rõ nếu bản ghi hoàn toàn hằng, profile hằng/gần-hằng ở mức sai số số học, profile ít hơn 1024 mẫu hoặc vượt cuối file. Không coi năng lượng nhỏ là không hợp lệ và không âm thầm đặt Pn giả.
- Command Window ghi thời điểm/profile thực tế và số mẫu đầu bị bỏ qua. Các tham số này hoạt động trong `.slx` mà không cần gọi script ngoài; logic lựa chọn vẫn nằm trong Mask Initialization như estimator Pn hiện có, không phải native-block estimator mới.

[verified: `results/noise_startup_20261107_172156_039/verification.log`] Bản thu `REC_261107_170257.wav` tự nhận 5150 mẫu đầu hằng; chọn noise profile 0.321875–0.571875 giây (4000 mẫu), xuất đủ 168619 mẫu/16 kHz. Đặt thủ công `Noise_Start_Auto=0`, `Noise_Start=0.321875` cho output giống hệt auto. Ca đối chứng không có đoạn hằng cho WAV và tám Pn giống hệt trước khi sửa. Đã thử prefix bằng 0/khác 0, các lỗi profile, chạy SLX ngoài project path và giữ layout hiện có. Output thử thực tế: `results/noise_startup_20261107_172156_039/REC_261107_170257_auto_denoised.wav`. Chưa xác nhận profile sau khi mic mở là chỉ có tiếng quạt, chưa nghe đánh giá; không dùng mức thay đổi dạng sóng làm bằng chứng tăng SNR hay giữ lời.

Tiện ích tùy chọn nếu muốn gọi bằng lệnh:
```matlab
[am_thanh, tep_wav, bang] = chay_mo_hinh_simulink('ban_thu.wav');
```
Chỉ truyền noisy hoặc truyền clean rỗng thì chạy không cần clean; bảng trả về SNR/RMSE là `NaN`. Có thể nghe noisy và WAV denoised bằng trình phát bên ngoài; model không có công tắc nghe thử.

**Cách 1: Chạy và xem bảng chỉ số đánh giá từ MATLAB:**
```matlab
[am_thanh, tep_wav, bang] = chay_mo_hinh_simulink( ...
    'data_DSP_speech/noisy/pink_10db/speech_001_pink_10dB.wav', ...
    'data_DSP_speech/clean/speech_001.wav');
```

**Cách 2: Mở giao diện Simulink để quan sát và điều chỉnh:**
```matlab
open_system('mo_hinh_dwt_wiener');
```
Sau đó chọn file và tham số trên model rồi bấm **Run**. Không cần `khoi_tao_mo_hinh_dwt`. `chay_mo_hinh_simulink()` không đối số dùng thiết lập hiện tại; truyền noisy và clean thì bật đánh giá tham chiếu. `tao_mo_hinh_simulink` chỉ dùng dựng lại model và có thể thay tên/layout chỉnh tay, không cần gọi mỗi lần mô phỏng.

[verified: `results/optional_clean_20261107_165835_313/verification.log`] Chế độ không clean cho WAV giống hệt baseline và chế độ có clean trên cùng noisy (48000 mẫu). Chạy thành công WAV tên tùy ý với đường dẫn clean không tồn tại: 144000 mẫu/48 kHz và 79000 mẫu/16 kHz; kiểm tra giữ nguyên Fs/số mẫu và các chỉ số không khả dụng. Saved SLX chạy được khi bỏ project khỏi MATLAB path. Đây là WAV thử từ bộ dữ liệu/resample, chưa phải bản thu điện thoại thực tế; chưa nghe thử hay xác nhận GUI.

### Đối chứng đã đo

[verified: `results/repair_20261107_154901/verification.log`] Mô phỏng phần mềm MATLAB R2026b, bốn cặp thực tế, cùng tham số mặc định với file chuẩn không sửa:

| Input | Số mẫu WAV ra | SNR In (dB, làm tròn) | SNR Out Simulink | SNR Out MATLAB |
| --- | ---: | ---: | ---: | ---: |
| pink 001 | 48000 | 10.000000 | 14.765507 | 14.739750 |
| pink 002 | 79000 | 10.000000 | 14.094206 | 14.100578 |
| white 001 | 48000 | 10.000000 | 17.230351 | 17.239668 |
| white 002 | 79000 | 10.000000 | 16.517267 | 16.510340 |

File `metrics.csv` cùng thư mục chứa cả sai số phổ biên độ so với clean. SNR Out gần chuẩn nhưng sai số dạng sóng tương đối so với output chuẩn vẫn khoảng 2.64–7.63%; không suy ra tương đương chỉ từ SNR. `results/input_snr_audit.json` kiểm tra SNR đầu vào toàn bộ 60 cặp pink/white 10 dB. Chưa nghe thử hay xác nhận hiển thị cửa sổ Analyzer bằng GUI.

---

## 3. Bản đối chứng STFT + Wiener (`khu_nhieu_stft_wiener.m`)

Bản này độc lập, không thay thế chương trình DWT và không dùng wavelet. Chạy `khu_nhieu_stft_wiener` để chọn WAV, hoặc:

```matlab
[am_thanh, tep_wav, bang] = khu_nhieu_stft_wiener( ...
    'data_DSP_speech/noisy/pink_10db/speech_012_pink_10dB.wav', ...
    'data_DSP_speech/clean/speech_012.wav', 0.25, true);
```

Cần Signal Processing Toolbox; đã kiểm tra trên MATLAB R2026b. Dùng cửa sổ Hann khoảng 32 ms, bước khoảng 8 ms (làm tròn theo Fs), phổ hai phía và tái tạo WOLA. Công suất nhiễu được đo riêng từng ô tần số từ đoạn đầu, không dùng clean. Công suất được làm mượt 64 ms, gain 40 ms, sàn gain 0.05. Giữ bước giảm đuôi và chống clipping như DWT; tự kiểm tra STFT/ISTFT không lọc và giữ nguyên số mẫu đầu ra.

---

## 4. Nội dung được giữ

Mã `.m` được chia vào `matlab_base/` và `simulink_dsp/`. File mô hình `.slx` nằm trong `simulink_dsp/`;
dữ liệu và `results/` ở gốc dự án. `startup.m` đã chuyển vào
`matlab_base/`; nếu cần chạy phần khởi tạo cũ, gọi
`run(fullfile('matlab_base', 'startup.m'))` từ gốc dự án.

- `matlab_base/khu_nhieu_am_thanh_core.m`: hàm lõi MATLAB dùng cho đối chứng.
- `matlab_base/thu_nghiem_khoi_1s.m`: so sánh bản offline và xử lý theo khối.
- `matlab_base/khu_nhieu_am_thanh.m`: file chạy chính DWT + Wiener độc lập, chú thích và biến tiếng Việt.
- `simulink_dsp/khu_nhieu_dwt_core.m`: hàm DSP chuẩn phục vụ mô hình Simulink và chạy kiểm nghiệm.
- `simulink_dsp/mo_hinh_dwt_wiener.slx`: mô hình mô phỏng Simulink trực quan.
- `simulink_dsp/khoi_tao_mo_hinh_dwt.m`: script khởi tạo tham số mô hình Simulink.
- `simulink_dsp/tao_mo_hinh_simulink.m`: script tạo/cập nhật cấu trúc sơ đồ khối `.slx`.
- `simulink_dsp/chay_mo_hinh_simulink.m`: script điều khiển chạy mô phỏng và xuất đồ thị/bảng chỉ số.
- `data_DSP_speech/`: dữ liệu gốc, giữ nguyên toàn bộ.
- `matlab_simulink_dsp-1.0.0/`: bốn file tham khảo DWT/IDWT và threshold theo dải, kèm README/giấy phép.
- `TONG_HOP_NEN_TANG_DU_AN_WAVELET.md`: bản tổng hợp kiến thức giai đoạn đầu; không phải mô tả cập nhật đầy đủ của thuật toán cuối.
- PDF đề cương nhóm: giữ nguyên.
- `results/`: kết quả chạy; dữ liệu người dùng có sẵn không bị xóa.

---

## 5. File cũ và khôi phục

Đã bỏ hỗ trợ GNU Octave. Thư mục tương thích cũ được chuyển nguyên vẹn vào `.trash/bo_octave_20261030/octave_compat/` để có thể khôi phục; không gỡ phần mềm Octave khỏi máy. Không thêm thư mục này vào MATLAB path.
