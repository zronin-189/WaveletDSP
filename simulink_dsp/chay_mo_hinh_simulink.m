function [outputAudio, outputFile, metrics] = chay_mo_hinh_simulink(inputFile, cleanFile)
modelName = 'mo_hinh_dwt_wiener';
projectFolder = fileparts(mfilename('fullpath'));
modelFile = fullfile(projectFolder, [modelName '.slx']);
assert(isfile(modelFile), 'WaveletDSP:MissingModel', 'Hãy tạo mô hình trước khi chạy công cụ kiểm chứng.');
load_system(modelFile);
source = role_block(modelName,'source');
filterBlock = role_block(modelName,'filter');
if nargin >= 1 && ~isempty(inputFile)
    set_param(source, 'inputFilename', inputFile);
end
if nargin >= 2 && ~isempty(cleanFile)
    set_param(role_block(modelName,'clean'),'inputFilename',cleanFile);
    set_param(filterBlock, 'Use_Clean','1','Auto_Clean', '0');
elseif nargin >= 1 && ~isempty(inputFile)
    set_param(filterBlock, 'Use_Clean','0');
end
forbiddenBlocks = find_system(modelName, 'LookUnderMasks', 'all', ...
    'BlockType', 'S-Function', 'FunctionName', 'sf_sfun');
assert(isempty(forbiddenBlocks), 'WaveletDSP:ForbiddenBlock', 'Không được dùng khối hàm MATLAB.');
simulationOutput = sim(modelName);
workspace = get_param(modelName, 'ModelWorkspace');
assert(evalin(workspace, 'Export_Complete'), 'WaveletDSP:IncompleteExport', 'Chưa xuất xong tệp đầu ra.');
inputFile = get_param(source, 'inputFilename');
outputFile = evalin(workspace,'Last_Output_File');
[noisyAudio, inputRate] = audioread(inputFile);
[outputAudio, outputRate] = audioread(outputFile);
assert(inputRate == outputRate && isequal(size(noisyAudio),size(outputAudio)), ...
    'WaveletDSP:ReferenceMismatch', 'Đầu ra phải đủ số mẫu và cùng tần số lấy mẫu với đầu vào.');
validCount = simulationOutput.get('valid_count');
assert(validCount(end) == numel(noisyAudio), 'WaveletDSP:SampleCount', 'Bộ đo chưa đếm đúng số mẫu hợp lệ của bản ghi.');
meterInput = simulationOutput.get('meter_1');
if evalin(workspace,'Reference_Available')
    cleanFile = evalin(workspace,'Run_Clean_File');
    [cleanAudio,cleanRate] = audioread(cleanFile);
    assert(cleanRate==inputRate && isequal(size(cleanAudio),size(noisyAudio)));
    cleanEnergy = sum(cleanAudio.^2);
    snrInput = 10*log10(cleanEnergy/sum((cleanAudio-noisyAudio).^2));
    snrOutput = 10*log10(cleanEnergy/sum((cleanAudio-outputAudio).^2));
    rmse = sqrt(mean((cleanAudio-outputAudio).^2));
    assert(abs(meterInput(end)-snrInput) < 1e-7, 'WaveletDSP:MeterMismatch', 'Tỷ số tín hiệu trên nhiễu đầu vào không khớp phép đo toàn bản ghi.');
else
    snrInput = NaN;
    snrOutput = NaN;
    rmse = NaN;
    assert(all(isnan(meterInput(:))));
    fprintf('Không có âm thanh sạch: các chỉ số đánh giá không khả dụng (NaN).\n');
end
metrics = table(numel(noisyAudio), inputRate, snrInput, snrOutput, snrOutput-snrInput, rmse, ...
    'VariableNames', {'Samples','SampleRate','SNR_Input_dB','SNR_Output_dB','Delta_SNR_dB','RMSE'});
disp(metrics);
fprintf('Đánh giá toàn bản ghi, không cắt đuôi: %s\n', outputFile);
end

function block=role_block(model,role)
matches=find_system(model,'LookUnderMasks','all','FollowLinks','off','IncludeCommented','on','Tag',['WaveletDSP:' role]);
assert(numel(matches)==1,'WaveletDSP:Role','Thiếu hoặc trùng định danh nội bộ: %s.',role);
block=get_param(matches{1},'Handle');
end
