function [am_thanh_khu_nhieu, tep_ket_qua, bang_chi_so] = khu_nhieu_am_thanh(tep_co_nhieu, tep_tham_chieu, thoi_gian_lay_nhieu, ~)
    % KHỬ NHIỄU PINK/WHITE: DWT coif5, mức 7 + Wiener theo dải.
    % Chạy khu_nhieu_am_thanh để chọn WAV; clean chỉ dùng đánh giá, không dùng lọc.
    % Đầu vào: noisy, clean ('' nếu không có), số giây đầu chỉ có nhiễu (0.25).
    % Đầu ra: mẫu âm thanh, đường dẫn WAV và bảng chỉ số; WAV được lưu vào results.
    % Ví dụ: [am_thanh, tep_wav, bang] = khu_nhieu_am_thanh('noisy.wav', 'clean.wav', 0.25);

    if nargin < 1 || isempty(tep_co_nhieu)
        [ten_tep, thu_muc] = uigetfile('*.wav', 'Chọn WAV có pink hoặc white noise');
        if isequal(ten_tep, 0)
            error('WaveletDSP:Cancelled', 'Đã hủy chọn file noisy.');
        end
        tep_co_nhieu = fullfile(thu_muc, ten_tep);
        if nargin < 2
            [ten_tep, thu_muc] = uigetfile('*.wav', 'Chọn clean cùng bản ghi; Cancel nếu không có');
            tep_tham_chieu = '';
            if ~isequal(ten_tep, 0)
                tep_tham_chieu = fullfile(thu_muc, ten_tep);
            end
        end
    end
    if nargin < 2 && ~exist('tep_tham_chieu', 'var')
        tep_tham_chieu = '';
    end
    if nargin < 3 || isempty(thoi_gian_lay_nhieu)
        thoi_gian_lay_nhieu = 0.25;
    end
    validateattributes(thoi_gian_lay_nhieu, {'numeric'}, {'scalar', 'real', 'finite', 'positive'});
    cac_ham_can_co = {'wavedec', 'waverec', 'spectrogram'};
    for chi_so_ham = 1:numel(cac_ham_can_co)
        if exist(cac_ham_can_co{chi_so_ham}, 'file') ~= 2
            error('WaveletDSP:MissingToolbox', ...
                'Thiếu %s. Cần Wavelet Toolbox và Signal Processing Toolbox.', ...
                cac_ham_can_co{chi_so_ham});
        end
    end

    % Giữ cấu hình nhóm đã chọn; không tự quét Wavelet hoặc mức phân rã.
    wavelet_me = 'coif5';
    muc_phan_ra = 7;
    [am_thanh_co_nhieu, tan_so_lay_mau] = audioread(tep_co_nhieu);
    validateattributes(am_thanh_co_nhieu, {'double'}, {'column', 'real', 'finite', 'nonempty'});
    so_mau = numel(am_thanh_co_nhieu);
    so_mau_nhieu = floor(thoi_gian_lay_nhieu * tan_so_lay_mau);
    if so_mau_nhieu < 1024 || so_mau_nhieu > so_mau
        error('WaveletDSP:InvalidNoiseProfile', ...
            'Noise profile cần ít nhất 1024 mẫu và không vượt độ dài file.');
    end
    if muc_phan_ra > floor(log2(so_mau)) || muc_phan_ra > floor(log2(so_mau_nhieu))
        error('WaveletDSP:LevelTooHigh', 'Tín hiệu và noise profile không đủ dài cho level 7.');
    end
    am_thanh_tham_chieu = [];
    if ~isempty(tep_tham_chieu)
        [am_thanh_tham_chieu, tan_so_tham_chieu] = audioread(tep_tham_chieu);
        validateattributes(am_thanh_tham_chieu, {'double'}, {'column', 'real', 'finite', 'nonempty'});
        if tan_so_tham_chieu ~= tan_so_lay_mau || numel(am_thanh_tham_chieu) ~= so_mau
            error('WaveletDSP:ReferenceMismatch', 'Clean/noisy phải cùng Fs, số mẫu và khớp thời gian.');
        end
        if sum(am_thanh_tham_chieu.^2) == 0
            error('WaveletDSP:SilentReference', 'Clean tham chiếu không được toàn số 0.');
        end
    end
    fprintf('DWT + Wiener %s | level %d\n', wavelet_me, muc_phan_ra);
    fprintf('Giả định %.3f giây đầu chỉ có nhiễu. Không đúng giả định có thể làm mất lời.\n', thoi_gian_lay_nhieu);
    % Biên phản xạ hạn chế gián đoạn ở hai đầu bản ghi.
    [he_so_wavelet, do_dai_cac_dai] = wavedec(am_thanh_co_nhieu, muc_phan_ra, wavelet_me, Mode='sym');
    % Kiểm tra biến đổi thuận/nghịch trước khi can thiệp vào các hệ số.
    am_thanh_tai_tao = waverec(he_so_wavelet, do_dai_cac_dai, wavelet_me, Mode='sym');
    if numel(am_thanh_tai_tao) ~= so_mau || ...
            norm(am_thanh_tai_tao(:) - am_thanh_co_nhieu) / max(norm(am_thanh_co_nhieu), eps) > 1e-6
        error('WaveletDSP:ReconstructionFailure', 'Kiểm tra DWT/IDWT không ngưỡng thất bại.');
    end
    % Biến đổi riêng đoạn chỉ có nhiễu để tiếng nói phía sau không lẫn vào ước lượng.
    [he_so_mau_nhieu, do_dai_cac_dai_nhieu] = wavedec(am_thanh_co_nhieu(1:so_mau_nhieu), muc_phan_ra, wavelet_me, Mode='sym');
    % Cửa sổ 64 ms đo công suất từng dải; gain 40 ms được làm mượt để bớt dao động.
    vi_tri_dau = 1;
    vi_tri_nhieu_dau = 1;
    for chi_so_dai = 1:muc_phan_ra + 1
        muc_dai = min(muc_phan_ra, muc_phan_ra + 2 - chi_so_dai);
        vi_tri_cuoi = vi_tri_dau + do_dai_cac_dai(chi_so_dai) - 1;
        vi_tri_nhieu_cuoi = vi_tri_nhieu_dau + do_dai_cac_dai_nhieu(chi_so_dai) - 1;
        he_so_dai = he_so_wavelet(vi_tri_dau:vi_tri_cuoi);
        cong_suat_nhieu_theo_dai = var(he_so_mau_nhieu(vi_tri_nhieu_dau:vi_tri_nhieu_cuoi), 1);
        tan_so_dai = tan_so_lay_mau / 2^muc_dai;
        cong_suat_cuc_bo = movmean(he_so_dai.^2, max(1, round(0.064 * tan_so_dai)));
        he_so_suy_giam = max(1 - cong_suat_nhieu_theo_dai ./ max(cong_suat_cuc_bo, realmin), 0.05);
        he_so_suy_giam = movmean(he_so_suy_giam, max(1, round(0.040 * tan_so_dai)));
        he_so_wavelet(vi_tri_dau:vi_tri_cuoi) = he_so_dai .* he_so_suy_giam;
        vi_tri_dau = vi_tri_cuoi + 1;
        vi_tri_nhieu_dau = vi_tri_nhieu_cuoi + 1;
    end
    am_thanh_khu_nhieu = waverec(he_so_wavelet, do_dai_cac_dai, wavelet_me, Mode='sym');
    am_thanh_khu_nhieu = am_thanh_khu_nhieu(:);
    validateattributes(am_thanh_khu_nhieu, {'double'}, {'column', 'real', 'finite', 'numel', so_mau});
    % Chỉ hạ gain khi vượt biên 1; không chuẩn hóa riêng clean và noisy.
    he_so_chong_clipping = 1 / max(1, max(abs(am_thanh_khu_nhieu)));
    am_thanh_khu_nhieu = am_thanh_khu_nhieu * he_so_chong_clipping;
    if he_so_chong_clipping < 1
        warning('WaveletDSP:ExportGain', ...
            'Giảm gain toàn tín hiệu %.9g để tránh clipping; chỉ số và đồ thị dùng cùng gain này.', he_so_chong_clipping);
    end

    % Không có tham chiếu thì để NaN, không suy đoán chỉ số từ riêng noisy.
    snr_dau_vao = NaN;
    snr_dau_ra = NaN;
    cai_thien_snr = NaN;
    sai_so_binh_phuong = NaN;
    can_sai_so_binh_phuong = NaN;
    if ~isempty(am_thanh_tham_chieu)
        nang_luong_tham_chieu = sum(am_thanh_tham_chieu.^2);
        snr_dau_vao = 10 * log10(nang_luong_tham_chieu / sum((am_thanh_tham_chieu - am_thanh_co_nhieu).^2));
        snr_dau_ra = 10 * log10(nang_luong_tham_chieu / sum((am_thanh_tham_chieu - am_thanh_khu_nhieu).^2));
        cai_thien_snr = snr_dau_ra - snr_dau_vao;
        sai_so_binh_phuong = mean((am_thanh_tham_chieu - am_thanh_khu_nhieu).^2);
        can_sai_so_binh_phuong = sqrt(sai_so_binh_phuong);
    else
        fprintf('Không có clean: không tính các chỉ số tham chiếu, không tạo clean giả.\n');
    end
    bang_chi_so = table({wavelet_me}, muc_phan_ra, he_so_chong_clipping, snr_dau_vao, snr_dau_ra, cai_thien_snr, sai_so_binh_phuong, can_sai_so_binh_phuong, ...
        'VariableNames', {'Wavelet', 'Level', 'ExportGain', ...
        'SNR_Input_dB', 'SNR_Output_dB', 'Delta_SNR_dB', 'MSE', 'RMSE'});
    disp(bang_chi_so);

    % Thêm số khi trùng tên để giữ nguyên các kết quả cũ.
    thu_muc_ket_qua = fullfile(fileparts(fileparts(mfilename('fullpath'))), 'results');
    if ~isfolder(thu_muc_ket_qua)
        [da_tao_thu_muc, thong_bao] = mkdir(thu_muc_ket_qua);
        if ~da_tao_thu_muc
            error('WaveletDSP:OutputDirectory', 'Không tạo được thư mục kết quả: %s', thong_bao);
        end
    end
    [~, ten_tep_goc] = fileparts(tep_co_nhieu);
    hau_to = '_dwt_coif5_L7_denoised';
    tep_ket_qua = fullfile(thu_muc_ket_qua, [ten_tep_goc hau_to '.wav']);
    so_thu_tu = 1;
    while isfile(tep_ket_qua) || isfolder(tep_ket_qua)
        tep_ket_qua = fullfile(thu_muc_ket_qua, sprintf('%s%s_%d.wav', ten_tep_goc, hau_to, so_thu_tu));
        so_thu_tu = so_thu_tu + 1;
    end
    audiowrite(tep_ket_qua, am_thanh_khu_nhieu, tan_so_lay_mau, 'BitsPerSample', 24);
    fprintf('Đã lưu WAV: %s\n', tep_ket_qua);
    cac_tin_hieu = {am_thanh_co_nhieu, am_thanh_khu_nhieu};
    cac_nhan = {'Noisy', 'Denoised - DWT + Wiener, coif5, level 7'};
    if ~isempty(am_thanh_tham_chieu)
        cac_tin_hieu = [{am_thanh_tham_chieu} cac_tin_hieu];
        cac_nhan = [{'Clean tham chiếu'} cac_nhan];
    end
    ve_so_sanh(cac_tin_hieu, cac_nhan, tan_so_lay_mau, ten_tep_goc, bang_chi_so);
end

function ve_so_sanh(cac_tin_hieu, cac_nhan, tan_so_lay_mau, ten_tep_goc, bang_chi_so)
    so_mau = numel(cac_tin_hieu{1});
    so_hang = numel(cac_tin_hieu);
    thoi_gian = (0:so_mau - 1)' / tan_so_lay_mau;
    do_dai_cua_so = min(512, so_mau);
    pho_db = cell(so_hang, 1);
    for hang = 1:so_hang
        [~, tan_so, thoi_gian_pho, mat_do_cong_suat] = spectrogram(cac_tin_hieu{hang}, ...
            do_dai_cua_so, floor(0.75 * do_dai_cua_so), max(1024, 2^nextpow2(do_dai_cua_so)), tan_so_lay_mau);
        pho_db{hang} = 10 * log10(max(mat_do_cong_suat, realmin));
    end
    % Dùng chung thang màu 80 dB và thang biên độ để so sánh công bằng.
    dinh_pho_db = max(cellfun(@(gia_tri) max(gia_tri(:)), pho_db));
    gioi_han_bien_do = max(1e-6, 1.05 * max(cellfun(@(am_thanh) max(abs(am_thanh)), cac_tin_hieu)));
    cua_so_do_thi = figure('Name', 'Khử nhiễu pink/white - DWT + Wiener, coif5 level 7', ...
        'NumberTitle', 'off', 'Color', 'w', 'Position', [50 50 1350 850]);
    truc_dang_song = gobjects(so_hang, 1);
    truc_pho = gobjects(so_hang, 1);
    for hang = 1:so_hang
        truc_dang_song(hang) = subplot(so_hang, 2, 2*hang-1, 'Parent', cua_so_do_thi);
        plot(truc_dang_song(hang), thoi_gian, cac_tin_hieu{hang});
        set(truc_dang_song(hang), 'Color', 'w', 'XColor', 'k', 'YColor', 'k');
        title(truc_dang_song(hang), [cac_nhan{hang} ' - Dạng sóng'], 'Color', 'k', 'Interpreter', 'none');
        xlabel(truc_dang_song(hang), 'Thời gian (s)', 'Color', 'k');
        ylabel(truc_dang_song(hang), 'Biên độ', 'Color', 'k');
        ylim(truc_dang_song(hang), [-gioi_han_bien_do gioi_han_bien_do]);
        xlim(truc_dang_song(hang), [0 so_mau/tan_so_lay_mau]);
        grid(truc_dang_song(hang), 'on');
        truc_pho(hang) = subplot(so_hang, 2, 2*hang, 'Parent', cua_so_do_thi);
        imagesc(truc_pho(hang), thoi_gian_pho, tan_so, pho_db{hang}, [dinh_pho_db-80 dinh_pho_db]);
        set(truc_pho(hang), 'YDir', 'normal', 'XColor', 'k', 'YColor', 'k');
        title(truc_pho(hang), [cac_nhan{hang} ' - Spectrogram'], 'Color', 'k', 'Interpreter', 'none');
        xlabel(truc_pho(hang), 'Thời gian (s)', 'Color', 'k');
        ylabel(truc_pho(hang), 'Tần số (Hz)', 'Color', 'k');
        ylim(truc_pho(hang), [0 tan_so_lay_mau/2]);
        xlim(truc_pho(hang), [0 so_mau/tan_so_lay_mau]);
        thanh_mau = colorbar(truc_pho(hang));
        set(thanh_mau, 'Color', 'k');
        thanh_mau.Label.String = 'PSD (dB/Hz, ref. 1 biên độ²/Hz)';
    end
    colormap(cua_so_do_thi, parula);
    mo_ta = sprintf('DWT + Wiener | coif5 | Level 7 | Delta SNR: %.3f dB | RMSE: %.6f', ...
        bang_chi_so.Delta_SNR_dB, bang_chi_so.RMSE);
    if isnan(bang_chi_so.RMSE)
        mo_ta = 'DWT + Wiener | coif5 | Level 7 | Không có clean tham chiếu';
    end
    sgtitle(cua_so_do_thi, {ten_tep_goc, mo_ta}, 'Interpreter', 'none', 'Color', 'k');
    % Đồng bộ thời gian cả hai cột, nhưng không trộn trục biên độ với tần số.
    linkaxes(truc_dang_song, 'y');
    linkaxes(truc_pho, 'y');
    linkaxes([truc_dang_song; truc_pho], 'x');
    drawnow;
end
