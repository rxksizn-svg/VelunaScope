#include <juce_audio_utils/juce_audio_utils.h>
#include "BinaryData.h"
#include "MeterCore.h"
using Channels=juce::AudioChannelSet;
static int channelRole(Channels::ChannelType type) {
    switch(type) {
        case Channels::left:return EBUR128_LEFT;
        case Channels::right:return EBUR128_RIGHT;
        case Channels::centre:return EBUR128_CENTER;
        case Channels::LFE:return EBUR128_UNUSED;
        case Channels::leftSurround:return EBUR128_Mp110;
        case Channels::rightSurround:return EBUR128_Mm110;
        case Channels::leftSurroundSide:return EBUR128_Mp090;
        case Channels::rightSurroundSide:return EBUR128_Mm090;
        case Channels::leftSurroundRear:return EBUR128_Mp135;
        case Channels::rightSurroundRear:return EBUR128_Mm135;
        case Channels::topFrontLeft:return EBUR128_Up030;
        case Channels::topFrontRight:return EBUR128_Um030;
        case Channels::topRearLeft:return EBUR128_Up135;
        case Channels::topRearRight:return EBUR128_Um135;
        default:return -1;
    }
}
struct Frame {std::array<float,12> audio{};uint64_t generation=0;};
class Processor final:public juce::AudioProcessor,private juce::Thread {
public:
    juce::AudioProcessorValueTreeState state;
    std::atomic<bool> paused{false},fault{false};
    std::atomic<unsigned long long> lost{0};
    std::atomic<uint64_t> generation{1};
    struct View {scope::Reading reading;juce::StringArray names;juce::String layout;bool ready=false;};
    Processor():AudioProcessor(BusesProperties().withInput("Input",Channels::stereo(),true)
        .withOutput("Output",Channels::stereo(),true)),Thread("Veluna loudness worker"),
        state(*this,nullptr,"ScopeState",parameters()) {}
    ~Processor()override{stopThread(10000);}
    static juce::AudioProcessorValueTreeState::ParameterLayout parameters() {
        juce::AudioProcessorValueTreeState::ParameterLayout p;
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"profile",1},"Reference",
            juce::StringArray{"Spotify Normal","YouTube (custom reference)","Apple Music (custom reference)","Dolby Atmos Music guide","Custom"},0));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"target",1},"Custom LUFS",-36.0f,-5.0f,-14.0f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"peak",1},"Custom dBTP",-6.0f,0.0f,-1.0f));
        return p;
    }
    void prepareToPlay(double sr,int)override {
        stopThread(10000);
        sampleRate=static_cast<int>(std::round(sr));channelSet=getChannelLayoutOfBus(true,0);
        channelCount=channelSet.size();mapping.clear();names.clear();centre=-1;
        for(int c=0;c<channelCount;++c) {
            const auto type=channelSet.getTypeOfChannel(c);mapping.push_back(channelRole(type));
            names.add(Channels::getAbbreviatedChannelTypeName(type));
            if(type==Channels::centre)centre=c;
        }
        const int capacity=std::max(4096,sampleRate*2);
        fifo=std::make_unique<juce::AbstractFifo>(capacity);
        frames.assign(static_cast<size_t>(capacity),Frame{});
        generation.fetch_add(1);lost.store(0);fault.store(false);
        {const juce::ScopedLock lock(viewLock);view=View{};view.names=names;view.layout=channelSet.getDescription();}
        setLatencySamples(0);startThread();
    }
    void releaseResources()override{stopThread(10000);}
    bool isBusesLayoutSupported(const BusesLayout& b)const override {
        const auto s=b.getMainInputChannelSet();
        return s==b.getMainOutputChannelSet()&&(s==Channels::mono()||s==Channels::stereo()
            ||s==Channels::create5point1()||s==Channels::create7point1()||s==Channels::create7point1point4());
    }
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&)override {
        // Analysis only: no changes to audio, no allocations, locks or library calls.
        if(paused.load()||!fifo)return;
        const int requested=buffer.getNumSamples();int a=0,n1=0,b=0,n2=0;
        fifo->prepareToWrite(requested,a,n1,b,n2);
        const auto tag=generation.load();
        auto copy=[&](int dest,int count,int source) {
            for(int i=0;i<count;++i) {
                auto& f=frames[static_cast<size_t>(dest+i)];f.generation=tag;
                for(int c=0;c<channelCount;++c)f.audio[static_cast<size_t>(c)]=buffer.getSample(c,source+i);
            }
        };
        copy(a,n1,0);copy(b,n2,n1);fifo->finishedWrite(n1+n2);
        if(n1+n2<requested)lost.fetch_add(static_cast<unsigned long long>(requested-n1-n2));
    }
    void resetMeasurement(){generation.fetch_add(1);lost.store(0);}
    View snapshot()const{const juce::ScopedLock lock(viewLock);return view;}
    const juce::String getName()const override{return "Veluna Scope";}
    bool hasEditor()const override{return true;}
    juce::AudioProcessorEditor* createEditor()override;
    bool acceptsMidi()const override{return false;}
    bool producesMidi()const override{return false;}
    double getTailLengthSeconds()const override{return 0;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}
    void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return "Default";}
    void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock& block)override{if(auto x=state.copyState().createXml())copyXmlToBinary(*x,block);}
    void setStateInformation(const void* data,int size)override {
        if(auto x=getXmlFromBinary(data,size))if(x->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*x));
    }
private:
    void run()override {
        try {
            scope::MeterCore core;uint64_t active=0;
            auto last=juce::Time::getMillisecondCounter();auto published=last;
            bool pending=false;
            auto publish=[&]{
                const juce::ScopedLock lock(viewLock);
                view.reading=core.read();view.ready=true;
            };
            while(!threadShouldExit()) {
                const auto now=juce::Time::getMillisecondCounter();
                const auto expected=generation.load();
                if(active!=expected) {
                    core.prepare(sampleRate,mapping,centre);active=expected;pending=false;
                    publish();published=now;
                }
                int a=0,n1=0,b=0,n2=0;fifo->prepareToRead(4096,a,n1,b,n2);
                auto consume=[&](int start,int count) {
                    for(int i=0;i<count;++i) {
                        const auto& f=frames[static_cast<size_t>(start+i)];
                        if(f.generation==active){core.push(f.audio.data());pending=true;}
                    }
                };
                consume(a,n1);consume(b,n2);fifo->finishedRead(n1+n2);
                if(n1+n2>0)last=now;
                if(now-published>=100){publish();published=now;}
                if(n1+n2==0) {
                    if(pending&&now-last>250){core.flushPartial();publish();pending=false;}
                    wait(2);
                }
            }
            if(active!=0){core.flushPartial();publish();}
        }catch(...){fault.store(true);}
    }
    int sampleRate=48000,channelCount=2,centre=-1;Channels channelSet;
    std::vector<int> mapping;juce::StringArray names;
    std::unique_ptr<juce::AbstractFifo> fifo;std::vector<Frame> frames;
    mutable juce::CriticalSection viewLock;View view;
};
class Editor final:public juce::AudioProcessorEditor,private juce::Timer {
public:
    Editor(Processor& proc):AudioProcessorEditor(proc),p(proc) {
        logo=juce::ImageCache::getFromMemory(BinaryData::VelunaLogo_png,BinaryData::VelunaLogo_pngSize);
        reference.addItemList({"Spotify Normal","YouTube - custom reference","Apple Music - custom reference","Dolby Atmos Music guide","Custom"},1);
        addAndMakeVisible(reference);
        profileLink=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"profile",reference);
        reset.setButtonText("RESET");reset.onClick=[this]{p.resetMeasurement();};addAndMakeVisible(reset);
        pause.setButtonText("PAUSE");pause.setClickingTogglesState(true);
        pause.onClick=[this]{p.paused.store(pause.getToggleState());};addAndMakeVisible(pause);
        for(auto* s:{&target,&peak}) {
            s->setSliderStyle(juce::Slider::LinearHorizontal);s->setTextBoxStyle(juce::Slider::TextBoxRight,false,80,24);
            addAndMakeVisible(*s);
        }
        targetLink=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"target",target);
        peakLink=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"peak",peak);
        pause.setToggleState(p.paused.load(),juce::dontSendNotification);
        setSize(960,720);startTimerHz(10);
    }
    ~Editor()override{stopTimer();}
    void resized()override {
        reference.setBounds(25,87,340,30);reset.setBounds(735,87,90,30);pause.setBounds(835,87,100,30);
        target.setBounds(110,474,310,25);peak.setBounds(580,474,330,25);
    }
    static juce::String number(double x,const char* unit) {
        return std::isfinite(x)?juce::String(x,1)+" "+unit:"---";
    }
    void paint(juce::Graphics& g)override {
        const auto v=p.snapshot();const auto& r=v.reading;
        g.fillAll(juce::Colour(0xff14171d));
        if(logo.isValid())g.drawImageWithin(logo,20,5,170,66,juce::RectanglePlacement::centred);
        g.setColour(juce::Colour(0xffe9edf5));g.setFont(25);g.drawText("SCOPE",215,15,260,34,juce::Justification::left);
        g.setFont(13);g.setColour(juce::Colour(0xff99a6bd));
        g.drawText("LOUDNESS / STREAMING REFERENCES / MULTICHANNEL",215,48,650,20,juce::Justification::left);
        g.drawText("INPUT: "+v.layout+"   |   "+juce::String(v.names.size())+" channels   |   "+juce::String(r.seconds,1)+" s",
                   385,92,340,23,juce::Justification::left);
        const std::array<juce::String,6> labels{"SHORT-TERM","MOMENTARY","INTEGRATED (LEVEL-GATED)","LOUDNESS RANGE","SPEECH-GATED (VAD ESTIMATE)","TRUE PEAK MAX"};
        const std::array<juce::String,6> values{number(r.shortTerm,"LUFS"),number(r.momentary,"LUFS"),
            number(r.integrated,"LUFS"),number(r.range,"LU"),number(r.speech,"LUFS"),number(r.truePeak,"dBTP")};
        for(int i=0;i<6;++i) {
            const int x=25+(i%2)*465,y=135+(i/2)*105;
            g.setColour(juce::Colour(0xff202632));g.fillRoundedRectangle(static_cast<float>(x),static_cast<float>(y),445,92,8);
            g.setColour(juce::Colour(0xff9ba8c1));g.setFont(12);g.drawText(labels[static_cast<size_t>(i)],x+16,y+9,410,20,juce::Justification::left);
            g.setColour(i==5&&r.truePeak>0?juce::Colour(0xffff7777):juce::Colour(0xffb6b6ff));
            g.setFont(29);g.drawText(values[static_cast<size_t>(i)],x+16,y+32,410,38,juce::Justification::left);
            if(i==4){g.setFont(11);g.setColour(juce::Colour(0xff99a6bd));g.drawText(juce::String(r.speechPercent,1)+"% VAD-selected audio / not Dolby speech gating",x+16,y+70,410,18,juce::Justification::left);}
        }
        const int profile=static_cast<int>(p.state.getRawParameterValue("profile")->load());
        const bool custom=profile==4;
        target.setEnabled(custom);peak.setEnabled(custom);
        double aim=custom?p.state.getRawParameterValue("target")->load():(profile==2?-16:(profile==3?-18:-14));
        double ceiling=custom?p.state.getRawParameterValue("peak")->load():-1;
        if(profile==0&&r.integrated>-14)ceiling=-2;
        g.setColour(juce::Colour(0xffc6d1e5));g.setFont(13);
        juce::String referenceInfo="REFERENCE: "+juce::String(aim,1)+" LUFS / "+juce::String(ceiling,1)+" dBTP";
        if(std::isfinite(r.integrated))referenceInfo+="   |   Target difference: "+juce::String(aim-r.integrated,1)+" dB";
        g.drawText(referenceInfo,25,452,910,21,juce::Justification::left);
        if(std::isfinite(r.truePeak)) {
            const bool above=r.truePeak>ceiling;
            g.setColour(above?juce::Colour(0xffffa18e):juce::Colour(0xff8fd0ba));
            g.drawText(above?"Peak ABOVE reference by "+juce::String(r.truePeak-ceiling,1)+" dB":"Peak within selected reference",
                       25,438,910,14,juce::Justification::left);
        }
        g.drawText("Custom LUFS",25,474,90,25,juce::Justification::left);
        g.drawText("Custom dBTP",465,474,110,25,juce::Justification::left);
        juce::String message;
        if(p.fault.load())message="MEASUREMENT ERROR - reload plugin. Audio remains unchanged.";
        else if(p.lost.load()>0)message="INCOMPLETE MEASUREMENT: analysis queue overflow. Reset and replay in real time.";
        else if(profile==3)message=v.names.size()==6?"Guide only: use the Renderer 5.1 LOUDNESS RE-RENDER; validate with Dolby tools.":"Atmos guide needs the Renderer 5.1 loudness re-render. This bus is not an Atmos master measurement.";
        else if(profile==1||profile==2)message="Editable planning reference via Custom. Not a verified platform delivery requirement.";
        else message="Reference comparison only. Playback normalization is not a delivery pass/fail test.";
        g.setColour(p.fault.load()||p.lost.load()>0?juce::Colour(0xffff807b):juce::Colour(0xffd3c490));
        g.drawFittedText(message,25,508,910,38,juce::Justification::left,2);
        g.setColour(juce::Colour(0xff9ba8c1));g.setFont(12);
        g.drawText("PER-CHANNEL TRUE PEAK MAX (LFE excluded from LUFS; included in peak)",25,552,910,23,juce::Justification::left);
        const int n=v.names.size();
        for(int c=0;c<n;++c) {
            const int x=25+c*910/std::max(1,n),w=910/std::max(1,n)-4;
            const double db=r.channelPeak[static_cast<size_t>(c)];
            g.setColour(juce::Colour(0xff293244));g.fillRect(x,584,w,44);
            const float h=static_cast<float>(juce::jlimit(0.0,1.0,(db+60)/60))*44;
            g.setColour(db>0?juce::Colour(0xffff7777):juce::Colour(0xff5b9ea8));
            g.fillRect(static_cast<float>(x),628-h,static_cast<float>(w),h);
            g.setColour(juce::Colour(0xffe1e8f4));g.setFont(10);
            g.drawText(v.names[c],x,631,w,17,juce::Justification::centred);
            g.drawText(number(db,""),x,649,w,17,juce::Justification::centred);
        }
        g.setColour(juce::Colour(0xff8d9ab1));g.setFont(11);
        g.drawText("ANALYSIS ONLY / ZERO ADDED AUDIO LATENCY / RESET BEFORE EACH COMPLETE PASS",25,679,910,18,juce::Justification::centred);
        g.drawText("7.1.4 PCM monitoring is supported. No Atmos object decoder, ADM validation or Dolby certification.",25,697,910,18,juce::Justification::centred);
    }
private:
    void timerCallback()override{repaint();}
    Processor& p;juce::Image logo;juce::ComboBox reference;juce::TextButton reset,pause;
    juce::Slider target,peak;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> profileLink;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> targetLink,peakLink;
};
juce::AudioProcessorEditor* Processor::createEditor(){return new Editor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Processor;}
