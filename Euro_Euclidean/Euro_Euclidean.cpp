#include "daisy_patch_sm.h"
#include "daisysp.h"
#include "dev/oled_ssd130x.h"
#include "UiHardware.h"
#include "module_state.h"
#include <string>
#include "bjorklund.h"

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
PersistentStorage<ModuleState>  SavedState(hw.qspi);

enum EditField {
	EDIT_NONE,
	EDIT_A,
	EDIT_B,
	EDIT_C,
	EDIT_D,
};

enum EditSubField {
	SUBEDIT_LENGTH,
	SUBEDIT_PULSES,
	SUBEDIT_PULSES_ATT,
	SUBEDIT_OFFSET,
	SUBEDIT_OFFSET_ATT,
};

EditField               editting = EDIT_NONE;
EditSubField            sub_editting = SUBEDIT_LENGTH;
int						currentPulses[4];
int						currentOffset[4];

#define MAX_LENGTH 99

#define SAVE_VERSION 1

void initFactoryState(ModuleState *factory) {
	factory->version = SAVE_VERSION;
	for (int i=0; i<4; i++) {
		factory->channel[i].length = 16;
		factory->channel[i].pulsesSetting = 4;
		factory->channel[i].offsetSetting = 0;
		factory->channel[i].pulsesAttenuvert = 0;
		factory->channel[i].offsetAttenuvert = 0;
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

void GenerateUiEvents()
{
	enc.Debounce();
    if(enc.RisingEdge())
        eventQueue.AddButtonPressed(bttnEncoder, 1);

    if(enc.FallingEdge())
        eventQueue.AddButtonReleased(bttnEncoder);

    const auto increments = enc.Increment();
    if(increments != 0)
        eventQueue.AddEncoderTurned(encoderMain, increments, 12);

    for(int i = 0; i < 4; i++)
        sel_sw[i].Debounce();

    if(sel_sw[0].RisingEdge())
        eventQueue.AddButtonPressed(bttnSelA, 1);
    if(sel_sw[1].RisingEdge())
        eventQueue.AddButtonPressed(bttnSelB, 1);
    if(sel_sw[2].RisingEdge())
        eventQueue.AddButtonPressed(bttnSelC, 1);
    if(sel_sw[3].RisingEdge())
        eventQueue.AddButtonPressed(bttnSelD, 1);
}

int calcPulses(ModuleState &eState, int ch, float cv) {
	int pulses = eState.channel[ch].pulsesSetting;
	float delta = (float) eState.channel[ch].pulsesAttenuvert * cv;
	pulses += (int) delta;
	return validValue(pulses, 0, 0, eState.channel[ch].length);
}

int calcOffset(ModuleState &eState, int ch, float cv) {
	int offset = eState.channel[ch].offsetSetting;
	float delta = (float) eState.channel[ch].offsetAttenuvert * cv;
	offset += (int) delta;
	return validValue(offset, 0, 0, eState.channel[ch].length-1);
}

std::string rotateRhythm(std::string s, int offset) {
	if (offset == 0)
		return s;

	for (int i = 0; i<offset; i++) {
    	char first = s[0];

    	s.assign(s, 1, s.size() - 1);
    	s.append(1, first);
	}

    return s;
}

bool clock_last = false;
bool reset_last = false;
int  euclidean_cycle[4] = {0, 0, 0, 0};
bool reset_request = false; // request from UI


std::string rhythm[4];

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
	if (reset_edge || clock_rising) {
		currentPulses[0] = calcPulses(euclidState, 0, hw.GetAdcValue(CV_1));
		currentOffset[0] = calcOffset(euclidState, 0, hw.GetAdcValue(CV_2));
		currentPulses[1] = calcPulses(euclidState, 1, hw.GetAdcValue(CV_3));
		currentOffset[1] = calcOffset(euclidState, 1, hw.GetAdcValue(CV_4));
		currentPulses[2] = calcPulses(euclidState, 2, hw.GetAdcValue(CV_5));
		currentOffset[2] = calcOffset(euclidState, 2, hw.GetAdcValue(CV_6));
		currentPulses[3] = calcPulses(euclidState, 3, hw.GetAdcValue(CV_7));
		currentOffset[3] = calcOffset(euclidState, 3, hw.GetAdcValue(CV_8));
	}

	// build rhythms

	for (int ch=0; ch<4; ch++) {
		rhythm[ch] = rotateRhythm(bjorklund(currentPulses[ch], euclidState.channel[ch].length), currentOffset[ch]);
	}

	for (size_t i = 0; i < size; i++)
	{
		output[0][i] = ((rhythm[0][euclidean_cycle[0]] == '1') && clock) ? 4095 : 0;
        output[1][i] = ((rhythm[1][euclidean_cycle[1]] == '1') && clock) ? 4095 : 0;
	}
	dsy_gpio_write(&hw.gate_out_1, (rhythm[2][euclidean_cycle[2]] == '1') && clock);
	dsy_gpio_write(&hw.gate_out_2, (rhythm[3][euclidean_cycle[3]] == '1') && clock);

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
            case UiEventQueue::Event::EventType::buttonPressed:
                // button presses
                switch(e.asButtonPressed.id)
                {
                    case bttnEncoder:
						editting = EDIT_NONE;
						reset_request = true;
						break;
                    case bttnSelA:
                        if(editting != EDIT_A)
                            sub_editting = SUBEDIT_LENGTH;
                        else
                            sub_editting
                                = (EditSubField)((sub_editting + 1) % 5);
                        editting = EDIT_A;
                        break;
                    case bttnSelB:
                        if(editting != EDIT_B)
                            sub_editting = SUBEDIT_LENGTH;
                        else
                            sub_editting
                                = (EditSubField)((sub_editting + 1) % 5);
                        editting = EDIT_B;
                        break;
                    case bttnSelC:
                        if(editting != EDIT_C)
                            sub_editting = SUBEDIT_LENGTH;
                        else
                            sub_editting
                                = (EditSubField)((sub_editting + 1) % 5);
                        editting = EDIT_C;
                        break;
                    case bttnSelD:
                        if(editting != EDIT_D)
                            sub_editting = SUBEDIT_LENGTH;
                        else
                            sub_editting
                                = (EditSubField)((sub_editting + 1) % 5);
                        editting = EDIT_D;
                        break;
                }
				setSelLEDs();
                break;

            // encoder turns
            case UiEventQueue::Event::EventType::encoderTurned:
                inc = e.asEncoderTurned.increments;
                if (editting != EDIT_NONE)
                {
                    ch = editting - EDIT_A;
                    switch(sub_editting)
                    {
						case SUBEDIT_LENGTH:
                            euclidState.channel[ch].length = validValue(euclidState.channel[ch].length, inc, 1, MAX_LENGTH);
							// adjust other values based on this length.
							euclidState.channel[ch].pulsesSetting = validValue(euclidState.channel[ch].pulsesSetting, 0, 0, euclidState.channel[ch].length);
							euclidState.channel[ch].pulsesAttenuvert = validValue(euclidState.channel[ch].pulsesAttenuvert, 0, -euclidState.channel[ch].length, euclidState.channel[ch].length);
							euclidState.channel[ch].offsetSetting = validValue(euclidState.channel[ch].offsetSetting, 0, 0, euclidState.channel[ch].length - 1);
							euclidState.channel[ch].offsetAttenuvert = validValue(euclidState.channel[ch].offsetAttenuvert, 0, -(euclidState.channel[ch].length - 1), euclidState.channel[ch].length - 1);
                            break;
                        case SUBEDIT_PULSES:
                            euclidState.channel[ch].pulsesSetting = validValue(euclidState.channel[ch].pulsesSetting, inc, 0, euclidState.channel[ch].length);
                            break;
                        case SUBEDIT_PULSES_ATT:
                            euclidState.channel[ch].pulsesAttenuvert = validValue(euclidState.channel[ch].pulsesAttenuvert, inc, -euclidState.channel[ch].length, euclidState.channel[ch].length);
                            break;
                        case SUBEDIT_OFFSET:
                            euclidState.channel[ch].offsetSetting = validValue(euclidState.channel[ch].offsetSetting, inc, 0, euclidState.channel[ch].length - 1);
                            break;
                        case SUBEDIT_OFFSET_ATT:
                            euclidState.channel[ch].offsetAttenuvert = validValue(euclidState.channel[ch].offsetAttenuvert, inc, -(euclidState.channel[ch].length - 1), euclidState.channel[ch].length - 1);
                            break;
                    }
                }
                break;

            default: break;
        }
    }
}

//void writeAt(char *str, int x, int y, bool highlight) {
//	display.SetCursor(x, y);
//	display.WriteString(str, Font_7x10, !highlight);
//}

void displayState(ModuleState &euclidState) {
	char sbuf[20];

	display.Fill(false); // clear display

	// Top line: column names
	display.SetCursor(0, 0);
	display.WriteString("  len pls att off att", Font_6x8, true);

	for (int i=0; i<4; i++) {
		display.SetCursor(0, 12 + i*13);
		display.WriteChar('A' + i, Font_7x10, true);
		
		// length
		display.WriteString(" ", Font_7x10, true);
		sprintf(sbuf, "%2d", euclidState.channel[i].length);
		display.WriteString(sbuf, Font_7x10, !((editting == (EDIT_A + i)) && (sub_editting == SUBEDIT_LENGTH)));
		
		// pulses setting
		display.WriteString(" ", Font_7x10, true);
		if (editting == (EDIT_A + i))
			sprintf(sbuf, "%2d", euclidState.channel[i].pulsesSetting);
		else
			sprintf(sbuf, "%2d", currentPulses[i]);
		display.WriteString(sbuf, Font_7x10, !((editting == (EDIT_A + i)) && (sub_editting == SUBEDIT_PULSES)));
		
		// pulses attenuvert
		display.WriteString(" ", Font_7x10, true);
		sprintf(sbuf, "%c%2.2d", euclidState.channel[i].pulsesAttenuvert >= 0 ? '+' : '-', abs(euclidState.channel[i].pulsesAttenuvert));
		display.WriteString(sbuf, Font_7x10, !((editting == (EDIT_A + i)) && (sub_editting == SUBEDIT_PULSES_ATT)));
		
		// offset setting
		display.WriteString(" ", Font_7x10, true);
		if (editting == (EDIT_A + i))
			sprintf(sbuf, "%2d", euclidState.channel[i].offsetSetting);
		else
			sprintf(sbuf, "%2d", currentOffset[i]);
		display.WriteString(sbuf, Font_7x10, !((editting == (EDIT_A + i)) && (sub_editting == SUBEDIT_OFFSET)));
		
		// offset attenuvert
		display.WriteString(" ", Font_7x10, true);
		sprintf(sbuf, "%c%2.2d", euclidState.channel[i].offsetAttenuvert >= 0 ? '+' : '-', abs(euclidState.channel[i].offsetAttenuvert));
		display.WriteString(sbuf, Font_7x10, !((editting == (EDIT_A + i)) && (sub_editting == SUBEDIT_OFFSET_ATT)));
	}


	display.Update();
}

void processUI() {
	ModuleState &euclidState = SavedState.GetSettings();
	readQueue(euclidState);
	displayState(euclidState);
}

int main(void)
{
	clock_last = false;
	reset_last = false;
	for (int i=0; i<4; i++)
		euclidean_cycle[i] = 0;
	reset_request = false;

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
