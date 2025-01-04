#include "daisy_patch_sm.h"
#include "daisysp.h"
#include "myled.h"
#include "slew.h"
#include "slew_sm.h"
#include "smoother.h"
#include "my_delayline.h"
#include "module_state.h"
#include "MyPersistantStorage.h"

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

#define ATTACK_CV	CV_1
#define DECAY_CV	CV_2
#define SUSTAIN_CV	CV_3
#define RELEASE_CV	CV_4
#define DELAY_CV	CV_5
#define CURVE_CV	CV_6
#define ATTACK		CV_7
#define DECAY		CV_8
#define SUSTAIN		ADC_9
#define RELEASE		ADC_10
#define	DELAY		ADC_11
#define CURVE		ADC_12
#define HI_SW		hw.D1
#define LOOP_SW		hw.D2
#define OUT_LED		hw.D3
#define GATE_SW		hw.D4
#define GATE_LED	hw.D5
#define TRIG_IN		hw.gate_in_1
#define GATE_IN		hw.gate_in_2
#define RISE_OUT	hw.gate_out_1
#define FALL_OUT	hw.gate_out_2

#define FIVE_SEC_DELAY 48000*5
#define SMOOTH_SAMPLES 2048

DaisyPatchSM hw;
Switch toggle_hi;		// Low/High toggle switch
Switch toggle_loop;		// Loop toggle switch
Switch button_gate;		// gate button
MyLed led_gate;			// gate LED
MyLed led_out;			// output level LED
//Smooth<float, SMOOTH_SAMPLES> DSY_SDRAM_BSS smoother;		// smoother for delay
Smoother smoother;		// filter for delay control
MyDelayLine<float, FIVE_SEC_DELAY> DSY_SDRAM_BSS audio_delay;	// 10 seconds of delay on audio
MyDelayLine<uint8_t, FIVE_SEC_DELAY> DSY_SDRAM_BSS tg_delay; 	// 10 seconds of delay on Trigger/Gte
Slew slew;				// slew generator
SlewSM state_machine;	// slew state machine
MyPersistentStorage<ModuleState>  SavedState(hw.qspi);

#define SAVE_VERSION 0
void initFactoryState(ModuleState *factory) {
	factory->version = SAVE_VERSION;
	for (int i=0; i<6; i++)
		factory->cv_offset[i] = 0.0f;
	factory->attack_offset = 0.0f;
	factory->release_offset = 0.0f;
}

const float DEADZONE_WIDTH = 0.05;
const float AUDIO_DEADZONE_WIDTH = 0.01;
// create a deadzone for a control at the leftmost position,
// takes a unipolar value (between 0 and 1.0)
// returns a unipolar value (between 0 and 1.0)
//
float u2u_left_deadzone(float raw, float width) {
	float half_width = width / 2.0f;
    if (raw <= half_width)
        return 0.f;

    // positive
    return (raw - half_width) / (1.0f - half_width);
}

// status from slew module - also output signals
bool rising = false;
bool falling = false;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	ModuleState &currentState = SavedState.GetSettings();

	hw.ProcessAllControls();

	toggle_hi.Debounce();
	toggle_loop.Debounce();
	button_gate.Debounce();

	// read trigger and gate jacks
	bool trigger_in = TRIG_IN.State();
    bool gate_in = GATE_IN.State();

	// OR gate button into gate and display gate/trigger on LED
    gate_in = gate_in || button_gate.Pressed();
    led_gate.Set(gate_in || trigger_in ? 1.0f : 0.0f);
	led_gate.Update();

	bool high = toggle_hi.Pressed();
	bool loop = toggle_loop.Pressed();

	// control knobs and CVs
	float attack = fclamp(hw.GetAdcValue(ATTACK) - currentState.attack_offset + hw.GetAdcValue(ATTACK_CV) - currentState.cv_offset[0], 0.0f, 1.0f);
	float decay = fclamp(hw.GetAdcValue(DECAY) + hw.GetAdcValue(DECAY_CV) - currentState.cv_offset[1], 0.0f, 1.0f);
	float sustain = fclamp(hw.GetAdcValue(SUSTAIN) + hw.GetAdcValue(SUSTAIN_CV) - currentState.cv_offset[2], 0.0f, 1.0f);
	float release = fclamp(hw.GetAdcValue(RELEASE) - currentState.release_offset + hw.GetAdcValue(RELEASE_CV) - currentState.cv_offset[3], 0.0f, 1.0f);
	float delay_in = fclamp(u2u_left_deadzone(hw.GetAdcValue(DELAY), DEADZONE_WIDTH) + hw.GetAdcValue(DELAY_CV) - currentState.cv_offset[4], 0.0f, 1.0f) / 25.0;
	float curve = fclamp(hw.GetAdcValue(CURVE) + hw.GetAdcValue(CURVE_CV) - currentState.cv_offset[5], 0.05f, 1.0f);
	
	// high: 0 - 0.2 sec
	// low: 0 - 10 sec
	if (high) {
		attack = attack / 10.0;
		decay = decay / 10.0;
		release = release / 10.0;
	} else {
		attack = attack * 10.0;
		decay = decay * 10.0;
		release = release * 10.0;
	}

	float delay = smoother.Process(delay_in);
	size_t delay_index = (size_t) (1.0f + delay * FIVE_SEC_DELAY);
	if (!high)
		delay_index *= 25; // less granularity, but more stability
	tg_delay.SetDelay((size_t) delay_index);
	audio_delay.SetDelay((size_t) delay_index);

	for (size_t i = 0; i < size; i++)
	{
		// delay lines
		uint8_t tg_in = trigger_in ? 0x2 : 0x0;
		tg_in = tg_in | (gate_in ? 0x1 : 0x0);
		tg_delay.Write(tg_in);
		audio_delay.Write(-IN_L[i]);

		uint8_t tg = tg_delay.Read();
		bool gate = (tg & 0x1) == 0x1;
		bool trigger = (tg & 0x2) == 0x2;
		float analog = audio_delay.Read();

		// state machine
		state_machine.Process(trigger, gate, loop, rising, falling);

		// input mux (and convert to unipolar)
		float slew_in = ((state_machine.attackPhase() ? 1.0 : (state_machine.sustainPhase() ? sustain : analog)) + 1.0f) / 2.0f;

		// slew
		float rise_time = attack;
		float fall_time = state_machine.sustainPhase() ? decay : release;
		float slew_out = slew.Process(slew_in, state_machine.resetOut(), rise_time, fall_time, curve, high);
		slew_out = (slew_out * 2.0f) - 1.0f; // unipolar -> bipolar

		OUT_L[i] = -slew_out;
		//OUT_L[i] = -audio_delay.Read(); // FIXME: test delay
		OUT_R[i] = 0.0f;
		rising = slew.getRising();
		falling = slew.getFalling();
		dsy_gpio_write(&RISE_OUT, rising);
		dsy_gpio_write(&FALL_OUT, falling);

		led_out.Set(fclamp(slew_out, 0.0f, 1.0f));
		led_out.Update();
	}
}

#define CALIBRATE_USECS 10000
#define CALIBRATE_FILTER_CONST 0.001f

// filter CV inputs, and attack and release knobs and then update offsets
//
void calibrateCV (ModuleState &currentState) {
	Smoother cv_filter[6];
	Smoother attack_filter;
	Smoother release_filter;

	// init smoothers
	for (int i = 0; i < 6; i++)
		cv_filter[i].Init(CALIBRATE_FILTER_CONST);
	attack_filter.Init(CALIBRATE_FILTER_CONST);
	release_filter.Init(CALIBRATE_FILTER_CONST);

	// load 'em up
	for (int t = 0; t < CALIBRATE_USECS; t++) {
		cv_filter[0].Process(hw.GetAdcValue(ATTACK_CV));
		cv_filter[1].Process(hw.GetAdcValue(DECAY_CV));
		cv_filter[2].Process(hw.GetAdcValue(SUSTAIN_CV));
		cv_filter[3].Process(hw.GetAdcValue(RELEASE_CV));
		cv_filter[4].Process(hw.GetAdcValue(DELAY_CV));
		cv_filter[5].Process(hw.GetAdcValue(CURVE_CV));
		attack_filter.Process(hw.GetAdcValue(ATTACK));
		release_filter.Process(hw.GetAdcValue(RELEASE));
		System::DelayUs(1);
	}

	// Save 'em
	for (int i = 0; i < 6; i++)
		currentState.cv_offset[i] = cv_filter[i].GetVal();
	currentState.attack_offset = attack_filter.GetVal();
	currentState.release_offset = release_filter.GetVal();
}

void blinkLEDs(int count, uint32_t delay) {
	for (int i = 0; i < count; i++) {
		// on
		led_gate.Set(1.0f);
		led_out.Set(1.0f);
		led_gate.Update();
		led_out.Update();
		System::Delay(delay);
		// off
		led_gate.Set(0.0f);
		led_out.Set(0.0f);
		led_gate.Update();
		led_out.Update();
		System::Delay(delay);
	}
}

int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	float sample_rate = hw.AudioSampleRate();
	float block_size = hw.AudioBlockSize();

	// Slew generator and state machine
	slew.Init(sample_rate);
	state_machine.Init();

	// Delay control smoother
	smoother.Init(0.0001);

	// Delay lines
	audio_delay.Init();
	tg_delay.Init();

	// Toggle switches
	toggle_hi.Init(HI_SW,  1000, Switch::TYPE_TOGGLE, Switch::POLARITY_NORMAL, Switch::PULL_UP);
	toggle_loop.Init(LOOP_SW,  1000, Switch::TYPE_TOGGLE, Switch::POLARITY_NORMAL, Switch::PULL_UP);

	// Buttons
	button_gate.Init(GATE_SW, sample_rate, Switch::TYPE_MOMENTARY, Switch::POLARITY_INVERTED, Switch::PULL_UP);

	// LEDs
	led_gate.Init(GATE_LED, false, sample_rate/block_size);
	led_out.Init(OUT_LED, false, sample_rate);

	// Saved calibration
	// factory initialize module state if needed
    ModuleState defState;
	initFactoryState(&defState);
    SavedState.Init(defState);

    // re-initialize if version number changed
    ModuleState &currentState = SavedState.GetSettings();
    if (currentState.version != defState.version) {
        SavedState.RestoreDefaults();
    }

	// Calibrate if GATE button is pressed at power-up
	if (button_gate.RawState()) {
		calibrateCV(currentState);
		SavedState.Save();

		// Blink LEDs 3 times quickly
		blinkLEDs(3, 100);
	} else {
		// Blink LEDs once slowly
		blinkLEDs(1, 500);
	}

	hw.StartAudio(AudioCallback);
	while(1) {}
}
