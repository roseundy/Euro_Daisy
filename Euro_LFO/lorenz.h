//
// Lorenz equation based chaotic "Oscillator"

#ifndef LORENZ_H_
#define LORENZ_H_

class Lorenz
{
  public:
    Lorenz() {}
    ~Lorenz() {}

    /** Initializes the Lorenz module.
        \param samplerate - The sample rate of the audio engine being run. 
    */
    void Init(float samplerate)
    {
        x_ = 2.0f;
        y_ = 1.0f;
        z_ = 1.0f;
        fail_ = 0;
        SetFrequency(1.0f);
    }

    /**  Creates the next sample
    */
    void Process()
    {
        float x = x_ + deltat_ * sigma_ * (y_ - x_);
        float y = y_ + deltat_ * (x_ * (rho_ - z_) - y_);
        float z = z_ + deltat_ * (x_ * y_- beta_ * z_);

        x_ = x;
        y_ = y;
        z_ = z;

        if ((fabsf(x_) > 1000.0f) || (fabsf(y_) > 1000.0f) || (fabsf(z_) > 1000.0f))
            fail_ = 1;
    }

    /** Set frequency
        \param freq  New frequency
    */
    void SetFrequency(float freq)
    {
        deltat_ = fabsf(freq) * tfactor_;
        if (deltat_ > 0.02f)
            deltat_ = 0.02f; // keep from exploding
    }

    float GetX() { return fail_ ? -1.0 : fclamp(x_ * x_scale_, -1.0f, 1.0f); } // centered on 0
    float GetY() { return fail_ ? -1.0 : fclamp(y_ * y_scale_, -1.0f, 1.0f); } // centered on 0
    float GetZ() { return fail_ ? -1.0 : fclamp(z_ * z_scale_ - 1.0f, -1.0f, 1.0f); } // always non-negative

  private:
    static constexpr float  beta_ = 8.0f / 3.0f;
    static constexpr float  sigma_ = 10.0f;
    static constexpr float  rho_ = 28.0f;
    static constexpr float  tfactor_ = 0.01f / 500.0f;
    static constexpr float  x_scale_ = 1.0f / 24.0f;
    static constexpr float  y_scale_ = 1.0f / 31.0f;
    static constexpr float  z_scale_ = 2.0f / 63.0f;
    float                   x_, y_, z_;
    float                   deltat_;
    int                     fail_;
};


#endif // LORENZ_H_