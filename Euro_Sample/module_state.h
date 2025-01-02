//
// module state

#ifndef MODULE_STATE_H_
#define MODULE_STATE_H_

struct SampleChannel
{
    int dirNum;
    int sampleNum;
    int level;
    int cvAttenuvert;
    int cvTarget;
    float cvOffset;

    bool operator== (const SampleChannel &rhs) const {
    return ((this->dirNum == rhs.dirNum) &&
            (this->sampleNum == rhs.sampleNum) &&
            (this->level == rhs.level) &&
            (this->cvAttenuvert == rhs.cvAttenuvert) &&
            (this->cvTarget == rhs.cvTarget) &&
            (this->cvOffset == rhs.cvOffset));
    };
};

struct ModuleState
{
    // Version
    int                 version;
    
    SampleChannel       channel[4];

    bool operator!= (const ModuleState &rhs) const {
    return !((this->version == rhs.version) &&
             (this->channel[0] == rhs.channel[0]) &&
             (this->channel[1] == rhs.channel[1]) &&
             (this->channel[2] == rhs.channel[2]) &&
             (this->channel[3] == rhs.channel[3]));
    };
};

#endif // MODULE_STATE_H_