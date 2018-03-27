clear all;
close all;
clc;
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%������Ϊ�����������α��������%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% temp=(randn(1,8192)>0);
% PN=zeros(1,16384);
% PN(1:8192)=temp;
% PN(16384:-1:8193)=-temp;
% w_k=zeros(1,16384);
% w_k(1:3715)=1;
% w_k(end-3715:end)=1;
% FFT_value=w_k.*exp(j*PN*pi);
% FFT_temp(1)=0;
% FFT_temp(2:16385)=FFT_value;
% IFFT_temp=ifft(FFT_temp,16385);
% IFFT_value=IFFT_temp(2:16385);
% FINAL_value=fftshift(real(IFFT_value));
% FINAL_value=resample(FINAL_value,2,11);
% PN_value=FINAL_value;
% PN_num=1;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%����һ������·���ĳ弤��Ӧ%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
N = 240; %240���� 
t = [0 : N-1]'; %0��1��239
tau = N./4; %tau=60
envelope = exp(-(t./tau));%�漴�źŵķ���Ȩֵ
w_k = randn(N,1).*envelope; 
w_k = [w_k(end-19 : end);w_k(1 : end-20)]; %�Ѻ�20����ŵ�ǰ��
w_k = w_k./sqrt(N);%�ٴγ���һ���̶�������
figure; 
plot(w_k);title('����·���ĳ����Ӧ')

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%�������������ź���ΪԶ�˺ͽ����ź�%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% array1=wavread('quiet',100000);%��������
% array2=wavread('wang2',100000);%Զ������

%array1=wavread('wang1',300000);%1 2 ...
% array1=wavread('���������CD����.wav',500000);%1 2 ...  ����Ƶ�� 44100Hz �����ź�
% array2=wavread('wang2',300000);
% array1(1:100000)=wavread('quiet',100000);
% array2(end-69999:end)=wavread('quiet',70000);
% array1=wavread('1-near.wav',500000);
[array1, sampFreq1, nBits1] = wavread('1-near.wav',500000);

% array1=wavread('quiet',300000);
%array2=wavread('wang2',300000); %10 11 ....
% array2=wavread('���ھ��������CD����.wav',500000); %10 11 .... Զ�� �ź�
% Rin2=array2(:,2)';
% array2=wavread('1-far.wav',500000);
[array2, sampFreq2, nBits2] = wavread('1-far.wav',500000);
Rin2=array2(:,1)';
array1=array1(:,1)';

array2=array2(:,1)';
%wavplay(array1,22050);
%wavplay(array2,22050);
M=length(array2); %�����ܳ���

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%ͨ��Զ�������ź������·���弤��Ӧ������ɻ����ź�%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
N=length(w_k);%�˲�������  240����
%pecho=zeros(1,N);
pecho=conv(array2,w_k); %%�������� ���ȱ�ΪM+N-1
pecho=pecho(1:M);%ģ��Ļ����ź�  ȡ����ԭʼ�����ź���ͬ�ĵ���
%array1= 0.05*mean(abs(pecho))*randn(1,M);

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%����N�����ݣ����ó�ֵ%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
Rin1=array2; %Զ���ź�  ��Ϊ�˲����������ź�

% Sinn=pecho;
Sinn=2*pecho+array1;%�����źš�������˵���˵������ź���������ӹ���  ��Ϊ�����ź�
Sinn=Sinn/max(Sinn);
% wavplay(pecho,8000);
% wavplay(Sinn,8000);

x1=Rin1(1:N);%������

w1=zeros(1,N);%%%�˲���ϵ���ĳ�ʼ��
w2=zeros(1,N);
a=0.0001;
u=0.01;%%����

energ_window=1/32;         %�̴����ʹ���
near_energ=0;
far_energ(1)=0;
for i=2:N-1
    far_energ(i)=(1-energ_window)*abs(far_energ(i-1))+energ_window*abs(x1(i-1));%��ʼ��Զ���źŵ�����
end;

Dtemp=x1(1:N-1)*x1(1:N-1)';%�˲��������źŵ�����
sout=zeros(1,M);
ff=0;
dd=0;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%NLMS�㷨������Ӧ�˲�%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
for i=N:M; 
    x1(N)=Rin1(i);
    x2(N)=Rin2(i);
    D=Dtemp+x1(N).^2; %%D ��Dtemp ���������
    far_energ(N)=(1-energ_window)*abs(far_energ(N-1))+energ_window*abs(x1(N-1));%Զ���ź�����
    near_energ=(1-energ_window)*abs(near_energ)+energ_window*abs(Sinn(i-1));%�����ź�����
    y1=dot(w1,x1);
    y2=dot(w2,x2);
    en=Sinn(i)-y1-y2;  
    if near_energ>1/2*max(far_energ); %%%%���жԽ������Ĺ��� 
        sout(i)=en;
        dd=dd+1;
%       sout(i)=nlp_new(en); ;%Զ��ģʽ������NLP 
    else
        
         w1=w1+u*en*x1/(a+D);   % D Dtemp ���˲��������źŵ����������� 
         w2=w2+u*en*x2/(a+D); 
         sout(i)=en;
%         sout(i)=nlp_new(en); ;%Զ��ģʽ������NLP 
        ff=ff+1;
    end;
%     mis(i)=norm(w1'-w_k)/norm(w_k);  %%????
    
    %%%%%%%%%�����˲��������źŵ�����ֵ%%%%%%%%%%
    Dtemp=D-x1(1).^2;
    x1(1:N-1)=x1(2:N);
    x2(1:N-1)=x2(2:N);
    far_energ(1:N-1)=far_energ(2:N);%%%����Զ���źŵ�����
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
end;
% figure;
% plot(array1);
% hold on;
% plot(pecho,'r');
% figure;
% plot(Sinn);
figure;
plot(Rin1);title('����Ӧ�˲����������ź�A·')
figure;
plot(Rin2);title('����Ӧ�˲����������ź�B·')
figure;
plot(array1);title('�����ź�')
figure; plot(Sinn); legend('near end signal');hold on; %���������ź�ʱ����
figure;plot(sout/max(sout),'r');legend('error signal');%��������źŵ�ʱ����
wavwrite(Sinn,sampFreq1,16,'Sinn');%%����Ӧ�˲����������ź�
% wavwrite(Rin,44100,16,'Rin');%%����Ӧ�˲��������ź�
wavwrite(sout,sampFreq1,16,'sout');%%����Ӧ�˲���������ź�
figure;subplot(4,1,1);plot(sout);title('���������ź�');
subplot(4,1,2);plot(array1);title('�����ź�')
subplot(4,1,3);plot(Sinn);title('�����ź���ز��Ļ��')
subplot(4,1,4);plot(array1);title('�ز�˥������')
% wavwrite(Sinn,8000,16,'Sinn');
% wavwrite(Rin,8000,16,'Rin');
% wavwrite(sout,8000,16,'sout');
%wavplay(sout,22050);

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%���ܲ���%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
Hd2 = dfilt.dffir(ones(1,1000));%%%%��Ƶ��˲���������

erle = filter(Hd2,(sout-array1(1:length(sout))).^2)./ ...
    (filter(Hd2,Sinn(1:length(sout)).^2));
erledB = -10*log10(erle); 
plot(erledB);title('�ز���˥������')



% P_Sinn=abs(Sinn).^2;
% P_en=abs(sout).^2;
% Ave_num=100;%��ԭʼ�����źŷ�֡ ÿ5600����Ϊһ֡
% P_num=fix(M/Ave_num);
% ERLE_temp=P_Sinn./P_en; %�����źŵ�����������źŵ�����
% for j=1:P_num;
%     Mark=(j-1)*Ave_num;
%     ERLE_win=ERLE_temp(Mark+1:Mark+Ave_num);
%     ERLE(j)=sum(ERLE_win)/Ave_num;
% end;
% figure;
% plot(ERLE);
% ERLE=10*log10(ERLE);%ȡ��Ȼ���� 
% figure;
% plot(ERLE);


% figure;
% plot(10*log10(mis));                     % plot misalignment curve
% ylabel('mis');xlabel('samples');
% title('Misalignment of NLMS algorithm');
