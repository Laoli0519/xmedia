function [ TT ] = func_plot_table(fname, TT, plotIdMap, defaultPlotId )
%UNTITLED Summary of this function goes here
%   Detailed explanation goes here

% file4plot = fullfile(fpath, fname);
% TT = readtable(file4plot, 'Delimiter',',', 'FileEncoding', 'UTF-8');

% ת����ʱ�����ֵ
% starttime = TT.(1)(1);
% TT.(1) = TT.(1)-starttime; % ��λ��Ժ���
% TT.(1) = TT.(1)/1000.0; % ��λΪ��

% numVar=���߸���+1
numVar = size(TT.Properties.VariableNames,2);



% ��û��ָ��plotId������Ĭ��Id
% �������ͼ��
maxPlotId = 0;
for i=2:numVar
    name = TT.Properties.VariableNames{i};
    if ~plotIdMap.isKey(name)
        plotIdMap(name) = defaultPlotId;      
    end
    if plotIdMap(name) > maxPlotId
        maxPlotId = plotIdMap(name);
    end
end
% plotId����0�����¸����µ�plotId
for i=2:numVar
  name = TT.Properties.VariableNames{i};
  if plotIdMap(name) == 0
      maxPlotId = maxPlotId + 1;
      plotIdMap(name) = maxPlotId;
  end
end

ynameMap = containers.Map('KeyType', 'int32', 'ValueType', 'any');
for i=2:numVar
  name = TT.Properties.VariableNames{i};
  plotId = plotIdMap(name);
  if plotId < 0
      continue;
  end
  
  if ~ynameMap.isKey(plotId)
      ynameMap(plotId) = cell(0);
  end
  tmp = ynameMap(plotId);
  tmp{end+1} = name;
  ynameMap(plotId) = tmp;
end


% numCounters����ÿһ��ͼ����ѻ����߼���
numCounters = zeros(1, maxPlotId);
figure('NumberTitle', 'off', 'Name', fname);
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
    
%     x = rows;
    x = pairTable.(1);
    yname = pairTable.Properties.VariableNames{2};
    plotId = plotIdMap(yname);
    
    if plotId < 0
        continue;
    end
    
    subplot(maxPlotId,1,plotId);
    if(~isempty(y))
        H1 = plot(x, y);
    end
    
%     names = ynames{plotId};
    names = ynameMap(plotId);

    % ��ǰͼ����������߶��Ѿ����꣬��ʾ��������
    numCounters(plotId) = numCounters(plotId)+1;
    if(numCounters(plotId) >= size(names,2))
        lgd=legend(names, 'Location','northeast'); % 'Orientation','horizontal'
        if(~isempty(lgd))
            lgd.FontSize = 12;
            lgd.TextColor = 'blue';
        end
    end
    hold on;
    grid on;
end

% �޸�ͼ������ʾ����
dcmObj = datacursormode; 
set(dcmObj,'UpdateFcn',@updateFcn);

end

