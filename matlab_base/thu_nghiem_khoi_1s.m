function [bang_so_sanh, tep_offline, tep_khoi] = thu_nghiem_khoi_1s(tep_noisy, tep_clean)
% THU_NGHIEM_KHOI_1S
% Offline L7 double vs 1-second block pipeline: coif5, level 5, float32, frozen Pn.
% Example:
%   [b, f1, f2] = thu_nghiem_khoi_1s( ...
%       'data_DSP_speech/noisy/white_5db/speech_001_white_5dB.wav', ...
%       'data_DSP_speech/clean/speech_001.wav');

if nargin < 1 || isempty(tep_noisy)
    [f, p] = uigetfile('*.wav', 'Select noisy WAV');
    if isequal(f, 0), error('WaveletDSP:Cancelled', 'Cancelled.'); end
    tep_noisy = fullfile(p, f);
end
if nargin < 2, tep_clean = ''; end

[x, fs] = audioread(tep_noisy);
x = double(x(:, 1));
N = numel(x);
fprintf('Fs = %d Hz, %d samples (%.3f s)\n', fs, N, N / fs);

wname = 'coif5';
level_block = 5;
noise_sec = 0.25;
win_pow = 0.064;
win_smooth = 0.040;
gain_floor = 0.05;
block_len = round(1.0 * fs);
overlap = 256;

y_off = khu_nhieu_am_thanh_core(x, fs, 1, 7, noise_sec, win_pow, win_smooth, gain_floor);
y_blk = denoise_block_1s_single(x, fs, wname, level_block, noise_sec, ...
    win_pow, win_smooth, gain_floor, block_len, overlap);

thu_muc = fullfile(fileparts(fileparts(mfilename('fullpath'))), 'results');
if ~isfolder(thu_muc), mkdir(thu_muc); end
[~, ten_goc] = fileparts(tep_noisy);
tep_offline = fullfile(thu_muc, [ten_goc '_offline_L7_double.wav']);
tep_khoi = fullfile(thu_muc, [ten_goc '_khoi1s_L5_single.wav']);
audiowrite(tep_offline, double(y_off(1:N)), fs, 'BitsPerSample', 24);
audiowrite(tep_khoi, double(y_blk(1:N)), fs, 'BitsPerSample', 24);

if isempty(tep_clean)
    s_in = NaN; s_off = NaN; s_blk = NaN; rm_off = NaN; rm_blk = NaN;
else
    c = double(audioread(tep_clean)); c = c(:, 1);
    assert(numel(c) == N, 'Clean/noisy length mismatch.');
    s_in = snr_db(c, x);
    s_off = snr_db(c, double(y_off(1:N)));
    s_blk = snr_db(c, double(y_blk(1:N)));
    rm_off = sqrt(mean((c - double(y_off(1:N))).^2));
    rm_blk = sqrt(mean((c - double(y_blk(1:N))).^2));
end

lat_off_ms = 1000 * N / fs;
lat_blk_ms = 1000 * block_len / fs;
bang_so_sanh = table( ...
    {'offline coif5-L7 double'; 'block-1s coif5-L5 float32'}, ...
    [s_in; s_in], [s_off; s_blk], [s_off - s_in; s_blk - s_in], ...
    [rm_off; rm_blk], [lat_off_ms; lat_blk_ms], ...
    'VariableNames', {'Ban', 'SNR_in_dB', 'SNR_out_dB', 'Delta_dB', 'RMSE', 'DoTre_ms'});
disp(bang_so_sanh);
fprintf('Saved:\n  %s\n  %s\n', tep_offline, tep_khoi);
fprintf(['Pn is estimated once from the first %.3f s, then frozen. ' ...
    'It is not recomputed at the start of every 1 s block.\n'], noise_sec);
end

function y = denoise_block_1s_single(x, fs, wname, level, noise_sec, ...
    win_pow, win_smooth, gain_floor, block_len, overlap)
x = x(:);
N = numel(x);
Pn = estimate_pn_frozen(x, fs, wname, level, noise_sec);
y = zeros(N, 1, 'single');
nBlocks = ceil(N / block_len);
for i = 1:nBlocks
    i0 = (i - 1) * block_len + 1;
    i1 = min(i0 + block_len - 1, N);
    xb = single(x(i0:i1));
    yb = wiener_dwt_single(xb, fs, wname, level, Pn, win_pow, win_smooth, gain_floor);
    yb = max(min(yb, single(1)), single(-1));
    y(i0:i1) = yb;
    if i > 1 && overlap > 0
        ov = min([overlap, i0 - 1, numel(yb)]);
        fade = single(linspace(0, 1, ov).');
        left = y(i0-ov:i0-1);
        y(i0-ov:i0-1) = left .* (1 - fade) + yb(1:ov) .* fade;
    end
end
end

function Pn = estimate_pn_frozen(x, fs, wname, level, noise_sec)
nNoise = min(numel(x), max(1024, round(noise_sec * fs)));
assert(level <= floor(log2(nNoise)), 'Noise profile too short for this level.');
[C, L] = wavedec(double(x(1:nNoise)), level, wname, 'Mode', 'sym');
nBand = level + 1;
Pn = zeros(1, nBand, 'single');
s = 1;
for k = 1:nBand
    e = s + L(k) - 1;
    Pn(k) = single(var(C(s:e), 1));
    s = e + 1;
end
end

function y = wiener_dwt_single(x, fs, wname, level, Pn, win_pow, win_smooth, gain_floor)
x = x(:);
n = numel(x);
nLevel = min(level, wmaxlev(n, wname));
nBand = nLevel + 1;
[C, L] = wavedec(double(x), nLevel, wname, 'Mode', 'sym');
C = single(C);
s = 1;
for k = 1:nBand
    e = s + L(k) - 1;
    band = C(s:e);
    sub_level = min(nLevel, nLevel + 2 - k);
    fs_band = fs / (2^sub_level);
    p_local = movmean(band.^2, max(1, round(win_pow * fs_band)));
    p_noise = Pn(k);
    gain_raw = max(1 - p_noise ./ max(p_local, realmin('single')), single(gain_floor));
    gain_smooth = movmean(gain_raw, max(1, round(win_smooth * fs_band)));
    C(s:e) = band .* gain_smooth;
    s = e + 1;
end
y = single(waverec(double(C), L, wname, 'Mode', 'sym'));
y = y(1:n);
end

function v = snr_db(c, y)
v = 10 * log10(sum(c.^2) / max(sum((c - y).^2), realmin));
end
