


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <chrono>
// for open h264 raw file.
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// #include "../../objs/include/srs_librtmp.h"
#include "srs_librtmp.h"
#include "Lab030-RTMPPush.hpp"

// see https://github.com/ossrs/srs/wiki/v2_CN_SrsLibrtmp
// see https://blog.csdn.net/win_lin/article/details/41170653
// see https://github.com/ossrs/srs/blob/master/trunk/research/librtmp/srs_h264_raw_publish.c
// see https://github.com/ossrs/srs/blob/master/trunk/research/librtmp/srs_aac_raw_publish.c
// see http://winlinvip.github.io/srs.release/3rdparty/720p.h264.raw
// see http://winlinvip.github.io/srs.release/3rdparty/audio.raw.aac



#define dbgd(...) srs_human_verbose(__VA_ARGS__)
#define dbgi(...) srs_human_trace(__VA_ARGS__)
#define dbge(...) srs_human_error(__VA_ARGS__)
//#define dbgi(...) do{printf(__VA_ARGS__); printf("\n");}while(0)


static
int found_nalu_annex(uint8_t * data, size_t data_size, size_t& pos, size_t& nalu_pos){
    int found_annex = 0;
    size_t last_check_pos = (data_size-4);
    for(;pos < last_check_pos; ++pos){
        if(data[pos+0] == 0 && data[pos+1] == 0){
            if(data[pos+2] == 1){
                nalu_pos = pos+3;
                found_annex = 1;
                break;
            }else if(data[pos+2] == 0 && data[pos+3] == 1){
                nalu_pos = pos+4;
                found_annex = 1;
                break;
            }
        }
    }
    if(!found_annex){
        if(pos == last_check_pos){
            ++pos;
            if(data[pos+0] == 0 && data[pos+1] == 0 && data[pos+2] == 1){
                nalu_pos = pos+3;
                found_annex = 1;
                return found_annex;
            }
        }
        pos = data_size;
        nalu_pos = pos;
    }
    return found_annex;
}

static
int read_h264_next_nalu(uint8_t * data, size_t data_size, size_t& pos, size_t &length, size_t& nalu_pos){
    
    pos += length;
    if(pos >= data_size){
        return -1;
    }
    
    size_t start_pos = pos;
    size_t next_nalu_pos = nalu_pos;
    int found_annex = found_nalu_annex(data, data_size, pos, next_nalu_pos);
    if(!found_annex){
        return -2;
    }
    
    start_pos = pos;
    nalu_pos = next_nalu_pos;
    pos = next_nalu_pos;
    found_annex = found_nalu_annex(data, data_size, pos, next_nalu_pos);
    length = pos - nalu_pos;
    
    pos = start_pos;
    return 0;
}

static
void dump_video_buf(uint8_t *raw_video_data, size_t raw_video_size){
    size_t raw_video_pos = 0;
    size_t raw_video_len = 0;
    int ret = 0;
    while(1){
        size_t nalu_pos = raw_video_pos;
        ret = read_h264_next_nalu(raw_video_data, raw_video_size, raw_video_pos, raw_video_len, nalu_pos);
        if(ret < 0){
            break;
        }
        dbgi("nalu: pos=%ld, nalu=%ld, len=%ld, end=%ld", raw_video_pos, nalu_pos, raw_video_len, nalu_pos+raw_video_len);
    }
}

static
int load_raw_file(const char * fname, bool dump, uint8_t **buf, size_t * buf_size){
    FILE * fp_rtmp_video = NULL;
    uint8_t * raw_video_data = NULL;
    size_t raw_video_size = 0;
    
    int ret = 0;
    do{
        fp_rtmp_video = fopen(fname, "rb");
        if(!fp_rtmp_video){
            dbge("fail to open video file [%s]", fname);
            break;
        }
        ret = fseek(fp_rtmp_video, 0, SEEK_END);
        raw_video_size = ftell(fp_rtmp_video);
        ret = fseek(fp_rtmp_video, 0, SEEK_SET);
        dbgi("video file size %ld", raw_video_size);
        if(raw_video_size <= 0){
            dbge("invalid video file size");
            ret = -1;
            break;
        }
        raw_video_data = (uint8_t *) malloc(raw_video_size);
        long bytes = fread(raw_video_data, 1, raw_video_size, fp_rtmp_video);
        if(bytes != raw_video_size){
            dbge("read video file fail, %ld", bytes);
            ret = -1;
            break;
        }
        
        if(dump){
            dump_video_buf(raw_video_data, raw_video_size);
        }
        
        if(buf){
            *buf = raw_video_data;
            raw_video_data = NULL;
        }
        if(buf_size){
            *buf_size = raw_video_size;
            raw_video_size = 0;
        }
        ret = 0;
        
    }while(0);
    if(fp_rtmp_video){
        fclose(fp_rtmp_video);
        fp_rtmp_video = NULL;
    }
    if(raw_video_data){
        free(raw_video_data);
        raw_video_data = NULL;
    }
    return ret;
}

static
int64_t get_now_ms(){
    unsigned long milliseconds_since_epoch =
    std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::system_clock::now().time_since_epoch()).count();
    return milliseconds_since_epoch;
};

static
int read_audio_frame(char* data, int size, char** pp, char** frame, int* frame_size)
{
    char* p = *pp;
    
    if (!srs_aac_is_adts(p, (int)(size - (p - data))) ) {
        srs_human_trace("aac adts raw data invalid.");
        return -1;
    }
    
    // @see srs_audio_write_raw_frame
    // each frame prefixed aac adts header, '1111 1111 1111'B, that is 0xFFF.,
    // for instance, frame = FF F1 5C 80 13 A0 FC 00 D0 33 83 E8 5B
    *frame = p;
    // skip some data.
    // @remark, user donot need to do this.
    p += srs_aac_adts_frame_size(p, (int)(size - (p - data)) );
    
    *pp = p;
    *frame_size = (int)(p - *frame);
    if (*frame_size <= 0) {
        srs_human_trace("aac adts raw data invalid.");
        return -1;
    }
    
    return 0;
}


class H264FileReader{
    uint8_t * buf_ = NULL;
    size_t buf_size_;
    size_t next_read_pos_ = 0;
    size_t next_data_pos_ = 0;
    size_t next_data_len_ = 0;
    int64_t interval_ms_ = 1000/25;
    int64_t next_time_ms_ = 0;
    int frameNum_ = 0;
public:
    H264FileReader(){
        
    }
    virtual ~H264FileReader(){
        close();
    }
    int open(const char * filename, int fps){
        int ret = 0;
        do{
            close();
            ret = load_raw_file(filename, false, &buf_, &buf_size_);
            if(ret){
                break;
            }
            ret = next();
            if(ret){
                break;
            }
            interval_ms_ = 1000/fps;
            next_time_ms_ = 0;
            ret = 0;
        }while(0);
        if(ret){
            close();
        }
        return ret;
    }
    void close(){
        if(buf_){
            free(buf_);
            buf_ = NULL;
        }
        rewind();
        frameNum_ = 0;
    }
    int rewind(){
        next_read_pos_ = 0;
        next_data_pos_ = 0;
        next_data_len_ = 0;
        if(buf_){
            return next();
        }else{
            return -1;
        }
    }
    int next(){
        int ret = read_h264_next_nalu(buf_, buf_size_, next_read_pos_, next_data_len_, next_data_pos_);
        if(ret){
            return ret;
        }
        ++frameNum_;
        next_time_ms_ += interval_ms_;
        uint8_t nut = payload()[0]& 0x1f;
        if((nut != 7) && (nut != 8)){
//            next_time_ms_ += interval_ms_;
        }
        return ret;
    }
    int frameNum(){
        return frameNum_;
    }
    uint8_t * data(){
        return &buf_[next_read_pos_];
    }
    size_t dataLength(){
        return next_data_pos_ - next_read_pos_ + next_data_len_;
    }
    uint8_t * payload(){
        return &buf_[next_data_pos_];
    }
    size_t payloadLength(){
        return next_data_len_;
    }
    int64_t timestamp(){
        return next_time_ms_;
    }
};

class AACFileReader{
    uint8_t * buf_ = NULL;
    size_t buf_size_;
    char * next_p_ = 0;
    uint8_t * next_data_ = 0;
    size_t next_data_len_ = 0;
    int64_t frame_duration_ = 40;
    int64_t next_time_ms_ = 0;
    int frameNum_ = 0;
public:
    AACFileReader(){
        
    }
    virtual ~AACFileReader(){
        close();
    }
    int open(const char * filename, int64_t frame_duration){
        int ret = 0;
        do{
            close();
            ret = load_raw_file(filename, false, &buf_, &buf_size_);
            if(ret){
                break;
            }
            frame_duration_ = frame_duration;
            next_p_ = (char*)buf_;
            ret = next();
            if(ret){
                break;
            }
            next_time_ms_ = 0;
            ret = 0;
        }while(0);
        if(ret){
            close();
        }
        return ret;
    }
    void close(){
        rewind();
        if(buf_){
            free(buf_);
            buf_ = NULL;
        }
        frameNum_ = 0;
    }
    int rewind(){
        next_p_ = (char*)buf_;
        next_data_ = buf_;
        next_data_len_ = 0;
        if(buf_){
            return next();
        }else{
            return -1;
        }
    }
    int next(){
        char * data = NULL;
        int sz = 0;
        int ret = read_audio_frame((char*)buf_, (int)buf_size_, &next_p_, &data, &sz);
        if(ret){
            return ret;
        }
        next_data_ = (uint8_t*)data;
        next_data_len_ = sz;
        next_time_ms_ += frame_duration_;
        ++frameNum_;
        return ret;
    }
    int frameNum(){
        return frameNum_;
    }
    uint8_t * data(){
        return next_data_;
    }
    size_t dataLength(){
        return next_data_len_;
    }
    uint8_t * payload(){
        return data();
    }
    size_t payloadLength(){
        return dataLength();
    }
    int64_t timestamp(){
        return next_time_ms_;
    }
};

static
int push_video_frame(srs_rtmp_t rtmp, H264FileReader * video){
    int ret = 0;
    do{
        ret = srs_h264_write_raw_frames(rtmp, (char*)video->data(), (int)video->dataLength(), (int)video->timestamp(), (int)video->timestamp());
        if (ret != 0) {
            const char * errmsg = "";
            if (srs_h264_is_dvbsp_error(ret)) {
                errmsg = "ignore drop video";
            } else if (srs_h264_is_duplicated_sps_error(ret)) {
                errmsg = "ignore dup sps";
            } else if (srs_h264_is_duplicated_pps_error(ret)) {
                errmsg = "ignore dup pps";
            } else {
                srs_human_trace("|No.%04d| video fail with known ret=%d", video->frameNum(), ret);
                break;
            }
            srs_human_trace("|No.%04d| video warn=[%s], ret=%d", video->frameNum(), errmsg,  ret);
        }else{
            // 5bits, 7.3.1 NAL unit syntax,
            // ISO_IEC_14496-10-AVC-2003.pdf, page 44.
            //  7: SPS, 8: PPS, 5: I Frame, 1: P Frame, 9: AUD, 6: SEI
            uint8_t nut = video->payload()[0] & 0x1f;
            const char * nutname=(nut == 7? "SPS":(nut == 8? "PPS":(nut == 5? "I":(nut == 1? "P":(nut == 9? "AUD":(nut == 6? "SEI":"Unknown"))))));
            
            srs_human_trace("|No.%04d| sent video, ts=%lld, sz=%ld, h264=[%s]", video->frameNum(), video->timestamp(), video->dataLength(), nutname);
        }
        ret = video->next();
        if(ret){
            ret = video->rewind();
        }
    }while(0);
    return ret;
}

static
int push_audio_frame(srs_rtmp_t rtmp, AACFileReader * audio){
    int ret = 0;
    do {
        // 0 = Linear PCM, platform endian
        // 1 = ADPCM
        // 2 = MP3
        // 7 = G.711 A-law logarithmic PCM
        // 8 = G.711 mu-law logarithmic PCM
        // 10 = AAC
        // 11 = Speex
        char sound_format = 10;
        // 2 = 22 kHz
        char sound_rate = 2;
        // 1 = 16-bit samples
        char sound_size = 1;
        // 1 = Stereo sound
        char sound_type = 1;
        
        ret = srs_audio_write_raw_frame(rtmp,
                                        sound_format, sound_rate, sound_size, sound_type,
                                        (char*)audio->data(), (int)audio->dataLength(), (uint32_t)audio->timestamp());
        if (ret) {
            srs_human_trace("|No.%04d| send audio fail, ret=%d", audio->frameNum(),  ret);
            ret = -1;
            break;
        }
        srs_human_trace("|No.%04d| sent audio, ts=%lld, sz=%ld", audio->frameNum()
                        , audio->timestamp()
                        , audio->dataLength());
        ret = audio->next();
        if(ret){
            ret = audio->rewind();
        }
    } while (0);
    return ret;
}

int main_push_av(int argc, char** argv)
{
    const char* video_raw_file = NULL;
    const char* audio_raw_file = NULL;
    const char* rtmp_url = NULL;
    
    rtmp_url = "rtmp://127.0.0.1/myapp/raw";
    video_raw_file = "/Users/simon/Downloads/720p.h264.raw";
    audio_raw_file = "/Users/simon/Downloads/audio.raw.aac";
    
    dbgi("video_raw_file=[%s]", video_raw_file);
    dbgi("audio_raw_file=[%s]", audio_raw_file);
    dbgi("rtmp_url=[%s]", rtmp_url);
    
    srs_rtmp_t rtmp = 0;
    H264FileReader * video = NULL;
    AACFileReader * audio = NULL;
    int ret = 0;
    do{
        if(video_raw_file){
            video = new H264FileReader();
            ret = video->open(video_raw_file, 25);
            if(ret){
                break;
            }
        }
        if(audio_raw_file){
            audio = new AACFileReader();
            ret = audio->open(audio_raw_file, 45);
            if(ret){
                break;
            }
        }
        
        // connect rtmp context
        rtmp = srs_rtmp_create(rtmp_url);
        
        if (srs_rtmp_handshake(rtmp) != 0) {
            dbge("simple handshake failed.");
            ret = -1;
            break;
        }
        dbgi("simple handshake success");
        
        if (srs_rtmp_connect_app(rtmp) != 0) {
            dbge("connect vhost/app failed.");
            ret = -1;
            break;
        }
        dbgi("connect vhost/app success");
        
        if (srs_rtmp_publish_stream(rtmp) != 0) {
            dbge("publish stream failed.");
            ret = -1;
            break;
        }
        dbgi("publish stream success");
        const int64_t MIN_DIFF = 0;
        int64_t start_time_ms = get_now_ms();
        while(1){
            int64_t elapsed_ms = get_now_ms() - start_time_ms;
            if(video && (video->timestamp() - elapsed_ms) <= MIN_DIFF){
//            if(video){
                ret = push_video_frame(rtmp, video);
                if(ret) break;
            }
            if(audio && (audio->timestamp() - elapsed_ms) <= MIN_DIFF){
                ret = push_audio_frame(rtmp, audio);
                if(ret) break;
            }
            // get min timestamp of next
            int64_t next_ts = 0x00FFFFFFFFFFFFFF;
            if(video && video->timestamp() <= next_ts){
                next_ts = video->timestamp();
            }
            if(audio && audio->timestamp() <= next_ts){
                next_ts = audio->timestamp();
            }
            int64_t ms = start_time_ms + next_ts - get_now_ms();
            if(ms > 0){
//                dbgi("next_ts=%lld(+%lld)", next_ts, ms);
                usleep((unsigned)(1000*ms));
            }
            
            ret = 0;
        }// loop
        
    }while (0);
    
    if(rtmp){
        srs_rtmp_destroy(rtmp);
        rtmp = 0;
    }
    
    if(video){
        delete video;
        video = NULL;
    }
    
    if(audio){
        delete audio;
        audio = NULL;
    }
    
    return 0;
}

#include <thread>
#include "FFMpegFramework.hpp"
#include "NUtil.hpp"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
//#include "rav_record.h"

#define odbgd(FMT, ARGS...) do{  printf("|%7s|D| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%7s|I| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%7s|E| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)




class FFVideoEncoder{
public:
//    /static const AVCodecID DEFAULT_CODEC_ID = AV_CODEC_ID_H264;
    static const AVPixelFormat DEFAULT_PIX_FMT = AV_PIX_FMT_YUV420P;
    static const int DEFAULT_WIDTH = 640;
    static const int DEFAULT_HEIGHT = 480;
    static const int DEFAULT_FRAMERATE = 25;
    static const int DEFAULT_GOPSIZE = DEFAULT_FRAMERATE;
    static const int64_t DEFAULT_BITRATE = 500000;
    
public:
    FFVideoEncoder():timebase_({1, 1000}){
        srcImgCfg_.setFormat(DEFAULT_PIX_FMT);
        srcImgCfg_.setWidth(DEFAULT_WIDTH);
        srcImgCfg_.setHeight(DEFAULT_HEIGHT);
    }
    
    virtual ~FFVideoEncoder(){
        close();
    }
    
    void setCodecName(const std::string& codec_name){
        codec_ = avcodec_find_encoder_by_name(codec_name.c_str());
        if(codec_){
            codecId_ = codec_->id;
        }else{
            codecId_ = AV_CODEC_ID_NONE;
        }
    }
    
    void setCodecId(AVCodecID codec_id){
        codecId_ = codec_id;
    }
    
    void setFramerate(int framerate){
        framerate_ = framerate;
    }
    
    void setBitrate(int64_t bitrate){
        bitrate_ = bitrate;
    }
    
    void setGopSize(int gop_size){
        gopSize_ = gop_size;
    }
    
    void setSrcImageConfig(const FFImageConfig& cfg){
        srcImgCfg_ = cfg;
    }
    
    void setTimebase(const AVRational& timebase){
        timebase_ = timebase;
    }
    
    void setCodecOpt(const std::string& k, const std::string& v){
        codecOpts_[k] = v;
    }
    
    AVCodecID codecId()const{
        return codecId_;
    }
    
    const FFImageConfig& encodeImageCfg() const{
        return encImgCfg_;
    }
    
    int open(){
        int ret = 0;
        do{
            
            AVCodecID codec_id = (AVCodecID)codecId_;
            if(codec_id <= AV_CODEC_ID_NONE){
                ret = -1;
                NERROR_FMT_SET(ret, "NOT spec video encoder");
                break;
            }
            
            const AVCodec * codec = codec_;
            if(!codec){
                codec = avcodec_find_encoder(codec_id);
                if (!codec) {
                    ret = -1;
                    NERROR_FMT_SET(ret, "fail to avcodec_find_encoder video id [{}]",
                                   (int)codec_id);
                    break;
                }
            }
            
            ctx_ = avcodec_alloc_context3(codec);
            if (!ctx_) {
                ret = -1;
                NERROR_FMT_SET(ret, "fail to avcodec_alloc_context3 video codec [{}]-[{}]",
                                     (int)codec->id, codec->name);
                break;
            }
            
            pkt_ = av_packet_alloc();
            if (!pkt_){
                ret = -1;
                NERROR_FMT_SET(ret, "can't allocate video packet");
                break;
            }
            
            ctx_->bit_rate = bitrate_ > 0 ? bitrate_ : DEFAULT_BITRATE;
            
            // TODO:
            ctx_->max_b_frames = 0; // aaa
            //ctx_->flags |= CODEC_FLAG_QSCALE;
            ctx_->rc_min_rate = ctx_->bit_rate*2/3;
            ctx_->rc_max_rate = ctx_->bit_rate *2;
            ctx_->rc_buffer_size = (int)(ctx_->rc_max_rate * 2);
            
            ctx_->width = srcImgCfg_.getWidth(DEFAULT_WIDTH);
            ctx_->height = srcImgCfg_.getHeight(DEFAULT_HEIGHT);
            
            ctx_->time_base = timebase_;
            
            ctx_->framerate = (AVRational){framerate_ > 0? framerate_ : DEFAULT_FRAMERATE, 1};
            
            
            /* emit one intra frame every ten frames
             * check frame pict_type before passing frame
             * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
             * then gop_size is ignored and the output of encoder
             * will always be I frame irrespective to gop_size
             */
            ctx_->gop_size = gopSize_ > 0 ? gopSize_ : DEFAULT_GOPSIZE;
            
            
            ctx_->pix_fmt = selectPixelFormat(codec, srcImgCfg_.getFormat());
            for(auto& it : codecOpts_){
                av_opt_set(ctx_->priv_data, it.first.c_str(), it.second.c_str(), 0);
            }
            
//            if (codec->id == AV_CODEC_ID_H264){
//                //av_opt_set(ctx_->priv_data, "preset", "slow", 0);
//                av_opt_set(ctx_->priv_data, "preset", "fast", 0);
//                //av_opt_set(ctx_->priv_data, "preset", "fast", AV_OPT_SEARCH_CHILDREN);
//                /*
//                ultrafast
//                superfast
//                veryfast
//                faster
//                fast
//                medium – default preset
//                slow
//                slower
//                veryslow
//                 */
//            }
            
            ret = avcodec_open2(ctx_, codec, NULL);
            if (ret != 0) {
                NERROR_FMT_SET(ret, "fail to avcodec_open2 video codec [{}]-[{}], err=[{}]",
                                     (int)codec->id, codec->name, av_err2str(ret));
                break;
            }
            
            encImgCfg_.setFormat(ctx_->pix_fmt);
            encImgCfg_.setWidth(ctx_->width);
            encImgCfg_.setHeight(ctx_->height);
            srcImgCfg_.match(encImgCfg_);
            ret = adatper_.open(srcImgCfg_, encImgCfg_);
            if(ret != 0){
                break;
            }
            
            ret = 0;
        }while(0);
        
        if(ret){
            close();
        }
        
        return ret;
    }
    
    void close(){
        if(ctx_){
            avcodec_free_context(&ctx_);
            ctx_ = nullptr;
        }
        
        if(pkt_){
            av_packet_free(&pkt_);
            pkt_ = nullptr;
        }
        adatper_.close();
        codecOpts_.clear();
    }
    
    int encode(const AVFrame *srcframe, const FFPacketFunc& func){
        int ret = 0;
        if(srcframe){
            ret = adatper_.adapt(srcframe, [this, &func](const AVFrame * dstframe)->int{
                return doEncode(dstframe, func);
            });
        }else{
            ret = doEncode(nullptr, func);
        }

        return ret;
    }

private:
    int doEncode(const AVFrame *frame, const FFPacketFunc& func){
        int ret = 0;
        ret = avcodec_send_frame(ctx_, frame);
        if (ret != 0) {
            NERROR_FMT_SET(ret, "fail to avcodec_send_frame video, err=[{}]", av_err2str(ret));
            return ret;
        }
        
        while (ret >= 0) {
            ret = avcodec_receive_packet(ctx_, pkt_);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
                return 0;
            }else if (ret != 0) {
                NERROR_FMT_SET(ret, "fail to avcodec_receive_packet video, err=[{}]", av_err2str(ret));
                return ret;
            }
            ret = func(pkt_);
            av_packet_unref(pkt_);
        }
        return ret;
    }
    
    static AVPixelFormat selectPixelFormat(const AVCodec *codec, AVPixelFormat prefer_fmt){
        if(!codec->pix_fmts || *codec->pix_fmts <= AV_PIX_FMT_NONE){
            // no supported format
            if(prefer_fmt > AV_PIX_FMT_NONE){
                // select prefer format
                return prefer_fmt;
            }else{
                // select default format
                return DEFAULT_PIX_FMT;
            }
        }
        
        const enum AVPixelFormat *p = codec->pix_fmts;
        while (*p != AV_PIX_FMT_NONE) {
            if (*p == prefer_fmt){
                // match, select it
                return prefer_fmt;
            }
            p++;
        }
        // NOT match, select first supported format
        return *codec->pix_fmts;
    }
    
private:
    AVCodecID codecId_ = AV_CODEC_ID_NONE;
    const AVCodec * codec_ = nullptr;
    int framerate_ = -1;
    int gopSize_ = -1;
    int64_t bitrate_ = -1;
    AVCodecContext * ctx_ = nullptr;
    AVPacket * pkt_ = nullptr;
    FFImageAdapter adatper_;
    FFImageConfig srcImgCfg_;
    FFImageConfig encImgCfg_;
    AVRational timebase_;
    std::map<std::string, std::string> codecOpts_;
};


class FFFramesizeConverter{
public:
    FFFramesizeConverter():outTimebase_({1, 1000}){
        
    }
    
    virtual ~FFFramesizeConverter(){
        close();
    }
    
    int open(const AVRational& timebase, int framesize){
        close();
        outTimebase_ = timebase;
        dstCfg_.setFrameSize(framesize);
        return 0;
    }
    
    void close(){
        if(outFrame_){
            av_frame_free(&outFrame_);
            outFrame_ = nullptr;
        }
        dstCfg_.reset();
    }
    
    int convert(const AVFrame * srcframe, const FFFrameFunc& func){
        int ret = 0;
        if(!srcframe){
            if(outFrame_ && outFrame_->nb_samples > 0){
                // fflush remain samples
                ret = func(outFrame_);
            }
            return ret;
        }
        
        if(!outFrame_){
            if(srcframe->nb_samples == dstCfg_.getFrameSize() || dstCfg_.getFrameSize() <= 0){
                // same frame size, output directly
                ret = func(srcframe);
                return ret;
            }
            
            dstCfg_.assign(srcframe);
            outFrame_ = av_frame_alloc();
        }
        
        outFrame_->pts = srcframe->pts - samples2Duration(outFrame_->nb_samples);
        
        int dst_offset = outFrame_->nb_samples;
        int src_offset = 0;
        while(src_offset < srcframe->nb_samples){
            checkAssignFrame();
            int remain_samples = srcframe->nb_samples-src_offset;
            int min = std::min(remain_samples+dst_offset, dstCfg_.getFrameSize());
            int num_copy = min - dst_offset;
            
            av_samples_copy(outFrame_->data, srcframe->data,
                            dst_offset,
                            src_offset,
                            num_copy, outFrame_->channels, (AVSampleFormat)outFrame_->format);
            dst_offset += num_copy;
            src_offset += num_copy;
            outFrame_->nb_samples = dst_offset;
            if(outFrame_->nb_samples == dstCfg_.getFrameSize()){
                outFrame_->pkt_size = av_samples_get_buffer_size(outFrame_->linesize, outFrame_->channels,
                                                                 outFrame_->nb_samples, (AVSampleFormat)outFrame_->format,
                                                                 1);
                ret = func(outFrame_);
                av_frame_unref(outFrame_);
                if(ret != 0){
                    return ret;
                }
                outFrame_->pts = outFrame_->pts + samples2Duration(outFrame_->nb_samples);
                outFrame_->nb_samples = 0;
                dst_offset = 0;
            }
        }
        return 0;
    }
    
private:
    int64_t samples2Duration(int out_samples){
        int64_t duration = out_samples * outTimebase_.den / dstCfg_.getSampleRate() / outTimebase_.num;
        return duration;
    }
    
    void checkAssignFrame(){
        if(!outFrame_->buf[0]){
            assignFrame();
            outFrame_->nb_samples = dstCfg_.getFrameSize();
            if(outFrame_->nb_samples > 0){
                av_frame_get_buffer(outFrame_, 0);
            }
            outFrame_->nb_samples = 0;
        }
    }
    
    void assignFrame(){
        outFrame_->sample_rate = dstCfg_.getSampleRate();
        outFrame_->format = dstCfg_.getFormat();
        outFrame_->channel_layout = dstCfg_.getChLayout();
        outFrame_->channels = av_get_channel_layout_nb_channels(outFrame_->channel_layout);
    }
    
private:
    //int framesize_ = 0;
    AVRational outTimebase_;
    AVFrame * outFrame_ = nullptr;
    FFSampleConfig dstCfg_;
};




class FFSampleConverter{
public:
    FFSampleConverter():timebase_({1, 1000}){
        
    }
    
    virtual ~FFSampleConverter(){
        close();
    }
    
    int open(const FFSampleConfig& srccfg, const FFSampleConfig& dstcfg, const AVRational& timebase){
        close();
        
        srccfg_ = srccfg;
        dstcfg_ = dstcfg;
        timebase_ = timebase;
        
        int ret = 0;
        if(!srccfg.equalBase(dstcfg)){
            
            swrctx_ = swr_alloc_set_opts(
                                        NULL,
                                        dstcfg.getChLayout(),
                                        dstcfg.getFormat(),
                                        dstcfg.getSampleRate(),
                                        srccfg.getChLayout(),
                                        srccfg.getFormat(),
                                        srccfg.getSampleRate(),
                                        0, NULL);
            if(!swrctx_){
                NERROR_FMT_SET(ret, "fail to swr_alloc_set_opts");
                return -61;
            }
            
            ret = swr_init(swrctx_);
            if(ret != 0){
                NERROR_FMT_SET(ret, "fail to swr_init , err=[{}]", av_err2str(ret));
                return ret;
            }
            
            // alloc buffer
            //reallocBuffer(dstcfg_.framesize);
            
            frame_ = av_frame_alloc();
            
        }else{
            ret = sizeConverter_.open(timebase_, dstcfg_.getFrameSize());
        }
        return ret;
    }
    
    void close(){
        if(swrctx_){
            if(swr_is_initialized(swrctx_)){
                swr_close(swrctx_);
            }
            swr_free(&swrctx_);
        }
        
        if(frame_){
            av_frame_free(&frame_);
            frame_ = nullptr;
        }
        
        sizeConverter_.close();
        realDstFramesize_ = 0;
    }
    
    int convert(const AVFrame * srcframe, const FFFrameFunc& func){
        if(!swrctx_){
            return sizeConverter_.convert(srcframe, func);
        }
        int ret = 0;
        if(!srcframe){
            if(remainDstSamples_ > 0){
                // pull remain samples
                checkAssignFrame();
                frame_->pts = nextDstPTS_;
                ret = swr_convert_frame(swrctx_, frame_, NULL);
                if(ret != 0){
                    av_frame_unref(frame_);
                    NERROR_FMT_SET(ret, "fail to swr_convert_frame 1st, err=[{}]", av_err2str(ret));
                    return ret;
                }
                frame_->pkt_size = av_samples_get_buffer_size(frame_->linesize, frame_->channels,
                                                              frame_->nb_samples, (AVSampleFormat)frame_->format, 1);
                if(frame_->nb_samples > 0){
                    ret = func(frame_);
                }
                av_frame_unref(frame_);
            }
            return ret;
        }
        
        int out_samples = swr_get_out_samples(swrctx_, srcframe->nb_samples);
        if(out_samples < realDstFramesize_){
            // not enough output samples
            ret = swr_convert_frame(swrctx_, NULL, srcframe);
            if(ret != 0){
                NERROR_FMT_SET(ret, "fail to swr_convert_frame 2nd, err=[{}]", av_err2str(ret));
                return ret;
            }
            return 0;
        }
        
//        if(dstcfg_.framesize <= 0){
//            reallocBuffer(srcframe->nb_samples);
//        }
        
        checkAssignFrame();

        int64_t dst_pts = srcframe->pts;
        frame_->pts = dst_pts - dstDuration(remainDstSamples_);
        frame_->pkt_size = av_samples_get_buffer_size(frame_->linesize, frame_->channels,
                                                      frame_->nb_samples, (AVSampleFormat)frame_->format, 1);
        
        ret = swr_convert_frame(swrctx_, frame_, srcframe);
        if(ret != 0){
            av_frame_unref(frame_);
            NERROR_FMT_SET(ret, "fail to swr_convert_frame 3rd, err=[{}]", av_err2str(ret));
            return ret;
        }
        
        // TODO:
//        static Int64Relative srcRelpts_;
//        const AVFrame * frame = frame_;
//        odbgd("src frame0: pts=%lld(%+lld,%+lld), ch=%d, nb_samples=%d, pkt_size=%d, fmt=[%s], samplerate=%d"
//              , frame->pts, srcRelpts_.offset(frame->pts), srcRelpts_.delta(frame->pts)
//              , frame->channels, frame->nb_samples, frame->pkt_size
//              , av_get_sample_fmt_name((AVSampleFormat)frame->format)
//              , frame->sample_rate);
        
        ret = func(frame_);
        av_frame_unref(frame_);
        if(ret){
            return ret;
        }
        
        out_samples -= frame_->nb_samples;
        frame_->pts += dstDuration(frame_->nb_samples);
        nextDstPTS_ = frame_->pts;
        
        while(realDstFramesize_>0 && out_samples >= realDstFramesize_){
            checkAssignFrame();
            //frame_->pts = nextPTS(srcframe->pts, out_samples);
            ret = swr_convert_frame(swrctx_, frame_, NULL);
            if(ret != 0){
                av_frame_unref(frame_);
                NERROR_FMT_SET(ret, "fail to swr_convert_frame 4th, err=[{}]", av_err2str(ret));
                return ret;
            }
            frame_->pkt_size = av_samples_get_buffer_size(frame_->linesize, frame_->channels,
                                                          frame_->nb_samples, (AVSampleFormat)frame_->format, 1);
            
//            // TODO:
//            const AVFrame * frame = frame_;
//            odbgd("src frame1: pts=%lld(%+lld,%+lld), ch=%d, nb_samples=%d, pkt_size=%d, fmt=[%s], samplerate=%d"
//                  , frame->pts, srcRelpts_.offset(frame->pts), srcRelpts_.delta(frame->pts)
//                  , frame->channels, frame->nb_samples, frame->pkt_size
//                  , av_get_sample_fmt_name((AVSampleFormat)frame->format)
//                  , frame->sample_rate);
            
            ret = func(frame_);
            av_frame_unref(frame_);
            if(ret){
                return ret;
            }
            
            out_samples -= frame_->nb_samples;
            frame_->pts += dstDuration(frame_->nb_samples);
            nextDstPTS_ = frame_->pts;
            //out_samples = swr_get_out_samples(swrctx_, 0);
        }
        
        remainDstSamples_ = out_samples;
        
        return 0;
    }
    
private:
    int64_t dstDuration(int out_samples){
        int64_t duration = out_samples * timebase_.den / dstcfg_.getSampleRate() / timebase_.num;
        return duration;
    }
    
    int checkAssignFrame(){
        frame_->sample_rate = dstcfg_.getSampleRate();
        frame_->format = dstcfg_.getFormat();
        frame_->channel_layout = dstcfg_.getChLayout();
        frame_->channels = av_get_channel_layout_nb_channels(frame_->channel_layout);
        
        int ret = 0;
        if(dstcfg_.getFrameSize() > 0){
            frame_->nb_samples = dstcfg_.getFrameSize();
            ret = av_frame_get_buffer(frame_, 0);
        }
        return ret;
    }
    
//    int reallocBuffer(int frameSize){
//        // alloc buffer
//        int ret = 0;
//        if(frameSize > 0 && frameSize > realDstFramesize_){
//            if(!frame_){
//                frame_ = av_frame_alloc();
//                frame_->sample_rate = dstcfg_.samplerate;
//                frame_->format = dstcfg_.samplefmt;
//                frame_->channel_layout = dstcfg_.channellayout;
//                frame_->channels = av_get_channel_layout_nb_channels(frame_->channel_layout);
//            }
//
//            if(frame_ && realDstFramesize_ > 0){
//                av_frame_unref(frame_);
//                realDstFramesize_ = 0;
//            }
//
//            frame_->nb_samples = frameSize;
//            ret = av_frame_get_buffer(frame_, 0);
//            if(ret == 0){
//                realDstFramesize_ = frameSize;
//            }
//        }
//        return ret;
//    }
    
private:
    FFSampleConfig srccfg_;
    FFSampleConfig dstcfg_;
    AVRational timebase_;
    SwrContext * swrctx_ = nullptr;
    AVFrame * frame_ = nullptr;
    int realDstFramesize_ = -1;
    int64_t nextDstPTS_ = 0;
    int remainDstSamples_ = 0;
    FFFramesizeConverter sizeConverter_;
};

class FFAudioEncoder{
public:
    //static const AVCodecID DEFAULT_CODEC_ID = AV_CODEC_ID_AAC;
    //static const int DEFAULT_CLOCKRATE = 44100;
    //static const int DEFAULT_CHANNELS = 2;
    static const int64_t DEFAULT_BITRATE = 24000;
    
public:
    FFAudioEncoder():timebase_((AVRational){1, 1000}){
        
    }
    
    virtual ~FFAudioEncoder(){
        close();
    }
    
    void setCodecName(const std::string& codec_name){
        codec_ = avcodec_find_encoder_by_name(codec_name.c_str());
        if(codec_){
            codecId_ = codec_->id;
        }else{
            codecId_ = AV_CODEC_ID_NONE;
        }
    }
    
    void setCodecId(AVCodecID codec_id){
        codecId_ = codec_id;
        codec_ = avcodec_find_encoder(codec_id);
    }
    
    void setBitrate(int64_t bitrate){
        bitrate_ = bitrate;
    }
    
    void setSrcConfig(const FFSampleConfig& cfg){
        srcCfg_ = cfg;
    }
    
    void setTimebase(const AVRational& timebase){
        timebase_ = timebase;
    }
    
    const FFSampleConfig& srcConfig(){
        return srcCfg_;
    }
    
    const FFSampleConfig& encodeConfig(){
        return encodeCfg_;
    }
    
    AVCodecID codecId() const{
        return codecId_;
    }
    
    int open(){
        int ret = 0;
        do{
            AVCodecID codec_id = (AVCodecID)codecId_;
            if(codec_id <= AV_CODEC_ID_NONE){
                ret = -1;
                break;
            }
            
            const AVCodec * codec = codec_;
            if(!codec){
                codec = avcodec_find_encoder(codec_id);
                if (!codec) {
                    ret = -1;
                    NERROR_FMT_SET(ret, "fail to avcodec_find_encoder audio codec id [{}]",
                                         (int)codec_id);
                    break;
                }
            }
            
            ctx_ = avcodec_alloc_context3(codec);
            if (!ctx_) {
                ret = -1;
                NERROR_FMT_SET(ret, "fail to avcodec_alloc_context3 audio codec [{}][{}]",
                                     (int)codec->id, codec->name);
                break;
            }
            
            pkt_ = av_packet_alloc();
            if (!pkt_){
                NERROR_FMT_SET(ret, "fail to av_packet_alloc audio");
                ret = -1;
                break;
            }
            
            ctx_->bit_rate = bitrate_ > 0 ? bitrate_ : DEFAULT_BITRATE;
            ctx_->sample_fmt     = selectSampleFormat(codec, srcCfg_.getFormat());
            ctx_->sample_rate    = selectSampleRate(codec, srcCfg_.getSampleRate());
            ctx_->channel_layout = selectChannelLayout(codec, srcCfg_.getChLayout() );
            ctx_->channels       = av_get_channel_layout_nb_channels(ctx_->channel_layout);
            ctx_->time_base      = timebase_;
            //ctx_->time_base      = srcCfg_.timebase;
            
            ret = avcodec_open2(ctx_, codec, NULL);
            if (ret != 0) {
                NERROR_FMT_SET(ret, "fail to avcodec_open2 audio codec [{}][{}], err=[{}]",
                                     (int)codec->id, codec->name, av_err2str(ret));
                break;
            }
            
            encodeCfg_.setFormat(ctx_->sample_fmt);
            encodeCfg_.setSampleRate(ctx_->sample_rate);
            encodeCfg_.setChLayout(ctx_->channel_layout);
            encodeCfg_.setFrameSize(ctx_->frame_size);
            //encodeCfg_.timebase = ctx_->time_base;
            
            srcCfg_.match(encodeCfg_);
            
            ret = converter_.open(srcCfg_, encodeCfg_, timebase_);
            if(ret != 0){
                break;
            }
            ret = 0;
        }while(0);
        
        if(ret){
            close();
        }
        
        return ret;
    }
    
    void close(){
        if(ctx_){
            avcodec_free_context(&ctx_);
            ctx_ = nullptr;
        }
        
        if(pkt_){
            av_packet_free(&pkt_);
            pkt_ = nullptr;
        }
    }
    
    int encode(const AVFrame *frame, const FFPacketFunc& out_func){
        int ret = 0;
//        if(frame){
//            odbgd("audio frame: pts=%lld(%+lld,%+lld), ch=%d, nb_samples=%d, pkt_size=%d, fmt=[%s], samplerate=%d"
//                  , frame->pts, srcRelpts_.offset(frame->pts), srcRelpts_.delta(frame->pts)
//                  , frame->channels, frame->nb_samples, frame->pkt_size
//                  , av_get_sample_fmt_name((AVSampleFormat)frame->format)
//                  , frame->sample_rate);
//        }
        
        ret = converter_.convert(frame, [this, out_func](const AVFrame * outframe)->int{
            return doEncode(outframe, out_func);
        });
        
        if(!frame && numInFrames_ > 0){
            ret = doEncode(nullptr, out_func);
        }
        return ret;
    }
    
private:
    int doEncode(const AVFrame *frame, const FFPacketFunc& out_func){
        if(frame){
            ++numInFrames_;
//            odbgd("No.%lld encode audio: pts=%lld(%+lld,%+lld), ch=%d, nb_samples=%d, pkt_size=%d, fmt=[%s], samplerate=%d"
//                  , numInFrames_
//                  , frame->pts, encRelpts_.offset(frame->pts), encRelpts_.delta(frame->pts)
//                  , frame->channels, frame->nb_samples, frame->pkt_size
//                  , av_get_sample_fmt_name((AVSampleFormat)frame->format)
//                  , frame->sample_rate);
        }
        int ret = avcodec_send_frame(ctx_, frame);
        if (ret != 0) {
            NERROR_FMT_SET(ret, "fail to avcodec_send_frame audio, err=[{}]",
                                 av_err2str(ret));
            return ret;
        }
        
        while (ret >= 0) {
            ret = avcodec_receive_packet(ctx_, pkt_);
            if (ret == AVERROR(EAGAIN)){
                return 0;
            }else if(ret == AVERROR_EOF){
                return 0;
            }else if (ret < 0) {
                NERROR_FMT_SET(ret, "fail to avcodec_receive_packet audio, err=[{}]",
                                     av_err2str(ret));
                return ret;
            }
            ++numOutPackets_;
//            odbgd("No.%lld audio pkt: pts=%lld(%+lld,%+lld), dts=%lld, size=%d",
//                  numOutPackets_,
//                  pkt_->pts,
//                  dstRelpts_.offset(pkt_->pts),
//                  dstRelpts_.delta(pkt_->pts),
//                  pkt_->dts, pkt_->size);
            ret = out_func(pkt_);
            av_packet_unref(pkt_);
            if(ret){
                return ret;
            }
        }
        return 0;
    }
    
    static int selectSampleRate(const AVCodec *codec, int prefer_samplerate){
        if(!codec->supported_samplerates || *codec->supported_samplerates == 0){
            // no supported samplerate
            if(prefer_samplerate > 0){
                // select prefer one
                return prefer_samplerate;
            }else{
                // select default one
                return 44100;
            }
        }
        
        int best_samplerate = 0;
        const int * p = codec->supported_samplerates;
        while (*p) {
            if (best_samplerate <= 0
                || (abs(prefer_samplerate - *p) < abs(prefer_samplerate - best_samplerate))){
                best_samplerate = *p;
            }
            p++;
        }
        return best_samplerate;
    }
    
    static uint64_t selectChannelLayout(const AVCodec *codec, uint64_t prefer_layout){
        if(!codec->channel_layouts || *codec->channel_layouts == 0){
            // no supported channel layout
            if(prefer_layout > 0){
                // select prefer one
                return prefer_layout; //  == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
            }else{
                // select default one
                return AV_CH_LAYOUT_MONO;
            }
        }
        
        if(prefer_layout == 0){
            // if no prefer, select first
            return *codec->channel_layouts;
        }
        
        int prefer_channels = av_get_channel_layout_nb_channels(prefer_layout);
        uint64_t best_ch_layout = 0;
        int best_nb_channels   = 0;
        const uint64_t *p = codec->channel_layouts;
        while (*p) {
            if(*p == prefer_layout){
                return *p;
            }
            
            int nb_channels = av_get_channel_layout_nb_channels(*p);
            if (best_nb_channels <= 0
                || (abs(prefer_channels - nb_channels) < abs(prefer_channels - best_nb_channels))){
                best_nb_channels = nb_channels;
            }
            p++;
        }
        return best_ch_layout;
    }
    
    static AVSampleFormat selectSampleFormat(const AVCodec *codec, AVSampleFormat prefer_fmt){
        if(!codec->sample_fmts || *codec->sample_fmts <= AV_SAMPLE_FMT_NONE){
            // no supported format
            if(prefer_fmt > AV_SAMPLE_FMT_NONE){
                // select prefer format
                return prefer_fmt;
            }else{
                // select default format
                return AV_SAMPLE_FMT_S16;
            }
        }
        
        const enum AVSampleFormat *p = codec->sample_fmts;
        while (*p != AV_SAMPLE_FMT_NONE) {
            if (*p == prefer_fmt){
                // match, select it
                return prefer_fmt;
            }
            p++;
        }
        // NOT match, select first supported format
        return *codec->sample_fmts;
    }
    
private:
    AVCodecID codecId_ = AV_CODEC_ID_NONE;
    FFSampleConfig  srcCfg_;
    FFSampleConfig  encodeCfg_;
    FFSampleConverter converter_;
    int64_t bitrate_ = -1;
    const AVCodec * codec_ = nullptr;
    AVCodecContext * ctx_ = nullptr;
    AVPacket * pkt_ = nullptr;
    AVRational timebase_;
    int64_t numInFrames_ = 0;
    int64_t numOutPackets_ = 0;
    Int64Relative srcRelpts_;
    Int64Relative encRelpts_;
    Int64Relative dstRelpts_;
};

int main_write_mp4(int argc, char** argv)
{
    
    const char* video_raw_file = NULL;
    const char* audio_raw_file = NULL;
    const char* output_file = NULL;
    
    output_file = "/tmp/out.mp4";
    video_raw_file = "/Users/simon/Downloads/720p.h264.raw";
    audio_raw_file = "/Users/simon/Downloads/audio.raw.aac";
    
    dbgi("video_raw_file=[%s]", video_raw_file);
    dbgi("audio_raw_file=[%s]", audio_raw_file);
    dbgi("output_file=[%s]", output_file);
    
    //ravrecord_t recorder = nullptr;
    FFWriter writer;
    H264FileReader * video = nullptr;
    AACFileReader * audio = nullptr;
    int ret = 0;
    do{
        if(video_raw_file){
            video = new H264FileReader();
            ret = video->open(video_raw_file, 25);
            if(ret){
                break;
            }
        }
        if(audio_raw_file){
            audio = new AACFileReader();
            ret = audio->open(audio_raw_file, 45);
            if(ret){
                break;
            }
        }
        
        
//        ret = writer.open(output_file,
//                           AV_CODEC_ID_H264,
//                           1280, 720, 25, // TODO: fps
//                           AV_CODEC_ID_AAC,
//                           44100,
//                           2);
        
        FFVideoConfig video_cfg;
        video_cfg.setCodecId(AV_CODEC_ID_H264);
        video_cfg.setTimeBase(AVRational{1, 1000});
        video_cfg.video.setWidth(1280);
        video_cfg.video.setHeight(720);
        
        FFAudioConfig audio_cfg;
        audio_cfg.setCodecId(AV_CODEC_ID_AAC);
        audio_cfg.setTimeBase(AVRational{1, 1000});
        audio_cfg.audio.setSampleRate(44100);
        audio_cfg.audio.setChLayout(av_get_default_channel_layout(2));
        
        int video_index = writer.addVideoTrack(video_cfg);
        int audio_index = writer.addAudioTrack(audio_cfg);
        ret = writer.open(output_file);
        if (ret) {
            dbge("fail to open writer");
            ret = -1;
            break;
        }
        dbgi("open writer success");
        
        int64_t MAX_TS = 0x00FFFFFFFFFFFFFF;
        int64_t video_ts = MAX_TS;
        int64_t audio_ts = MAX_TS;
        
        if(video){
            video_ts = video->timestamp();
        }
        if(audio){
            audio_ts = audio->timestamp();
        }
        
        while(video_ts != MAX_TS || audio_ts != MAX_TS){
            if(audio && audio_ts <= video_ts && audio_ts != MAX_TS){
                //rr_process_audio(recorder, audio_ts, audio->data(), (int)audio->dataLength());
                //writer.writeAudio(audio_ts, audio->data(), (int)audio->dataLength());
                writer.write(audio_index, audio_ts, audio_ts, audio->data(), (int)audio->dataLength());
                ret = audio->next();
                audio_ts = ret == 0 ? audio->timestamp() : MAX_TS;
            }else if(video && video_ts != MAX_TS){
                
                bool is_key_frame = false;
                int naltype = video->data()[4] & 0x1f;
                if ((naltype == 0x07) || (naltype==0x05) || (naltype == 0x08)) {
                    is_key_frame = true;
                } else {
                    is_key_frame = false;
                }
                //writer.writeVideo(video_ts, video->data(), (int)video->dataLength(), is_key_frame);
                writer.write(video_index, video_ts, video_ts,
                             video->data(), (int)video->dataLength(), is_key_frame);
                ret = video->next();
                video_ts = ret == 0 ? video->timestamp() : MAX_TS;
            }
            
            ret = 0;
        }// loop
        
    }while (0);
    

    writer.close();
    
    if(video){
        delete video;
        video = NULL;
    }
    
    if(audio){
        delete audio;
        audio = NULL;
    }
    
    return 0;
}

//int record_camera(){
//    const std::string output_path = "/tmp";
//
//    const std::string output_encoded_file = output_path +"/out.mp4";
//    const std::string deviceName = "FaceTime HD Camera"; // or "2";
//    const std::string optFormat = "avfoundation";
//    const std::string optPixelFmt;
//    FILE * out_yuv_fp = NULL;
//
//
//    odbgi("output_encoded_file=[%s]", output_encoded_file.c_str());
//
//    FFContainerReader * deviceReader = nullptr;
//    FFVideoEncoder videoEncoder;
//    FFContainerWriter writer;
//    int ret = -1;
//    do{
//        deviceReader = new FFContainerReader("dev");
//        deviceReader->setVideoOptions(640, 480, 30, optPixelFmt);
//        ret = deviceReader->open(optFormat, deviceName);
//        if(ret){
//            odbge("fail to open camera, ret=%d", ret);
//            break;
//        }
//
//        FFVideoTrack * videoTrack = deviceReader->openVideoTrack(AV_PIX_FMT_YUV420P);
//        //FFVideoTrack * videoTrack = deviceReader->openVideoTrack(AV_PIX_FMT_NONE);
//        if(!videoTrack){
//            odbge("fail to open video track");
//            ret = -22;
//            break;
//        }
//        odbgi("open camera ok");
//
//        FFImageConfig imgcfg;
//        imgcfg.pix_fmt = videoTrack->getPixelFormat();
//        imgcfg.width = videoTrack->getWidth();
//        imgcfg.height = videoTrack->getHeight();
//        videoEncoder.setSrcImageConfig(imgcfg);
//        videoEncoder.setCodecId(AV_CODEC_ID_H264);
//        videoEncoder.setBitrate(1000*1000);
//        ret = videoEncoder.open();
//        if (ret) {
//            odbge("fail to open encoder");
//            ret = -1;
//            break;
//        }
//        odbgi("open encoder success");
//
//        ret = writer.open(output_encoded_file,
//                          AV_CODEC_ID_H264,
//                          videoTrack->getWidth(), videoTrack->getHeight(), 25, // TODO: fps
//                          AV_CODEC_ID_NONE,
//                          44100,
//                          2);
//        if (ret) {
//            odbge("fail to open writer");
//            ret = -1;
//            break;
//        }
//        odbgi("open writer success");
//
//        const std::string output_yuv_file = fmt::format("{}/out_{}x{}_{}.yuv",
//                                                        output_path,
//                                                        imgcfg.width,
//                                                        imgcfg.height,
//                                                        av_get_pix_fmt_name(imgcfg.pix_fmt));
//        out_yuv_fp = fopen(output_yuv_file.c_str(), "wb");
//        if(!out_yuv_fp){
//            odbge("fail to write open [%s]", output_yuv_file.c_str());
//            ret = -1;
//            break;
//        }
//        odbgi("opened output_yuv_file=[%s]", output_yuv_file.c_str());
//
//        AVRational cap_time_base = {1, 1000000};
//        AVRational mux_time_base = {1, 1000};
//        FFPTSConverter pts_converter(cap_time_base, mux_time_base);
//
//        Int64Relative cap_rel_pts;
//        Int64Relative enc_rel_pts;
//
//        AVFrame * avframe = NULL;
//        for(uint32_t nframes = 0; nframes < 30*3; ++nframes){
//            FFTrack * track = deviceReader->readNext();
//            if(track->getMediaType() == AVMEDIA_TYPE_VIDEO){
//                avframe = track->getLastFrame();
//                if(avframe){
//                    //avframe->pts = NUtil::get_now_ms();
//                    avframe->pts = pts_converter.convert(avframe->pts);
//                    odbgi("camera image: pts=%lld(%+lld,%+lld) w=%d, h=%d, size=%d, fmt=%s",
//                          avframe->pts, cap_rel_pts.offset(avframe->pts), cap_rel_pts.delta(avframe->pts),
//                          avframe->width, avframe->height, avframe->pkt_size,
//                          av_get_pix_fmt_name((AVPixelFormat)avframe->format));
//
//                    if(out_yuv_fp ){
//                        if(imgcfg.pix_fmt == AV_PIX_FMT_YUV420P){
//                            ret = (int)fwrite(avframe->data[0], sizeof(char), imgcfg.width*imgcfg.height, out_yuv_fp);
//                            ret = (int)fwrite(avframe->data[1], sizeof(char), imgcfg.width*imgcfg.height/4, out_yuv_fp);
//                            ret = (int)fwrite(avframe->data[2], sizeof(char), imgcfg.width*imgcfg.height/4, out_yuv_fp);
//                        }else if(imgcfg.pix_fmt == AV_PIX_FMT_UYVY422){
//                            ret = (int)fwrite(avframe->data[0], sizeof(char), imgcfg.width*imgcfg.height*2, out_yuv_fp);
//                        }
//
//                    }
//
//                    videoEncoder.encode(avframe, [&writer, &enc_rel_pts](AVPacket * pkt)->int{
//                        odbgi("encode pkt: pts=%lld(%+lld,%+lld), size=%d, keyfrm=%d",
//                              pkt->pts, enc_rel_pts.offset(pkt->pts), enc_rel_pts.delta(pkt->pts),
//                              pkt->size,
//                              pkt->flags & AV_PKT_FLAG_KEY);
//                        return writer.writeVideo(pkt->pts, pkt->data, pkt->size, pkt->flags & AV_PKT_FLAG_KEY);
//                    });
//                }
//            }
//
//        }
//
//        odbgi("ecode remain frames");
//        videoEncoder.encode(nullptr, [&writer, &enc_rel_pts](AVPacket * pkt)->int{
//            odbgi("encode pkt: pts=%lld(%+lld,%+lld), size=%d, keyfrm=%d",
//                  pkt->pts, enc_rel_pts.offset(pkt->pts), enc_rel_pts.delta(pkt->pts),
//                  pkt->size,
//                  pkt->flags & AV_PKT_FLAG_KEY);
//            return writer.writeVideo(pkt->pts, pkt->data, pkt->size, pkt->flags & AV_PKT_FLAG_KEY);
//        });
//
//        ret = 0;
//    }while(0);
//
//    deviceReader->close();
//    delete deviceReader;
//
//    videoEncoder.close();
//    writer.close();
//
//    if(out_yuv_fp){
//        fclose(out_yuv_fp);
//        out_yuv_fp = nullptr;
//    }
//
//    odbgi("record_camera done");
//    return ret;
//}
//int main_record_camera(int argc, char** argv){
//
//    return record_camera();
//}
//
//
//
//int main_record_microphone(int argc, char** argv){
//
//
//    const char* output_file = NULL;
//    output_file = "/tmp/out.mp4";
//
//
//    const std::string deviceName = ":Built-in Microphone"; // or ":0";
//    const std::string optFormat = "avfoundation";
//    const std::string optPixelFmt;
//
//
//    dbgi("output_file=[%s]", output_file);
//
//    FFContainerReader deviceReader("device");
//    FFAudioEncoder audioEncoder;
//    FFContainerWriter writer;
//    int ret = -1;
//    do{
//        ret = deviceReader.open(optFormat, deviceName);
//        if(ret){
//            odbge("fail to open microphone, ret=%d", ret);
//            break;
//        }
//
//        FFAudioTrack * audioTrack = deviceReader.openAudioTrack(-1, -1, AV_SAMPLE_FMT_NONE);
//        //FFAudioTrack * audioTrack = deviceReader.openAudioTrack(-1, -1, AV_SAMPLE_FMT_S16);
//        if(!audioTrack){
//            odbge("fail to open audio track");
//            ret = -22;
//            break;
//        }
//        odbgi("microphone: ch=%d, ar=%d, fmt=[%s], timeb=[%d,%d]",
//              audioTrack->getChannels(),
//              audioTrack->getSamplerate(),
//              av_get_sample_fmt_name(audioTrack->getSampleFormat()),
//              audioTrack->getTimebase().num, audioTrack->getTimebase().den );
//
//        AVRational cap_time_base = {1, 1000000};
//        AVRational mux_time_base = {1, 1000};
//        FFPTSConverter pts_converter(cap_time_base, mux_time_base);
//
//        FFSampleConfig src_cfg;
//        //src_cfg.timebase = cap_time_base;
//        src_cfg.samplerate = audioTrack->getSamplerate();
//        src_cfg.samplefmt = audioTrack->getSampleFormat();
//        src_cfg.channellayout = av_get_default_channel_layout(audioTrack->getChannels()); // TODO:
//
//        audioEncoder.setCodecName("libfdk_aac"); //audioEncoder.setCodecId(AV_CODEC_ID_AAC);
//        audioEncoder.setSrcConfig(src_cfg);
//        audioEncoder.setTimebase(mux_time_base);
//        ret = audioEncoder.open();
//        if (ret) {
//            odbge("fail to open encoder");
//            ret = -1;
//            break;
//        }
//        odbgi("open encoder success");
//
//        ret = writer.open(output_file,
//                          AV_CODEC_ID_NONE,
//                          0, 0, 0,
//                          AV_CODEC_ID_AAC,
//                          audioEncoder.encodeConfig().samplerate,
//                          av_get_channel_layout_nb_channels(audioEncoder.encodeConfig().channellayout));
//        if (ret) {
//            odbge("fail to open writer");
//            ret = -1;
//            break;
//        }
//        odbgi("open writer success");
//
//        int64_t start_ms = NUtil::get_now_ms();
//        AVFrame * avframe = NULL;
//        for(uint32_t nframes = 0; (NUtil::get_now_ms()-start_ms) < 5000; ++nframes){
//            FFTrack * track = deviceReader.readNext();
//            if(track->getMediaType() == AVMEDIA_TYPE_AUDIO){
//                avframe = track->getLastFrame();
//                if(avframe){
//                    avframe->pts = pts_converter.convert(avframe->pts);
//                    ret = audioEncoder.encode(avframe, [&writer](AVPacket * pkt)->int{
//                        return writer.writeAudio(pkt->pts, pkt->data, pkt->size);
//                    });
//                    if(ret){
//                        odbge("fail to encode audio, err=[%d]-[%s]",
//                              ret, NThreadError::lastMsg().c_str());
//                        break;
//                    }
//                }
//            }
//        }
//
//        if(ret == 0){
//            odbgi("encode remain frames");
//            ret = audioEncoder.encode(nullptr, [&writer](AVPacket * pkt)->int{
//                return writer.writeAudio(pkt->pts, pkt->data, pkt->size);
//            });
//            if(ret){
//                odbge("fail to encode audio, err=[%d]-[%s]",
//                      ret, NThreadError::lastMsg().c_str());
//                break;
//            }
//        }
//
//        ret = 0;
//    }while(0);
//
//    deviceReader.close();
//    audioEncoder.close();
//    writer.close();
//
//    return 0;
//}




int main_record_mp4(int argc, char** argv){


    const std::string video_codec_name = "libx264";
    const std::string audio_codec_name = "libfdk_aac";
    
    const std::string deviceName = "FaceTime HD Camera:Built-in Microphone"; // or "0:0";
    //const std::string deviceName = "FaceTime HD Camera"; // camera only
    //const std::string deviceName = ":Built-in Microphone"; // microphone only
    const std::string optFormat = "avfoundation";
    const std::string optPixelFmt;
    
    const std::string output_path = "/tmp";
    const std::string output_encoded_file = output_path +"/out.mp4";
    FILE * out_yuv_fp = NULL;
    

    odbgi("output_encoded_file=[%s]", output_encoded_file.c_str());

    FFContainerReader deviceReader("device");
    FFAudioEncoder audioEncoder;
    FFVideoEncoder videoEncoder;
    FFWriter writer;
    FFDecodeReader reader;
    int ret = -1;
    do{
//        FFImageConfig camera_cfg;
//        camera_cfg.setWidth(640);
//        camera_cfg.setHeight(480);
//        camera_cfg.setFrameRate(30);
//        reader.setOptions(camera_cfg);
//        ret = reader.open(optFormat, deviceName,std::map<std::string, std::string>());
//        if(ret){
//            odbge("fail to open device, ret=%d", ret);
//            break;
//        }
//        odbgi("open input OK");
//        
//        FFMediaConfig cap_audio_cfg;
//        FFMediaConfig cap_video_cfg;
//        
//        int cap_audio_index = reader.getFirstIndex(AVMEDIA_TYPE_AUDIO);
//        int cap_video_index = reader.getFirstIndex(AVMEDIA_TYPE_VIDEO);
//        if(cap_audio_index >= 0){
//            //cap_audio_cfg.assignDecode(reader.getCodecCtx(cap_audio_index));
//            cap_audio_cfg = reader.getCfg(cap_audio_index);
//        }
//        if(cap_video_index >= 0){
//            //cap_video_cfg.assignDecode(reader.getCodecCtx(cap_video_index));
//            cap_video_cfg = reader.getCfg(cap_video_index);
//        }
//        
//        while(1){
//            ret = reader.read([&cap_audio_index, &cap_video_index]
//                              (int index, const AVFrame * frame)->int{
//                odbgi("got frame: index=%d, pts=%lld, format=%d", index, frame->pts, frame->format);
//                if (index == cap_audio_index){
//                    
//                }else if (index == cap_video_index){
//                    
//                }
//                return 0;
//            });
//        }

        
        AVRational cap_time_base = {1, 1000000};
        AVRational mux_time_base = {1, 1000};
        FFPTSConverter pts_converter(cap_time_base, mux_time_base);
        
        deviceReader.setVideoOptions(640, 480, 30, optPixelFmt);
        //deviceReader.setVideoOptions(1280, 720, 30, optPixelFmt);
        ret = deviceReader.open(optFormat, deviceName);
        if(ret){
            odbge("fail to open device, ret=%d", ret);
            break;
        }

        FFAudioTrack * audioTrack = deviceReader.openAudioTrack(-1, -1, AV_SAMPLE_FMT_NONE);
        //FFAudioTrack * audioTrack = deviceReader.openAudioTrack(-1, -1, AV_SAMPLE_FMT_S16);
        if(!audioTrack){
            odbgi("no audio track");
        }else{
            odbgi("opened audio: ch=%d, ar=%d, fmt=[%s], timeb=[%d,%d]",
                  audioTrack->getChannels(),
                  audioTrack->getSamplerate(),
                  av_get_sample_fmt_name(audioTrack->getSampleFormat()),
                  audioTrack->getTimebase().num, audioTrack->getTimebase().den );
            
            FFSampleConfig sample_cfg;
            sample_cfg.setSampleRate(audioTrack->getSamplerate());
            sample_cfg.setFormat(audioTrack->getSampleFormat());
            sample_cfg.setChLayout(av_get_default_channel_layout(audioTrack->getChannels())); // TODO:
            
            //audioEncoder.setCodecId(AV_CODEC_ID_AAC);
            audioEncoder.setCodecName(audio_codec_name);
            audioEncoder.setSrcConfig(sample_cfg);
            audioEncoder.setTimebase(mux_time_base);
            ret = audioEncoder.open();
            if (ret) {
                odbge("fail to open encoder");
                ret = -1;
                break;
            }
            odbgi("open audio encoder success");
        }
        
        FFImageConfig imgcfg;
        FFVideoTrack * videoTrack = deviceReader.openVideoTrack(AV_PIX_FMT_NONE);
        //FFVideoTrack * videoTrack = deviceReader.openVideoTrack(AV_PIX_FMT_YUV420P);
        if(!videoTrack){
            odbgi("no video track");
        }else{
            odbgi("opened video: size=%dx%d, fmt=[%s], timeb=[%d,%d]",
                  videoTrack->getWidth(), videoTrack->getHeight(),
                  av_get_pix_fmt_name(videoTrack->getPixelFormat()),
                  videoTrack->getTimebase().num, videoTrack->getTimebase().den );
            
            imgcfg.setFormat(videoTrack->getPixelFormat());
            imgcfg.setWidth(videoTrack->getWidth());
            imgcfg.setHeight(videoTrack->getHeight());
            videoEncoder.setSrcImageConfig(imgcfg);
            //videoEncoder.setCodecId(AV_CODEC_ID_H264);
            videoEncoder.setCodecName(video_codec_name);
            videoEncoder.setBitrate(1000*1000);
            videoEncoder.setCodecOpt("preset", "fast");
            ret = videoEncoder.open();
            if (ret) {
                odbge("fail to open encoder");
                ret = -1;
                break;
            }
            odbgi("open video encoder success");
            
            const std::string output_yuv_file = fmt::format("{}/out_{}x{}_{}.yuv",
                                                            output_path,
                                                            imgcfg.getWidth(),
                                                            imgcfg.getHeight(),
                                                            av_get_pix_fmt_name(imgcfg.getFormat()));
            out_yuv_fp = fopen(output_yuv_file.c_str(), "wb");
            if(!out_yuv_fp){
                odbge("fail to write open [%s]", output_yuv_file.c_str());
                ret = -1;
                break;
            }
            odbgi("opened output_yuv_file=[%s]", output_yuv_file.c_str());
        }
        
        if(!audioTrack && !videoTrack){
            odbge("empty track");
            ret = -22;
            break;
        }
        
//        ret = writer.open(output_encoded_file,
//                          videoEncoder.codecId(),
//                          videoEncoder.encodeImageCfg().width,
//                          videoEncoder.encodeImageCfg().height,
//                          25,
//                          audioEncoder.codecId(),
//                          audioEncoder.encodeConfig().samplerate,
//                          av_get_channel_layout_nb_channels(audioEncoder.encodeConfig().channellayout));
        
        FFAudioConfig out_audio_cfg;
        FFVideoConfig out_video_cfg;
        
        int write_audio_index = -1;
        if(audioTrack){
            out_audio_cfg.setCodecId(audioEncoder.codecId());
            out_audio_cfg.setTimeBase(mux_time_base);
            out_audio_cfg.audio = audioEncoder.encodeConfig();
            ret = writer.addAudioTrack(out_audio_cfg);
            write_audio_index = ret;
        }
        
        int write_video_index = -1;
        if(videoTrack){
            out_video_cfg.setCodecId(videoEncoder.codecId());
            out_video_cfg.setTimeBase(mux_time_base);
            out_video_cfg.video = videoEncoder.encodeImageCfg();
            ret = writer.addVideoTrack(out_video_cfg);
            write_video_index = ret;
        }
        
        ret = writer.open(output_encoded_file);
        if (ret) {
            odbge("fail to open writer");
            ret = -1;
            break;
        }
        odbgi("open writer success");

        Int64Relative audio_cap_pts;
        Int64Relative audio_enc_pts;
        
        Int64Relative video_cap_pts;
        Int64Relative video_enc_pts;

        int64_t start_ms = NUtil::get_now_ms();
        AVFrame * avframe = NULL;
        for(uint32_t nframes = 0; (NUtil::get_now_ms()-start_ms) < 5000; ++nframes){
            FFTrack * track = deviceReader.readNext();
            if(!track){
                odbgi("reach device end");
                break;
            }
            avframe = track->getLastFrame();
            if(!avframe){
                continue;
            }

            if(track->getMediaType() == AVMEDIA_TYPE_AUDIO){
                avframe->pts = pts_converter.convert(avframe->pts);
                odbgi("audio frame: pts=%lld(%+lld,%+lld), samplerate=%d, ch=%d, nb_samples=%d, fmt=[%s], pkt_size=%d",
                      avframe->pts, audio_cap_pts.offset(avframe->pts), audio_cap_pts.delta(avframe->pts),
                      avframe->sample_rate, avframe->channels, avframe->nb_samples,
                      av_get_sample_fmt_name((AVSampleFormat)avframe->format),
                      avframe->pkt_size);
                ret = audioEncoder.encode(avframe, [&writer, &audio_enc_pts, &write_audio_index](AVPacket * pkt)->int{
                    odbgi("write audio: pts=%lld(%+lld,%+lld), dts=%lld, size=%d",
                          pkt->pts, audio_enc_pts.offset(pkt->pts), audio_enc_pts.delta(pkt->pts), pkt->dts, pkt->size);
                    //return writer.writeAudio(pkt->pts, pkt->data, pkt->size);
                    return writer.write(write_audio_index, pkt);
                });
                
                if(ret){
                    odbge("fail to encode audio, err=[%d]-[%s]",
                          ret, NThreadError::lastMsg().c_str());
                    break;
                }
                
            }else if(track->getMediaType() == AVMEDIA_TYPE_VIDEO){
                //avframe->pts = NUtil::get_now_ms();
                avframe->pts = pts_converter.convert(avframe->pts);
                odbgi("video frame: pts=%lld(%+lld,%+lld), w=%d, h=%d, fmt=[%s], size=%d",
                      avframe->pts, video_cap_pts.offset(avframe->pts), video_cap_pts.delta(avframe->pts),
                      avframe->width, avframe->height,
                      av_get_pix_fmt_name((AVPixelFormat)avframe->format),
                      avframe->pkt_size);
                
                if(out_yuv_fp ){
                    if(imgcfg.getFormat() == AV_PIX_FMT_YUV420P){
                        ret = (int)fwrite(avframe->data[0], sizeof(char), imgcfg.getImageSize(), out_yuv_fp);
                        ret = (int)fwrite(avframe->data[1], sizeof(char), imgcfg.getImageSize()/4, out_yuv_fp);
                        ret = (int)fwrite(avframe->data[2], sizeof(char), imgcfg.getImageSize()/4, out_yuv_fp);
                    }else if(imgcfg.getFormat() == AV_PIX_FMT_UYVY422){
                        ret = (int)fwrite(avframe->data[0], sizeof(char), imgcfg.getImageSize()*2, out_yuv_fp);
                    }
                    
                }
                
                ret = videoEncoder.encode(avframe, [&writer, &video_enc_pts, &write_video_index](AVPacket * pkt)->int{
                    odbgi("write video pkt: pts=%lld(%+lld,%+lld), dts=%lld, size=%d, keyfrm=%d",
                          pkt->pts, video_enc_pts.offset(pkt->pts), video_enc_pts.delta(pkt->pts),
                          pkt->dts, pkt->size,
                          pkt->flags & AV_PKT_FLAG_KEY);
                    //return writer.writeVideo(pkt->pts, pkt->data, pkt->size, pkt->flags & AV_PKT_FLAG_KEY);
                    return writer.write(write_video_index, pkt);
                });
                
                if(ret){
                    odbge("fail to encode video, err=[%d]-[%s]",
                          ret, NThreadError::lastMsg().c_str());
                    break;
                }
            }
        }

        odbgi("ecode remain frames");
        
        if(audioTrack){
            ret = audioEncoder.encode(nullptr, [&writer, &audio_enc_pts, &write_audio_index](AVPacket * pkt)->int{
                odbgi("write audio pkt: pts=%lld(%+lld,%+lld), dts=%lld, size=%d",
                      pkt->pts, audio_enc_pts.offset(pkt->pts), audio_enc_pts.delta(pkt->pts), pkt->dts, pkt->size);
                //return writer.writeAudio(pkt->pts, pkt->data, pkt->size);
                return writer.write(write_audio_index, pkt);
            });
        }

        if(videoTrack){
            ret = videoEncoder.encode(nullptr, [&writer, &video_enc_pts, &write_video_index](AVPacket * pkt)->int{
                odbgi("write video pkt: pts=%lld(%+lld,%+lld), dts=%lld, size=%d, keyfrm=%d",
                      pkt->pts, video_enc_pts.offset(pkt->pts), video_enc_pts.delta(pkt->pts),
                      pkt->dts, pkt->size,
                      pkt->flags & AV_PKT_FLAG_KEY);
                //return writer.writeVideo(pkt->pts, pkt->data, pkt->size, pkt->flags & AV_PKT_FLAG_KEY);
                return writer.write(write_video_index, pkt);
            });
        }


        ret = 0;
    }while(0);

    deviceReader.close();
    audioEncoder.close();
    videoEncoder.close();
    writer.close();
    
    if(out_yuv_fp){
        fclose(out_yuv_fp);
        out_yuv_fp = nullptr;
    }
    
    odbgi("record done");
    
    return ret;
}


class Camera2YUV{
public:
    
    int capture(const std::string& deviceName,
                const std::string& optFormat,
                const std::string& outPath,
                std::string& outFile){
        const std::string video_codec_name = "libx264";
        const std::string audio_codec_name = "libfdk_aac";
        
        FILE * out_yuv_fp = NULL;
        FFDecodeReader reader;
        FFImageAdapter image_adapter;
        int ret = 0;
        do{
            FFImageConfig req_cfg;
            req_cfg.setWidth(640);
            req_cfg.setHeight(480);
            req_cfg.setFrameRate(30);
            reader.setOptions(req_cfg);
            ret = reader.open(optFormat, deviceName,std::map<std::string, std::string>());
            if(ret){
                odbge("fail to open device, ret=%d", ret);
                break;
            }
            odbgi("open input OK");
            
            FFMediaConfig cap_audio_cfg;
            FFMediaConfig cap_video_cfg;
            
            int cap_audio_index = reader.getFirstIndex(AVMEDIA_TYPE_AUDIO);
            int cap_video_index = reader.getFirstIndex(AVMEDIA_TYPE_VIDEO);
            if(cap_audio_index >= 0){
                //cap_audio_cfg.assignDecode(reader.getCodecCtx(cap_audio_index));
                cap_audio_cfg = reader.getCfg(cap_audio_index);
            }
            if(cap_video_index >= 0){
                //cap_video_cfg.assignDecode(reader.getCodecCtx(cap_video_index));
                cap_video_cfg = reader.getCfg(cap_video_index);
            }
            
            //FFImageConfig& imgcfg = cap_video_cfg.video;
            FFImageConfig imgcfg = cap_video_cfg.video;
            imgcfg.setFormat(AV_PIX_FMT_YUV420P);
            image_adapter.open(imgcfg);
            
            
            char name_buf[512];
            sprintf(name_buf,
                    "%s/out_%dx%d_%s.yuv",
                    outPath.c_str(),
                    imgcfg.getWidth(),
                    imgcfg.getHeight(),
                    av_get_pix_fmt_name(imgcfg.getFormat()));
            outFile = name_buf;
            
            out_yuv_fp = fopen(outFile.c_str(), "wb");
            if(!out_yuv_fp){
                odbge("fail to write open [%s]", outFile.c_str());
                ret = -1;
                break;
            }
            odbgi("opened output_yuv_file=[%s]", outFile.c_str());
            
            int nframes = 100;
            for(int i = 0; i < nframes; ++i){
                ret = reader.read([&image_adapter, &cap_audio_index, &cap_video_index, &imgcfg, &out_yuv_fp]
                                  (int index, const AVFrame * frame)->int{
                                      odbgi("got frame: index=%d, pts=%lld, format=%d", index, frame->pts, frame->format);
                                      int ret = 0;
                                      if (index == cap_audio_index){
                                          
                                      }else if (index == cap_video_index){
                                          ret = image_adapter.adapt(frame, [&cap_audio_index, &cap_video_index, &imgcfg, &out_yuv_fp]
                                                                    (const AVFrame * frame)->int{
                                                                        int ret = 0;
                                                                        if(out_yuv_fp ){
                                                                            if(imgcfg.getFormat() == AV_PIX_FMT_YUV420P){
                                                                                ret = (int)fwrite(frame->data[0], sizeof(char), imgcfg.getImageSize(), out_yuv_fp);
                                                                                ret = (int)fwrite(frame->data[1], sizeof(char), imgcfg.getImageSize()/4, out_yuv_fp);
                                                                                ret = (int)fwrite(frame->data[2], sizeof(char), imgcfg.getImageSize()/4, out_yuv_fp);
                                                                            }else if(imgcfg.getFormat() == AV_PIX_FMT_UYVY422){
                                                                                ret = (int)fwrite(frame->data[0], sizeof(char), imgcfg.getImageSize()*2, out_yuv_fp);
                                                                            }
                                                                        }
                                                                        return ret;
                                                                    });
                                      }
                                      return ret;
                                      
                                      
                                      
                                  });
                if(ret) break;
            }
            
            odbgi("capture ok");
            odbgi("command: ffplay -f rawvideo -video_size %dx%d -pixel_format %s %s",
                  imgcfg.getWidth(),
                  imgcfg.getHeight(),
                  av_get_pix_fmt_name(imgcfg.getFormat()),
                  outFile.c_str());
            ret = 0;
        }while(0);
        
        reader.close();
        image_adapter.close();
        
        if(out_yuv_fp){
            fclose(out_yuv_fp);
            out_yuv_fp = nullptr;
        }
        
        return ret;
    }
};

int lab030_rtmp_push_main(int argc, char** argv){
    av_register_all();
    avdevice_register_all();
    avcodec_register_all();
    
//    {
//        std::string outFile;
//        Camera2YUV().capture("FaceTime HD Camera", "avfoundation", "/tmp", outFile);
//    }
    
    //return main_push_av(argc, argv);
    //return main_write_mp4(argc, argv);
    //return main_record_camera(argc, argv);
    //return main_record_microphone(argc, argv);
    return main_record_mp4(argc, argv);
}
