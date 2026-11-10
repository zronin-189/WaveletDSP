function y_out = khu_nhieu_am_thanh_core(u, fs, w_code, lev, noise_sec, win_pow, win_smooth, gain_floor, ~)
% KHU_NHIEU_AM_THANH_CORE
% DSP Core Engine dong bo 100% voi thuat toan trong khu_nhieu_am_thanh.m.
% Da loai bo hoan toan tail_reduction theo yeu cau.

    x = double(u(:));
    N_samples = length(x);
    if N_samples < 32
        y_out = x;
        return;
    end

    if nargin < 2 || isempty(fs), fs = 16000; end
    if nargin < 3 || isempty(w_code), w_code = 1; end
    if nargin < 4 || isempty(lev), lev = 7; end
    if nargin < 5 || isempty(noise_sec), noise_sec = 0.25; end
    if nargin < 6 || isempty(win_pow), win_pow = 0.064; end
    if nargin < 7 || isempty(win_smooth), win_smooth = 0.040; end
    if nargin < 8 || isempty(gain_floor), gain_floor = 0.05; end

    wopts = {'coif5', 'db4', 'sym4', 'sym8', 'db8'};
    w_idx = min(max(1, round(w_code)), numel(wopts));
    wname = wopts{w_idx};

    N_level = min(max(1, round(lev)), wmaxlev(N_samples, wname));
    N_noise = min(N_samples, max(16, round(noise_sec * fs)));

    % 1. Bien doi Wavelet DWT da muc (che do bien doi doi xung Mode='sym' tranh meo bien)
    [C, L] = wavedec(x, N_level, wname, 'Mode', 'sym');
    [Cn, Ln] = wavedec(x(1:N_noise), N_level, wname, 'Mode', 'sym');

    % 2. Loc Wiener thich nghi tren tung dai con (cA va cac dai cD)
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
        P_local = movmean(band_coef.^2, max(1, round(win_pow * fs_band)));

        gain_raw = max(1 - P_noise_band ./ max(P_local, realmin), gain_floor);
        gain_smooth = movmean(gain_raw, max(1, round(win_smooth * fs_band)));

        C(idx_start:idx_end) = band_coef .* gain_smooth;

        idx_start = idx_end + 1;
        idx_noise_start = idx_noise_end + 1;
    end

    % 3. Tai tao tin hieu bang IDWT (waverec)
    y_out = waverec(C, L, wname, 'Mode', 'sym');
    y_out = y_out(:);

    % 4. Chong clipping bien do
    scale_clip = 1 / max(1, max(abs(y_out)));
    y_out = y_out * scale_clip;
    y_out = y_out(1:N_samples);
end
