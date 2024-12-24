/*
Copyright (c) 2020 Electrosmith, Corp

Use of this source code is governed by an MIT-style
license that can be found in the LICENSE file or at
https://opensource.org/licenses/MIT.
*/

#pragma once
#ifndef MYDELAYLINE_H
#define MYDELAYLINE_H
#include <stdlib.h>
#include <stdint.h>
//namespace daisysp
//{
/** Simple Delay line.
November 2019

Converted to Template December 2019

declaration example: (1 second of floats)

DelayLine<float, SAMPLE_RATE> del;

By: shensley
*/
template <typename T, size_t max_size>
class MyDelayLine
{
  public:
    MyDelayLine() {}
    ~MyDelayLine() {}
    /** initializes the delay line by clearing the values within, and setting delay to 1 sample.
    */
    void Init() { Reset(); }
    /** clears buffer, sets write ptr to 0, and delay to 1 sample.
    */
    void Reset()
    {
        for(size_t i = 0; i < max_size; i++)
        {
            line_[i] = T(0);
        }
        write_ptr_ = 0;
        delay_     = 1;
    }

    /** sets the delay time in samples
        If a float is passed in, a fractional component will be calculated for interpolating the delay line.
    */
    inline void SetDelay(size_t delay)
    {
        delay_ = delay < max_size ? delay : max_size - 1;
    }

    /** writes the sample of type T to the delay line, and advances the write ptr
    */
    inline void Write(const T sample)
    {
        line_[write_ptr_] = sample;
        write_ptr_        = (write_ptr_ - 1 + max_size) % max_size;
    }

    /** returns the next sample of type T in the delay line, interpolated if necessary.
    */
    inline const T Read() const
    {
        T a = line_[(write_ptr_ + delay_) % max_size];
        return a;
    }

  private:
    size_t write_ptr_;
    size_t delay_;
    T      line_[max_size];
};
//} // namespace daisysp
#endif