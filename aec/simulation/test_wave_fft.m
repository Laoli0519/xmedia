% ʹ�� MATLAB ���������Ļ�������
% http://www.lab-z.com/wp-content/uploads/2013/02/mlb.pdf

[snd, sampFreq, nBits] = wavread('channels2.wav');
ds=size(snd)
datalength=ds(1)
% sound(snd, sampFreq, nBits) 
s1 = snd(:,1); 
timeArray = (0:datalength-1) / sampFreq; % ����һ������ʱ��������
timeArray = timeArray * 1000; % ʱ���Ŵ󵽺��뼶
plot(timeArray, s1, 'k'); 
% n=size(s1);
n= length(s1);
p=fft(s1);
disp('fft done');

is1=ifft(p);
% is1=roundn(is1,-2); % ��������
% s1=roundn(s1,-2);
disp('ifft done');
figure;
plot(timeArray, is1, 'k') 
% sound(is1, sampFreq, nBits) % ����ifft�����Ƶ
% figure;
% plot(timeArray, is1-s1, 'k') % ���� ԭʼ�ź���ifft֮����źŲ�ֵ

equ=isequal(s1, is1);
disp(['equ=' num2str(equ)]);

nUniquePts = ceil((n+1)/2);
p = p(1:nUniquePts); % ѡ��ǰ�벿����Ϊ��벿��ǰ�벿��һ������
p = abs(p); % ȡ����ֵ�����߳�֮Ϊ���� 
  % FFT ����������Ƶ����ֵ�������Ⱥ���λ��Ϣ�����Ը�������ʽ�����ģ����ظ�
  % �������Ը���Ҷ�任��Ľ��ȡ����ֵ�����ǾͿ���ȡ��Ƶ�ʷ����ķ�����Ϣ��

 p = p/n; % ʹ�õ������������ţ��������Ⱥ��źų��Ȼ����������Ƶ���޹�
 p = p.^2; % ƽ���õ����� ���� 2��ԭ����ο�������ĵ���
 if rem(n, 2) % ������nfft ��Ҫ�ų��ο�˹�ص�
 p(2:end) = p(2:end)*2;
 else
 p(2:end -1) = p(2:end -1)*2;
 end
freqArray = (0:nUniquePts-1) * (sampFreq / n); % ����Ƶ������
plot(freqArray/1000, 10*log10(p), 'k')
xlabel('Frequency (kHz)')
ylabel('Power (dB)') 

disp('done');
