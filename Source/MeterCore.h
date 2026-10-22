#pragma once
#include <ebur128.h>
#include <fvad.h>
#include <array>
#include <vector>
#include <cmath>
#include <limits>
#include <algorithm>
#include <stdexcept>

namespace scope {
constexpr double empty=-std::numeric_limits<double>::infinity();
struct Reading {
    double momentary=empty,shortTerm=empty,integrated=empty,range=empty,truePeak=empty;
    double speech=empty,speechPercent=0,seconds=0;
    std::array<double,12> channelPeak{};
    bool speechAvailable=false;
    Reading(){channelPeak.fill(empty); }
};
class MeterCore {
public:
    ~MeterCore(){clear();}
    MeterCore()=default;
    MeterCore(const MeterCore&)=delete;
    MeterCore& operator=(const MeterCore&)=delete;
    void prepare(int sampleRate,const std::vector<int>& mapping,int centre) {
        clear(); rate=sampleRate; map=mapping; channels=static_cast<int>(map.size()); centreIndex=centre;
        if(channels<1||channels>12||rate<8000||rate>192000)throw std::runtime_error("Unsupported measurement format");
        const int mode=EBUR128_MODE_I|EBUR128_MODE_S|EBUR128_MODE_LRA|EBUR128_MODE_TRUE_PEAK|EBUR128_MODE_HISTOGRAM;
        full=ebur128_init(static_cast<unsigned>(channels),static_cast<unsigned long>(rate),mode);
        voice=ebur128_init(static_cast<unsigned>(channels),static_cast<unsigned long>(rate),EBUR128_MODE_I|EBUR128_MODE_HISTOGRAM);
        vad=fvad_new();
        if(!full||!voice||!vad){clear();throw std::runtime_error("Meter initialization failed");}
        for(int c=0;c<channels;++c) {
            if(ebur128_set_channel(full,static_cast<unsigned>(c),map[static_cast<size_t>(c)])!=EBUR128_SUCCESS
              ||ebur128_set_channel(voice,static_cast<unsigned>(c),map[static_cast<size_t>(c)])!=EBUR128_SUCCESS)
                throw std::runtime_error("Invalid channel mapping");
        }
        if(fvad_set_sample_rate(vad,16000)!=0||fvad_set_mode(vad,2)!=0)throw std::runtime_error("VAD setup failed");
        frameLength=rate/50; // All supported project sample rates have exact 20 ms frames.
        audio.assign(static_cast<size_t>(frameLength*channels),0);
        filtered.assign(static_cast<size_t>(frameLength),0);
        masked.assign(audio.size(),0);
        count=0;total=0;voiced=0;filter.fill(0);
    }
    void push(const float* frame) {
        for(int c=0;c<channels;++c) audio[static_cast<size_t>(count*channels+c)]=std::isfinite(frame[c])?frame[c]:0;
        ++count;
        if(count==frameLength){processFrame();count=0;}
    }
    void flushPartial() {
        // Loudness still includes final partial blocks. VAD requires complete 20 ms.
        if(count>0) {
            if(ebur128_add_frames_float(full,audio.data(),static_cast<size_t>(count))!=EBUR128_SUCCESS)
                throw std::runtime_error("Meter frame ingestion failed");
            total+=static_cast<unsigned long long>(count);count=0;
        }
    }
    Reading read() const {
        Reading r;r.channelPeak.fill(empty);r.seconds=static_cast<double>(total)/rate;
        auto get=[&](auto fn,double& dest) {double v=empty;if(fn(full,&v)==EBUR128_SUCCESS)dest=v;};
        if(r.seconds>=0.4) {get(ebur128_loudness_momentary,r.momentary);get(ebur128_loudness_global,r.integrated);}
        if(r.seconds>=3) {get(ebur128_loudness_shortterm,r.shortTerm);get(ebur128_loudness_range,r.range);}
        double maxPeak=0;
        for(int c=0;c<channels;++c) {
            double p=0;
            if(ebur128_true_peak(full,static_cast<unsigned>(c),&p)==EBUR128_SUCCESS) {
                r.channelPeak[static_cast<size_t>(c)]=p>0?20*std::log10(p):empty;
                maxPeak=std::max(maxPeak,p);
            }
        }
        r.truePeak=maxPeak>0?20*std::log10(maxPeak):empty;
        r.speechAvailable=total>=static_cast<unsigned long long>(rate)*0.4;
        if(r.speechAvailable&&voiced>0)ebur128_loudness_global(voice,&r.speech);
        if(total>0)r.speechPercent=100.0*static_cast<double>(voiced)/static_cast<double>(total);
        return r;
    }
private:
    void clear(){
        if(full)ebur128_destroy(&full);
        if(voice)ebur128_destroy(&voice);
        if(vad){fvad_free(vad);vad=nullptr;}
    }
    void processFrame() {
        if(ebur128_add_frames_float(full,audio.data(),static_cast<size_t>(frameLength))!=EBUR128_SUCCESS)
            throw std::runtime_error("Meter frame ingestion failed");
        // Low-pass the VAD source only. Measurement signal is never filtered here.
        const double a=1-std::exp(-2*3.141592653589793*6000/rate);
        for(int i=0;i<frameLength;++i) {
            double x;
            if(centreIndex>=0)x=audio[static_cast<size_t>(i*channels+centreIndex)];
            else if(channels==1)x=audio[static_cast<size_t>(i)];
            else x=(audio[static_cast<size_t>(i*channels)]+audio[static_cast<size_t>(i*channels+1)])*0.5;
            for(auto& stage:filter){stage+=a*(x-stage);x=stage;}
            filtered[static_cast<size_t>(i)]=static_cast<float>(x);
        }
        std::array<int16_t,320> pcm{};
        for(size_t i=0;i<pcm.size();++i) {
            const double position=static_cast<double>(i)*frameLength/320;
            const auto lo=static_cast<size_t>(position);
            const auto hi=std::min(lo+1,filtered.size()-1);
            const double t=position-static_cast<double>(lo);
            const double x=filtered[lo]+(filtered[hi]-filtered[lo])*t;
            pcm[i]=static_cast<int16_t>(std::clamp(x,-1.0,1.0)*32767);
        }
        const int decision=fvad_process(vad,pcm.data(),pcm.size());
        if(decision<0)throw std::runtime_error("VAD frame rejected");
        if(decision==1) {
            voiced+=static_cast<unsigned long long>(frameLength);
            masked=audio;
        } else std::fill(masked.begin(),masked.end(),0);
        // An explicit approximation: unvoiced 20ms frames are zeroed before R128 gating.
        // This is NOT Dolby Dialogue Intelligence / official dialogue-gated loudness.
        if(ebur128_add_frames_float(voice,masked.data(),static_cast<size_t>(frameLength))!=EBUR128_SUCCESS)
            throw std::runtime_error("Speech meter ingestion failed");
        total+=static_cast<unsigned long long>(frameLength);
    }
    ebur128_state* full=nullptr;ebur128_state* voice=nullptr;Fvad* vad=nullptr;
    int rate=48000,channels=2,centreIndex=-1,frameLength=960,count=0;
    unsigned long long total=0,voiced=0;
    std::vector<int> map;std::vector<float> audio,filtered,masked;
    std::array<double,4> filter{};
};
}
