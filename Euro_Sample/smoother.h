//
// Smoother

#ifndef SMOOTHER_H_
#define SMOOTHER_H_

#include <stdlib.h>
#include <stdint.h>

class Smoother
{
  public:
    Smoother() {}
    ~Smoother() {}

    /** 
    Initializes the smoother
    */
    void Init(float beta)
    {
        SetBeta(beta);
        Reset();
    }

    /** 
    This processes the smoother
    */
    float Process(float signal)
    {
        out_ = signal * beta_ + out_ * one_minus_beta_;
        return out_;
    }

    void Reset() {
        out_ = 0.0f;
    }

    void SetBeta(float newbeta) {
        beta_ = newbeta;
        one_minus_beta_ = (1.0f - newbeta);
    }

    inline float GetVal() { return out_; }

  private:
    float beta_;
    float one_minus_beta_;
    float out_;
};

#endif // SLEW_H_