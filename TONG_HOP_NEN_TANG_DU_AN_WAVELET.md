# Tổng hợp nền tảng dự án khử nhiễu tiếng nói bằng Wavelet

**Ngày tổng hợp:** 24/10/2026.  
**Giai đoạn:** tìm hiểu nền tảng, thống nhất hướng làm và chuẩn bị mô phỏng; chưa phải báo cáo kết quả khử nhiễu hoàn chỉnh.  
**Phạm vi:** tài liệu nhóm cung cấp, mã MATLAB tham khảo và dữ liệu WAV hiện có trong thư mục dự án.

**Cập nhật sau dọn thư mục:** Đây là bản tổng hợp nền tảng giai đoạn đầu. Code hiện tại là `matlab_base/khu_nhieu_am_thanh.m` (MODWT `coif5`, level 7), xem `README.md` để chạy. Bộ DSP tham khảo đã rút gọn; file cũ không còn ở thư mục làm việc được lưu trong `.trash/don_dep_20261024/` theo đường dẫn tương ứng.

## 1. Phạm vi tổng hợp

Tài liệu phân biệt hiện trạng dự án, kiến thức nền tảng, đề xuất triển khai và những vấn đề nhóm cần tiếp tục xác nhận. Các đề xuất không được xem là quyết định cuối cùng của nhóm.

Nguồn tham khảo được liệt kê ở cuối tài liệu. Với PDF, số trang là số trang của file, không phải số mục.

## 2. Nhóm đang làm bài toán gì?

Đề tài là nghiên cứu biến đổi Wavelet trong xử lý tín hiệu, ứng dụng **khử nhiễu tiếng nói thu bằng microphone**. Nhóm có bốn người; P4 thực hiện mô phỏng trong MATLAB.

Chuỗi xử lý trong bản đề cương:

```text
File WAV hoặc microphone
        ↓
Tiền xử lý và chuẩn bị tín hiệu có nhiễu              P1
        ↓
DWT: phân rã tín hiệu thành các hệ số Wavelet         P2
        ↓
Ước lượng nhiễu và thresholding các hệ số             P3
        ↓
IDWT: tái tạo tín hiệu từ hệ số sau thresholding      P4
        ↓
Đánh giá, nghe so sánh, hiển thị và demo              P4
```

Có thể hiểu ngắn gọn: **P2 đổi cách biểu diễn tín hiệu; P3 xử lý nhiễu trong biểu diễn đó; P4 đưa kết quả về âm thanh và kiểm tra chất lượng.** Chỉ thực hiện DWT rồi IDWT mà không thay đổi hệ số không phải là một thuật toán khử nhiễu.

### 2.1. Những điểm đã xuất hiện trong thảo luận

- Các chỉ số: MSE, RMSE, SNR, STOI, PESQ, SI-SDR.
- Danh sách Wavelet đang nhắc đến: `haar`, `db4`, `db6`, `sym4`, `coif5`.
- Nhiễu trắng và nhiễu hồng.
- DWT, IDWT, mức phân rã và hệ số phân rã.
- Nhóm đang cùng tìm hiểu nền tảng, chưa ở giai đoạn chốt kết quả.

### 2.2. Những điểm chưa được phép coi là quyết định cuối cùng

- PDF đề xuất `db4`, `sym8`, `coif5`; thảo luận mới nhắc `haar`, `db4`, `db6`, `sym4`, `coif5`. **`sym4` và `sym8` là hai tên khác nhau; không tự động thay thế cho nhau.**
- PDF dự kiến triển khai bằng Python và Streamlit; P4 đang xác định hướng MATLAB. Chưa có xác nhận rằng cả nhóm đã chuyển toàn bộ hệ thống sang MATLAB.
- PDF đề xuất khảo sát mức phân rã 3, 4, 5 và SNR đầu vào 0, 5, 10, 15 dB. Đó là kế hoạch, không phải bằng chứng đã chạy đủ các trường hợp.
- PDF tập trung SNR, cải thiện SNR và RMSE. MSE, STOI, PESQ, SI-SDR là các chỉ số được bổ sung trong trao đổi, chưa có kết quả tính trong tài liệu đã đọc.

## 3. Thư mục hiện có những gì?

### 3.1. Bản đề cương PDF

File **“cùng kỹ thuật wavelet cung cấp vài ứng dụng khác nhau cho xử lý âm thanh.pdf”** có 22 trang, gồm:

- Mục tiêu đề tài và chuỗi xử lý.
- Phân công P1–P4, đầu ra module, phần báo cáo và slide.
- DWT nhiều mức, ước lượng nhiễu MAD, VisuShrink, BayesShrink, hard/soft thresholding.
- Hai nhóm thử nghiệm: dữ liệu có tham chiếu sạch và microphone thực tế không có tham chiếu sạch.
- Kế hoạch ba tuần, giao diện dự kiến và cấu trúc chương trình Python.

Đây là **đề cương/hướng triển khai tham khảo**, không phải tài liệu chứng minh thuật toán đã chạy hoặc các cấu hình đã tối ưu.

### 3.2. Bộ mã MATLAB tham khảo

Thư mục `matlab_simulink_dsp-1.0.0/` là bộ ví dụ DSP với nhiều chủ đề, không chỉ riêng khử nhiễu tiếng nói.

Các file liên quan trực tiếp:

- `ds10_04_03_DWTFB.m:1`: ví dụ DWT/IDWT một chiều, chọn `haar`, phân rã ba mức, so sánh tái tạo bằng filter bank và hàm Wavelet.
- `ds10_04_03_DWTFB.m:16`: gọi `[app_detail,lengths_of_ad]=wavedec(xn,M,wname)`.
- `ds10_04_03_DWTFB.m:31`: gọi `xh1 = waverec(app_detail,lengths_of_ad,wname)`.
- `DWT_filterbank.m:1`: ví dụ ngân hàng lọc phân tích/tổng hợp nhiều mức.
- `DWT_dec.m:1`: dựng các thành phần approximation/detail về độ dài tín hiệu gốc để quan sát.
- `dyadic_filterbank.m:18`: soft threshold riêng từng dải; giữ để tham khảo phần ngưỡng. Các ví dụ hai chiều dành cho ảnh đã được cất khỏi thư mục làm việc.

Chưa chạy các ví dụ này trong MATLAB, chưa xác nhận phiên bản MATLAB hoặc toolbox trên máy. Có mã tham khảo không đồng nghĩa đã có chương trình khử nhiễu của nhóm.

### 3.3. Bộ dữ liệu âm thanh

- `data_DSP_speech/clean/`: 30 file, đặt tên từ `speech_001.wav` đến `speech_030.wav`.
- `data_DSP_speech/noisy/white_10db/`: 30 file, ví dụ `speech_001_white_10dB.wav`.
- `data_DSP_speech/noisy/pink_10db/`: 30 file, ví dụ `speech_001_pink_10dB.wav`.
- Tất cả đều mono, tần số lấy mẫu 16.000 Hz.
- Thời lượng từ 2,46875 đến 5,375 giây.
- File trong `clean/` là PCM số nguyên 16 bit; các file trong hai thư mục `noisy/` là WAV số thực IEEE float 32 bit.
- Sau khi ghép theo mã `speech_XXX`, mỗi nhóm noisy đều có đủ 30 cặp tương ứng với clean; các cặp có cùng tần số lấy mẫu và số mẫu.

**Kiểm tra SNR thực tế, không chỉ tin vào tên thư mục:**

Đọc PCM sạch về thang số thực bằng cách chia cho 32768, giữ nguyên mẫu WAV float, rồi tính sai số `noisy − clean` trên toàn file:

- Nhóm `white_10db`: SNR nhỏ nhất/trung bình/lớn nhất đều là **10,000000 dB**, khi làm tròn sáu chữ số thập phân.
- Nhóm `pink_10db`: kết quả tương tự, **10,000000 dB** khi làm tròn sáu chữ số thập phân.
- RMSE trung bình giữa noisy và clean của mỗi nhóm là **0,014151148** khi làm tròn chín chữ số thập phân. Đây là **sai số trước khử nhiễu**, không phải thành tích thuật toán.
- Biên độ tuyệt đối lớn nhất quan sát được nhỏ hơn 1 ở cả ba nhóm: clean **0,593322754**; white **0,613206208**; pink **0,599427283**.

Chưa xác minh nguồn gốc bản ghi, nội dung/ngôn ngữ tiếng nói, mã sinh nhiễu, seed ngẫu nhiên hoặc đặc tính phổ của từng nhiễu. Tên `white`/`pink` là nhãn dữ liệu; phép đo SNR không đủ để chứng minh nhiễu đúng loại. `clean` là nhãn tham chiếu của bộ dữ liệu, chưa phải bằng chứng bản ghi hoàn toàn không có nhiễu.

## 4. Phân công và ranh giới giữa bốn người

### P1 — Tiền xử lý tín hiệu và mô phỏng nhiễu

- Đọc WAV hoặc thu microphone.
- Xử lý mono/stereo, tần số lấy mẫu, biên độ và clipping.
- Chuẩn bị clean, noise, noisy; trộn nhiễu theo SNR yêu cầu.
- Bàn giao tín hiệu cùng thông tin lấy mẫu và kịch bản nhiễu.

### P2 — Biến đổi Wavelet và phân rã tín hiệu

- Tìm hiểu Wavelet, DWT, ngân hàng lọc và phân tích đa phân giải.
- Phân rã tín hiệu thành approximation/detail ở các mức.
- Khảo sát lựa chọn Wavelet và mức phân rã.
- Bàn giao hệ số cùng metadata đủ để P3 xử lý và P4 tái tạo.

### P3 — Ước lượng nhiễu và Wavelet thresholding

- Ước lượng độ lệch chuẩn nhiễu từ hệ số.
- Xác định ngưỡng theo phương pháp được chọn.
- Áp dụng hard hoặc soft thresholding.
- Bàn giao hệ số đã xử lý, phương pháp ngưỡng và các tham số liên quan.

### P4 — Tái tạo tín hiệu, đánh giá, tích hợp và demo

- IDWT từ hệ số sau thresholding.
- Kiểm tra chiều dài, căn chỉnh và biên độ đầu ra.
- Tính chỉ số trên dữ liệu có tham chiếu sạch.
- Vẽ waveform/spectrogram, nghe so sánh, lưu kết quả.
- Ghép module và chuẩn bị demo; viết phần tái tạo, thực nghiệm, đánh giá.

PDF vừa giao P2 tìm hiểu hàm tái tạo vừa giao P4 phụ trách tái tạo trong hệ thống. Cách tránh làm trùng: P2 và P4 cùng xác nhận cặp phân rã–tái tạo đúng; P4 sử dụng giao diện đã thống nhất để xử lý đầu ra của P3. P4 không cần tự viết lại thuật toán của cả P1–P3.

## 5. Các ký hiệu nên thống nhất

- `x[n]`: tín hiệu sạch/tham chiếu.
- `v[n]`: nhiễu được cộng vào.
- `y[n] = x[n] + v[n]`: tín hiệu có nhiễu.
- `x_hat[n]`: tín hiệu sau khử nhiễu.
- `Fs`: tần số lấy mẫu, đơn vị Hz.
- `N`: số mẫu của đoạn tín hiệu đang đánh giá.
- `J`: mức phân rã DWT.
- `cA_j`, `cD_j`: các hệ số approximation và detail ở mức `j`.
- `lambda`: ngưỡng thresholding; `sigma`: độ lệch chuẩn nhiễu ước lượng.

**Không dùng lẫn “mức phân rã” và “hệ số phân rã”:**

- Mức phân rã là số tầng, ví dụ 3, 4, 5.
- Hệ số phân rã là dữ liệu thu được sau biến đổi: các giá trị trong `cA`, `cD` hoặc biểu diễn hệ số tương ứng.
- Hệ số bộ lọc là một khái niệm khác: các tap của bộ lọc phân tích và tổng hợp.

## 6. DWT, mức phân rã và IDWT

### 6.1. DWT một mức và nhiều mức

Ở mỗi tầng, tín hiệu được phân tích qua nhánh thông thấp và thông cao, rồi giảm mẫu. Kết quả là approximation và detail. DWT nhiều mức tiếp tục phân rã nhánh approximation:

```text
y → (cA1, cD1)
cA1 → (cA2, cD2)
cA2 → (cA3, cD3)
```

Với ba mức, dữ liệu cần cho tái tạo gồm approximation cuối `cA3` và các detail `cD3`, `cD2`, `cD1`, kèm thông tin cấu trúc/chiều dài và cấu hình biến đổi. Không chỉ đưa riêng một dãy detail cho P4.

### 6.2. Hiểu dải tần ở từng mức

Có thể hình dung các dải lý tưởng gần đúng:

- `D1`: 4.000–8.000 Hz.
- `D2`: 2.000–4.000 Hz.
- `D3`: 1.000–2.000 Hz.
- `D4`: 500–1.000 Hz.
- `D5`: 250–500 Hz.
- Nếu phân rã năm mức, `A5` tương ứng vùng xấp xỉ 0–250 Hz.

Các khoảng trên giúp hiểu nguyên lý; không phải ranh giới cắt lý tưởng của bộ lọc thực. **Mức 1 có detail ở dải cao nhất; mức phân rã sâu hơn ứng với dải thấp hơn.** Cụm “detail mức cao nhất” trong PDF dễ gây nhầm: công thức MAD tại đó đang dùng `cD1`, không phải detail ở mức sâu nhất. 

### 6.3. IDWT làm gì?

IDWT dùng các hệ số và bộ lọc tổng hợp để đưa tín hiệu trở lại miền thời gian. Trong pipeline, đầu vào là hệ số **đã qua thresholding**, không phải mặc định dùng lại hệ số ban đầu.

Trước khi đánh giá khử nhiễu, cần kiểm tra phép biến đổi khứ hồi:

```text
Tín hiệu → DWT → giữ nguyên hệ số → IDWT → so sánh với tín hiệu ban đầu
```

Nếu phép khứ hồi còn sai đáng kể hoặc lệch thời gian thì chưa thể quy lỗi chất lượng cho thresholding. P2/P4 cần thống nhất Wavelet, mức, xử lý biên, cấu trúc hệ số và chiều dài gốc.

## 7. Wavelet và thresholding đang được tìm hiểu

### 7.1. Danh sách Wavelet

Năm lựa chọn đang được nhắc là `haar`, `db4`, `db6`, `sym4`, `coif5`.

Nên gọi đây là **năm lựa chọn Wavelet đang khảo sát**, chưa gọi là năm cấu hình tối ưu. Với tên gọi chuẩn của các lựa chọn, `db4` và `db6` thuộc cùng nhóm Daubechies; không nên hiểu mỗi tên là một họ độc lập. Chưa có số liệu trong dự án để kết luận lựa chọn nào tốt nhất.

Đề cương có ý định chọn một Wavelet và một mức phù hợp trước khi so sánh các phương pháp thresholding chính. Đây là cách giảm số yếu tố thay đổi cùng lúc; khi so sánh Wavelet nên giữ dữ liệu, mức nhiễu và quy tắc ngưỡng nhất quán.

### 7.2. Ước lượng nhiễu và chọn ngưỡng

PDF đề xuất:

$$
\hat{\sigma} = \frac{\operatorname{median}(|cD_1|)}{0.6745}
$$

và ngưỡng Universal/VisuShrink:

$$
\lambda = \hat{\sigma}\sqrt{2\ln N}.
$$

Đây là công thức đang được đề cương dùng, không phải bằng chứng đã kiểm chứng cho mọi loại nhiễu. P3 cần ghi rõ `N` là số mẫu toàn tín hiệu hay số hệ số của dải đang xét, cách xử lý từng mức, và có áp dụng ngưỡng lên approximation hay không. Không tự động coi một ước lượng từ `cD1` là đủ cho cả nhiễu trắng lẫn nhiễu hồng.

BayesShrink được nêu như phương pháp chọn ngưỡng dựa trên phương sai tín hiệu và nhiễu. Tài liệu chưa cung cấp triển khai cụ thể hoặc kết quả so sánh BayesShrink của nhóm.

### 7.3. Hard và soft thresholding

Với hệ số `w` và ngưỡng `lambda`:

$$
\hat{w}_{hard}=\begin{cases}
w,& |w|\ge\lambda,\\
0,& |w|<\lambda.
\end{cases}
$$

$$
\hat{w}_{soft}=\operatorname{sign}(w)\max(|w|-\lambda,0).
$$

Hard giữ nguyên hệ số qua ngưỡng; soft còn co biên độ của chúng. Chọn **cách tính ngưỡng** và chọn **hàm hard/soft** là hai việc khác nhau. Giảm nhiều hệ số không mặc nhiên tốt: có thể giảm cả thành phần tiếng nói cần giữ.

Bốn tổ hợp dự kiến là VisuShrink + Hard, VisuShrink + Soft, BayesShrink + Hard và BayesShrink + Soft. SureShrink/Smooth Thresholding chỉ được nêu là phần mở rộng.

## 8. Các chỉ số đánh giá: hiểu và dùng đúng

### 8.1. MSE và RMSE

$$
MSE=\frac{1}{N}\sum_{n=0}^{N-1}(x[n]-\hat{x}[n])^2,
\qquad RMSE=\sqrt{MSE}.
$$

- Trong cùng điều kiện đánh giá, nhỏ hơn nghĩa là sai số theo mẫu thấp hơn. RMSE có cùng đơn vị biên độ với tín hiệu; MSE có đơn vị biên độ bình phương.
- Trên cùng một cặp tín hiệu, MSE và RMSE không phải hai bằng chứng độc lập vì liên hệ qua căn bậc hai. Khi tổng hợp nhiều file, trung bình RMSE không nhất thiết bằng căn của trung bình MSE.
- Đổi thang biên độ sẽ đổi các sai số này; phải thống nhất cách đọc PCM/float và chuẩn hóa trước khi so sánh.

### 8.2. SNR đầu vào, đầu ra và cải thiện SNR

Với cùng tín hiệu sạch tham chiếu:

$$
SNR_{in}=10\log_{10}\frac{\sum x[n]^2}{\sum(x[n]-y[n])^2},
$$

$$
SNR_{out}=10\log_{10}\frac{\sum x[n]^2}{\sum(x[n]-\hat{x}[n])^2},
\qquad \Delta SNR=SNR_{out}-SNR_{in}.
$$

Đơn vị là dB. `Delta SNR > 0` nghĩa là sai số tổng thể so với tham chiếu giảm theo định nghĩa trên. Sai số đầu ra gồm cả nhiễu còn lại và méo tiếng nói do xử lý; không nên gọi toàn bộ mẫu sai số là nhiễu môi trường nguyên bản.

Nếu cùng tham chiếu, chiều dài và thang biên độ, gọi `Px = sum(x²)/N` thì `SNR_out = 10 log10(Px/MSE)`. Vì thế tăng SNR và giảm MSE ở điều kiện này có liên hệ toán học, không phải hai kiểm chứng độc lập về cảm nhận nghe.

### 8.3. STOI — khả năng hiểu lời nói

STOI là **Short-Time Objective Intelligibility**, được xây dựng để dự đoán khả năng hiểu tiếng nói bị nhiễu hoặc xử lý theo thời gian–tần số; đánh giá dựa trên tiếng nói sạch và tiếng nói bị suy giảm.

STOI trả lời câu hỏi khác với MSE: “nội dung lời nói còn dễ hiểu không?”. Không diễn giải một giá trị STOI thành tỷ lệ phần trăm từ mà người nghe chắc chắn nhận đúng, và không dùng nó thay thế hoàn toàn thử nghe.

Chưa xác nhận hàm/triển khai STOI, phiên bản, yêu cầu đầu vào hoặc kết quả chạy trong MATLAB của nhóm.

### 8.4. PESQ — chất lượng tiếng nói cảm nhận

PESQ là **Perceptual Evaluation of Speech Quality**. Tiêu chuẩn P.862 nói về đánh giá khách quan chất lượng tiếng nói đầu cuối của mạng thoại băng hẹp và codec. Trang ITU ghi P.862 cùng các khuyến nghị liên quan P.862.1/.2/.3 đã lỗi thời và bị rút ngày 05/01/2024, hướng người đọc sang P.863 và các phần liên quan.

Nếu dùng PESQ để đối chiếu nghiên cứu trước, cần ghi rõ biến thể, triển khai, chế độ và cách chuẩn bị dữ liệu. Không tự đổi tên PESQ thành một chỉ số khác hoặc khẳng định đây là tiêu chuẩn ITU mới nhất. Không diễn giải PESQ như “độ chính xác” hoặc tỷ lệ từ nghe được.

Chưa kiểm tra triển khai PESQ, điều kiện sử dụng/bản quyền, chế độ áp dụng và khả năng chạy với dữ liệu của nhóm. Không ghi giá trị mẫu như thể đã tính thật.

### 8.5. SI-SDR — tỷ lệ tín hiệu/méo không phụ thuộc gain tổng thể

SI-SDR là **Scale-Invariant Signal-to-Distortion Ratio**. Theo định nghĩa phép chiếu, với tín hiệu tham chiếu `x` và ước lượng `x_hat`:

$$
\alpha=\frac{\langle\hat{x},x\rangle}{\|x\|^2},
\qquad s_{target}=\alpha x,
\qquad e=\hat{x}-s_{target},
$$

$$
SI\text{-}SDR=10\log_{10}\frac{\|s_{target}\|^2}{\|e\|^2}.
$$

Điểm khác quan trọng với SNR ở trên là phép cho phép điều chỉnh một hệ số gain toàn cục khi so sánh. Chỉ số không có nghĩa là bỏ qua mọi loại méo hoặc lệch thời gian. Nhóm cần thống nhất có bỏ giá trị trung bình trước khi tính hay không và cách xử lý trường hợp suy biến.

### 8.6. Quy tắc đánh giá chung

1. Cùng nội dung, tần số lấy mẫu, kênh, số mẫu và căn chỉnh thời gian.
2. Không tự chuẩn hóa riêng clean/noisy/denoised trước khi tính SNR/MSE/RMSE, vì có thể làm thay đổi bài toán so sánh.
3. Giữ bản tín hiệu tái tạo gốc để tính chỉ số; tạo bản phát/lưu riêng nếu cần gain chống clipping. Ghi rõ khi đánh giá cả bước hậu xử lý.
4. Với các công thức trên, nếu năng lượng tham chiếu bằng 0 hoặc mẫu số bằng 0, cần quy ước kết quả đặc biệt. Không âm thầm cộng một hằng số rồi trình bày như giá trị chính xác.
5. Với STOI/PESQ, tuân theo chuẩn bị đầu vào của triển khai đã chọn, không suy ra API từ tên chỉ số.
6. Chạy trên nhiều file, lưu kết quả từng file trước khi tính trung bình. Một câu nói đẹp không chứng minh toàn hệ thống tốt.
7. Chưa có clean đúng cặp thì không có cơ sở tính trực tiếp các chỉ số tham chiếu trên như “ground truth”.

Giai đoạn đầu nên làm chắc SNR, cải thiện SNR, MSE/RMSE và nghe/đồ thị. STOI, PESQ, SI-SDR là lớp đánh giá bổ sung sau khi đã xác minh triển khai; đây là **đề xuất ưu tiên**, chưa phải giới hạn bắt buộc của đề tài.

## 9. Nhiễu trắng, nhiễu hồng và dữ liệu microphone

Đề cương phân biệt AWGN, pink noise và nhiễu thực tế như quạt, xe, môi trường.

- **Nhiễu trắng:** mô hình có mật độ phổ công suất phẳng theo tần số. “Trắng” mô tả phổ, còn “Gaussian” mô tả phân bố; không nên xem hai từ là đồng nghĩa.
- **Nhiễu hồng:** mô hình mật độ phổ công suất tỷ lệ gần `1/f` trong dải xét; tương đối nhiều năng lượng ở tần số thấp hơn nhiễu trắng khi so về hình dạng phổ.
- **Cùng SNR không đồng nghĩa cùng tác động lên tiếng nói:** năng lượng nhiễu có thể phân bố khác nhau giữa các dải Wavelet.

Hai nhóm noisy hiện có đều đo được khoảng 10 dB. Chưa kiểm tra PSD để xác nhận nhãn phổ white/pink; chưa thấy bộ dữ liệu 0, 5, 15 dB hoặc dữ liệu môi trường thực trong các tài liệu/dữ liệu đã kiểm tra.

### Hai kiểu thử nghiệm cần tách riêng

- **Có clean tham chiếu:** tạo noisy từ clean để đánh giá định lượng. Bộ dữ liệu ghép cặp hiện tại phù hợp để bắt đầu kiểm tra các sai số theo mẫu.
- **Microphone thực tế:** thường chỉ có bản thu chứa cả tiếng nói và nhiễu. Đề cương tập trung nghe trước/sau, waveform, spectrogram.
- Một người đọc lại cùng câu trong phòng yên tĩnh không tạo ra tham chiếu khớp mẫu với bản thu nhiễu trước đó. Không ghép hai bản thu khác nhau rồi tính SNR/RMSE như cùng một tín hiệu gốc.
- Tín hiệu nhỏ hơn hoặc spectrogram tối hơn chưa đủ chứng minh khử nhiễu tốt: có thể hệ thống chỉ giảm âm lượng hoặc làm mất tiếng nói.

## 10. P4 nên bắt đầu như thế nào trong MATLAB?

### 10.1. Việc làm được ngay, không cần đợi cả hệ thống

1. Chọn một cặp `speech_XXX` clean/noisy, đọc đúng định dạng và kiểm tra Fs, số mẫu, biên độ.
2. Tính SNR đầu vào, MSE/RMSE trước xử lý để có baseline. Kết quả SNR trên bộ hiện tại phải gần 10 dB.
3. Vẽ clean/noisy theo cùng trục thời gian và cùng cách đặt thang; nghe so sánh.
4. Phối hợp P2 kiểm tra DWT → IDWT giữ nguyên hệ số. Chưa gắn bước ngưỡng ở phép thử này.
5. Khi có đầu ra P3, tái tạo và tính cùng bộ chỉ số sau xử lý.
6. Chạy tiếp trên toàn bộ cặp dữ liệu, lưu tham số cùng kết quả từng file.
7. Chỉ sau khi đường xử lý chạy đúng mới ghép giao diện demo.

Mã tham khảo đã có lời gọi phân rã nhiều mức `wavedec` và tái tạo nhiều mức `waverec`. Đây là cách gọi quan sát trong file tham khảo, chưa xác nhận tương thích với MATLAB/toolbox đang dùng. Tài liệu này không cung cấp đoạn code với cam kết đã chạy.

### 10.2. Hợp đồng bàn giao tối thiểu

- P1 → các phần còn lại: clean nếu có, noisy, Fs, mã file, loại nhiễu, SNR mục tiêu và đo được, thao tác tiền xử lý.
- P2 → P3/P4: hệ số, cấu trúc/chiều dài hệ số, Wavelet, mức phân rã, quy tắc xử lý biên, chiều dài tín hiệu gốc và quy ước vector hàng/cột.
- P3 → P4: hệ số đã thresholding giữ cấu trúc tương thích; phương pháp ước lượng nhiễu; giá trị ngưỡng toàn cục hoặc theo mức; hard/soft; dải nào đã xử lý.
- P4 → nhóm: tín hiệu tái tạo chưa đổi gain, bản nghe/lưu nếu khác, kết quả chỉ số, đồ thị và cấu hình tạo ra chúng.

### 10.3. Demo tối thiểu

- Chọn WAV hoặc nguồn microphone nếu chức năng đó đã có.
- Chọn cấu hình Wavelet, mức, phương pháp ngưỡng và hard/soft đã thực sự triển khai.
- Chạy pipeline, nghe noisy/denoised; nghe clean nếu có.
- Xem waveform/spectrogram với thang hiển thị đủ nhất quán để so sánh.
- Hiển thị chỉ số tham chiếu khi có clean; nếu không có thì ghi “không có tín hiệu tham chiếu”, không hiện số giả hoặc số 0 thay thế.
- Lưu âm thanh và kết quả đi kèm cấu hình.

Chưa cần dành thời gian làm GUI đẹp trước khi kiểm tra đúng phép tái tạo và chỉ số. Thu rồi xử lý cả đoạn là một demo hợp lệ ở mức prototype; chưa được gọi là xử lý thời gian thực nếu chưa đo độ trễ và xác nhận yêu cầu đó.

## 11. Thiết kế thử nghiệm phù hợp giai đoạn nền

1. **Baseline:** kiểm tra clean/noisy và chỉ số đầu vào trên dữ liệu 10 dB sẵn có.
2. **Đúng phép biến đổi:** kiểm tra tái tạo không ngưỡng, bao gồm chiều dài và căn chỉnh.
3. **Một cấu hình xuyên suốt:** ghép đủ P1 → P2 → P3 → P4 trước khi quét nhiều lựa chọn.
4. **Khảo sát có kiểm soát:** thay từng yếu tố Wavelet/mức/ngưỡng; giữ nguyên các yếu tố còn lại và cùng noisy cho các cấu hình được so sánh.
5. **Tổng hợp:** lưu chỉ số từng file rồi tính thống kê theo cùng quy tắc; không chỉ chọn file tốt nhất.
6. **Mở rộng:** thêm mức SNR khác và microphone thực tế khi nhóm đã thống nhất yêu cầu.

Nếu dùng cùng một tập file để chọn tham số rồi báo cáo kết quả trên chính tập đó, cần ghi rõ đây là kết quả khảo sát cấu hình, không khẳng định đã chứng minh khả năng tổng quát trên dữ liệu độc lập.

## 12. Những câu hỏi nhóm cần chốt tiếp

- Toàn bộ dự án dùng MATLAB, hay chỉ P4 dùng MATLAB? Nếu khác ngôn ngữ, trao đổi hệ số và metadata bằng định dạng nào?
- Danh sách Wavelet cuối cùng có đúng năm tên đang nêu không? Có giữ `sym8` trong PDF không?
- Mức phân rã nào được khảo sát và cách chọn cấu hình cuối là gì?
- Xử lý biên, căn chỉnh và độ dài tín hiệu được thống nhất ra sao?
- P3 dùng ngưỡng toàn cục hay theo dải, xử lý những hệ số nào, và ước lượng nhiễu thế nào với pink noise?
- Chỉ số bắt buộc gồm những gì? STOI/PESQ/SI-SDR là phần bắt buộc hay mở rộng? Đã có triển khai được kiểm chứng chưa?
- Dữ liệu đến từ đâu, có được phép sử dụng/chia sẻ không, và nhãn white/pink được tạo thế nào?
- Demo là xử lý file, thu rồi xử lý, hay thực sự yêu cầu thời gian thực?

## 13. Kết luận hiện tại

Nhóm đã có đề cương, phân công, bộ mã DSP/Wavelet tham khảo và bộ dữ liệu clean/noisy ghép cặp ở 16 kHz, khoảng 10 dB SNR đầu vào.

Chưa có bằng chứng trong các nguồn đã đọc về pipeline khử nhiễu tiếng nói hoàn chỉnh của nhóm, cấu hình tốt nhất, kết quả sau khử nhiễu, triển khai STOI/PESQ/SI-SDR hoặc GUI MATLAB đã hoạt động. Việc kiểm tra lần này chưa thực hiện DWT/IDWT trong MATLAB, chưa thử nghe và chưa xác minh phổ nhiễu.

Trọng tâm gần nhất của P4 là **xây baseline đánh giá đúng trên dữ liệu có sẵn, thống nhất dữ liệu bàn giao với P2/P3, rồi kiểm tra tái tạo**. Đây là nền cần có trước khi kết luận thuật toán tốt hay làm giao diện hoàn chỉnh.

## 14. Nguồn và phạm vi kiểm chứng

- **S1 — Đề cương nhóm:** [PDF gốc](cùng%20kỹ%20thuật%20wavelet%20cung%20cấp%20vài%20ứng%20dụng%20khác%20nhau%20cho%20xử%20lý%20âm%20thanh.pdf). Đã trích xuất nội dung toàn bộ 22 trang và xem hình trang 10 để đối chiếu phần P4. Những nội dung “dự kiến” vẫn được giữ là kế hoạch.
- **S2 — Mã tham khảo:** [README rút gọn](matlab_simulink_dsp-1.0.0/README.md), [ví dụ DWT/IDWT một chiều](matlab_simulink_dsp-1.0.0/ds10_04_03_DWTFB.m), [filter bank](matlab_simulink_dsp-1.0.0/DWT_filterbank.m), [tái tạo các thành phần](matlab_simulink_dsp-1.0.0/DWT_dec.m), [ngưỡng theo dải](matlab_simulink_dsp-1.0.0/dyadic_filterbank.m). Nội dung tổng hợp ban đầu dựa trên đọc mã; các ví dụ không liên quan đã chuyển sang bản lưu.
- **S3 — Dữ liệu cục bộ:** [thư mục dữ liệu](data_DSP_speech/). Kiểm tra trực tiếp header RIFF/WAV, giải mã mẫu PCM16/float32, kiểm tra cặp file/Fs/chiều dài và tính SNR/RMSE toàn file bằng Python tiêu chuẩn. Các giá trị đầu vào đã đo không phải kết quả thuật toán denoising.
- **S4 — STOI:** C. H. Taal và cộng sự, *An Algorithm for Intelligibility Prediction of Time–Frequency Weighted Noisy Speech*, 2011, DOI `10.1109/TASL.2011.2114881`. [Bản bài báo trên trang tác giả](https://ceestaal.nl/Taal%282011%29.pdf). Dùng để đối chiếu mục đích của STOI, không xác nhận một API MATLAB cụ thể.
- **S5 — PESQ:** [ITU-T P.862](https://www.itu.int/rec/T-REC-P.862/en). Trang chính thức được đối chiếu ngày 24/10/2026, gồm mục đích và tình trạng rút khuyến nghị. Chưa tải/chạy triển khai PESQ.
- **S6 — SI-SDR:** J. Le Roux và cộng sự, *SDR – Half-Baked or Well Done?*, ICASSP 2019. [Bản bài báo](https://www.jonathanleroux.org/pdf/LeRoux2019ICASSP05sdr.pdf). Đối chiếu khái niệm SI-SDR và phép chiếu theo hệ số tỷ lệ; chưa chạy triển khai chỉ số.

**Lưu ý:** Tài liệu này là bản tổng hợp nền tảng tại thời điểm nêu trên. Không tự nâng một ý tưởng trong đề cương hoặc một giải thích hoặc đề xuất thành quyết định đã được toàn nhóm thông qua.
