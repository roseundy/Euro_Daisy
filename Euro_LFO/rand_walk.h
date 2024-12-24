//
// Random Walk "Oscillator"

#ifndef RAND_WALK_H_
#define RAND_WALK_H_

using namespace daisysp;

class RandWalk
{
  public:
    RandWalk() {}
    ~RandWalk() {}

    /** Initializes the RandWalk module.
        \param samplerate - The sample rate of the audio engine being run. 
    */
    void Init(float samplerate)
    {
        sample_period_ = 1.0f / samplerate;
        randseed_ = 1;
        val_ = 0;
        SetFrequency(1.0f);
    }

    /**  Returns the next sample
    */
    float Process()
    {
        randseed_ *= 16807;
        int32_t delta = (int32_t)((randseed_ * coeff_) * range_);
        if (delta > 8388607)
            delta = 8388607;
        if (delta < -8388607)
            delta = -8388607;
        val_ += delta;

        // never allow |val_| to remain above 2^24
        if(val_ > 16777215)
            val_ -= 16777215;
        else if(val_ < -16777215)
            val_ += 16777215;

        // "mirror" the output if |val_| >= 2^23
        // this allows the walk to continue even when near the max/min value
        int32_t rval = val_;
        if(rval > 8388607)
            rval = 16777215 - val_;
        else if(rval < -8388607)
            rval = -16777215 - val_;

        return (float)(rval / 8388608.0f);
    }

    /** Set frequency
        \param freq  New frequency
    */
    void SetFrequency(float freq)
    {
        range_ = fabsf(freq) * range_factor_;
        if (range_ > 8388607.0f)
            range_ = 8388607;
    }

  private:
    int32_t                 val_;
    float                   sample_period_;
    float                   range_;
    static constexpr float  coeff_ = 4.6566129e-010f;
    //static constexpr float  range_factor_ = 699;
    static constexpr float  range_factor_ = 2000;
    int32_t                 randseed_;
};


#endif // RAND_WALK_H_