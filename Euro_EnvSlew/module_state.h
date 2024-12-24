//
// module state

#ifndef MODULE_STATE_H_
#define MODULE_STATE_H_

struct ModuleState
{
    // Version
    int                 version;
    
    float       cv_offset[6];
    float       attack_offset;
    float       release_offset;

    bool operator!=(const ModuleState &rhs) const
    {
        return !((this->version == rhs.version)
                 && (this->cv_offset[0] == rhs.cv_offset[0])
                 && (this->cv_offset[1] == rhs.cv_offset[1])
                 && (this->cv_offset[2] == rhs.cv_offset[2])
                 && (this->cv_offset[2] == rhs.cv_offset[3])
                 && (this->cv_offset[2] == rhs.cv_offset[4])
                 && (this->cv_offset[2] == rhs.cv_offset[5])
                 && (this->cv_offset[2] == rhs.attack_offset)
                 && (this->cv_offset[3] == rhs.release_offset));
    };
};

#endif // MODULE_STATE_H_