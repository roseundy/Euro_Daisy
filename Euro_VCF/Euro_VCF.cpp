#include "daisy_patch_sm.h"
#include "daisysp.h"

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

DaisyPatchSM 	hw;
Svf				filter;

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
	
	// CV1 - frequency CV
	// CV2 - resonance CV
	// CV3 - frequency offset
	// CV4 - resonance offset
	// CV5 - frequency CV attenuvert
	// CV6 - resonance CV attenuvert
	// CV7 - drive and select between LPF and HPF output
    
	float tune = u_flip(hw.GetAdcValue(CV_3), 0.0f) * 5.0f ;
    float voct = hw.GetAdcValue(CV_1) * u2b_middle_deadzone(u_flip(hw.GetAdcValue(CV_5), CV5_ADJ)) * 5.0f ;
    float freq = cv_to_freq(voct + tune, 0.125);

	float res = u_flip(hw.GetAdcValue(CV_4), 0.0f) + hw.GetAdcValue(CV_2) * u2b_middle_deadzone(u_flip(hw.GetAdcValue(CV_6), CV6_ADJ));
	float drive = u2b_middle_deadzone(u_flip(hw.GetAdcValue(CV_7), 0.0f));
	bool low_out = (drive <= 0);
	drive = fabsf(drive);

	filter.SetFreq(freq);
	filter.SetRes(res);
	filter.SetDrive(drive);
 
	for (size_t i = 0; i < size; i++)
	{
		filter.Process(IN_L[i]);

		OUT_L[i] = low_out ? filter.Low() : filter.High();
		OUT_R[i] = filter.Band();
	}
}

int main(void)
{	hw.Init();
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

	float sample_rate = hw.AudioSampleRate();
	//float block_size = hw.AudioBlockSize();

	filter.Init(sample_rate);

	hw.StartAudio(AudioCallback);
	while(1) {}
}
