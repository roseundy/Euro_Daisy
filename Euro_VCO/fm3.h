//
// fm oscillator with two modulators and one carrier

#ifndef FM3_H_
#define FM3_H_

using namespace daisysp;

class Fm3
{
  public:
    Fm3() {}
    ~Fm3() {}

    /** Initializes the FM3 module.
        \param samplerate - The sample rate of the audio engine being run. 
    */
    void Init(float samplerate)
    {
        //init oscillators
        car_.Init(samplerate);
        mod1_.Init(samplerate);
        mod2_.Init(samplerate);

        //set some reasonable values
        lfreq_  = 440.f;
        lratio1_ = 2.f;
        lratio2_ = 2.f;
        SetFrequency(lfreq_);
        SetRatio1(lratio1_);
        SetRatio2(lratio2_);

        car_.SetAmp(1.f);
        mod1_.SetAmp(1.f);
        mod2_.SetAmp(1.f);

        car_.SetWaveform(Oscillator::WAVE_SIN);
        mod1_.SetWaveform(Oscillator::WAVE_SIN);
        mod2_.SetWaveform(Oscillator::WAVE_SIN);

        idx1_ = 1.f;
        idx2_ = 1.f;
    }

    /**  Returns the next sample
    */
    float Process()
    {
        if(lratio1_ != ratio1_ || lratio2_ != ratio2_ || lfreq_ != freq_)
        {
            lratio1_ = ratio1_;
            lratio2_ = ratio2_;
            lfreq_  = freq_;
            car_.SetFreq(lfreq_);
            mod1_.SetFreq(lfreq_ * lratio1_);
            mod2_.SetFreq(lfreq_ * lratio2_);
        }

        float modval1 = mod1_.Process();
        float modval2 = mod2_.Process();
        car_.PhaseAdd(modval1 * idx1_ + modval2 * idx2_);
        return car_.Process();
    }

    /** Carrier freq. setter
        \param freq Carrier frequency in Hz
    */
    void SetFrequency(float freq) { freq_ = fabsf(freq); }

    /** Set modulator freq. relative to carrier
        \param ratio New modulator freq = carrier freq. * ratio
    */
    void SetRatio1(float ratio) { ratio1_ = fabsf(ratio); }
    void SetRatio2(float ratio) { ratio2_ = fabsf(ratio); }

    /** Index setter
      \param FM depth, 5 = 2PI rads
     */
    void SetIndex1(float index) { idx1_ = index * kIdxScalar; }
    void SetIndex2(float index) { idx2_ = index * kIdxScalar; }


    /** Returns the current FM index. */
    float GetIndex1() { return idx1_ * kIdxScalarRecip; }
    float GetIndex2() { return idx2_ * kIdxScalarRecip; }

    /** Resets all oscillators */
    void Reset()
    {
        car_.Reset();
        mod1_.Reset();
        mod2_.Reset();
    }

  private:
    //static constexpr float kIdxScalar      = 0.2f;
    static constexpr float kIdxScalar = 1.f / TWOPI_F;
    static constexpr float kIdxScalarRecip = 1.f / kIdxScalar;

    Oscillator mod1_, mod2_, car_;
    float      idx1_, idx2_;
    float      freq_, lfreq_, ratio1_, ratio2_, lratio1_, lratio2_;
};


#endif // FM3_H_