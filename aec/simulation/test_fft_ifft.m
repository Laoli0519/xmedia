% ��ʾ���ݾ���fft �� ifft �������
% ���Խ��: �����źţ����Ϊ0���������źţ����ǳ�С��e-16��
% �������ݣ�������������ݺ�����������������룬���Ϊ0��
% ����wav�������

N=16*100000; % ���ɲ������ݸ���
A=0.6; % �������ݵ����ֵ
dot=4; % ��������С��

% x=A*rand(N,1); % ���ɸ�����
% x=floor(A*rand(N,1)); % ��������
% x=[8  6  0  9  0  5  1  3  8  2  8  7  3  2  2  7]'; % �̶���
[snd, sampFreq, nBits] = wavread('channels2.wav'); x=snd(:,1); % �� wav �ļ���ȡ

p=fft(x);
x2=ifft(p);
x2r=round(x2,dot); %  С����������
xr=round(x,dot); % С����������
diff=x2-x;
diffr=x2r-xr;
% diff=diffr;
diffabs=abs(diff);
equ=isequal(x2r, xr); % �Ƿ����
[maxerr, maxerri]=max(diffabs); % ������
[minerr, minerri]=min(diffabs); % ������

disp(['length=' num2str(length(x))]);
% disp(['x=' num2str(x')]);
% disp(['x2=' num2str(x2')]);
% disp(['x2r=' num2str(x2r')]);
% disp(['diff=' num2str(diff')]);
disp(['maxerr=' num2str(maxerr)]);
disp(['minerr=' num2str(minerr)]);
fprintf('  x[%d] =%f\n',maxerri, x(maxerri));
fprintf(' x2[%d] =%f\n',maxerri,x2(maxerri));
fprintf(' xr[%d] =%f\n',maxerri, xr(maxerri));
fprintf('x2r[%d] =%f\n',maxerri,x2r(maxerri));
disp(['equ=' num2str(equ)]);

fprintf('round(a,%d) =%f\n',dot, round(a, dot));
fprintf('round( x(%d),%d) =%f\n',maxerri, dot, round(x(maxerri), dot));
fprintf('round(x2(%d),%d) =%f\n',maxerri, dot, round(x2(maxerri), dot));
fprintf('  diff[%d] =%f\n',maxerri, x2(maxerri)-x(maxerri));
