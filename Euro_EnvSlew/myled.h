//
// Fixed LED

#ifndef MYLED_H_
#define MYLED_H_

#include "per/tim.h"

class MyLed
{
  public:
    MyLed() {}
    ~MyLed() {}

    /** 
    Initializes an LED using the specified hardware pin.
    \param pin chooses LED pin
    \param invert will set whether to internally invert the brightness due to hardware config.
        \param samplerate sets the rate at which 'Update()' will be called (used for software PWM)
    */
    void Init(dsy_gpio_pin pin, bool invert, float samplerate = 1000.0f)
    {
        // Init hardware LED
        // Simple OUTPUT GPIO for now.
        hw_pin_.pin  = pin;
        hw_pin_.mode = DSY_GPIO_MODE_OUTPUT_PP;
        dsy_gpio_init(&hw_pin_);
        // Set internal stuff.
        bright_  = 0.0f;
        pwm_ = 0.0f;
        Set(bright_);
        invert_     = invert;
        samplerate_ = samplerate;
        if(invert_)
        {
            on_  = false;
            off_ = true;
        }
        else
        {
            on_  = true;
            off_ = false;
        }
    }

    /** 
    Sets the brightness of the Led.
    \param val will be cubed for gamma correction, and then quantized to 8-bit values for Software PWM
    8-bit is for more flexible update rate options, as 12-bit or more would require faster update rates.
    */
    void Set(float val)
    {
        bright_     = cube(val);
    }

    /** 
    This processes the pwm of the LED
    sets the hardware accordingly.
    */
    void Update()
    {
        // Shout out to @grrwaaa for the quick fix for pwm
        pwm_ += 120.f / samplerate_;
        if(pwm_ > 1.f)
            pwm_ -= 1.f;
        dsy_gpio_write(&hw_pin_, bright_ > pwm_ ? on_ : off_);

        // Once we have a slower timer set up:
        // Right now its too fast.

        //    dsy_gpio_write(&hw_pin_,
        //                   (dsy_tim_get_tick() & RESOLUTION_MAX) < pwm_thresh_ ? on_
        //                                                                       : off_);
    }

    /** Set the rate at which you'll update the leds without reiniting the led
     *  \param sample_rate New update rate in hz.
    */
    inline void SetSampleRate(float sample_rate) { samplerate_ = sample_rate; }

  private:
    float    bright_;
    float    pwm_;
    float    samplerate_;
    bool     invert_, on_, off_;
    dsy_gpio hw_pin_;

};

#endif // MLED_H_