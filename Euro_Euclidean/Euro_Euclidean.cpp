#include "Euro_Euclidean.h"
#include "daisy_patch_sm.h"
#include "daisysp.h"
#include "dev/oled_ssd130x.h"
#include "UiHardware.h"
#include "lut.h"
#include "module_state.h"
#include <string>
#include "bjorklund.h"
#include "smoother.h"
#include "MyPersistantStorage.h"

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;
using MyOledDisplay = OledDisplay<SSD130xI2c128x64Driver>;

DaisyPatchSM 			hw;
Encoder 				enc;
Switch 					sel_sw[4];
Switch                  clock_sw;
Led                     sel_led[4];
Led                     clock_led;
MyOledDisplay   		display;
UiEventQueue       		eventQueue;
MyPersistentStorage<ModuleState>  SavedState(hw.qspi);

EditField               editting = EDIT_NONE;
EditSubField            sub_editting = SUBEDIT_LENGTH;
int						home_channel = 0; // channel to be adjusted on home page
int						currentPulses[4]; // number of pulses based on settings and CVs
int						currentOffset[4]; // offset based on settings and CVs

void initFactoryState(ModuleState *factory) {
	factory->version = SAVE_VERSION;
	for (int i=0; i<4; i++) {
		factory->channel[i].length = 16;
		factory->channel[i].pulsesSetting = 4;
		factory->channel[i].offsetSetting = 0;
		factory->channel[i].pulsesAttenuvert = 0;
		factory->channel[i].offsetAttenuvert = 0;
		factory->channel[i].pulsesCVOffset = 0.0f;
		factory->channel[i].offsetCVOffset = 0.0f;
		factory->channel[i].muted = false;
	}
}

int validValue(int old, int inc, int min, int max) {
	int value = old + inc;
	if (value < min)
		value = min;
	else if (value > max)
		value = max;
	return value;
}

int time_enc_held; // time in ms that encoder is being held down
int time_held[4]; // time in ms that button is being held down
void GenerateUiEvents()
{
	enc.Debounce();
	if (enc.Pressed())
		time_enc_held = enc.TimeHeldMs();

    if(enc.FallingEdge()) {
		if (time_enc_held > 250)
        	eventQueue.AddButtonPressed(bttnEncoder, 1);
		else
			eventQueue.AddButtonReleased(bttnEncoder);
	}

    const auto increments = enc.Increment();
    if(increments != 0)
        eventQueue.AddEncoderTurned(encoderMain, increments, 12);

    for (int i = 0; i < 4; i++) {
        sel_sw[i].Debounce();

		if (sel_sw[i].Pressed())
			time_held[i] = sel_sw[i].TimeHeldMs();

		if(sel_sw[i].FallingEdge()) {
			if (time_held[i] > 250) {
				// Call it "Pressed" if held for long enough...
        		eventQueue.AddButtonPressed(bttnSelA + i, 1);
			} else {
				// ...otherwise call it "Released"
				eventQueue.AddButtonReleased(bttnSelA + i);

			}
		}
	}
}

int calcPulses(ModuleState &eState, int ch, float cv) {
	cv -= eState.channel[ch].pulsesCVOffset;
	int pulses = eState.channel[ch].pulsesSetting;
	float delta = ((float) eState.channel[ch].pulsesAttenuvert * eState.channel[ch].length) / 100.0f * cv;
	pulses += (int) delta;
	return validValue(pulses, 0, 0, eState.channel[ch].length);
}

int calcOffset(ModuleState &eState, int ch, float cv) {
	cv -= eState.channel[ch].offsetCVOffset;
	int offset = eState.channel[ch].offsetSetting;
	float delta = ((float) eState.channel[ch].offsetAttenuvert * eState.channel[ch].length) / 100.0f * cv;
	offset += (int) delta;
	return validValue(offset, 0, 1-eState.channel[ch].length, eState.channel[ch].length-1);
}

std::string rotateRhythm(std::string s, int offset) {
	if (offset == 0)
		return s;

	if (offset > 0) {
		for (int i = 0; i<offset; i++) {
    		char first = s[0];

    		s.assign(s, 1, s.size() - 1);
    		s.append(1, first);
		}
	} else {
		for (int i = 0; i<-offset; i++) {
    		std::string last = s.substr(s.size()-1, 1);

    		s.assign(s, 0, s.size() - 1);
    		s.insert(0, last);
		}
	}
    return s;
}

bool clock_last = false;
bool reset_last = false;
int  euclidean_cycle[4] = {0, 0, 0, 0};
int  cycle_copy[4];
bool reset_request = false; // request from UI
volatile int  updating = 0;
bool force_update = false; // request from UI


std::string rhythm[4];
std::string rhythm_copy[4];

void GateCallback(uint16_t **output, size_t size)
{
	hw.ProcessAllControls();
	GenerateUiEvents();
	ModuleState &euclidState = SavedState.GetSettings();

	// read and display clock signal/button
	bool clock = hw.gate_in_1.State();
	clock_sw.Debounce();
    clock = clock || clock_sw.Pressed();
    clock_led.Set(clock ? 1.0f : 0.0f);
    clock_led.Update();
    bool clock_rising = clock && !clock_last;
	bool clock_falling = !clock && clock_last;

	// read reset signal
	bool reset = hw.gate_in_2.State();
	bool reset_edge = (reset && !reset_last) || reset_request;
	reset_request = false;

	updating = 1;
	for (int ch=0; ch<4; ch++) {
		rhythm_copy[ch] = rhythm[ch];
		cycle_copy[ch] = euclidean_cycle[ch];
	}
	updating = 0;

	for (int ch=0; ch<4; ch++) {
		if (reset_edge)
			euclidean_cycle[ch] = 0;
		else if (clock_falling)
			euclidean_cycle[ch] = (euclidean_cycle[ch] + 1) % euclidState.channel[ch].length;
		else
			// handle length changing between clocks
			euclidean_cycle[ch] = euclidean_cycle[ch] % euclidState.channel[ch].length;
	}

	// calculate current pulses and offsets based on state and CV inputs
	if (reset_edge || clock_rising || force_update) {
		for (int ch=0; ch<4; ch++) {
			currentPulses[ch] = calcPulses(euclidState, ch, hw.GetAdcValue(CV_1 + ch * 2));
			currentOffset[ch] = calcOffset(euclidState, ch, hw.GetAdcValue(CV_2 + ch * 2));
		}
		force_update = false;
	}

	// build rhythms

	for (int ch=0; ch<4; ch++) {
		rhythm[ch] = rotateRhythm(bjorklund(currentPulses[ch], euclidState.channel[ch].length), currentOffset[ch]);
	}

	for (size_t i = 0; i < size; i++)
	{
		output[0][i] = ((rhythm[0][euclidean_cycle[0]] == '1') && clock && !euclidState.channel[0].muted) ? 4095 : 0;
        output[1][i] = ((rhythm[1][euclidean_cycle[1]] == '1') && clock && !euclidState.channel[1].muted) ? 4095 : 0;
	}
	dsy_gpio_write(&hw.gate_out_1, (rhythm[2][euclidean_cycle[2]] == '1') && clock && !euclidState.channel[2].muted);
	dsy_gpio_write(&hw.gate_out_2, (rhythm[3][euclidean_cycle[3]] == '1') && clock && !euclidState.channel[3].muted);

	reset_last = reset;
	clock_last = clock;
}

void setSelLEDs() {
	for (int i=0; i<4; i++) {
		sel_led[i].Set(editting == (EDIT_A + i));
		sel_led[i].Update();
	}
}

void readQueue(ModuleState &euclidState)
{
	int inc, ch;
    while(!eventQueue.IsQueueEmpty())
    {
        UiEventQueue::Event e = eventQueue.GetAndRemoveNextEvent();
        switch(e.type)
        {
            case UiEventQueue::Event::EventType::buttonReleased:
                // button short presses
                switch(e.asButtonPressed.id)
                {
                    case bttnEncoder:
						if (editting == EDIT_NONE)
							home_channel = (home_channel + 1) % 4;
						editting = EDIT_NONE;
						break;
                    case bttnSelA:
                        if(editting != EDIT_A)
                            sub_editting = SUBEDIT_LENGTH;
                        else
                            sub_editting
                                = (EditSubField)((sub_editting + 1) % NUM_SUBEDIT);
                        editting = EDIT_A;
                        break;
                    case bttnSelB:
                        if(editting != EDIT_B)
                            sub_editting = SUBEDIT_LENGTH;
                        else
                            sub_editting
                                = (EditSubField)((sub_editting + 1) % NUM_SUBEDIT);
                        editting = EDIT_B;
                        break;
                    case bttnSelC:
                        if(editting != EDIT_C)
                            sub_editting = SUBEDIT_LENGTH;
                        else
                            sub_editting
                                = (EditSubField)((sub_editting + 1) % NUM_SUBEDIT);
                        editting = EDIT_C;
                        break;
                    case bttnSelD:
                        if(editting != EDIT_D)
                            sub_editting = SUBEDIT_LENGTH;
                        else
                            sub_editting
                                = (EditSubField)((sub_editting + 1) % NUM_SUBEDIT);
                        editting = EDIT_D;
                        break;
                }
				setSelLEDs();
                break;

			// long presses
			case UiEventQueue::Event::EventType::buttonPressed:
			 	switch(e.asButtonPressed.id)
                {
                    case bttnEncoder:
						editting = EDIT_NONE;
						reset_request = true;
						break;
                    case bttnSelA:
					 	euclidState.channel[0].muted = !euclidState.channel[0].muted;
                        break;
					case bttnSelB:
						euclidState.channel[1].muted = !euclidState.channel[1].muted;
                        break;
                    case bttnSelC:
						euclidState.channel[2].muted = !euclidState.channel[2].muted;
                        break;
					case bttnSelD:
						euclidState.channel[3].muted = !euclidState.channel[3].muted;
                        break;
				}
				break;

            // encoder turns
            case UiEventQueue::Event::EventType::encoderTurned:
                inc = e.asEncoderTurned.increments;
				if (editting == EDIT_NONE) {
					euclidState.channel[home_channel].pulsesSetting = 
						validValue(euclidState.channel[home_channel].pulsesSetting, inc, 0, euclidState.channel[home_channel].length);
					force_update = true;
				} else {
                    ch = editting - EDIT_A;
                    switch(sub_editting)
                    {
						case SUBEDIT_LENGTH:
                            euclidState.channel[ch].length = validValue(euclidState.channel[ch].length, inc, 1, MAX_LENGTH);
							// adjust other values based on this length.
							euclidState.channel[ch].pulsesSetting = validValue(euclidState.channel[ch].pulsesSetting, 0, 0, euclidState.channel[ch].length);
							euclidState.channel[ch].offsetSetting = validValue(euclidState.channel[ch].offsetSetting, 0, 1 - euclidState.channel[ch].length, euclidState.channel[ch].length - 1);
                            break;
                        case SUBEDIT_PULSES:
                            euclidState.channel[ch].pulsesSetting = validValue(euclidState.channel[ch].pulsesSetting, inc, 0, euclidState.channel[ch].length);
                            break;
                        case SUBEDIT_PULSES_ATT:
                            euclidState.channel[ch].pulsesAttenuvert = validValue(euclidState.channel[ch].pulsesAttenuvert, inc, -100, 100);
                            break;
                        case SUBEDIT_OFFSET:
                            euclidState.channel[ch].offsetSetting = validValue(euclidState.channel[ch].offsetSetting, inc, 1 - euclidState.channel[ch].length, euclidState.channel[ch].length - 1);
                            break;
                        case SUBEDIT_OFFSET_ATT:
                            euclidState.channel[ch].offsetAttenuvert = validValue(euclidState.channel[ch].offsetAttenuvert, inc, -100, 100);
                            break;
						default:
							break;
                    }
                }
                break;

            default: break;
        }
    }
}

void homePage(ModuleState &eS) {
	for (int ch=0; ch<4; ch++) {
		int start_index = 0;
		display.SetCursor(0, ch * 16);
		while (updating);
		if ((eS.channel[ch].length <= MAX_DISP_WIDTH) || (cycle_copy[ch] < MAX_DISP_WIDTH)) {
			// fits on screen
			display.WriteChar(' ', Font_6x8, true);
		} else {
			// not showing bits on left
			display.WriteChar('<', Font_6x8, true);
			start_index = cycle_copy[ch] - MAX_DISP_WIDTH + 1;
		}

		for (int i=0; i<eS.channel[ch].length && i<MAX_DISP_WIDTH; i++) {
			char c = rhythm_copy[ch][i+start_index] == '1' ? '+' : '-';
			display.WriteChar(c, Font_6x8, (i + start_index) != cycle_copy[ch]);
		}

		if ((eS.channel[ch].length > MAX_DISP_WIDTH) && (cycle_copy[ch] < (eS.channel[ch].length - 1)))
			// not showing bits on right
			display.WriteChar('>', Font_6x8, true);
		else
			display.WriteChar(' ', Font_6x8, true);

		// pad
		for (int i=0; i<(MAX_DISP_WIDTH - eS.channel[ch].length); i++)
			display.WriteChar(' ', Font_6x8, true);

		// status
		display.WriteChar(eS.channel[ch].muted ? 'M' : ' ', Font_6x8, true);
		display.WriteChar(ch == home_channel ? '*' : ' ', Font_6x8, true);
	}
}

void drawRhythm(ModuleState &eS, int ch, int pos_x, int pos_y, int rad, int text_x, int text_y) {
	int pulse_rad = 4;
	int gap_rad = 1;
	int cycle_delta = pulse_rad;
	if (eS.channel[ch].length > 32) {
		pulse_rad = 1;
		gap_rad = 0;
	} else if (eS.channel[ch].length > 16) {
		pulse_rad = 3;
		gap_rad = 1;
	}
	float inc = 2.0f * PI_F / (float) eS.channel[ch].length;
	float dinc = 360.0 / (float) eS.channel[ch].length;
	
	for (int i = 0; i < eS.channel[ch].length; i++) {
		float theta = inc * (float) i + PI_F / 2.0f;
		int x = pos_x - (int) (lookup_cos(theta) * (float) rad);
		int y = pos_y - (int) (lookup_sin(theta) * (float) rad);

		// cycle position
		if (i == cycle_copy[ch]) {
			display.DrawArc(pos_x, pos_y, rad - cycle_delta - 1, (int) (-dinc * i - dinc/2.0) + 90, (int) dinc, true);
			display.DrawArc(pos_x, pos_y, rad + cycle_delta, (int) (-dinc * i - dinc/2.0) + 90, (int) dinc, true);

			int x0 = pos_x - (int) (lookup_cos(theta - inc/2.0) * (float) (rad - cycle_delta));
			int y0 = pos_y - (int) (lookup_sin(theta - inc/2.0) * (float) (rad - cycle_delta));
			int x1 = pos_x - (int) (lookup_cos(theta - inc/2.0) * (float) (rad + cycle_delta));
			int y1 = pos_y - (int) (lookup_sin(theta - inc/2.0) * (float) (rad + cycle_delta));
			display.DrawLine(x0, y0, x1, y1, true);

			x0 = pos_x - (int) (lookup_cos(theta + inc/2.0) * (float) (rad - cycle_delta));
			y0 = pos_y - (int) (lookup_sin(theta + inc/2.0) * (float) (rad - cycle_delta));
			x1 = pos_x - (int) (lookup_cos(theta + inc/2.0) * (float) (rad + cycle_delta));
			y1 = pos_y - (int) (lookup_sin(theta + inc/2.0) * (float) (rad + cycle_delta));
			display.DrawLine(x0, y0, x1, y1, true);

			x0 = pos_x - (int) (lookup_cos(theta) * (float) (rad - cycle_delta));
			y0 = pos_y - (int) (lookup_sin(theta) * (float) (rad - cycle_delta));
			display.DrawLine(x0, y0, pos_x, pos_y, true);
		}

		// pulse or gap
		if (rhythm_copy[ch][i] == '1') {
			// pulse (large circle and point)
			display.DrawCircle(x, y, pulse_rad, true);
			display.DrawPixel(x, y, true);
		} else if (gap_rad > 0) {
			// large gap (small circle)
			display.DrawCircle(x, y, gap_rad, true);
		} else {
			// small gap (point)
			display.DrawPixel(x, y, true);
		}
	}

	if (home_channel == ch)
		display.DrawRect(text_x, text_y, text_x + 6 *(eS.channel[ch].muted ? 2 : 1), text_y + 8, true, true);
	display.SetCursor(text_x+1, text_y+1);
	display.WriteChar('A' + ch, Font_6x8, home_channel != ch);
	if (eS.channel[ch].muted)
		display.WriteChar('M', Font_6x8, home_channel != ch);
}

void homePage2(ModuleState &euclidState) {
	while (updating);
	drawRhythm(euclidState, 0, 31, 31, 27, 0, 0);
	drawRhythm(euclidState, 1, 31, 31, 16, 0, 54);
	drawRhythm(euclidState, 2, 95, 31, 27, 114, 0);
	drawRhythm(euclidState, 3, 95, 31, 16, 114, 54);
}

void attenuvertString(char *buf, int value) {
	int att = abs(value);
	sprintf(buf, "%c%1.1d.%2.2d", value >= 0 ? '+' : '-', att / 100, att % 100);
}

void displayState(ModuleState &euclidState) {
	char sbuf[20];

	display.Fill(false); // clear display

	if (editting == EDIT_NONE) {
		// Home screen display
		homePage2(euclidState);
	} else {
		int ch = editting - EDIT_A;

		display.SetCursor(0, 0);
		display.WriteString("Channel ", Font_7x10, false);
		display.WriteChar('A' + ch, Font_7x10, false);
		if (euclidState.channel[ch].muted)
			display.WriteString("  MUTED  ", Font_7x10, false);
		else
			display.WriteString("         ", Font_7x10, false);
		
		switch (sub_editting) {
			case SUBEDIT_LENGTH:
				display.SetCursor(0, 15);
				display.WriteString("Length: ", Font_7x10, true);
				sprintf(sbuf, "%3d", euclidState.channel[ch].length);
				display.WriteString(sbuf, Font_7x10, false);
				break;

			case SUBEDIT_PULSES:
			case SUBEDIT_PULSES_ATT:
				display.SetCursor(0, 15);
				display.WriteString("Pulses:    ", Font_7x10, true);
				sprintf(sbuf, "%3d", euclidState.channel[ch].pulsesSetting);
				display.WriteString(sbuf, Font_7x10, sub_editting != SUBEDIT_PULSES);

				display.SetCursor(0, 30);
				display.WriteString("Pulse Att: ", Font_7x10, true);
				attenuvertString(sbuf, euclidState.channel[ch].pulsesAttenuvert);
				display.WriteString(sbuf, Font_7x10, sub_editting != SUBEDIT_PULSES_ATT);

				display.SetCursor(0, 45);
				display.WriteString("Curr Pulses: ", Font_7x10, true);
				sprintf(sbuf, "%3d", currentPulses[ch]);
				display.WriteString(sbuf, Font_7x10, true);
				break;

			case SUBEDIT_OFFSET:
			case SUBEDIT_OFFSET_ATT:
				display.SetCursor(0, 15);
				display.WriteString("Offset:     ", Font_7x10, true);
				sprintf(sbuf, "%3d", euclidState.channel[ch].offsetSetting);
				display.WriteString(sbuf, Font_7x10, sub_editting != SUBEDIT_OFFSET);

				display.SetCursor(0, 30);
				display.WriteString("Offset Att: ", Font_7x10, true);
				attenuvertString(sbuf, euclidState.channel[ch].offsetAttenuvert);
				display.WriteString(sbuf, Font_7x10, sub_editting != SUBEDIT_OFFSET_ATT);

				display.SetCursor(0, 45);
				display.WriteString("Curr Offset: ", Font_7x10, true);
				sprintf(sbuf, "%3d", currentOffset[ch]);
				display.WriteString(sbuf, Font_7x10, true);
				break;

			default:
				break;
		}
	}

	display.Update();
}

void processUI() {
	ModuleState &euclidState = SavedState.GetSettings();
	readQueue(euclidState);
	displayState(euclidState);
}

// filter CV inputs and then update offsets
//
void calibrateCV (ModuleState &currentState) {
	Smoother cv_filter[8];

	// init smoothers
	for (int i = 0; i < 8; i++)
		cv_filter[i].Init(CALIBRATE_FILTER_CONST);

	// load 'em up
	for (int t = 0; t < CALIBRATE_USECS; t++) {
		for (int i = 0; i < 8; i++)
			cv_filter[i].Process(hw.GetAdcValue(CV_1 + i));
		System::DelayUs(1);
	}

	// Save 'em
	for (int ch = 0; ch < 4; ch++) {
		currentState.channel[ch].pulsesCVOffset = cv_filter[ch * 2].GetVal();
		currentState.channel[ch].offsetCVOffset = cv_filter[ch * 2 + 1].GetVal();
	}
}


int main(void)
{
	clock_last = false;
	reset_last = false;
	for (int i=0; i<4; i++)
		euclidean_cycle[i] = 0;
	reset_request = false;

	InitLUT();

	hw.Init();

	// encoder
	enc.Init(DaisyPatchSM::D7, DaisyPatchSM::D6, DaisyPatchSM::D5);

	// buttons
	sel_sw[0].Init(DaisyPatchSM::D1);
	sel_sw[1].Init(DaisyPatchSM::D2);
	sel_sw[2].Init(DaisyPatchSM::D3);
	sel_sw[3].Init(DaisyPatchSM::D4);
	clock_sw.Init(DaisyPatchSM::D9);

	// LEDs
	sel_led[0].Init(DaisyPatchSM::A2, false);
	sel_led[1].Init(DaisyPatchSM::A3, false);
	sel_led[2].Init(DaisyPatchSM::A9, false);
	sel_led[3].Init(DaisyPatchSM::A8, false);
	clock_led.Init(DaisyPatchSM::D8, false);

	// set up display
	MyOledDisplay::Config disp_cfg;
	display.Init(disp_cfg);

	// factory initialize module state if needed
    ModuleState defState;
	initFactoryState(&defState);
    SavedState.Init(defState);

    // re-initialize if version number changed
    ModuleState &writtenState = SavedState.GetSettings();
    if (writtenState.version != defState.version) {
        SavedState.RestoreDefaults();
    }

	// Calibrate CV inputs if SELA is pressed at power-up
	if (sel_sw[0].RawState()) {
		calibrateCV(writtenState);
		display.Fill(false);
		display.WriteString("Calibrated", Font_7x10, true);
		display.Update();
		System::Delay(500);
	}

	hw.StartDac(GateCallback);

	uint32_t last_save = 0;
	while(1) {
		processUI();
		uint32_t now = System::GetNow();
        
		if ((now > (last_save + 1000)) || (now < last_save)) {
            SavedState.Save(); // should only write flash if state has changed
            last_save = now;
        }
	}
}
