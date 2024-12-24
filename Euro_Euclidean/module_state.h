//
// module state

#ifndef MODULE_STATE_H_
#define MODULE_STATE_H_

struct EuclidChannel {
    int                 length;
	int                 pulsesSetting;
    int                 pulsesAttenuvert;
    int                 offsetSetting;
    int                 offsetAttenuvert;

    bool operator== (const EuclidChannel &rhs) const {
    return ((this->length == rhs.length) &&
            (this->pulsesSetting == rhs.pulsesSetting) &&
            (this->pulsesAttenuvert == rhs.pulsesAttenuvert) &&
            (this->offsetSetting == rhs.offsetSetting) &&
            (this->offsetAttenuvert == rhs.offsetAttenuvert));
    };

};

struct ModuleState
{
    // Version
    int                 version;
    EuclidChannel       channel[4];

    bool operator!= (const ModuleState &rhs) const {
    return !((this->version == rhs.version) &&
             (this->channel[0] == rhs.channel[0]) &&
             (this->channel[1] == rhs.channel[1]) &&
             (this->channel[2] == rhs.channel[2]) &&
             (this->channel[3] == rhs.channel[3]));
    };
};

#endif // MODULE_STATE_H_