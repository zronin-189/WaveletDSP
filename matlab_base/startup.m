% STARTUP.M - Tu dong cau hinh moi truong cho WaveletDSP
fprintf('[WaveletDSP] Dang khoi dong moi truong MATLAB...\n');
projectFolder = fileparts(fileparts(mfilename('fullpath')));
addpath(projectFolder, fullfile(projectFolder, 'matlab_base'), ...
    fullfile(projectFolder, 'simulink_dsp'));
if ~bdIsLoaded('dspsrcs4')
    load_system('dspsrcs4');
end
try
    set_param('dspsrcs4', 'Lock', 'off');
    if isempty(find_system('dspsrcs4', 'SearchDepth', 1, 'Name', 'To Multimedia File'))
        b = add_block('dspsnks4/To Multimedia File', 'dspsrcs4/To Multimedia File');
        set_param(b, 'LinkStatus', 'none');
        m = Simulink.Mask.get(b);
        if isempty(m)
            m = Simulink.Mask.create(b);
        end
        m.addParameter('Prompt', 'Sample Rate', 'Name', 'sampleRate', 'Value', '48000');
    end
catch
end
fprintf('[WaveletDSP] San sang lam viec!\n');
