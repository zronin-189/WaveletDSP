function y_out = khu_nhieu_dwt_core(u_in, fs, wavelet_code, level, ...
    noise_profile_sec, win_power_sec, win_smooth_sec, gain_floor_min)
% KHU_NHIEU_DWT_CORE
% Core DSP Engine: DWT da muc + Bo loc Wiener theo tung bang (cA va cD).
% Dong bo voi thuat toan trong khu_nhieu_am_thanh.m (khong dung giam duoi).

    x = double(u_in(:));
    N_samples = length(x);
    if N_samples < 32
        y_out = x;
        return;
    end

    if nargin < 2 || isempty(fs) || fs == 48000
        fs = 16000;
    end
    if nargin < 3 || isempty(wavelet_code), wavelet_code = 1; end
    if nargin < 4 || isempty(level), level = 7; end
    if nargin < 5 || isempty(noise_profile_sec), noise_profile_sec = 0.25; end
    if nargin < 6 || isempty(win_power_sec), win_power_sec = 0.064; end
    if nargin < 7 || isempty(win_smooth_sec), win_smooth_sec = 0.040; end
    if nargin < 8 || isempty(gain_floor_min), gain_floor_min = 0.05; end

    switch round(wavelet_code)
        case 1, wname = 'coif5';
        case 2, wname = 'db4';
        case 3, wname = 'sym4';
        case 4, wname = 'sym8';
        case 5, wname = 'db8';
        otherwise, wname = 'coif5';
    end

    L_max = wmaxlev(N_samples, wname);
    N_level = min(max(1, round(level)), L_max);

    N_noise = min(N_samples, max(16, round(noise_profile_sec * fs)));

    % Bien doi DWT cho toan bo tin hieu va rieng doan mau nhieu dau
    [C, L] = wavedec(x, N_level, wname, 'Mode', 'sym');
    [Cn, Ln] = wavedec(x(1:N_noise), N_level, wname, 'Mode', 'sym');

    % Loc Wiener tren toan bo cac dai (bao gom ca dai xap xi cA va tat ca dai chi tiet cD)
    idx_start = 1;
    idx_noise_start = 1;
    for k = 1:N_level + 1
        sub_level = min(N_level, N_level + 2 - k);
        idx_end = idx_start + L(k) - 1;
        idx_noise_end = idx_noise_start + Ln(k) - 1;

        band_coef = C(idx_start:idx_end);
        noise_band = Cn(idx_noise_start:idx_noise_end);
        P_noise_band = var(noise_band, 1);

        fs_band = fs / (2^sub_level);
        W_pow = max(1, round(win_power_sec * fs_band));
        P_local = movmean(band_coef.^2, W_pow);

        gain_raw = max(1 - P_noise_band ./ max(P_local, realmin), gain_floor_min);
        W_smooth = max(1, round(win_smooth_sec * fs_band));
        gain_smooth = movmean(gain_raw, W_smooth);

        C(idx_start:idx_end) = band_coef .* gain_smooth;

        idx_start = idx_end + 1;
        idx_noise_start = idx_noise_end + 1;
    end

    % Tai tao tin hieu
    y_rec = waverec(C, L, wname, 'Mode', 'sym');
    y_out = y_rec(:);

    % Chong clipping
    scale_clipping = 1 / max(1, max(abs(y_out)));
    y_out = y_out * scale_clipping;
    y_out = y_out(1:N_samples);
end
