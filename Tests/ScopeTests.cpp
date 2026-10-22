#include "MeterCore.h"
#include <iostream>
#include <cstdlib>
static void check(bool ok,const char* reason){if(!ok){std::cerr<<reason<<"\n";std::exit(1);}}
static scope::Reading tone(const std::vector<int>& map,int active,double amp=0.1,int rate=48000) {
    scope::MeterCore m;m.prepare(rate,map,-1);
    std::array<float,12> f{};
    for(int i=0;i<rate*4;++i) {
        f.fill(0);f[static_cast<size_t>(active)]=static_cast<float>(amp*std::sin(2*3.141592653589793*1000*i/rate));
        m.push(f.data());
    }
    m.flushPartial();return m.read();
}
int main(){
    for(int rate:{44100,48000,96000}) {
        auto r=tone({EBUR128_LEFT},0,0.1,rate);
        check(std::abs(r.integrated+23.0)<0.25,"1 kHz mono loudness calibration failed");
        check(std::abs(r.momentary-r.integrated)<0.15,"Momentary window differs on steady tone");
        check(std::abs(r.shortTerm-r.integrated)<0.15,"Short-term window differs on steady tone");
        check(std::abs(r.truePeak+20)<0.15,"Sine true peak calibration failed");
    }
    auto front=tone({EBUR128_LEFT},0);
    auto side=tone({EBUR128_Mp090},0);
    check(std::abs((side.integrated-front.integrated)-1.5)<0.15,"Side surround weighting failed");
    auto lfe=tone({EBUR128_LEFT,EBUR128_RIGHT,EBUR128_CENTER,EBUR128_UNUSED,EBUR128_Mp090,EBUR128_Mm090},3);
    check(!std::isfinite(lfe.integrated),"LFE should not contribute to loudness");
    check(std::abs(lfe.truePeak+20)<0.15,"LFE must contribute to peak measurement");
    auto height=tone({EBUR128_Up030},0);
    check(std::abs(height.integrated-front.integrated)<0.15,"Height weighting failed");
    scope::MeterCore m;m.prepare(48000,{EBUR128_LEFT,EBUR128_RIGHT},-1);
    std::array<float,2> f{};
    for(int i=0;i<192000;++i) {
        const float x=static_cast<float>(0.1*std::sin(2*3.141592653589793*1000*i/48000));
        f={x,x};m.push(f.data());
    }
    check(std::abs(m.read().integrated-front.integrated-3.0103)<0.15,"Stereo energy summation failed");
    for(int i=0;i<192000;++i){f={0,0};m.push(f.data());}
    check(std::abs(m.read().integrated-front.integrated-3.0103)<0.5,"Silence loudness gating failed");
    m.prepare(48000,{EBUR128_LEFT,EBUR128_RIGHT},-1);
    for(int i=0;i<192000;++i){f={0,0};m.push(f.data());}
    auto silent=m.read();
    check(!std::isfinite(silent.integrated)&&!std::isfinite(silent.truePeak),"Silence should be blank");
    check(silent.speechPercent==0,"Silence VAD false-positive");
    m.prepare(48000,{EBUR128_LEFT},-1);
    for(int i=0;i<96000;++i) {
        f[0]=static_cast<float>(0.9*std::sin(3.141592653589793*i/2+3.141592653589793/4));
        m.push(f.data());
    }
    check(m.read().truePeak>-1.4&&m.read().truePeak<-0.6,"Inter-sample true peak detection failed");
    std::cout<<"Scope core checks passed. Not a certification or a host/UI validation.\n";
}
