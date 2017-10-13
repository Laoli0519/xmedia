% refer http://bbs.loveuav.com/forum.php?mod=viewthread&tid=274&extra=page%3D1&page=1
% һ�����ϵ�kalman���ӣ��������ݣ�Ȼ���˲�
clear
close all
clc

% ���ɲ�������
interval = pi/18;
t = 1:interval:100*pi;
len = size(t, 2);
a = t + 4 * (rand(1,len)-0.5);
b = t .* sin(t/10) +  10 * (rand(1,len)-0.5);
z = [a; b];
%save('z.mat','z');
%plot(z(1,:),z(2,:),'o');

% 1����ֹ
dim_observe = 2;      %�۲�ֵά��
n = dim_observe;  %״̬ά��
A = eye(n);
H = eye(n);
cQ = 1e-8;
cR = 1e-2; % 1e-2 1e-4 1e-8
app1_run( dim_observe, n, A, H, cQ, cR, z);

cR = 1e-4; 
app1_run( dim_observe, n, A, H, cQ, cR, z);

% 2�������˶�
n = 2 * dim_observe;  %״̬ά�����۲�״̬ÿ��ά�ȶ���1���ٶȣ������2
A = [1,0,1,0;0,1,0,1;0,0,1,0;0,0,0,1];
H = [1,0,0,0;0,1,0,0];
cQ = 1e-8;
cR = 1e-2;
app1_run( dim_observe, n, A, H, cQ, cR, z);

cR = 1e-4;
app1_run( dim_observe, n, A, H, cQ, cR, z);




