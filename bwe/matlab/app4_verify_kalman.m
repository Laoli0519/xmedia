
clear;

% ���Կ������˲���

% ���ɲ�������
count = 100;
tt = 1:1:count; tt=tt';% ʱ����

ideal = 7 + 4*tt'; ideal = ideal'; % ����ֵ



% zz = ideal + 15*randn(size(ideal)); % ����ֵ
zz = ideal + 5*sin(ideal); % ����ֵ


k1.cQ=1e-8; k1.cR=1e-4; k1=kalman(k1, 1, 1);
k1.P = 0.0001; % Ϊ�˻�KPPʱ��Ҫƫ��̫�ർ�¿������Сֵ
k2.cQ=1e-15; k2.cR=1e-1;  k2=kalman(k2, 2, 1);
k2.x = [1, 1]'; 
% k2.Q = [ 1e-8, 0; 0, 1e-9];

zzEst1 = zeros(count, 1);  % ����ֵ
zzEst2 = zeros(count, 1);  % ����ֵ
zzOffset2 = zeros(count, 1);  % ƫ�ƹ���ֵ
zzSlop2 = zeros(count, 1);  % б�ʹ���ֵ
zzDiff2 = zeros(count, 1);

% �ÿ������˲�����������
lastT = tt(1);
for i=1:count
    deltaT = tt(i); %-tt(1);
    lastT = tt(i);
    
    k1.z = zz(i);
    k1 = kalman(k1);
    zzEst1(i)=k1.x;
    
    k2.z = zz(i);
    k2.H = [1 deltaT];
    k2 = kalman(k2);
    
    zzEst2(i)=k2.x(1,1)+k2.x(2,1)*deltaT;
    zzOffset2(i) = k2.x(1,1);
    zzSlop2(i) = k2.x(2,1);
    zzDiff2(i) = zzEst2(i) - ideal(i);
end
% zzSlop2(1:3) = 0;

% ��ͼ
table4Draw = table;
table4Draw.tt = tt;
table4Draw.ideal = ideal;
table4Draw.zz = zz;
table4Draw.zzEst1 = zzEst1;
table4Draw.zzEst2 = zzEst2;
table4Draw.zzOffset2 = zzOffset2;
table4Draw.zzSlop2 = zzSlop2;
table4Draw.zzDiff2 = zzDiff2;
plotIdMap = containers.Map;
plotIdMap('ideal')    = 1;
plotIdMap('zz')    = 1;
plotIdMap('zzEst1')    = 1;
plotIdMap('zzEst2')    = 1;

func_plot_table('kalman', table4Draw, plotIdMap, 0);
return;

