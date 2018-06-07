% close all;
% N=64;
% std_time_data=[1:2*N];
% time_data_for_fft=std_time_data;
% freq_data_for_fft=fft(time_data_for_fft);
% inv_time_data=ifft(freq_data_for_fft);
% diff=inv_time_data-std_time_data;
% [max,index]=max(abs(diff));
% max
% index


close all;
clear;  
% N=1024;  %����  
N=64;  %����
d=0;     %�ӳٵ���
Fs=500;  %����Ƶ��  
n=0:N-1;  
t=n/Fs;   %ʱ������  
a1=1;     %�źŷ���  
a2=1;    
x1=a1*cos(2*pi*10*n/Fs);     %�ź�1  
% x1=x1+randn(size(x1))/max(randn(size(x1)));      %������  
x2=a2*cos(2*pi*10*(n+d)/Fs); %�ź�2  
% x2=x2+randn(size(x2));   % ����

% N = 64;
% x1 = [1:N];
% x2 = [d:N,1:d-1];
offset=abs(d);
x2=[x1(offset+1:N), x1(1:offset)];
x2=[x1(N-offset:N), x1(1:N-offset-1)];

ex1=[x1, zeros(1,N)];
ex2=[x2, zeros(1,N)];
Y1=fft(ex1);  
Y2=fft(ex2); 
S12=Y1.*conj(Y2);  
time12=ifft(S12);
time12_real=real(time12);
Cxy=fftshift(time12_real);  
[maxval,location]=max(Cxy);%������ֵmax,�����ֵ���ڵ�λ�ã��ڼ��У�location;  
d2=location-N ; %����ӳٵ���
Delay2=d2/Fs ;   %���ʱ���ӳ�  

Cxy2 = xcorr(x1,x2);
DiffCxy=abs(Cxy2-Cxy(2:2*N));
[MaxDiffCxy]=max(DiffCxy);

d2
Delay2
maxval
MaxDiffCxy

% figure; plot(x1);
% figure; plot(x2);

% figure; plot(time12);
% figure; plot(time12_real);
figure; plot(Cxy);
figure; plot(Cxy2);
