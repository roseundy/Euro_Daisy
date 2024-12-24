#include "daisy_patch_sm.h"
#include "daisysp.h"
#include "rand_walk.h"
#include "lorenz.h"
#include "myled.h"

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

	// CV1 - CV A
	// CV2 - FMCV A
	// CV3 - ADJUST A
	// CV4 - 
	// CV5 - CV B
	// CV6 - FMCV B
	// CV7 - ADJ B
	// CV8 - 
	// ADC9 - FUNC A
	// ADC10 - FREQ A
	// ADC11 - FUNC B
	// ADC12 - FREQ B
	//
	// (D1) - HI A
	// (D2) - BIPOLAR A
	// (D3) - LED A
	// (D4) - HI B
	// (D5) - BIPOLAR B
	// (D6) - LED B
	//
	// GATEIN1 - SYNC A
	// GATEIN2 - SYNC B
	//
	// AUDIO OUT L - OUT A
	// AUDIO OUT R - OUT B

#define CV_A	CV_1
#define FMCV_A	CV_2
#define ADJ_A	CV_3
#define FUNC_A	ADC_9
#define FREQ_A	ADC_10
#define HI_A	hw.D1
#define BI_A	hw.D2
#define LED_A	hw.D3
#define SYNC_A	hw.B10

#define CV_B	CV_5
#define FMCV_B	CV_6
#define ADJ_B	CV_7
#define FUNC_B	ADC_11
#define FREQ_B	ADC_12
#define HI_B	hw.D4
#define BI_B	hw.D5
#define LED_B	hw.D6
#define SYNC_B	hw.B9

DaisyPatchSM hw;
Oscillator   			osc_a, osc_b;       		// For Sine Oscillator
// FIXME: need a VariableShapeOscillator with reset
VariableShapeOscillator tri_a, tri_b; 				// For Triangle/Saw Oscillator
VariableShapeOscillator square_a, square_b;			// Square/Pulse Oscillator
RandWalk                rw_a, rw_b;					// Random Walk
Lorenz					chaos_a, chaos_b;			// Lorenz-based chaos

Switch 					toggle_hi_a, toggle_hi_b;   // Low/High toggle switches
Switch					toggle_bi_a, toggle_bi_b;   // Unipolar/Bipolar toggle switches
MyLed                   led_a, led_b;               // Ouput LEDs

float cv_to_freq(float volts, float octave) {
	// convert to freq
	float freq = 440.0f * octave * exp2f(volts);
	return freq;
}

#define FUNC_SINE 0.0f
#define FUNC_TRIANGLE 1.0f
#define FUNC_SQUARE 2.0f
#define FUNC_RAND 3.0f
#define FUNC_CHAOS 4.0f
#define FUNC_DUAL 4.5f
#define NUM_FUNCS 6

// 0.0 <= control <= 4.5
void VoiceFactors(float control, float *factors)
{
	for (int i=0; i<NUM_FUNCS; i++)
		factors[i] = 0.0f;

	if (control <= FUNC_TRIANGLE) {
		factors[0] = (1.0f + FUNC_SINE - control);
		factors[1] = (control - FUNC_SINE);
	} else if (control <= FUNC_SQUARE) {
		factors[1] = (1.0f + FUNC_TRIANGLE - control);
		factors[2] = (control - FUNC_TRIANGLE);
	} else if (control <= (FUNC_RAND - 0.5)) {
		factors[2] = 1.0f;
	} else if (control <= (FUNC_CHAOS - 0.5)) {
		factors[3] = 1.0f;
	} else if (control <= (FUNC_DUAL - 0.25)) {
		factors[4] = 1.0f;
	} else {
		factors[5] = 1.0f;
	}
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	float factors_a[NUM_FUNCS], factors_b[NUM_FUNCS];
	hw.ProcessAllControls();

	toggle_hi_a.Debounce();
	toggle_bi_a.Debounce();
	toggle_hi_b.Debounce();
	toggle_bi_b.Debounce();
	
	bool high_a = toggle_hi_a.Pressed();
	bool bipolar_a = toggle_bi_a.Pressed();
	bool high_b = toggle_hi_b.Pressed();
	bool bipolar_b = toggle_bi_b.Pressed();

	float octave_a = high_a ? 1.0f / 8.0f : 1.0f / 65536.0f;
	float octave_b = high_b ? 1.0f / 8.0f : 1.0f / 65536.0f;

	float tune_a = hw.GetAdcValue(FREQ_A) * (high_a ? 5.0f : 10.0f);
    float voct_a = hw.GetAdcValue(FMCV_A) * 5.0f ;
    float freq_a = cv_to_freq(voct_a + tune_a, octave_a);
	//freq_a = 1000.f; // FIXME

	float tune_b = hw.GetAdcValue(FREQ_B) * (high_b ? 5.0f : 10.0f);
    float voct_b = hw.GetAdcValue(FMCV_B) * 5.0f ;
    float freq_b = cv_to_freq(voct_b + tune_b, octave_b);
	//freq_b = 1.0; // FIXME

	float func_a = hw.GetAdcValue(FUNC_A) * FUNC_DUAL;
	VoiceFactors(func_a, factors_a);
	//VoiceFactors(FUNC_RAND, factors_a); // FIXME
	
	float func_b = hw.GetAdcValue(FUNC_B) * FUNC_DUAL;
	VoiceFactors(func_b, factors_b);
	//VoiceFactors(FUNC_RAND, factors_b); // FIXME

	// Get adjustment. No scaling needed since ADJ pot only sees 0-5V
	// Clip so it's always between 0.0 and 1.0
	float adj_a = fclamp(hw.GetAdcValue(ADJ_A) + hw.GetAdcValue(CV_A), 0.0f, 1.0f);
	float adj_b = fclamp(hw.GetAdcValue(ADJ_B) + hw.GetAdcValue(CV_B), 0.0f, 1.0f);

	//adj_a = 0.5f; // FIXME
	//adj_b = 0.5f; // FIXME

	// set A oscillators
	osc_a.SetFreq(freq_a);
	tri_a.SetFreq(freq_a);
	tri_a.SetSyncFreq(freq_a);
	tri_a.SetPW(adj_a);
	square_a.SetFreq(freq_a);
	square_a.SetSyncFreq(freq_a);
	square_a.SetPW(adj_a);
	rw_a.SetFrequency(freq_a);
	//rw_a.SetRange(adj_a);
	chaos_a.SetFrequency(freq_a);

	// set B oscillators
	osc_b.SetFreq(freq_b);
	tri_b.SetFreq(freq_b);
	tri_b.SetSyncFreq(freq_b);
	tri_b.SetPW(adj_b);
	square_b.SetFreq(freq_b);
	square_b.SetSyncFreq(freq_b);
	square_b.SetPW(adj_b);
	rw_b.SetFrequency(freq_b);
	//rw_b.SetRange(adj_b);
	chaos_b.SetFrequency(freq_b);

	float out_a = 0.0f;
	float out_b = 0.0f;
	float aout_a = 0.0f;
	float aout_b = 0.0f;

	for (size_t i = 0; i < size; i++)
	{
		out_a = osc_a.Process() * factors_a[0] + tri_a.Process() * factors_a[1] + square_a.Process() * factors_a[2] + rw_a.Process() * factors_a[3];
		//out_a = osc_a.Process() * factors_a[0] + tri_a.Process() * factors_a[1] + square_a.Process() * factors_a[2];
		chaos_a.Process();
		float chaos_out_a = chaos_a.GetZ() * adj_a + chaos_a.GetX() * (1.0f - adj_a);
		//float chaos_out_a = chaos_a.GetX();
		out_a += (factors_a[4] + factors_a[5]) * chaos_out_a;

		out_b = osc_b.Process() * factors_b[0] + tri_b.Process() * factors_b[1] + square_b.Process() * factors_b[2] + rw_b.Process() * factors_b[3];
		//out_b = osc_b.Process() * factors_b[0] + tri_b.Process() * factors_b[1] + square_b.Process() * factors_b[2];
		chaos_b.Process();
		float chaos_out_b = chaos_b.GetZ() * adj_b + chaos_b.GetX() * (1.0f - adj_b);
		//float chaos_out_b = chaos_b.GetX();
		out_b += factors_b[4] * chaos_out_b + factors_b[5] * chaos_a.GetY();

		led_a.Set((out_a + 1.0f) / 2.0f);
		led_b.Set((out_b + 1.0f) / 2.0f);
		led_a.Update();
		led_b.Update();

		aout_a = bipolar_a ? out_a : (out_a + 1.0f) * 5.0f / 18.0f; // unipolar: make positive and limit between 0-5V
		aout_b = bipolar_b ? out_b : (out_b + 1.0f) * 5.0f / 18.0f; // unipolar: make positive and limit between 0-5V

		OUT_L[i] = -aout_a;
		OUT_R[i] = -aout_b;
	}


}

int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	float sample_rate = hw.AudioSampleRate();

	// Sine oscillators
	osc_a.Init(sample_rate);
    osc_a.SetWaveform(osc_a.WAVE_SIN);
    osc_a.SetAmp(1.0);
	
	osc_b.Init(sample_rate);
	osc_b.SetWaveform(osc_b.WAVE_SIN);
    osc_b.SetAmp(1.0);

	// Triangle/Saw oscillators
	tri_a.Init(sample_rate);
	tri_a.SetWaveshape(0.f);
	tri_a.SetPW(0.5f);
	tri_a.SetSyncFreq(440.f);
	tri_a.SetSync(false);

	tri_b.Init(sample_rate);
	tri_b.SetWaveshape(0.f);
	tri_b.SetPW(0.5f);
	tri_b.SetFreq(440.f);
	tri_b.SetSyncFreq(440.f);
	tri_b.SetSync(false);

	// Square/pulse oscillators
	square_a.Init(sample_rate);
	square_a.SetWaveshape(1.f);
	square_a.SetPW(0.5f);
	square_a.SetSyncFreq(440.f);
	square_a.SetSync(false);

	square_b.Init(sample_rate);
	square_b.SetWaveshape(1.f);
	square_b.SetPW(0.5f);
	square_b.SetFreq(440.f);
	square_b.SetSyncFreq(440.f);
	square_b.SetSync(false);

	// Random Walks
	rw_a.Init(sample_rate);
	rw_b.Init(sample_rate);

	// Chaos
	chaos_a.Init(sample_rate);
	chaos_b.Init(sample_rate);

	// Toggle switches
	toggle_hi_a.Init(HI_A,  1000, Switch::TYPE_TOGGLE, Switch::POLARITY_NORMAL, Switch::PULL_UP);
	toggle_bi_a.Init(BI_A,  1000, Switch::TYPE_TOGGLE, Switch::POLARITY_NORMAL, Switch::PULL_UP);
	toggle_hi_b.Init(HI_B,  1000, Switch::TYPE_TOGGLE, Switch::POLARITY_NORMAL, Switch::PULL_UP);
	toggle_bi_b.Init(BI_B,  1000, Switch::TYPE_TOGGLE, Switch::POLARITY_NORMAL, Switch::PULL_UP);

	// LEDs
	led_a.Init(LED_A, false, sample_rate);
	led_b.Init(LED_B, false, sample_rate);

	// ... and flash them
	led_a.Set(1.0f);
	led_b.Set(1.0f);
	led_a.Update();
	led_b.Update();
	System::Delay(500);
	led_a.Set(0.0f);
	led_b.Set(0.0f);
	led_a.Update();
	led_b.Update();

	hw.StartAudio(AudioCallback);
	while(1) {}
}
