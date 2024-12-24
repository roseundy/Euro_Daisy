//
// fm oscillator with three modulators and one carrier

#ifndef FM4OP_H_
#define FM4OP_H_

using namespace daisysp;

class Fm4Op
{
  public:
    Fm4Op() {}
    ~Fm4Op() {}

    /** Initializes the FM module.
        \param samplerate - The sample rate of the audio engine being run. 
    */
    void Init(float samplerate)
    {
        //set some reasonable values and init oscillators
        lfreq_[0]  = 440.f;
        lfreq_[1] = 220.f;
        lfreq_[2] = 110.f;
        lfreq_[3] = 55.f;
        for (int i=0; i<4; i++) {
            op_[i].Init(samplerate);
            op_[i].SetWaveform(Oscillator::WAVE_SIN);
            op_[i].SetFreq(lfreq_[i]);
            op_[i].SetAmp(1.f);
            SetFrequency(i, lfreq_[i]);
            SetIndex(i, 1.0f);
        }
        SetAlgorithm(7);
        fbnode_ = 0.f;
    }

    /**  Returns the next sample
    */
    float Process()
    {
        if(lfreq_[0] != freq_[0] || lfreq_[1] != freq_[1] || lfreq_[2] != freq_[2] || lfreq_[3] != freq_[3])
        {
            for (int i=0; i<4; i++) {
                lfreq_[i] = freq_[i];
                op_[i].SetFreq(lfreq_[i]);
            }
        }

        // OP3
        op_[3].PhaseAdd(fbnode_ * fb_);
        float out3 = op_[3].Process() * idx_[3];

        // OP2
        op_[2].PhaseAdd(coeff_23_ * out3);
        float out2 = op_[2].Process() * idx_[2];

        // OP1
        op_[1].PhaseAdd(coeff_13_ * out3 + coeff_12_ * out2);
        float out1 = op_[1].Process() * idx_[1];

        // OP0
        op_[0].PhaseAdd(coeff_03_ * out3 + coeff_02_ * out2 + coeff_01_ * out1);
        float out0 = op_[0].Process() * idx_[0];

        // Output node - use unscaled idx for outputs
        float output = (coeff_out3_ * out3 + coeff_out2_ * out2 + coeff_out1_ * out1 + coeff_out0_ * out0) * kIdxScalarRecip;

        // Feeback node
        fbnode_ = coeff_fb3_ * out3 + coeff_fb2_ * out2 + coeff_fb1_ * out1 + coeff_fb0_ * out0;

        return output;
    }

    /** Algorithm setter
        \param algorithm number to use (0-8)
     */
    void SetAlgorithm(int algo) {
        algo_ = algo;

        coeff_23_ = 0.f;
        coeff_13_ = coeff_12_ = 0.f;
        coeff_03_ = coeff_02_ = coeff_01_ = 0.f;
        coeff_out3_ = coeff_out2_ = coeff_out1_ = 0.f;
        coeff_out0_ = 1.0f;
        coeff_fb3_ = coeff_fb2_ = coeff_fb1_ = coeff_fb0_ = 0.f;


        switch(algo_) {
            case 0:
                coeff_23_ = 1.0f;
                coeff_12_ = 1.0f;
                coeff_01_ = 1.0f;
                coeff_fb3_ = 1.0f;
                break;

            case 1:
                coeff_23_ = 1.0f;
                coeff_01_ = 1.0f;
                coeff_out2_ = 0.5f;
                coeff_out0_ = 0.5f;
                coeff_fb3_ = 1.0f;
                break;

            case 2:
                coeff_23_ = 1.0f;
                coeff_01_ = 1.0f;
                coeff_out2_ = 0.5f;
                coeff_out0_ = 0.5f;
                coeff_fb2_ = 1.0f;
                coeff_fb0_ = 1.0f;
                break;

            case 3:
                coeff_23_ = 1.0f;
                coeff_02_ = 1.0f;
                coeff_fb3_ = 1.0f;
                break;

            case 4:
                coeff_23_ = 1.0f;
                coeff_02_ = 1.0f;
                coeff_01_ = 1.0f;
                coeff_fb3_ = 1.0f;
                break;

            case 5:
                coeff_03_ = 1.0f;
                coeff_02_ = 1.0f;
                coeff_01_ = 1.0f;
                coeff_fb3_ = 1.0f;
                break;

            case 6:
                coeff_23_ = 1.0f;
                coeff_13_ = 1.0f;
                coeff_03_ = 1.0f;
                coeff_out2_ = 0.333f;
                coeff_out1_ = 0.333f;
                coeff_out0_ = 0.333f;
                coeff_fb3_ = 1.0f;
                break;

            case 7:
                coeff_out3_ = 0.25f;
                coeff_out2_ = 0.25f;
                coeff_out1_ = 0.25f;
                coeff_out0_ = 0.25f;
                coeff_fb3_ = 1.0f;
                coeff_fb2_ = 1.0f;
                coeff_fb1_ = 1.0f;
                coeff_fb0_ = 1.0f;
                break;

            default:
                coeff_13_ = 0.5f;
                coeff_12_ = 0.5f;
                coeff_01_ = 1.0f;
                coeff_fb3_ = 1.0f;
                break;
        }
    }

    /** Op freq. setter
        \param freq Op frequency in Hz
    */
    void SetFrequency(int op, float freq)
    {
         freq_[op] = fabsf(freq);
    }

    /** Index setter
      \param FM depth
     */
    void SetIndex(int i, float index) { idx_[i] = index * kIdxScalar; }

    /** Feedbak setter
     \param OP3 Feedback amount
     */
    void SetFB(float fb) { fb_ = fb * kIdxScalar; }

    /** Returns the current FM index. */
    float GetIndex(int i) { return idx_[i] * kIdxScalarRecip; }
    float GetFB3() { return fb_ * kIdxScalarRecip; }

    /** Resets all oscillators */
    void Reset()
    {
        for (int i=0; i<4; i++)
            op_[i].Reset();
    }

  private:
    static constexpr float kIdxScalar      = 0.2f;
    //static constexpr float kIdxScalar = 1.f / TWOPI_F;
    static constexpr float kIdxScalarRecip = 1.f / kIdxScalar;

    int        algo_;
    float      coeff_23_;
    float      coeff_13_, coeff_12_;
    float      coeff_03_, coeff_02_, coeff_01_;
    float      coeff_out3_, coeff_out2_, coeff_out1_, coeff_out0_;
    float      coeff_fb3_, coeff_fb2_, coeff_fb1_, coeff_fb0_;

    Oscillator op_[4];
    float      idx_[4];
    float      fb_;
    float      freq_[4], lfreq_[4];
    float      fbnode_;
};


#endif // FM3_H_