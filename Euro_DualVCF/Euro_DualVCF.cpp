#include "daisy_patch_sm.h"
#include "daisysp.h"

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

DaisyPatchSM 	hw;
Svf				filter1, filter2;
Switch 			toggle1, toggle2;

const float DEADZONE_WIDTH      = 0.1f;
const float DEADZONE_HALF_WIDTH = DEADZONE_WIDTH / 2.0f;
const float LIVE_WIDTH          = 0.5 - DEADZONE_HALF_WIDTH;

// create a deadzone for a control at 12 o'clock,
// with left giving negative values and right giving positive values
// takes a unipolar value (between 0 and 1.0)
// returns a bipolar value (between -1.0 and 1.0)
//
float u2b_middle_deadzone(float raw) {
    if ((raw >= (0.5f - DEADZONE_HALF_WIDTH)) && (raw <= (0.5f + DEADZONE_HALF_WIDTH)))
        return 0.f;

    if (raw < 0.5f)
        // negative
        return (raw - LIVE_WIDTH) / LIVE_WIDTH;

    // positive
    return (raw - 1.0f + LIVE_WIDTH) / LIVE_WIDTH;
}

// create a deadzone for a control at the leftmost position,
// takes a unipolar value (between 0 and 1.0)
// returns a unipolar value (between 0 and 1.0)
//
float u2u_left_deadzone(float raw) {
    if (raw <= DEADZONE_HALF_WIDTH)
        return 0.f;

    // positive
    return (raw - DEADZONE_HALF_WIDTH) / (1.0f - DEADZONE_HALF_WIDTH);
}

// unipolar flip with correction
float u_flip(float in, float adj) {
    return (1.0f - in - adj);
}

float cv_to_freq(float volts, float octave) {
	// convert to freq
	float freq = 440.0f * octave * exp2f(volts);
	return freq;
}

#define CV5_ADJ 0.117
#define CV6_ADJ 0.117

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	hw.ProcessAllControls();
	
	// CV1 - frequency CV1
	// CV2 - resonance CV1
	// CV3 - frequency control 1
	// CV4 - resonance control 1
	// CV5 - frequency CV2
	// CV6 - resonance CV2
	// CV7 - frequency control 2
	// CV8 - resonance control 2
	// ADC9 - frequency attenuvert 1
	// ADC10 - resonance attenuvert 1
	// ADC11 - frequency attenuvert 2
	// ADC12 - resonance attenuvert 2
	// (D1) - LP/HP switch 1
	// (D10) - LP/HP switch 2
    
	float tune1 = u_flip(hw.GetAdcValue(CV_3), 0.0f) * 5.0f ;
    float voct1 = hw.GetAdcValue(CV_1) * u2b_middle_deadzone(u_flip(hw.GetAdcValue(ADC_9), 0.0f)) * 5.0f ;
    float freq1 = cv_to_freq(voct1 + tune1, 0.125);
	float res1 = u_flip(hw.GetAdcValue(CV_4), 0.0f) + hw.GetAdcValue(CV_2) * u2b_middle_deadzone(u_flip(hw.GetAdcValue(ADC_10), 0.0f));

	filter1.SetFreq(freq1);
	filter1.SetRes(res1);
	filter1.SetDrive(0.5f);

	toggle1.Debounce();
	bool highpass1 = toggle1.Pressed();

	float tune2 = u_flip(hw.GetAdcValue(CV_7), 0.0f) * 5.0f ;
    float voct2 = hw.GetAdcValue(CV_5) * u2b_middle_deadzone(u_flip(hw.GetAdcValue(ADC_11), 0.0f)) * 5.0f ;
    float freq2 = cv_to_freq(voct2 + tune2, 0.125);
	float res2 = u_flip(hw.GetAdcValue(CV_8), 0.0f) + hw.GetAdcValue(CV_6) * u2b_middle_deadzone(u_flip(hw.GetAdcValue(ADC_12), 0.0f));

	filter2.SetFreq(freq2);
	filter2.SetRes(res2);
	filter2.SetDrive(0.5f);

	toggle2.Debounce();
	bool highpass2 = toggle2.Pressed();
 
	for (size_t i = 0; i < size; i++) {
		filter1.Process(IN_L[i]);
		filter2.Process(IN_R[i]);

		OUT_L[i] = highpass1 ? filter1.High() : filter1.Low();
		OUT_R[i] = highpass2 ? filter2.High() : filter2.Low();
	}
}

int main(void)
{	hw.Init();
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

	float sample_rate = hw.AudioSampleRate();
	//float block_size = hw.AudioBlockSize();

	filter1.Init(sample_rate);
	filter2.Init(sample_rate);

	toggle1.Init(hw.D1,  1000, Switch::TYPE_TOGGLE, Switch::POLARITY_INVERTED, Switch::PULL_NONE);
	toggle2.Init(hw.D10, 1000, Switch::TYPE_TOGGLE, Switch::POLARITY_INVERTED, Switch::PULL_NONE);

	hw.StartAudio(AudioCallback);
	while(1) {}
}
