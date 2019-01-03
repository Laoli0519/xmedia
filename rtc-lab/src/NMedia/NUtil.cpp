

#include "NUtil.hpp"

int NUtil::LoadFile(const std::string& fname, uint8_t **buf, size_t * buf_size){
    FILE * fp_rtmp_video = NULL;
    uint8_t * raw_video_data = NULL;
    size_t raw_video_size = 0;
    
    int ret = -1;
    do{
        fp_rtmp_video = fopen(fname.c_str(), "rb");
        if(!fp_rtmp_video){
            //dbge("fail to open video file [%s]", fname.c_str());
            ret = -1;
            break;
        }
        ret = fseek(fp_rtmp_video, 0, SEEK_END);
        raw_video_size = ftell(fp_rtmp_video);
        ret = fseek(fp_rtmp_video, 0, SEEK_SET);
        //dbgi("video file size %ld", raw_video_size);
        if(raw_video_size <= 0){
            //dbge("invalid video file size");
            ret = -2;
            break;
        }
        raw_video_data = new uint8_t[raw_video_size];
        long bytes = fread(raw_video_data, 1, raw_video_size, fp_rtmp_video);
        if(bytes != raw_video_size){
            //dbge("read video file fail, %ld", bytes);
            ret = -3;
            break;
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
        delete[] raw_video_data;
        raw_video_data = NULL;
    }
    return ret;
}

int NUtil::ReadTLV(FILE *fp, unsigned char *buf, size_t bufsize, size_t *plength, int *ptype) {
    int ret = 0;
    int type;
    int length;
    size_t bytes;
    unsigned char t4[4];
    
    do
    {
        bytes = fread(t4, sizeof(char), 4, fp);
        if(bytes != 4){
            ret = -1;
            break;
        }
        // new tlv format using 4 bytes for T, old was 2 bytes
        type = le_get4(t4, 0);
        //dbgi("tlv: type=%d\n", type);
        
        bytes = fread(t4, sizeof(char), 4, fp);
        if(bytes != 4){
            break;
        }
        length = le_get4(t4, 0);
        //dbgi("tlv: length=%d\n", length);
        
        if(length > bufsize){
            //dbge("too big tlv length %d, bufsize=%d\n", length, bufsize);
            ret = -1;
            break;
        } else if (length == 0){
            ret = 0;
            *ptype = type;
            *plength = 0;
            break;
        }
        
        bytes = fread(buf, sizeof(char), length, fp);
        if(bytes != length){
            ret = -1;
            break;
        }
        
        *ptype = type;
        *plength = length;
        
        ret = 0;
    } while (0);
    return ret;
}





// TODO: remove 
class A1{
public:
    DECLARE_CLASS_ENUM(Type,
                       T1,
                       T2,
                       T3,
                       Unknown
                       );
public:
    virtual ~A1(){
        std::cout << "(" << (void *)this << ") " << "bye A1" << std::endl;
    }
    
    virtual Type getType() const{
        return T1;
    }
    
    virtual void print() const{
        std::cout << "(" << (void *)this << ") " << "hello A1" << std::endl;
    }
    
};

class A2 : public A1{
public:
    virtual ~A2(){
        std::cout << "(" << (void *)this << ") " << "bye A2" << std::endl;
    }
    
    virtual Type getType() const override{
        return T2;
    }
    
    virtual void print() const override{
        std::cout << "(" << (void *)this << ") " << "hello A2" << std::endl;
    }
    
    void sayMorning() const{
        std::cout << "(" << (void *)this << ") " << "morning A2" << std::endl;
    }
};

class A3 : public A2{
public:
    virtual ~A3(){
        std::cout << "(" << (void *)this << ") " << "bye A3" << std::endl;
    }
    
    virtual Type getType() const override{
        return T3;
    }
    virtual void print() const override{
        std::cout << "(" << (void *)this << ") " << "hello A3" << std::endl;
    }
    
};


int test_pool(){
    using APtr = NObjectPool<A1>::Unique;
    APtr a3 = NObjectPool<A1>::MakeNullPtr();
    {
        NPool<A1>       pool1;
        NPool<A2, A1>   pool2;
        NPool<A3, A1>   pool3;
        
        APtr a1 = pool1.get();
        APtr a2 = pool2.get();
        a1->print();
        a2->print();
        A2 * aa = (A2 *) a2.get();
        aa->sayMorning();
        a3 = pool3.get();
        a3->print();
    }
    return 0;
}

