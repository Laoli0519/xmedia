#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h> // SIGINT

#include "webrtc/modules/audio_processing/aecm/echo_control_mobile.h"
#include "webrtc/modules/audio_processing/aec/echo_cancellation.h"

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

// #include "util.h"
#include "xwavfile.h"
#include "aec.h"

#define dbgv(...) do{  printf("<aec>[D] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbgi(...) do{  printf("<aec>[I] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)
#define dbge(...) do{  printf("<aec>[E] " __VA_ARGS__); printf("\n"); fflush(stdout); }while(0)


static 
int run_aecm(wavfile_reader_t readerNear
		, wavfile_reader_t readerFar
		, wavfile_writer_t writerOut
		, int32_t sampFreq
		, int nrOfSamples
		, int msInSndCardBuf
		, int16_t near_buf[]
		, int16_t far_buf[]
		, int16_t out_buf[] 
		, int framebytes
		, int& frameCount){

	int ret = 0;
	void* aecmInst = NULL;
	AecmConfig aecmConfig = {.cngMode=0, .echoMode=3}; 

	do{

		aecmInst = WebRtcAecm_Create();
		if(!aecmInst){
			dbge("WebRtcAecm_Create fail");
			break;
		}
		dbgi("WebRtcAecm_Create success, aecmInst=%p", aecmInst);

		ret = WebRtcAecm_Init(aecmInst, sampFreq);
		if(ret != 0){
			dbge("WebRtcAecm_Init fail ret=%d", ret);
			break;
		}
		dbgi("WebRtcAecm_Init success");

		ret = WebRtcAecm_set_config(aecmInst, aecmConfig);
		if(ret != 0){
			dbge("WebRtcAecm_set_config fail ret=%d, config=[%d, %d]", ret, aecmConfig.cngMode, aecmConfig.echoMode);
			break;
		}
		dbgi("WebRtcAecm_set_config success, config=[.cngMode=%d, .echoMode=%d]", aecmConfig.cngMode, aecmConfig.echoMode);



		
		frameCount = 0;
		while(1){
			ret = wavfile_reader_read(readerFar, far_buf, framebytes);
			if(ret != framebytes){
				dbgi("reach far-end file end");
				ret = 0;
				break;
			}

			ret = WebRtcAecm_BufferFarend(aecmInst, far_buf, nrOfSamples);
			if(ret != 0){
				dbge("WebRtcAecm_BufferFarend fail ret=%d", ret);
				break;
			}

			ret = wavfile_reader_read(readerNear, near_buf, framebytes);
			if(ret != framebytes){
				dbgi("reach near-end file end");
				ret = 0;
				break;
			}

			ret = WebRtcAecm_Process(aecmInst,
			                           near_buf, // nearendNoisy,
			                           near_buf, // nearendClean,
			                           out_buf,
			                           nrOfSamples,
			                           msInSndCardBuf);
			if(ret != 0){
				dbge("WebRtcAecm_Process fail ret=%d", ret);
				break;
			}

			//memcpy(out_buf, near_buf, framebytes);
			ret = wavfile_writer_write(writerOut, out_buf, framebytes);
			if(ret < 0){
				dbge("wavfile_writer_write fail ret=%d", ret);
				break;
			}

			ret = 0;
			frameCount++;
		}

	}while(0);

	if(aecmInst){
		dbgi("WebRtcAecm_Free aecmInst=%p", aecmInst);
		WebRtcAecm_Free(aecmInst);
		aecmInst = NULL;
	}

	return ret;
}

static
inline void copy_int16_to_float(int16_t src[], float dst[], int num){
	for(int i = 0; i < num; i++){
		dst[i] = src[i];
	}
}

static
inline void copy_float_to_int16(float src[], int16_t dst[], int num){
	for(int i = 0; i < num; i++){
		dst[i] = src[i];
	}
}

static 
int run_aec(wavfile_reader_t readerNear
		, wavfile_reader_t readerFar
		, wavfile_writer_t writerOut
		, int32_t sampFreq
		, int nrOfSamples
		, int msInSndCardBuf
		, int16_t near_buf[]
		, int16_t far_buf[]
		, int16_t out_buf[] 
		, int framebytes
		, int& frameCount){

	int ret = 0;
	void* aInst = NULL;
	// AecConfig aConfig = {.nlpMode=kAecNlpModerate, .skewMode=kAecFalse, .metricsMode=kAecFalse, .delay_logging=kAecFalse};
	// AecConfig aConfig = {.nlpMode=kAecNlpAggressive, .skewMode=kAecFalse, .metricsMode=kAecFalse, .delay_logging=kAecFalse};
	AecConfig config = {.nlpMode=kAecNlpConservative, .skewMode=kAecFalse, .metricsMode=kAecFalse, .delay_logging=kAecFalse};
	AecConfig * aConfig = NULL; // &config;
	
	do{

		aInst = WebRtcAec_Create();
		if(!aInst){
			dbge("WebRtcAec_Create fail");
			break;
		}
		dbgi("WebRtcAec_Create success, aInst=%p", aInst);

		ret = WebRtcAec_Init(aInst, sampFreq, sampFreq);
		if(ret != 0){
			dbge("WebRtcAec_Init fail ret=%d", ret);
			break;
		}
		dbgi("WebRtcAec_Init success");

		if(aConfig){
			ret = WebRtcAec_set_config(aInst, *aConfig);
			if(ret != 0){
				dbge("WebRtcAec_set_config fail ret=%d, config=[%d, %d, %d, %d]", ret, aConfig->nlpMode, aConfig->skewMode, aConfig->metricsMode, aConfig->delay_logging);
				break;
			}
			dbgi("WebRtcAec_set_config success, config=[%d, %d, %d, %d]", aConfig->nlpMode, aConfig->skewMode, aConfig->metricsMode, aConfig->delay_logging);
		}else{
			dbgi("skip set config");
		}



		float far_data[nrOfSamples] ;
		float near_data[nrOfSamples] ;
		float out_data[nrOfSamples] ;
		
		frameCount = 0;
		while(1){
			ret = wavfile_reader_read(readerFar, far_buf, framebytes);
			if(ret != framebytes){
				dbgi("reach far-end file end");
				ret = 0;
				break;
			}
			copy_int16_to_float(far_buf, far_data, nrOfSamples);

			ret = WebRtcAec_BufferFarend(aInst, far_data, nrOfSamples);
			if(ret != 0){
				dbge("WebRtcAecm_BufferFarend fail ret=%d", ret);
				break;
			}

			ret = wavfile_reader_read(readerNear, near_buf, framebytes);
			if(ret != framebytes){
				dbgi("reach near-end file end");
				ret = 0;
				break;
			}
			copy_int16_to_float(near_buf, near_data, nrOfSamples);
			float * nearend[1]  = {near_data};
			float * out[1]  = {out_data};
			ret = WebRtcAec_Process(aInst,
			                           nearend, // nearend,
			                           1, // ,
			                           out,
			                           nrOfSamples,
			                           msInSndCardBuf,
			                           0);
			if(ret != 0){
				dbge("WebRtcAec_Process fail ret=%d", ret);
				break;
			}
			copy_float_to_int16(out_data, out_buf, nrOfSamples);

			//memcpy(out_buf, near_buf, framebytes);
			ret = wavfile_writer_write(writerOut, out_buf, framebytes);
			if(ret < 0){
				dbge("wavfile_writer_write fail ret=%d", ret);
				break;
			}

			ret = 0;
			frameCount++;
		}

	}while(0);

	if(aInst){
		dbgi("WebRtcAec_Free aInst=%p", aInst);
		WebRtcAec_Free(aInst);
		aInst = NULL;
	}

	return ret;
}


static 
int run_speex(wavfile_reader_t readerNear
		, wavfile_reader_t readerFar
		, wavfile_writer_t writerOut
		, int32_t sampFreq
		, int nrOfSamples
		, int msInSndCardBuf
		, int16_t near_buf[]
		, int16_t far_buf[]
		, int16_t out_buf[] 
		, int framebytes
		, int& frameCount){

	int ret = 0;

	SpeexPreprocessState *preprocess_state = NULL;
	SpeexEchoState *echo_state = NULL;
	int  preprocess_on = 1;

	nrOfSamples = 2 * sampFreq/100; // 20 ms
	framebytes = nrOfSamples * 2;
	int frame_size = nrOfSamples;
	int filter_length =  sampFreq/10/2; // 100 ms

	dbgi("use speex echo (preprocess_on=%d)", preprocess_on);
	dbgi("frame_size=%d", frame_size);
	dbgi("filter_length=%d", filter_length);

	do{

		echo_state = speex_echo_state_init(frame_size, filter_length);
		if(!echo_state){
			dbge("speex_echo_state_init fail");
			break;
		}
		dbgi("speex_echo_state_init success, obj=%p", echo_state);

		int sampleRate = sampFreq;
		speex_echo_ctl(echo_state, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);

		if(preprocess_on){
			preprocess_state = speex_preprocess_state_init(frame_size, sampFreq);
			if(!preprocess_state){
				dbge("speex_preprocess_state_init fail");
				break;
			}
			dbgi("speex_preprocess_state_init success, obj=%p", preprocess_state);


			ret = speex_preprocess_ctl(preprocess_state, SPEEX_PREPROCESS_SET_ECHO_STATE, echo_state);
			if(ret != 0){
				dbge("speex_preprocess_ctl (SPEEX_PREPROCESS_SET_ECHO_STATE) fail, ret=%d", ret);
				break;
			}
			dbge("speex_preprocess_ctl (SPEEX_PREPROCESS_SET_ECHO_STATE) success");
		}


		
		frameCount = 0;
		while(1){
			ret = wavfile_reader_read(readerFar, far_buf, framebytes);
			if(ret != framebytes){
				dbgi("reach far-end file end");
				ret = 0;
				break;
			}

			ret = wavfile_reader_read(readerNear, near_buf, framebytes);
			if(ret != framebytes){
				dbgi("reach near-end file end");
				ret = 0;
				break;
			}

			// if(preprocess_state){
			// 	speex_preprocess_run(preprocess_state, near_buf);
			// }
			speex_preprocess_run(preprocess_state, near_buf);
			speex_echo_playback(echo_state, far_buf);
			speex_echo_capture(echo_state, near_buf, out_buf);


			// speex_echo_cancellation(echo_state, far_buf, near_buf, out_buf);
			// speex_preprocess_run(preprocess_state, out_buf);
      		


			//memcpy(out_buf, near_buf, framebytes);
			ret = wavfile_writer_write(writerOut, out_buf, framebytes);
			if(ret < 0){
				dbge("wavfile_writer_write fail ret=%d", ret);
				break;
			}

			ret = 0;
			frameCount++;
		}

	}while(0);

	if(preprocess_state){
		dbgi("speex_preprocess_state_destroy aInst=%p", preprocess_state);
		speex_preprocess_state_destroy(preprocess_state);
		preprocess_state = NULL;
	}

	if(echo_state){
		dbgi("speex_echo_state_destroy aInst=%p", echo_state);
		speex_echo_state_destroy(echo_state);
		echo_state = NULL;
	}

	return ret;
}

static 
int run_kphone_aec(wavfile_reader_t readerNear
		, wavfile_reader_t readerFar
		, wavfile_writer_t writerOut
		, int32_t sampFreq
		, int nrOfSamples
		, int msInSndCardBuf
		, int16_t near_buf[]
		, int16_t far_buf[]
		, int16_t out_buf[] 
		, int framebytes
		, int& frameCount){

	int ret = 0;

	AEC aec;
	// aec.setambient(MAXPCM*dB2q(1.0));    

	int update = 1;
	nrOfSamples = 2 * sampFreq/100; // 20 ms
	framebytes = nrOfSamples * 2;
	// int frame_size = nrOfSamples;
	// int filter_length =  sampFreq/10/2; // 100 ms

	dbgi("use kphone aec ");
	// dbgi("frame_size=%d", frame_size);
	// dbgi("filter_length=%d", filter_length);


	do{
		
		frameCount = 0;
		while(1){
			ret = wavfile_reader_read(readerFar, far_buf, framebytes);
			if(ret != framebytes){
				dbgi("reach far-end file end");
				ret = 0;
				break;
			}

			ret = wavfile_reader_read(readerNear, near_buf, framebytes);
			if(ret != framebytes){
				dbgi("reach near-end file end");
				ret = 0;
				break;
			}

			
			// if(update && frameCount > 1000){
			// 	dbgi("change update to zero when frameCount=%d", frameCount);
			// 	update = 0;
			// }

			for(int i = 0; i < nrOfSamples; i++){
				int s0 = near_buf[i];
				int s1 = far_buf[i];
				int y=0;
				s0 = aec.doAEC(s0, s1, update, &y);
				out_buf[i] = s0;
				// out_buf[i] = y;
			}

			//memcpy(out_buf, near_buf, framebytes);
			ret = wavfile_writer_write(writerOut, out_buf, framebytes);
			if(ret < 0){
				dbge("wavfile_writer_write fail ret=%d", ret);
				break;
			}

			ret = 0;
			frameCount++;
		}

	}while(0);

	return ret;
}




int main(int argc, char** argv){

	// const char * fileNear = "tmp/input.wav"; // "tmp/input.wav"; "tmp/mix.wav";
	// const char * fileFar = "tmp/reverse.wav";
	// const char * fileOut = "tmp/out.wav";

	const char * fileNear = "tmp/input.wav"; // "tmp/input.wav"; "tmp/mix.wav";
	const char * fileFar = "tmp/reverse.wav";
	const char * fileOut = "tmp/out.wav";

	int ret = 0;
	wavfile_reader_t readerNear = NULL;
	wavfile_reader_t readerFar = NULL;
	wavfile_writer_t writerOut = NULL;

	int32_t sampFreq = 16000;
	int nrOfSamples = 160;

	// int msInSndCardBuf = 150; // milliseconds
	int msInSndCardBuf = 150; // milliseconds
	// int msInSndCardBuf = 300; // milliseconds

	int dropMilliSeconds = 300;
	
	int16_t near_buf[nrOfSamples*10];
	int16_t far_buf[nrOfSamples*10];
	int16_t out_buf[nrOfSamples*10];

	if(argc > 3){
		fileNear = argv[1];
		fileFar = argv[2];
		fileOut = argv[3];
	}

	if(argc > 4){
		dropMilliSeconds = atoi(argv[4]);
	}

	if(argc > 5){
		msInSndCardBuf = atoi(argv[5]);
	}


	dbgi("usage: %s [fileNear] [fileFar] [fileOut] [dropMilliSeconds] [msInSndCardBuf]", argv[0]);
	dbgi("fileNear = %s", fileNear);
	dbgi("fileFar = %s", fileFar);
	dbgi("fileOut = %s", fileOut);

	dbgi("sampFreq = %d", sampFreq);
	dbgi("nrOfSamples = %d", nrOfSamples);
	dbgi("msInSndCardBuf = %d", msInSndCardBuf);
	dbgi("dropMilliSeconds = %d", dropMilliSeconds);

	do{
		readerNear = wavfile_reader_open(fileNear);
		if(!readerNear){
			dbge("fail to open %s", fileNear);
			ret = -1;
			break;
		}
		dbge("successfully open %s", fileNear);

		readerFar = wavfile_reader_open(fileFar);
		if(!readerFar){
			dbge("fail to open %s", fileFar);
			ret = -1;
			break;
		}
		dbge("successfully open %s", fileFar);

		writerOut = wavfile_writer_open_with_reader(fileOut, readerNear);
		if(!writerOut){
			dbge("fail to open %s", fileOut);
			ret = -1;
			break;
		}
		dbge("successfully open %s", fileOut);


		int framebytes = nrOfSamples * sizeof(int16_t);
		int frameCount = 0;

		int samplerate = wavfile_reader_samplerate(readerFar);
		int dropSamples = samplerate * dropMilliSeconds / 1000;
		int dropFrames = (dropSamples + (nrOfSamples-1)) /nrOfSamples;
		// dropFrames *= 2;
		// dropFrames = 30;
		dbgi("drop: samplerate=%d, dropSamples=%d, dropFrames=%d", samplerate, dropSamples, dropFrames);
		while(frameCount < dropFrames){
			ret = wavfile_reader_read(readerFar, far_buf, framebytes);
			if(ret != framebytes){
				dbgi("unexpectd reaching far-end file end");
				ret = -1;
				break;
			}
			ret = 0;
			frameCount++;
		}
		if(ret) break;
		dbgi("dropped frames %d", frameCount);

		// run_aecm
		// run_aec
		// run_speex
		// run_kphone_aec
		ret =  run_kphone_aec( readerNear
		,  readerFar
		,  writerOut
		,  sampFreq
		,  nrOfSamples
		,  msInSndCardBuf
		,  near_buf
		,  far_buf
		,  out_buf
		,  framebytes
		,  frameCount);


		dbgi("processed frameCount = %d", frameCount);
		dbgi("processed samples = %d", frameCount*nrOfSamples);
		if(ret == 0){
			dbgi("process OK");
		}else{
			dbgi("process fail !!!");
		}

	}while(0);



	if(readerNear){
		wavfile_reader_close(readerNear);
		readerNear = NULL;
	}

	if(readerFar){
		wavfile_reader_close(readerFar);
		readerFar = NULL;
	}

	if(writerOut){
		wavfile_writer_close(writerOut);
		writerOut = NULL;
	}

	dbgi("bye!");

}

