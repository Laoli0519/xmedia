% ��txt�ļ��ж�ȡ���ݣ���kalman����myrtc���ɵ�rtt��loss������
% 

clear;
% clc;
% close all;

% ��ȡ�ļ�����ͼ
[fname, fpath] = uigetfile(...
    {'*.txt', '*.*'}, ...
    'Pick a file');
if(~ischar(fname))
    disp 'wrong filename';
    return;
end
file4plot = fullfile(fpath, fname);
TT = readtable(file4plot, 'Delimiter',',', 'FileEncoding', 'UTF-8');
% ת����ʱ�����ֵ
starttime = TT.(1)(1);
TT.(1) = TT.(1)-starttime; % ��λ��Ժ���
TT.(1) = TT.(1)/1000.0; % ��λΪ��

% �� remote_bitrate_estimator_single_stream ������
% LastT = Offset * min(num_deltas, 60


plotIdMap = containers.Map;
plotIdMap('LastT')    = 1;
plotIdMap('LastTEst')    = 1;
plotIdMap('Threshold')    = 1;

plotIdMap('RecvRate')    = 2;
plotIdMap('RecvBWE')    = 2;


% plotIdMap('RecvRateEst')    = 2;
% plotIdMap('RCVarMax')    = 0;
% plotIdMap('RCAvgMax')    = 1;
% plotIdMap('SendTime24')    = 0;
% plotIdMap('RtpTimeDelta')    = 0;
% plotIdMap('TimeDelta')    = 0;
% plotIdMap('TTsDelta')    = 0;
% plotIdMap('TTSDelta')    = 0;
% plotIdMap('TTSDeltaEst')    = 0;
% plotIdMap('TTSIntegral')    = 0;

                                                         

plotIdMap('AvgT')    = 1;
plotIdMap('RCAvg')    = 2;  
plotIdMap('VarT')    = 3;
plotIdMap('RangeT')    = 3; 
plotIdMap('DiffTIntegral')    = 0; 
                           
plotIdMap('VarIntegral')    = 0;
plotIdMap('OverIntegral')    = 0;
plotIdMap('Action')    = 0;


plotIdMap('TVar')    = 0;
plotIdMap('TVarIntegral')    = 0;

% plotIdMap('BWState')    = 0;
% plotIdMap('NoiseVar')    = 0;
% plotIdMap('LastSlope')    = 0;
% % plotIdMap('Threshold2')    = 2;



% plotIdMap('Value')    = 1;
% plotIdMap('ValueEst')    = -1;
% plotIdMap('SlopeEst')    = 2;
% plotIdMap('SlopeRange')    = 0;

% plotIdMap('TTSDelta')    = -1;
% plotIdMap('TTSDeltaEst')    = 4;
% plotIdMap('EstT')    = 1;
% plotIdMap('Hz')    = 0;

func_plot_table(fname, TT, plotIdMap, -1);
return; 