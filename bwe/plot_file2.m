% ��txt�ж�ȡ���ݲ���ͼ
% ���ݸ�ʽ����һ���Ǻ����꣬������Ϊ�������ݣ���������Ϊ��-����������û������
% һ���ļ����ӣ�
% timestamp rtt loss
% 100 200 100
% 200 - 10
% 300 300 150

clear all
% clc;
% close all;

% plotIds ��ά����������߸���һ��
plotIds = [1, 1, 1, 2, 1, 1, 1, 1];

[fname, fpath] = uigetfile(...
    {'*.txt', '*.*'}, ...
    'Pick a file');
if(~ischar(fname))
    disp 'wrong filename';
    return;
end
file4read = fullfile(fpath, fname);
TT = readtable(fullfile(fpath, fname), 'Delimiter',' ', 'FileEncoding', 'UTF-8');

% numVar=���߸���+1
numVar = size(TT.Properties.VariableNames,2);

% ���û�� plotIds������Ĭ�ϵ�
if(~exist('plotIds','var'))
    plotIds = 1:1:numVar-1;
end

% ��� plotIds �����Ƿ���ȷ
numIds = size(plotIds, 2);
if(numIds == 0)
    plotIds = 1:1:numVar-1;
    numIds = size(plotIds, 2);
elseif(numIds ~= (numVar-1))
    disp 'wrong size of plotIds. should be:';
    disp (numVar-1);
    return;
end

% �������ͼ��
maxPlotId = 1;
for i=1:numIds
  if plotIds(i) > maxPlotId 
      maxPlotId = plotIds(i);
  end
end

% ynames����ÿһ��ͼ���������������
ynames = cell(1, maxPlotId);
for i=1:numIds
  nId = plotIds(i);
  tmp = ynames{nId};
  tmp{end+1}=TT.Properties.VariableNames{i+1};
  ynames{nId} = tmp;
end

% numCounters����ÿһ��ͼ����ѻ����߼���
numCounters = zeros(1, maxPlotId);
for i=2:numVar
    % ȡ����i�����������в���'-'����
    rows = find(~ismember(TT.(i), '-'));
    pairTable = TT(rows,[1,i]);
    if(iscell(pairTable.(2)) == 1)
        % ������ַ�����ת��double
        y = cellfun(@(x)str2double(x), pairTable.(2));
    else
        y = pairTable.(2);
    end
    
    x = pairTable.(1);
%     yname = pairTable.Properties.VariableNames{2};
    
    plotId = plotIds(i-1);
    subplot(maxPlotId,1,plotId);
    H1 = plot(x, y);
    
    % ��ǰͼ����������߶��Ѿ����꣬��ʾ��������
    numCounters(plotId) = numCounters(plotId)+1;
    if(numCounters(plotId) >= size(ynames{plotId},2))
        lgd=legend(ynames{plotId}, 'Location','northeast'); % 'Orientation','horizontal'
        lgd.FontSize = 12;
        lgd.TextColor = 'blue';
    end
    hold on;
end

