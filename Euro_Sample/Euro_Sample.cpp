#include "daisy_patch_sm.h"
#include "daisysp.h"
#include "dev/oled_ssd130x.h"
#include "UiHardware.h"
#include "module_state.h"
#include "sample_player.h"
#include "string_sort.h"
#include "smoother.h"
#include "MyPersistantStorage.h"
//#include <vector>
//#include <string>
//#include <algorithm>

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;
using MyOledDisplay = OledDisplay<SSD130xI2c128x64Driver>;

DaisyPatchSM 			hw;
Encoder 				enc;
Switch 					sel_sw[4];
Led                     sel_led[4];
MyOledDisplay   		display;
SdmmcHandler   			sdcard;
FatFSInterface 			fsi;
UiEventQueue       		eventQueue;
MyPersistentStorage<ModuleState>  SavedState(hw.qspi);
SamplePlayer            sPlayer[4];

enum EditField {
	EDIT_NONE,
	EDIT_A,
	EDIT_B,
	EDIT_C,
	EDIT_D,
};

enum EditSubField {
	SUBEDIT_DIR = 0,
	SUBEDIT_SAMPLE = 1,
	SUBEDIT_LEVEL = 2,
	SUBEDIT_CVATT = 3,
	SUBEDIT_CVTARGET = 4,

	NUM_SUBEDIT,
};

enum CVTargetField {
	CVTARGET_SAMPLE = 0,
	CVTARGET_LEVEL = 1,

	NUM_CVTARGET,
};
const char *target_names[] = {"Sample", "Level"};

EditField               editting = EDIT_NONE;
EditSubField            sub_editting = SUBEDIT_DIR;

#define MAX_FILES 100
struct RootDir {
	int numDirs;
	char *dirNames[MAX_FILES];
} root;


#define SDRAM_BYTES 67108864
#define SDRAM_SAMPLES (SDRAM_BYTES / 2)
#define BYTES_PER_CHANNEL (SDRAM_BYTES / 4)
#define SAMPLES_PER_CHANNEL (SDRAM_SAMPLES / 4)

int16_t DSY_SDRAM_BSS sampleBuff[SDRAM_SAMPLES]; // storage for samples
//int16_t sampleBuff[2000]; // FIXME

struct SampleInfo {
	char *fileName;
	uint32_t size;
	int16_t *buffPnt;
};

// compare for sorting samples by file name
//bool sampleComp (SampleInfo i, SampleInfo j) { return (i.fileName<j.fileName); }

struct ChannelInfo {
	int numFiles;
	unsigned long totalSize;
	SampleInfo files[MAX_FILES];
};

ChannelInfo channelData[4];
volatile int channel_updating[4];

#define SAVE_VERSION 3
void initFactoryState(ModuleState *factory) {
	factory->version = SAVE_VERSION;
	for (int i=0; i<4; i++) {
		factory->channel[i].dirNum = i;
		factory->channel[i].sampleNum = 0;
		factory->channel[i].level = 50;
		factory->channel[i].cvAttenuvert = 0;
		factory->channel[i].cvTarget = CVTARGET_SAMPLE;
		factory->channel[i].cvOffset = 0.0f;
	}
}

int getValue(int old, int inc, int min, int max, bool mod) {
	int ret = old + inc;
	if (ret > max)
		ret = mod ? min : max;
	else if (ret < min)
		ret = mod ? max : min;
	return ret;
}

int time_enc_held; // time in ms that encoder is being held down
int time_held[4]; // time in ms that button is being held down
bool button_trigger[4];

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

    for(int i = 0; i < 4; i++) {
        sel_sw[i].Debounce();
		button_trigger[i] = false;

		if (sel_sw[i].Pressed())
			time_held[i] = sel_sw[i].TimeHeldMs();

		if(sel_sw[i].FallingEdge()) {
			if (time_held[i] > 250) {
				// Call it "Pressed" if held for long enough...
        		eventQueue.AddButtonPressed(bttnSelA + i, 1);
				button_trigger[i] = true; // play sample on long press
			} else {
				// ...otherwise call it "Released"
				eventQueue.AddButtonReleased(bttnSelA + i);

			}
		}
	}
}

bool trigger_last[4] = {false, false, false, false};
int sampleNum[4] = {0, 0, 0, 0};

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	hw.ProcessAllControls();
	GenerateUiEvents();
	ModuleState &sampleState = SavedState.GetSettings();

	// get triggers
	bool trigger[4];
	bool triggered[4];
	trigger[0] = hw.GetAdcValue(CV_5) > 0.5f;
	trigger[1] = hw.GetAdcValue(CV_6) > 0.5f;
	trigger[2] = hw.GetAdcValue(CV_7) > 0.5f;
	trigger[3] = hw.GetAdcValue(CV_8) > 0.5f;
	for (int ch=0; ch<4; ch++) {
		triggered[ch] = (trigger[ch] && !trigger_last[ch]) || button_trigger[ch];
		trigger_last[ch] = trigger[ch];
	}

	// light buttons when pressed or triggered
	for (int ch=0; ch<4; ch++) {
		float val = trigger[ch] ? 1.0f : ((editting == EDIT_A + ch) ? 0.5f : 0.0f);
		if (ch < 2)
			hw.WriteCvOut(ch+1, val * 5.0f);
		else {
			sel_led[ch].Set(val);
			sel_led[ch].Update();
		}
	}

	// read CVs
	float cvVal[4];
	cvVal[0] = hw.GetAdcValue(CV_1) - sampleState.channel[0].cvOffset;
	cvVal[1] = hw.GetAdcValue(CV_2) - sampleState.channel[1].cvOffset;
	cvVal[2] = hw.GetAdcValue(CV_3) - sampleState.channel[2].cvOffset;
	cvVal[3] = hw.GetAdcValue(CV_4) - sampleState.channel[3].cvOffset;

	float cvNum[4];
	float level[4];
	for (int ch=0; ch<4; ch++) {
		if (!channel_updating[ch]) {
			cvVal[ch] *= ((float) sampleState.channel[ch].cvAttenuvert) / 100.0f;
			cvNum[ch] = sampleState.channel[ch].cvTarget == CVTARGET_SAMPLE ? cvVal[ch] * 60.0f : 0.0f; // convert cv to midi #
			sampleNum[ch] = getValue (sampleState.channel[ch].sampleNum + (int) cvNum[ch], 0, 0, channelData[ch].numFiles - 1, false);
			level[ch] = ((float) sampleState.channel[ch].level) / 100.0f;
			level[ch] += sampleState.channel[ch].cvTarget == CVTARGET_LEVEL ? cvVal[ch] : 0.0f;
			if (level[ch] < 0.0f)
				level[ch] = 0.0f; 
			sPlayer[ch].SetSample(sampleState.channel[ch].dirNum,
						  	  	  sampleNum[ch],
							  	  channelData[ch].files[sampleNum[ch]].buffPnt,
							      channelData[ch].files[sampleNum[ch]].size);
		}
	}
	
	float sampleOut[4];

	for (size_t i = 0; i < size; i++)
	{
		for (int ch=0; ch<4; ch++)
			sampleOut[ch] = sPlayer[ch].Process(triggered[ch]) * level[ch]; 

		OUT_L[i] = sampleOut[2] + sampleOut[3];
		OUT_R[i] = sampleOut[0] + sampleOut[1];
	}
}

// read, store and sort directory names on root dir of sdcard
//
// apparently some of these can't be on the stack
FILINFO fno;
DIR     dir;
FIL     fil;
void readRoot(const char *path) {
	FRESULT result = FR_OK;

	display.Fill(false);
	display.SetCursor(0,0);
	display.WriteString("Reading SDCard...", Font_6x8, true);
	display.Update();

    // Open Dir and scan for files.
	root.numDirs = 0;
    if(f_opendir(&dir, path) != FR_OK)
    {
		display.SetCursor(0, 20);
		display.WriteString("SDCard failure", Font_7x10, true);
		display.Update();
		while (1) {};
    }
    do
    {			
        result = f_readdir(&dir, &fno);
        // Exit if bad read or NULL fname
        if(result != FR_OK || fno.fname[0] == 0) {
			break;
		}
        // Skip if its a hidden file or not a directory
        if((fno.fattrib & AM_HID) || !(fno.fattrib & AM_DIR))
            continue;
   
		root.dirNames[root.numDirs] = (char *) malloc(strlen(fno.fname) + 1);
		strcpy(root.dirNames[root.numDirs], fno.fname);
		root.numDirs++;

		// stop when we hit max files in a directory
		if (root.numDirs >= MAX_FILES)
			break;

    } while(result == FR_OK);
    f_closedir(&dir);

	// sort the directory names
	stringSort(root.dirNames, root.numDirs);

	char sbuf[20];
	sprintf(sbuf, "%d folders found", root.numDirs);
	display.SetCursor(0,10);
	display.WriteString(sbuf, Font_7x10, true);
	display.SetCursor(0,20);
	display.WriteString(path, Font_7x10, true);

	//sprintf(sbuf, "first: %s", root.dirNames[0].c_str());
	//display.SetCursor(0, 10);
	//display.WriteString(sbuf, Font_7x10, true);

	//sprintf(sbuf, "last: %s", root.dirNames.back().c_str());
	//display.SetCursor(0, 20);
	//display.WriteString(sbuf, Font_7x10, true);

	display.Update();
}

// read data from channel's directory into
// appropriate structures
//
// called at reset and when directory changes
//
// FIXME: make interrupt safe
//
#define DATA_SUBCHUNKID 0x61746164
typedef struct
{
    uint32_t SubChunkID;   /**< & */
    uint32_t SubChunkSize; /**< & */
} WAV_SubChunkDef;

WAV_FormatTypeDef header;
WAV_SubChunkDef subchunk_header;



char *fileList[MAX_FILES];

unsigned int bytesread;
void updateChannel(ModuleState &sampleState, int ch, int new_dir, int new_sample) {
	FRESULT result = FR_OK;
	char    path[256];
	char    *fn;
	char 	sbuf[80];
	int     fileCnt = 0;

	channel_updating[ch] = 1;

	while (sPlayer[ch].Playing()) {}

	sampleState.channel[ch].dirNum = new_dir;
	sampleState.channel[ch].sampleNum = new_sample;

	display.Fill(false);
	display.SetCursor(0,0);
	display.WriteString("Reading Files...", Font_6x8, true);
	display.Update();

	strcpy(path, fsi.GetSDPath());
	strcat(path, root.dirNames[sampleState.channel[ch].dirNum]);


	// clear old file list
	for (int i=0; i<channelData[ch].numFiles; i++) {
		free(channelData[ch].files[i].fileName);
		channelData[ch].files[i].size = 0;
		channelData[ch].files[i].buffPnt = NULL;
	}
	channelData[ch].totalSize = 0;
	channelData[ch].numFiles = 0;

	int16_t *current_pnt = &(sampleBuff[SAMPLES_PER_CHANNEL * ch]);

	// Open Dir and scan for files.
    if(f_opendir(&dir, path) != FR_OK)
    {
		display.WriteString("opendir failure-", Font_7x10, true);
		display.SetCursor(0, 10);
		display.WriteString(path, Font_7x10, true);
		display.Update();
		while(1) {};
    }
    do
    {
        result = f_readdir(&dir, &fno);
        // Exit if bad read or NULL fname
        if(result != FR_OK || fno.fname[0] == 0)
            break;
        // Skip if its a directory or a hidden file.
        if(fno.fattrib & (AM_HID | AM_DIR))
            continue;
        // Now we'll check if its .wav and add to the list.
        fn = fno.fname;

        if(strstr(fn, ".wav") || strstr(fn, ".WAV"))
        {
			// add to list
			fileList[fileCnt] = (char *) malloc(strlen(fn) + 1);
			strcpy(fileList[fileCnt], fn);
			fileCnt++;
        }

		// stop early if too many files in this directory
		if (fileCnt >= MAX_FILES)
			break;

    } while(result == FR_OK);
    f_closedir(&dir);

	// sort the files by name
	stringSort(fileList, fileCnt);

    // Now we'll go through each file and load the wave data into the sample buffer if there is space
    for(int i = 0; i < fileCnt; i++)
    {
        strcpy(path, fsi.GetSDPath());
        strcat(path, root.dirNames[sampleState.channel[ch].dirNum]);
        strcat(path, "/");
        strcat(path, fileList[i]);

        if(f_open(&fil, path, (FA_OPEN_EXISTING | FA_READ)) == FR_OK)
        {
            // Read header
            if((result = f_read(&fil, (void *)&header, sizeof(header), &bytesread))
               == FR_OK)
            {
				//display.Fill(false);
				//display.SetCursor(0, 0);
				//display.WriteString(channelData[ch].files[i].fileName.c_str(), Font_6x8, true);
				//display.Update();

				uint32_t subChunkID = header.SubChunk2ID;
				uint32_t subChunkSize = header.SubCHunk2Size;

				// skip over metadata before wave data
				while (subChunkID != DATA_SUBCHUNKID) {
					// read unwanted subchunk data
					if (f_read(&fil, (void *)current_pnt, subChunkSize, &bytesread) != FR_OK) {
						display.SetCursor(0, 10);
						display.WriteString("fail chunk read", Font_6x8, true);
                        display.Update();
                        f_close(&fil);
						while(1) {};
					}

					// read next subchunk header
					if (f_read(&fil, (void *)&subchunk_header, sizeof(subchunk_header), &bytesread) != FR_OK) {
						display.SetCursor(0, 10);
						display.WriteString("fail subheader read", Font_6x8, true);
                        display.Update();
                        f_close(&fil);
						while(1) {};
					}
					subChunkID = subchunk_header.SubChunkID;
					subChunkSize = subchunk_header.SubChunkSize;
				}

                uint32_t bytes = subChunkSize;

                // Read samples into buffer
                if(channelData[ch].totalSize + bytes <= BYTES_PER_CHANNEL)
                {
                    if(f_read(&fil, (void *)current_pnt, bytes, &bytesread)
                       == FR_OK)
                    {
                        channelData[ch].files[channelData[ch].numFiles].buffPnt = current_pnt;
                        channelData[ch].files[channelData[ch].numFiles].size = bytes;
						channelData[ch].files[channelData[ch].numFiles].fileName = fileList[i]; // no need to malloc
						channelData[ch].numFiles++;
						channelData[ch].totalSize += bytes;
                        current_pnt += bytes / 2;
						
						//display.SetCursor(0, 10);
						//display.WriteString("Loaded", Font_6x8, true);
						//display.SetCursor(0, 20);
						//sprintf(sbuf, "bits: %d", header.BitPerSample);
						//display.WriteString(sbuf, Font_6x8, true);
						//display.SetCursor(0, 30);
						//sprintf(sbuf, "bytes: %ld", bytes);
						//display.WriteString(sbuf, Font_6x8, true);
						//display.SetCursor(0, 40);
						//sprintf(sbuf, "file: %ld", header.FileSize);
						//display.WriteString(sbuf, Font_6x8, true);
						//display.Update();
						//display.SetCursor(0, 50);
						//sprintf(sbuf, "block: %c%c%c%c", subChunkID & 0xff, (subChunkID >> 8) & 0xff, (subChunkID>> 16) & 0xff, (subChunkID >> 24) & 0xff);
						//display.WriteString(sbuf, Font_6x8, true);
						//display.Update();
                    }
                    else
                    {
                        // failure
						display.SetCursor(0, 10);
                        display.WriteString(
                            "fail read sample", Font_7x10, true);
                        display.Update();
                        f_close(&fil);
						while(1) {};
                    }
                }
                else
                {
                    // Out of room!

					// free up unused file names
					for (int j=i; j<fileCnt; j++)
						free(fileList[j]);

					display.SetCursor(0, 10);
                    display.WriteString("out of room", Font_7x10, true);
                    display.Update();
                    f_close(&fil);
					channel_updating[ch] = 0;
					return;
                }
            }
            else
            {
                display.WriteString("fail read header", Font_7x10, true);
                display.SetCursor(0, 10);
                display.WriteString(path, Font_6x8, true);
                display.SetCursor(0, 20);
                display.WriteString(fileList[i], Font_6x8, true);

                sprintf(sbuf, "result = %d", result);
                display.SetCursor(0, 30);
                display.WriteString(sbuf, Font_7x10, true);
                display.Update();
                f_close(&fil);
                while(1) {};
            }

            f_close(&fil);
        }
        else
        {
            // failure
			display.SetCursor(0, 10);
            display.WriteString("fail open wave", Font_7x10, true);
            display.Update();
            while(1) {};
        }
    }
	channel_updating[ch] = 0;
}

void readQueue(ModuleState &sampleState)
{
	int inc, ch, newval;
    while(!eventQueue.IsQueueEmpty())
    {
        UiEventQueue::Event e = eventQueue.GetAndRemoveNextEvent();
        switch(e.type)
        {
            case UiEventQueue::Event::EventType::buttonReleased:
                // short button presses
                switch(e.asButtonPressed.id)
                {
                    case bttnEncoder:
						if (editting != EDIT_NONE)
							sub_editting = (EditSubField)((sub_editting + 1) % NUM_SUBEDIT);
						break;
                    case bttnSelA:
                        if(editting != EDIT_A)
                            sub_editting = SUBEDIT_DIR;
                        else
                            sub_editting
                                = (EditSubField)((sub_editting + 1) % NUM_SUBEDIT);
                        editting = EDIT_A;
                        break;
                    case bttnSelB:
                        if(editting != EDIT_B)
                            sub_editting = SUBEDIT_DIR;
                        else
                            sub_editting
                                = (EditSubField)((sub_editting + 1) % NUM_SUBEDIT);
                        editting = EDIT_B;
                        break;
                    case bttnSelC:
                        if(editting != EDIT_C)
                            sub_editting = SUBEDIT_DIR;
                        else
                            sub_editting
                                = (EditSubField)((sub_editting + 1) % NUM_SUBEDIT);
                        editting = EDIT_C;
                        break;
                    case bttnSelD:
                        if(editting != EDIT_D)
                            sub_editting = SUBEDIT_DIR;
                        else
                            sub_editting
                                = (EditSubField)((sub_editting + 1) % NUM_SUBEDIT);
                        editting = EDIT_D;
                        break;
                }
                break;

            // encoder turns
            case UiEventQueue::Event::EventType::encoderTurned:
                inc = e.asEncoderTurned.increments;
                if (editting != EDIT_NONE)
                {
                    ch = editting - EDIT_A;
                    switch(sub_editting)
                    {
						case SUBEDIT_DIR:
							newval = getValue(sampleState.channel[ch].dirNum, inc, 0, root.numDirs - 1, true);
							updateChannel(sampleState, ch, newval, 0);
                            break;

                        case SUBEDIT_SAMPLE:
							sampleState.channel[ch].sampleNum = getValue(sampleState.channel[ch].sampleNum, inc, 0, channelData[ch].numFiles - 1, true);
                            break;

						case SUBEDIT_LEVEL:
							sampleState.channel[ch].level = getValue(sampleState.channel[ch].level, inc, 0, 100, false);
                            break;
                        
						case SUBEDIT_CVATT:
							sampleState.channel[ch].cvAttenuvert = getValue(sampleState.channel[ch].cvAttenuvert, inc, -100, 100, false);
                            break;
                        
						case SUBEDIT_CVTARGET:
							sampleState.channel[ch].cvTarget = getValue(sampleState.channel[ch].cvTarget, inc, 0, NUM_CVTARGET-1, true);
                            break;

						default:
							break;
                    }
                }
                break;

			// Long presses
			case UiEventQueue::Event::EventType::buttonPressed:
			    switch(e.asButtonPressed.id)
                {
					// long press on encoder takes us to "home" screen
                 	case bttnEncoder:
						editting = EDIT_NONE;
						break;

					default:
						break;
				}
				break;

            default: break;
        }
    }
}

// remove extension from file name
void stripName(char *strip, const char *fname) {
	strcpy(strip, fname);
	char *pnt = strip;
	while (*pnt != '.' && *pnt != '\0') {
		pnt++;
	}
	*pnt = '\0';
}

void displayState(ModuleState &sampleState) {
	char sbuf[80];
	char fname[60];

	display.Fill(false); // clear display

	if (editting == EDIT_NONE) {
		for (int ch=0; ch<4; ch++) {
			display.SetCursor(0, ch * 16);
			stripName(fname, channelData[ch].files[sampleNum[ch]].fileName);
			sprintf(sbuf, "%c: ", 'A' + ch);
			display.WriteString(sbuf, Font_7x10, true);
			display.WriteString(fname, Font_7x10, !sPlayer[ch].Playing());
		}
	} else {
		display.SetCursor(0, 0);
		int ch = editting - EDIT_A;
		sprintf(sbuf, "      -- %c --", 'A' + ch);
		display.WriteString(sbuf, Font_7x10, true);
		
		// Directory name
		display.SetCursor(0, 10);
		sprintf(sbuf, "%s", root.dirNames[sampleState.channel[ch].dirNum]);
		display.WriteString(sbuf, Font_7x10, sub_editting != SUBEDIT_DIR);
		
		// Sample (file) name
		display.SetCursor(0, 20);
		stripName(fname, channelData[ch].files[sampleState.channel[ch].sampleNum].fileName);
		sprintf(sbuf, "%d: %s", sampleState.channel[ch].sampleNum, fname);
		display.WriteString(sbuf, Font_7x10, sub_editting != SUBEDIT_SAMPLE);

		// Level
		display.SetCursor(0, 30);
		display.WriteString("Level:   ", Font_7x10, true);
		sprintf(sbuf, "%d", sampleState.channel[ch].level);
		display.WriteString(sbuf, Font_7x10, sub_editting != SUBEDIT_LEVEL);

		// Attenuvert
		display.SetCursor(0, 40);
		display.WriteString("Atten:   ", Font_7x10, true);
		int att = abs(sampleState.channel[ch].cvAttenuvert);
		sprintf(sbuf, "%c%1.1d.%2.2d", sampleState.channel[ch].cvAttenuvert >= 0 ? '+' : '-', att / 100, att % 100);
		display.WriteString(sbuf, Font_7x10, sub_editting != SUBEDIT_CVATT);

		// Offset
		display.SetCursor(0, 50);
		display.WriteString("CVSel:  ", Font_7x10, true);
		display.WriteString(target_names[sampleState.channel[ch].cvTarget], Font_7x10, sub_editting != SUBEDIT_CVTARGET);
	}

	display.Update();
}

void processUI() {
	ModuleState &sampleState = SavedState.GetSettings();
	readQueue(sampleState);
	displayState(sampleState);
}

#define CALIBRATE_USECS 10000
#define CALIBRATE_FILTER_CONST 0.001f
// filter CV inputs and then update offsets
//
void calibrateCV (ModuleState &currentState) {
	Smoother cv_filter[4];

	// init smoothers
	for (int i = 0; i < 4; i++)
		cv_filter[i].Init(CALIBRATE_FILTER_CONST);

	// load 'em up
	for (int t = 0; t < CALIBRATE_USECS; t++) {
		cv_filter[0].Process(hw.GetAdcValue(CV_1));
		cv_filter[1].Process(hw.GetAdcValue(CV_2));
		cv_filter[2].Process(hw.GetAdcValue(CV_3));
		cv_filter[3].Process(hw.GetAdcValue(CV_4));
		System::DelayUs(1);
	}

	// Save 'em
	for (int i = 0; i < 4; i++)
		currentState.channel[i].cvOffset = cv_filter[i].GetVal();
}

int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

	// encoder
	enc.Init(DaisyPatchSM::D9, DaisyPatchSM::D1, DaisyPatchSM::D8);

	// buttons
	sel_sw[0].Init(DaisyPatchSM::A3);
	sel_sw[1].Init(DaisyPatchSM::A8);
	sel_sw[2].Init(DaisyPatchSM::A9);
	sel_sw[3].Init(DaisyPatchSM::A2);

	// LEDs
	sel_led[2].Init(DaisyPatchSM::B5, false);
	sel_led[3].Init(DaisyPatchSM::B6, false);

	// set up display
	MyOledDisplay::Config disp_cfg;
	display.Init(disp_cfg);
	display.Fill(false);
	display.WriteString("Hello", Font_7x10, true);
	display.Update();
	System::Delay(250);

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

	// Load up sdcard
	SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
	sd_cfg.speed = SdmmcHandler::Speed::STANDARD;
    sdcard.Init(sd_cfg);
    fsi.Init(FatFSInterface::Config::MEDIA_SD);
    f_mount(&fsi.GetSDFileSystem(), "/", 1);
	readRoot(fsi.GetSDPath());

	//char sbuf[80];
	// update channels with initial state
	for (int ch=0; ch < 4; ch++) {
		channel_updating[ch] = 0;
		updateChannel(writtenState, ch, writtenState.channel[ch].dirNum, writtenState.channel[ch].sampleNum);
		//display.SetCursor(0, 20);
		//sprintf(sbuf, "Done with channel %d", ch);
		//display.WriteString(sbuf, Font_6x8, true);
		//display.Update();
	}

	for (int ch=0; ch<4; ch++)
		sPlayer[ch].Init(&(channel_updating[ch]));

	hw.StartAudio(AudioCallback);

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
