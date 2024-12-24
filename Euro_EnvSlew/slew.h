//
// Slew Generator

#ifndef SLEW_H_
#define SLEW_H_

//#include "per/tim.h"
#include <math.h>

#define EXPF expf
// This causes with infinity with certain curves,
// which then causes NaN erros...
//#define EXPF expf_fast

// Fast Exp approximation
// 10x multiply version
inline float expf_fast(float x)
{
    x = 1.0f + x / 1024.0f;
    x *= x;
    x *= x;
    x *= x;
    x *= x;
    x *= x;
    x *= x;
    x *= x;
    x *= x;
    x *= x;
    x *= x;
    return x;
}

class Slew
{
  public:
    Slew() {}
    ~Slew() {}

    /** 
    Initializes the slew generator with the given sample rate
    */
    void Init(float samplerate = 1000.0f)
    {
        sample_rate_ = samplerate;
        out_ = 0.5f;
        Reset();
    }

    /** 
    This processes the slew generator
    */
    float Process(float signal_in, bool reset_in, float rise_time, float fall_time, float curve, bool fast)
    {
        if (reset_in) {
            Reset();
            return out_;
        }

        curve = curve * 2.0f - 1.0f; // scale to -1.0 -> 1.0
        float linear_factor, convex_factor, concave_factor;

        if (curve > 0.0f) {
            linear_factor = 1.0f - curve;
            concave_factor = curve;
            convex_factor = 0.0f;
        } else {
            linear_factor = 1.0f + curve;
            concave_factor = 0.0f;
            convex_factor = -curve;
        }
        
        bool rise = false;
        bool fall = false;

        if (signal_in > out_) {
            if (rise_time > (fast ? fast_epsilon_ : slow_epsilon_))
                rise = true;
            fall_start_ = signal_in;
        }
        if (signal_in < out_) {
            if (fall_time > (fast ? fast_epsilon_ : slow_epsilon_))
                fall = true;
            rise_start_ = signal_in;
        }

        float linear_delta, convex_delta, concave_delta;
        float n_rise = sample_rate_ * rise_time;
        float n_fall = sample_rate_ * fall_time;
        
        if (rise) {
            float y0 = (out_ - rise_start_) / (signal_in - rise_start_); 
            linear_delta = 1.0f / n_rise;
            convex_delta = exp_factor * (1.0f - y0) / n_rise;
            concave_delta = 1.0f / (exp_factor * EXPF(-exp_factor * y0) * n_rise);
        }

        if (fall) {
            float y0 = (fall_start_ - out_) / (fall_start_ - signal_in);
            linear_delta = -1.0f / n_fall;
            // switch curves for more interesting wave shaping
            concave_delta = -exp_factor * (1.0f - y0) / n_fall;
            convex_delta = -1.0f / (exp_factor * EXPF(-exp_factor * y0) * n_fall);
        }

        if (!fall && !rise) {
            out_ = signal_in;
            fall_start_ = signal_in;
            rise_start_ = signal_in;
        } else {
            out_ = out_ + linear_delta * linear_factor + convex_delta * convex_factor + concave_delta * concave_factor;
        }

        if (rise && (out_ >= signal_in)) {
            rise = false;
            out_ = signal_in;
        }
        if (fall && (out_ <= signal_in)) {
            fall = false;
            out_ = signal_in;
        }
        rising_ = rise;
        falling_ = fall;

        return out_;
    }

    void Reset() {
        falling_ = false;
        rising_ = false;
        fall_start_ = out_;
        rise_start_ = out_;
    }

    /** Get the "falling" state variable
    */
    inline bool getFalling() { return falling_; }

    /** Get the "rising" state variable
    */
    inline bool getRising() { return rising_; }

  private:
    static constexpr float fast_epsilon_ = 0.001f;
    static constexpr float slow_epsilon_ = 0.05f;
    static constexpr float exp_factor = 6.0f;
    float   fall_start_;
    float   rise_start_;
    bool    falling_;
    bool    rising_;
    float   sample_rate_;
    float   out_;
};

#endif // SLEW_H_