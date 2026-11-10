% KHOI_TAO_MO_HINH_DWT
% Luu y: Mo hinh mo_hinh_dwt_wiener.slx hien tai da la Full Block Thuan (Standalone),
% tich hop toan bo thong so trong Subsystem Mask va Model Workspace.
% Ban co the mo truc tiep file .slx va nhan Run ma khong can chay script nay.

Wavelet_Mother_Code = 1;         % 1=coif5, 2=db4, 3=sym4, 4=sym8, 5=db8
Decomposition_Level = 7;         % 1 -> 7
Noise_Profile_Sec = 0.25;        % Giay
Local_Power_Window_Sec = 0.064;  % Giay
Gain_Smoothing_Window_Sec = 0.040;% Giay
Gain_Floor_Min = 0.05;           % Nguong gain toi thieu

vars = {'Wavelet_Mother_Code', 'Decomposition_Level', 'Noise_Profile_Sec', ...
        'Local_Power_Window_Sec', 'Gain_Smoothing_Window_Sec', 'Gain_Floor_Min'};
vals = {Wavelet_Mother_Code, Decomposition_Level, Noise_Profile_Sec, ...
        Local_Power_Window_Sec, Gain_Smoothing_Window_Sec, Gain_Floor_Min};
for idx_v = 1:numel(vars)
    assignin('base', vars{idx_v}, vals{idx_v});
end

projectFolder = fileparts(fileparts(mfilename('fullpath')));
input_audio_file = fullfile(projectFolder, 'data_DSP_speech', 'noisy', 'pink_10db', 'speech_001_pink_10dB.wav');
output_audio_file = fullfile(projectFolder, 'results', 'simulink_denoised_output.wav');

if ~exist(fullfile(projectFolder, 'results'), 'dir')
    mkdir(fullfile(projectFolder, 'results'));
end

fprintf('[khoi_tao] Mo hinh hien tai hoat dong doc lap (Standalone Native Blocks).\n');
