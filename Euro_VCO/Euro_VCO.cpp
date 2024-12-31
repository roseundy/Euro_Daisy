#include "daisysp.h"
#include "daisysp-lgpl.h"
#include "daisy_patch_sm.h"
#include "UiHardware.h"
#include "module_state.h"
#include "OffsetCustomItem.h"
#include "fm3.h"
#include "fm4op.h"
#include "dev/oled_ssd130x.h"
#include "dsp/dsp.h"
#include "dsp/performance_state.h"
#include "dsp/part.h"
#include "dsp/strummer.h"
#include "dsp/string_synth_part.h"

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;
using namespace VCO;
using MyOledDisplay = OledDisplay<SSD130xI2c128x64Driver>;

#define REV0P0
//#define REV0P1

DaisyPatchSM 			hw;

// oscillators
//
Oscillator   			osc1, osc2;       		// For Sine Oscillator
VariableShapeOscillator vs_osc1, vs_osc2; 		// For Square/Triangle/Saw Oscillator
#define                 NUM_HARMONICS   12
HarmonicOscillator<NUM_HARMONICS> harm_osc;
Fm4Op                   fm_osc;
Adsr                    fm_env[4];
VosimOscillator         form_osc;
#define                 NUM_STRINGS     7
Oscillator              string_section[NUM_STRINGS];  // For string section
Svf                     string_filter;
Oscillator              chord_voice[4];
Pluck                   pluck;
AnalogBassDrum          bassDrum;
AnalogSnareDrum         snareDrum;
HiHat<>                 hiHat;
WhiteNoise              white;
torus::Part DSY_SDRAM_BSS part;
//torus::Part             part;
torus::StringSynthPart  string_synth;
torus::Strummer         strummer;
uint16_t DSY_SDRAM_BSS  ring_reverb_buffer[32768];
float DSY_SDRAM_BSS     torus::lut_sine[LUT_SINE_SIZE];
#ifdef REV0P1
Led                     gateLed;
Switch                  gateSwitch;
#endif

Adsr 					env;                    // For voices that have an optional envelope

Encoder 				enc;
MyOledDisplay           display;
daisy::UI 				ui;
UiEventQueue            eventQueue;

FullScreenItemMenu 		mainMenu;
FullScreenItemMenu		voiceConfMenu;
FullScreenItemMenu      attenuvertMenu;
FullScreenItemMenu      envelopeMenu;
FullScreenItemMenu      offsetMenu;
FullScreenItemMenu      fmOp0Menu;
FullScreenItemMenu      fmOp1Menu;
FullScreenItemMenu      fmOp2Menu;
FullScreenItemMenu      fmOp3Menu;

const int                kNumMainMenuItems = 9;
AbstractMenu::ItemConfig mainMenuItems[kNumMainMenuItems];
const int                kNumVoiceConfMenuItems = 15;
AbstractMenu::ItemConfig voiceConfMenuItems[kNumVoiceConfMenuItems];
const int                kNumAttenuvertMenuItems = 5;
AbstractMenu::ItemConfig attenuvertMenuItems[kNumAttenuvertMenuItems];
const int                kNumEnvelopeMenuItems = 5;
AbstractMenu::ItemConfig envelopeMenuItems[kNumEnvelopeMenuItems];
const int                kNumOffsetMenuItems = 6;
AbstractMenu::ItemConfig offsetMenuItems[kNumOffsetMenuItems];
const int                kNumFmOp0MenuItems = 9;
AbstractMenu::ItemConfig fmOp0MenuItems[kNumFmOp0MenuItems];
const int                kNumFmOp1MenuItems = 9;
AbstractMenu::ItemConfig fmOp1MenuItems[kNumFmOp1MenuItems];
const int                kNumFmOp2MenuItems = 9;
AbstractMenu::ItemConfig fmOp2MenuItems[kNumFmOp2MenuItems];
const int                kNumFmOp3MenuItems = 10;
AbstractMenu::ItemConfig fmOp3MenuItems[kNumFmOp3MenuItems];

// voice menu items
const char* voiceListValues[]
    = {"Pair", "Harmonic", "FM", "Formant", "Strings", "Chord", "Pluck", "Bass Drum", "Snare", "HiHat", "Rings"};
MappedStringListValue voiceListValue(voiceListValues, 11, 0);

// pair menu items
const char* pairListValues[]
    = {"Sine", "Square", "Triangle"};
MappedStringListValue pairListValueOne(pairListValues, 3, 0);
MappedStringListValue pairListValueTwo(pairListValues, 3, 0);

// pluck
const char* pluckListValues[]
    = {"Recursive", "Average"};
MappedStringListValue pluckListValue(pluckListValues, 2, 0);

// bass drum
MappedFloatValue bassAccentValue(0.0f, 1.0f, 0.0f, MappedFloatValue::Mapping::lin, "", 2, true);

// ring polyphonic
const char* ringPolyValues[]
    = {"1 Voice", "2 Voices", "4 Voices"};
MappedStringListValue ringPolyValue(ringPolyValues, 3, 0);

// ring model
const char* ringModelValues[]
    = {"Modal", "Symp Str", "Inhrm Str", "Fm Voice", "Westn Chrd", "Str & Verb", "Easter Egg" };
MappedStringListValue ringModelValue(ringModelValues, 7, 0);

const char* ringFxValues[]
    = {"Formant", "Chorus", "Reverb", "Formant2", "Ensemble", "Reverb2"};
MappedStringListValue ringFxValue(ringFxValues, 6, 0);

// ring normalization
const char* ringNormalValues[]
    = {"V/OCT+Gate", "No V/OCT", "No Gate"};
MappedStringListValue ringNormalValue(ringNormalValues, 3, 0);

// output attenuvert values
MappedFloatValue attenuvertValueP1(-1.0f, 1.0f, 0.0f, MappedFloatValue::Mapping::lin, "", 2, true);
MappedFloatValue attenuvertValueP2(-1.0f, 1.0f, 0.0f, MappedFloatValue::Mapping::lin, "", 2, true);
MappedFloatValue attenuvertValueP3(-1.0f, 1.0f, 0.0f, MappedFloatValue::Mapping::lin, "", 2, true);
MappedFloatValue attenuvertValueP4(-1.0f, 1.0f, 0.0f, MappedFloatValue::Mapping::lin, "", 2, true);

// base octave value
MappedIntValue   octiveValue(1, 6, 2, 1, 1, "", false);

// output envelope values
MappedFloatValue envelopeValueA(0.0f, 10.0f, 0.1f, MappedFloatValue::Mapping::lin, "sec", 1, false);
MappedFloatValue envelopeValueD(0.0f, 10.0f, 0.1f, MappedFloatValue::Mapping::lin, "sec", 1, false);
MappedFloatValue envelopeValueR(0.0f, 10.0f, 0.1f, MappedFloatValue::Mapping::lin, "sec", 1, false);
MappedIntValue   envelopeValueS(0, 100, 50, 1, 10, "%", false);

// gain
MappedFloatValue overallGain(0.0f, 2.0f, 0.8f, MappedFloatValue::Mapping::lin, "", 2, false);

// offsets
OffsetCustomItem voctOffsetItem;
OffsetCustomItem p1OffsetItem;
OffsetCustomItem p2OffsetItem;
OffsetCustomItem p3OffsetItem;
OffsetCustomItem p4OffsetItem;

// FM stuff
MappedIntValue fmAlgorithmValue(0, 8, 7, 1, 1, "",false);

struct FmOpControls {
    MappedIntValue   ratioN;
    MappedIntValue   ratioD;
    MappedIntValue   maxIndex;
    MappedIntValue   maxFB;
    bool             useEnvelope;
    MappedFloatValue attack;
    MappedFloatValue decay;
    MappedFloatValue release;
    MappedIntValue   sustain;

    FmOpControls() :
        ratioN(1, 16, 1, 1, 5, "", false),
        ratioD(1, 16, 1, 1, 5, "", false),
        maxIndex(0, 200, 0, 1, 5, "%", false),
        maxFB(0, 200, 0, 1, 5, "%", false),
        useEnvelope(false),
        attack(0.0f, 10.0f, 0.1f, MappedFloatValue::Mapping::lin, "sec", 1, false),
        decay(0.0f, 10.0f, 0.1f, MappedFloatValue::Mapping::lin, "sec", 1, false),
        release(0.0f, 10.0f, 0.1f, MappedFloatValue::Mapping::lin, "sec", 1, false),
        sustain(0, 100, 50, 1, 10, "%", false)
        {}
} fmOp0Controls, fmOp1Controls, fmOp2Controls, fmOp3Controls;

float voctOffset;
float p1Offset;
float p2Offset;
float p3Offset;
float p4Offset;

PersistentStorage<moduleState>  SavedState(hw.qspi);
bool                            gate_last;
bool                            useOutputEnvelope;
bool                            pairSync;
bool                            quantize;

void initFmOp(fmOpState *opState) {
    opState->ratioN = 1;
    opState->ratioD = 1;
    opState->maxFB = 0;
    opState->maxIndex = 0;
    opState->useOpEnvelope = false;
    opState->opEnvValues.attack = 0.1f;
    opState->opEnvValues.decay = 0.1f;
    opState->opEnvValues.sustain = 0.5f;
    opState->opEnvValues.release = 0.1f;
}

#ifdef REV0P0
#define CV6_ADJ 0.117
#endif

void initFactoryState(moduleState *state) {
    state->version = 11;

	state->voice = MODE_PAIR;
	state->pairWaveform[0] = PAIR_SINE;
	state->pairWaveform[1] = PAIR_SINE;
	state->pairSync = false;
	state->useOutputEnvelope = false;

    state->fmAlogrithm = 7;
    for (int i=0; i<3; i++)
        initFmOp(&(state->fmOpSettings[i]));

    state->pluckMode = PLUCK_MODE_RECURSIVE;

    state->bassAccent = 0.0f;

    state->ringPoly = RING_POLY_ONE;
    state->ringModel = RING_MODEL_MODAL;
    state->ringFx = RING_FX_FORMANT;
    state->ringNormal = RING_NORMAL_NONE;

    state->p1Attenuvert = 0.0f;
    state->p2Attenuvert = 0.1f;
    state->p3Attenuvert = 0.0f;
    state->p4Attenuvert = 0.0f;

    state->outputEnvValues.attack = 0.1f;
    state->outputEnvValues.decay = 0.1f;
    state->outputEnvValues.release = 0.1f;
    state->outputEnvValues.sustain = 0.5f;

    state->octave = 0.25f;
    state->octInt = 2;

    state->quantize = false;

    state->gain = 0.8f;

    state->voctOffset = 0.0f;
    state->p1Offset = 0.0f;
    state->p2Offset = 0.0f;
    state->p3Offset = 0.0f;
    state->p4Offset = 0.0f;
}

void InitUi()
{
    UI::SpecialControlIds specialControlIds;
    specialControlIds.okBttnId
        = bttnEncoder; // Encoder button is our okay button
    specialControlIds.menuEncoderId
        = encoderMain; // Encoder is used as the main menu navigation encoder

	// Connect encoder
	enc.Init(DaisyPatchSM::D5, DaisyPatchSM::D6, DaisyPatchSM::D4);

    // This is the canvas for the OLED display.
    UiCanvasDescriptor oledDisplayDescriptor;
    oledDisplayDescriptor.id_     = canvasOledDisplay; // the unique ID
    oledDisplayDescriptor.handle_ = &display;   // a pointer to the display
    oledDisplayDescriptor.updateRateMs_      = 50; // 50ms == 20Hz
    oledDisplayDescriptor.screenSaverTimeOut = 0;  // display always on
    oledDisplayDescriptor.clearFunction_     = &ClearCanvas;
    oledDisplayDescriptor.flushFunction_     = &FlushCanvas;

    ui.Init(eventQueue,
            specialControlIds,
            {oledDisplayDescriptor},
            canvasOledDisplay);
}

void InitUiPages()
{
    // ====================================================================
    // The main menu
    // ====================================================================

    mainMenuItems[0].type = daisy::AbstractMenu::ItemType::valueItem;
    mainMenuItems[0].text = "Voice";
    mainMenuItems[0].asMappedValueItem.valueToModify = &voiceListValue;

    mainMenuItems[1].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    mainMenuItems[1].text = "Voice Conf";
    mainMenuItems[1].asOpenUiPageItem.pageToOpen = &voiceConfMenu;

    mainMenuItems[2].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    mainMenuItems[2].text = "Attenuvert";
    mainMenuItems[2].asOpenUiPageItem.pageToOpen = &attenuvertMenu;

	mainMenuItems[3].type = daisy::AbstractMenu::ItemType::checkboxItem;
    mainMenuItems[3].text = "Out Env?";
    mainMenuItems[3].asCheckboxItem.valueToModify = &useOutputEnvelope;

    mainMenuItems[4].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    mainMenuItems[4].text = "Envelope";
    mainMenuItems[4].asOpenUiPageItem.pageToOpen = &envelopeMenu;

    mainMenuItems[5].type = daisy::AbstractMenu::ItemType::valueItem;
    mainMenuItems[5].text = "Octave";
    mainMenuItems[5].asMappedValueItem.valueToModify = &octiveValue;

	mainMenuItems[6].type = daisy::AbstractMenu::ItemType::checkboxItem;
    mainMenuItems[6].text = "Quantize";
    mainMenuItems[6].asCheckboxItem.valueToModify = &quantize;

	mainMenuItems[7].type = daisy::AbstractMenu::ItemType::valueItem;
    mainMenuItems[7].text = "Gain";
    mainMenuItems[7].asMappedValueItem.valueToModify = &overallGain;

    mainMenuItems[8].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    mainMenuItems[8].text = "Offsets";
    mainMenuItems[8].asOpenUiPageItem.pageToOpen = &offsetMenu; 

    mainMenu.Init(mainMenuItems, kNumMainMenuItems);

    // ====================================================================
    // "Voice Configure" sub-menu
    // ====================================================================

    voiceConfMenuItems[0].type = daisy::AbstractMenu::ItemType::valueItem;
    voiceConfMenuItems[0].text = "Pair Out1";
    voiceConfMenuItems[0].asMappedValueItem.valueToModify = &pairListValueOne;

    voiceConfMenuItems[1].type = daisy::AbstractMenu::ItemType::valueItem;;
    voiceConfMenuItems[1].text = "Pair Out2";
    voiceConfMenuItems[1].asMappedValueItem.valueToModify = &pairListValueTwo;

    voiceConfMenuItems[2].type = daisy::AbstractMenu::ItemType::checkboxItem;
    voiceConfMenuItems[2].text = "Pair Sync?";
    voiceConfMenuItems[2].asCheckboxItem.valueToModify = &pairSync;

    voiceConfMenuItems[3].type = daisy::AbstractMenu::ItemType::valueItem;
    voiceConfMenuItems[3].text = "FM Algo";
    voiceConfMenuItems[3].asMappedValueItem.valueToModify = &fmAlgorithmValue;

    voiceConfMenuItems[4].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    voiceConfMenuItems[4].text = "FM OP0";
    voiceConfMenuItems[4].asOpenUiPageItem.pageToOpen = &fmOp0Menu;

    voiceConfMenuItems[5].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    voiceConfMenuItems[5].text = "FM OP1";
    voiceConfMenuItems[5].asOpenUiPageItem.pageToOpen = &fmOp1Menu;

    voiceConfMenuItems[6].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    voiceConfMenuItems[6].text = "FM OP2";
    voiceConfMenuItems[6].asOpenUiPageItem.pageToOpen = &fmOp2Menu;

    voiceConfMenuItems[7].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    voiceConfMenuItems[7].text = "FM OP3";
    voiceConfMenuItems[7].asOpenUiPageItem.pageToOpen = &fmOp3Menu;
    
    voiceConfMenuItems[8].type = daisy::AbstractMenu::ItemType::valueItem;
    voiceConfMenuItems[8].text = "Pluck Mode";
    voiceConfMenuItems[8].asMappedValueItem.valueToModify = &pluckListValue;

    voiceConfMenuItems[9].type = daisy::AbstractMenu::ItemType::valueItem;
    voiceConfMenuItems[9].text = "Bass Acnt";
    voiceConfMenuItems[9].asMappedValueItem.valueToModify = &bassAccentValue;

    voiceConfMenuItems[10].type = daisy::AbstractMenu::ItemType::valueItem;
    voiceConfMenuItems[10].text = "Ring Poly";
    voiceConfMenuItems[10].asMappedValueItem.valueToModify = &ringPolyValue;

    voiceConfMenuItems[11].type = daisy::AbstractMenu::ItemType::valueItem;
    voiceConfMenuItems[11].text = "Ring Model";
    voiceConfMenuItems[11].asMappedValueItem.valueToModify = &ringModelValue;

    voiceConfMenuItems[12].type = daisy::AbstractMenu::ItemType::valueItem;
    voiceConfMenuItems[12].text = "Ring EggFx";
    voiceConfMenuItems[12].asMappedValueItem.valueToModify = &ringFxValue;

    voiceConfMenuItems[13].type = daisy::AbstractMenu::ItemType::valueItem;
    voiceConfMenuItems[13].text = "Ring Norm";
    voiceConfMenuItems[13].asMappedValueItem.valueToModify = &ringNormalValue;

    voiceConfMenuItems[14].type = daisy::AbstractMenu::ItemType::closeMenuItem;
    voiceConfMenuItems[14].text = "Back";

    voiceConfMenu.Init(voiceConfMenuItems, kNumVoiceConfMenuItems);

    // ====================================================================
    // "Attenuvert" sub-menu
    // ====================================================================

    attenuvertMenuItems[0].type = daisy::AbstractMenu::ItemType::valueItem;
    attenuvertMenuItems[0].text = "P1 AttVrt";
    attenuvertMenuItems[0].asMappedValueItem.valueToModify = &attenuvertValueP1;

    attenuvertMenuItems[1].type = daisy::AbstractMenu::ItemType::valueItem;
    attenuvertMenuItems[1].text = "P2 AttVrt";
    attenuvertMenuItems[1].asMappedValueItem.valueToModify = &attenuvertValueP2;

    attenuvertMenuItems[2].type = daisy::AbstractMenu::ItemType::valueItem;
    attenuvertMenuItems[2].text = "P3 AttVrt";
    attenuvertMenuItems[2].asMappedValueItem.valueToModify = &attenuvertValueP3;

    attenuvertMenuItems[3].type = daisy::AbstractMenu::ItemType::valueItem;
    attenuvertMenuItems[3].text = "P4 AttVrt";
    attenuvertMenuItems[3].asMappedValueItem.valueToModify = &attenuvertValueP4;

    attenuvertMenuItems[4].type = daisy::AbstractMenu::ItemType::closeMenuItem;
    attenuvertMenuItems[4].text = "Back";

    attenuvertMenu.Init(attenuvertMenuItems, kNumAttenuvertMenuItems);

    // ====================================================================
    // "Envelope" sub-menu
    // ====================================================================

    envelopeMenuItems[0].type = daisy::AbstractMenu::ItemType::valueItem;
    envelopeMenuItems[0].text = "Attack";
    envelopeMenuItems[0].asMappedValueItem.valueToModify = &envelopeValueA;

    envelopeMenuItems[1].type = daisy::AbstractMenu::ItemType::valueItem;
    envelopeMenuItems[1].text = "Decay";
    envelopeMenuItems[1].asMappedValueItem.valueToModify = &envelopeValueD;

    envelopeMenuItems[2].type = daisy::AbstractMenu::ItemType::valueItem;
    envelopeMenuItems[2].text = "Sustain";
    envelopeMenuItems[2].asMappedValueItem.valueToModify = &envelopeValueS;

    envelopeMenuItems[3].type = daisy::AbstractMenu::ItemType::valueItem;
    envelopeMenuItems[3].text = "Release";
    envelopeMenuItems[3].asMappedValueItem.valueToModify = &envelopeValueR;

    envelopeMenuItems[4].type = daisy::AbstractMenu::ItemType::closeMenuItem;
    envelopeMenuItems[4].text = "Back";

    envelopeMenu.Init(envelopeMenuItems, kNumEnvelopeMenuItems);

    // ====================================================================
    // "Offset" sub-menu
    // ====================================================================

    offsetMenuItems[0].type = daisy::AbstractMenu::ItemType::customItem;
    offsetMenuItems[0].text = "V/Oct";
    offsetMenuItems[0].asCustomItem.itemObject = &voctOffsetItem;

    offsetMenuItems[1].type = daisy::AbstractMenu::ItemType::customItem;
    offsetMenuItems[1].text = "P1 CV";
    offsetMenuItems[1].asCustomItem.itemObject = &p1OffsetItem;

    offsetMenuItems[2].type = daisy::AbstractMenu::ItemType::customItem;
    offsetMenuItems[2].text = "P2 CV";
    offsetMenuItems[2].asCustomItem.itemObject = &p2OffsetItem;

    offsetMenuItems[3].type = daisy::AbstractMenu::ItemType::customItem;
    offsetMenuItems[3].text = "P3 CV";
    offsetMenuItems[3].asCustomItem.itemObject = &p3OffsetItem;

    offsetMenuItems[4].type = daisy::AbstractMenu::ItemType::customItem;
    offsetMenuItems[4].text = "P4 CV";
    offsetMenuItems[4].asCustomItem.itemObject = &p4OffsetItem;

    offsetMenuItems[5].type = daisy::AbstractMenu::ItemType::closeMenuItem;
    offsetMenuItems[5].text = "Back";

    offsetMenu.Init(offsetMenuItems, kNumOffsetMenuItems);

    // ====================================================================
    // "OP0" sub-menu
    // ====================================================================

    fmOp0MenuItems[0].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp0MenuItems[0].text = "Ratio N";
    fmOp0MenuItems[0].asMappedValueItem.valueToModify = &(fmOp0Controls.ratioN);

    fmOp0MenuItems[1].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp0MenuItems[1].text = "Ratio D";
    fmOp0MenuItems[1].asMappedValueItem.valueToModify = &(fmOp0Controls.ratioD);

    fmOp0MenuItems[2].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp0MenuItems[2].text = "Max Idx";
    fmOp0MenuItems[2].asMappedValueItem.valueToModify = &(fmOp0Controls.maxIndex);

    fmOp0MenuItems[3].type = daisy::AbstractMenu::ItemType::checkboxItem;
    fmOp0MenuItems[3].text = "Use Env?";
    fmOp0MenuItems[3].asCheckboxItem.valueToModify = &(fmOp0Controls.useEnvelope);

    fmOp0MenuItems[4].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp0MenuItems[4].text = "Attack";
    fmOp0MenuItems[4].asMappedValueItem.valueToModify = &(fmOp0Controls.attack);

    fmOp0MenuItems[5].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp0MenuItems[5].text = "Decay";
    fmOp0MenuItems[5].asMappedValueItem.valueToModify = &(fmOp0Controls.decay);

    fmOp0MenuItems[6].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp0MenuItems[6].text = "Sustain";
    fmOp0MenuItems[6].asMappedValueItem.valueToModify = &(fmOp0Controls.sustain);

    fmOp0MenuItems[7].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp0MenuItems[7].text = "Release";
    fmOp0MenuItems[7].asMappedValueItem.valueToModify = &(fmOp0Controls.release);

    fmOp0MenuItems[8].type = daisy::AbstractMenu::ItemType::closeMenuItem;
    fmOp0MenuItems[8].text = "Back";

    fmOp0Menu.Init(fmOp0MenuItems, kNumFmOp0MenuItems);

    // ====================================================================
    // "OP1" sub-menu
    // ====================================================================

    fmOp1MenuItems[0].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp1MenuItems[0].text = "Ratio N";
    fmOp1MenuItems[0].asMappedValueItem.valueToModify = &(fmOp1Controls.ratioN);

    fmOp1MenuItems[1].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp1MenuItems[1].text = "Ratio D";
    fmOp1MenuItems[1].asMappedValueItem.valueToModify = &(fmOp1Controls.ratioD);
    
    fmOp1MenuItems[2].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp1MenuItems[2].text = "Max Idx";
    fmOp1MenuItems[2].asMappedValueItem.valueToModify = &(fmOp1Controls.maxIndex);

    fmOp1MenuItems[3].type = daisy::AbstractMenu::ItemType::checkboxItem;
    fmOp1MenuItems[3].text = "Use Env?";
    fmOp1MenuItems[3].asCheckboxItem.valueToModify = &(fmOp1Controls.useEnvelope);

    fmOp1MenuItems[4].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp1MenuItems[4].text = "Attack";
    fmOp1MenuItems[4].asMappedValueItem.valueToModify = &(fmOp1Controls.attack);

    fmOp1MenuItems[5].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp1MenuItems[5].text = "Decay";
    fmOp1MenuItems[5].asMappedValueItem.valueToModify = &(fmOp1Controls.decay);

    fmOp1MenuItems[6].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp1MenuItems[6].text = "Sustain";
    fmOp1MenuItems[6].asMappedValueItem.valueToModify = &(fmOp1Controls.sustain);

    fmOp1MenuItems[7].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp1MenuItems[7].text = "Release";
    fmOp1MenuItems[7].asMappedValueItem.valueToModify = &(fmOp1Controls.release);

    fmOp1MenuItems[8].type = daisy::AbstractMenu::ItemType::closeMenuItem;
    fmOp1MenuItems[8].text = "Back";

    fmOp1Menu.Init(fmOp1MenuItems, kNumFmOp1MenuItems);

    // ====================================================================
    // "OP2" sub-menu
    // ====================================================================

    fmOp2MenuItems[0].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp2MenuItems[0].text = "Ratio N";
    fmOp2MenuItems[0].asMappedValueItem.valueToModify = &(fmOp2Controls.ratioN);

    fmOp2MenuItems[1].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp2MenuItems[1].text = "Ratio D";
    fmOp2MenuItems[1].asMappedValueItem.valueToModify = &(fmOp2Controls.ratioD);

    fmOp2MenuItems[2].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp2MenuItems[2].text = "Max Idx";
    fmOp2MenuItems[2].asMappedValueItem.valueToModify = &(fmOp2Controls.maxIndex);

    fmOp2MenuItems[3].type = daisy::AbstractMenu::ItemType::checkboxItem;
    fmOp2MenuItems[3].text = "Use Env?";
    fmOp2MenuItems[3].asCheckboxItem.valueToModify = &(fmOp2Controls.useEnvelope);

    fmOp2MenuItems[4].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp2MenuItems[4].text = "Attack";
    fmOp2MenuItems[4].asMappedValueItem.valueToModify = &(fmOp2Controls.attack);

    fmOp2MenuItems[5].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp2MenuItems[5].text = "Decay";
    fmOp2MenuItems[5].asMappedValueItem.valueToModify = &(fmOp2Controls.decay);

    fmOp2MenuItems[6].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp2MenuItems[6].text = "Sustain";
    fmOp2MenuItems[6].asMappedValueItem.valueToModify = &(fmOp2Controls.sustain);

    fmOp2MenuItems[7].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp2MenuItems[7].text = "Release";
    fmOp2MenuItems[7].asMappedValueItem.valueToModify = &(fmOp2Controls.release);

    fmOp2MenuItems[8].type = daisy::AbstractMenu::ItemType::closeMenuItem;
    fmOp2MenuItems[8].text = "Back";

    fmOp2Menu.Init(fmOp2MenuItems, kNumFmOp2MenuItems);

    // ====================================================================
    // "OP2" sub-menu
    // ====================================================================

    fmOp3MenuItems[0].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp3MenuItems[0].text = "Ratio N";
    fmOp3MenuItems[0].asMappedValueItem.valueToModify = &(fmOp3Controls.ratioN);

    fmOp3MenuItems[1].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp3MenuItems[1].text = "Ratio D";
    fmOp3MenuItems[1].asMappedValueItem.valueToModify = &(fmOp3Controls.ratioD);

    fmOp3MenuItems[2].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp3MenuItems[2].text = "Max Idx";
    fmOp3MenuItems[2].asMappedValueItem.valueToModify = &(fmOp3Controls.maxIndex);

    fmOp3MenuItems[3].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp3MenuItems[3].text = "Max FB";
    fmOp3MenuItems[3].asMappedValueItem.valueToModify = &(fmOp3Controls.maxFB);

    fmOp3MenuItems[4].type = daisy::AbstractMenu::ItemType::checkboxItem;
    fmOp3MenuItems[4].text = "Use Env?";
    fmOp3MenuItems[4].asCheckboxItem.valueToModify = &(fmOp3Controls.useEnvelope);

    fmOp3MenuItems[5].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp3MenuItems[5].text = "Attack";
    fmOp3MenuItems[5].asMappedValueItem.valueToModify = &(fmOp3Controls.attack);

    fmOp3MenuItems[6].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp3MenuItems[6].text = "Decay";
    fmOp3MenuItems[6].asMappedValueItem.valueToModify = &(fmOp3Controls.decay);

    fmOp3MenuItems[7].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp3MenuItems[7].text = "Sustain";
    fmOp3MenuItems[7].asMappedValueItem.valueToModify = &(fmOp3Controls.sustain);

    fmOp3MenuItems[8].type = daisy::AbstractMenu::ItemType::valueItem;
    fmOp3MenuItems[8].text = "Release";
    fmOp3MenuItems[8].asMappedValueItem.valueToModify = &(fmOp3Controls.release);

    fmOp3MenuItems[9].type = daisy::AbstractMenu::ItemType::closeMenuItem;
    fmOp3MenuItems[9].text = "Back";

    fmOp3Menu.Init(fmOp3MenuItems, kNumFmOp3MenuItems);
}

void State2UiFM(FmOpControls *controls, fmOpState state) {
    controls->maxIndex.Set(state.maxIndex);
    controls->maxFB.Set(state.maxFB);
    controls->ratioN.Set(state.ratioN);
    controls->ratioD.Set(state.ratioD);
    controls->useEnvelope = state.useOpEnvelope;
    controls->attack.Set(state.opEnvValues.attack);
    controls->decay.Set(state.opEnvValues.decay);
    controls->sustain.Set(state.opEnvValues.sustain);
    controls->release.Set(state.opEnvValues.release);
}

// copied saved state to UI controls
//
void State2Ui(moduleState &VCOState) {
   // Voice
    voiceListValue.SetIndex(VCOState.voice);

    // Voice: Pair
    pairListValueOne.SetIndex(VCOState.pairWaveform[0]);
    pairListValueTwo.SetIndex(VCOState.pairWaveform[1]);
    pairSync = VCOState.pairSync;

    // Voice: FM
    fmAlgorithmValue.Set(VCOState.fmAlogrithm);
    State2UiFM(&fmOp0Controls, VCOState.fmOpSettings[0]);
    State2UiFM(&fmOp1Controls, VCOState.fmOpSettings[1]);
    State2UiFM(&fmOp2Controls, VCOState.fmOpSettings[2]);
    State2UiFM(&fmOp3Controls, VCOState.fmOpSettings[3]);

    // Voice: Pluck
    pluckListValue.SetIndex(VCOState.pluckMode);

    // Voice: Bass Drum
    bassAccentValue.Set(VCOState.bassAccent);

    // Voice: Rings
    ringPolyValue.SetIndex(VCOState.ringPoly);
    ringModelValue.SetIndex(VCOState.ringModel);
    ringFxValue.SetIndex(VCOState.ringFx);
    ringNormalValue.SetIndex(VCOState.ringNormal);

    // Attenuverts
    attenuvertValueP1.Set(VCOState.p1Attenuvert);
    attenuvertValueP2.Set(VCOState.p2Attenuvert);
    attenuvertValueP3.Set(VCOState.p3Attenuvert);
    attenuvertValueP4.Set(VCOState.p4Attenuvert);

    // Envelope
    useOutputEnvelope = VCOState.useOutputEnvelope;
    envelopeValueA.Set(VCOState.outputEnvValues.attack);
    envelopeValueD.Set(VCOState.outputEnvValues.decay);
    envelopeValueR.Set(VCOState.outputEnvValues.release);
    envelopeValueS.Set(VCOState.outputEnvValues.sustain * 100.f);

    // octave -> VCOState.octave = ???
    // 1      -> 0 Volts = 440Hz / 8
    // 2      -> 0 Volts = 440Hz / 4
    // 3      -> 0 Volts = 440Hz / 2
    // 4      -> 0 Volts = 440Hz
    // 5      -> 0 Volts = 440Hz * 2 
    // 6      -> 0 Volts = 440Hz * 4
    octiveValue.Set(VCOState.octInt);
    int ioct = VCOState.octInt;
    float foct = 0.125f;
    for (; ioct > 1; ioct--)
        foct *= 2.0;
    VCOState.octave = foct;

    // Quantize
    quantize = VCOState.quantize;

    // Gain
    overallGain.Set(VCOState.gain);

    // Offsets
    voctOffset = VCOState.voctOffset;
    p1Offset = VCOState.p1Offset;
    p2Offset = VCOState.p2Offset;
    p3Offset = VCOState.p3Offset;
    p4Offset = VCOState.p4Offset;
}

void GenerateUiEvents()
{
    if(enc.RisingEdge())
        eventQueue.AddButtonPressed(bttnEncoder, 1);

    if(enc.FallingEdge())
        eventQueue.AddButtonReleased(bttnEncoder);

    const auto increments = enc.Increment();
    if(increments != 0)
        eventQueue.AddEncoderTurned(encoderMain, increments, 12);
}

void Ui2StateFM(fmOpState *state, FmOpControls controls) {
    state->maxIndex = controls.maxIndex.Get();
    state->maxFB = controls.maxFB.Get();
    state->ratioN = controls.ratioN.Get();
    state->ratioD = controls.ratioD.Get();
    state->useOpEnvelope = controls.useEnvelope;
    state->opEnvValues.attack = controls.attack.Get();
    state->opEnvValues.decay = controls.decay.Get();
    state->opEnvValues.sustain = controls.sustain.Get();
    state->opEnvValues.release = controls.release.Get();
}

moduleState &ProcessState() {
    moduleState &VCOState = SavedState.GetSettings();

    // Voice
    VCOState.voice = (voiceType) voiceListValue.GetIndex();

    // Voice: Pair
    VCOState.pairWaveform[0] = (pairWaveformType) pairListValueOne.GetIndex();
    VCOState.pairWaveform[1] = (pairWaveformType) pairListValueTwo.GetIndex();
    VCOState.pairSync = pairSync;

    // Voice: FM
    VCOState.fmAlogrithm = fmAlgorithmValue.Get();
    Ui2StateFM(&(VCOState.fmOpSettings[0]), fmOp0Controls);
    Ui2StateFM(&(VCOState.fmOpSettings[1]), fmOp1Controls);
    Ui2StateFM(&(VCOState.fmOpSettings[2]), fmOp2Controls);
    Ui2StateFM(&(VCOState.fmOpSettings[3]), fmOp3Controls);

    // Voice: Pluck
    VCOState.pluckMode = pluckListValue.GetIndex();

    // Voice: Bass Drum
    VCOState.bassAccent = bassAccentValue.Get();

    // Voice: Rings
    VCOState.ringPoly = (ringPolyType) ringPolyValue.GetIndex();
    VCOState.ringModel = (ringModelType) ringModelValue.GetIndex();
    VCOState.ringFx = (ringFxType) ringFxValue.GetIndex();
    VCOState.ringNormal = (ringNormalType) ringNormalValue.GetIndex();

    // Attenuverts
    VCOState.p1Attenuvert = attenuvertValueP1.Get();
    VCOState.p2Attenuvert = attenuvertValueP2.Get();
    VCOState.p3Attenuvert = attenuvertValueP3.Get();
    VCOState.p4Attenuvert = attenuvertValueP4.Get();

    // Envelope
    VCOState.useOutputEnvelope = useOutputEnvelope;
    VCOState.outputEnvValues.attack = envelopeValueA.Get();
    VCOState.outputEnvValues.decay = envelopeValueD.Get();
    VCOState.outputEnvValues.release = envelopeValueR.Get();
    VCOState.outputEnvValues.sustain = envelopeValueS.Get() / 100.f;

    // octave -> VCOState.octave = ???
    // 1      -> 0 Volts = 440Hz / 8
    // 2      -> 0 Volts = 440Hz / 4
    // 3      -> 0 Volts = 440Hz / 2
    // 4      -> 0 Volts = 440Hz
    // 5      -> 0 Volts = 440Hz * 2 
    // 6      -> 0 Volts = 440Hz * 4
    int ioct = octiveValue.Get();
    VCOState.octInt = ioct;
    float foct = 0.125f;
    for (; ioct > 1; ioct--)
        foct *= 2.0;
    VCOState.octave = foct;

    // Quantize
    VCOState.quantize = quantize;

    // Gain
    VCOState.gain = overallGain.Get();

    // Offsets
    VCOState.voctOffset = voctOffset;
    VCOState.p1Offset = p1Offset;
    VCOState.p2Offset = p2Offset;
    VCOState.p3Offset = p3Offset;
    VCOState.p4Offset = p4Offset;

    return(VCOState);
}

// unipolar flip with correction
float u_flip(float in, float adj) {
    return (1.0f - in - adj);
}

const float LEFT_DEADZONE_WIDTH = 0.05;
const float LEFT_DEADZONE_HALF_WIDTH = LEFT_DEADZONE_WIDTH / 2.0f;
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
    if (raw <= LEFT_DEADZONE_HALF_WIDTH)
        return 0.f;

    // positive
    return (raw - LEFT_DEADZONE_HALF_WIDTH) / (1.0f - LEFT_DEADZONE_HALF_WIDTH);
}

float cv_to_freq(float volts, float octave) {
	// convert to freq
	float freq = 440.0f * octave * exp2f(volts);
	return freq;
}

// Round to chromatic note
float quantizeFreq(float raw) {
    float quant = raw * 12.0f;
    int iquant = quant; // truncate
    return ((float) iquant / 12.0f);
}

void Normalize(float *vals, int size, float max_total) {
    float total = 0.0f;
    for (int i=0; i<size; i++)
        total += vals[i];
    float adj = max_total / total;
    for (int i=0; i<size; i++)
        vals[i] *= adj;
}

// generate chord from select parameter
// chords supported:
// 0: just the root (1)
// 1: Major         (1-3-5)
// 2: Major6        (1-3-5-6)
// 3: Major7        (1-3-5-m7)
// 4: maj7          (1-3-5-7)
// 5: minor         (1-m3-5)
// 6: minor6        (1-m3-5-6)
// 7: minor7        (1-m3-5-m7)
// 8: dim           (1-m3-m5)
// 9: augmented     (1-3-m6)
// 10: sus4         (1-4-5)
//
const float semi = 1.059463094;
const float min_third = semi * semi * semi;
const float maj_third = min_third * semi;
const float fourth = maj_third * semi;
const float min_fifth = fourth * semi;
const float maj_fifth = min_fifth * semi;
const float min_sixth = maj_fifth * semi;
const float maj_sixth = min_sixth * semi;
const float min_seventh = maj_sixth * semi;
const float maj_seventh = min_seventh * semi;
const float maj_ninth = 2.0f * semi * semi;
void Chord2Freq(float select, float root_freq, float *nonroot_freq, float *mute) {
    int iselect = (int) (select * 11.0f);
    nonroot_freq[2] = nonroot_freq[1] = nonroot_freq[0] = root_freq;
    mute[2] = mute[1] = mute[0] = 1.0f;
    switch (iselect) {
        case 0: // root
            mute[2] = mute[1] = mute[0] = 0.0f;
            break;
        case 1: // Major
            mute[2] = 0.0f;
            nonroot_freq[0] = maj_third * root_freq;
            nonroot_freq[1] = maj_fifth * root_freq;
            break;
        case 2: // 6
            nonroot_freq[0] = maj_third * root_freq;
            nonroot_freq[1] = maj_fifth * root_freq;
            nonroot_freq[2] = maj_sixth * root_freq;
            break;
        case 3: // 7
            nonroot_freq[0] = maj_third * root_freq;
            nonroot_freq[1] = maj_fifth * root_freq;
            nonroot_freq[2] = min_seventh * root_freq;
            break;
        case 4: // maj7
            nonroot_freq[0] = maj_third * root_freq;
            nonroot_freq[1] = maj_fifth * root_freq;
            nonroot_freq[2] = maj_seventh * root_freq;
            break;
        case 5: // Minor
            mute[2] = 0.0f;
            nonroot_freq[0] = min_third * root_freq;
            nonroot_freq[1] = maj_fifth * root_freq;
            break;
        case 6: // min6
            nonroot_freq[0] = min_third * root_freq;
            nonroot_freq[1] = maj_fifth * root_freq;
            nonroot_freq[2] = maj_sixth * root_freq;
            break;
        case 7: // min7
            nonroot_freq[0] = min_third * root_freq;
            nonroot_freq[1] = maj_fifth * root_freq;
            nonroot_freq[2] = min_seventh * root_freq;
            break;
        case 8: // dim
            mute[2] = 0.0f;
            nonroot_freq[0] = min_third * root_freq;
            nonroot_freq[1] = min_fifth * root_freq;
            break;
        case 9: // aug
            mute[2] = 0.0f;
            nonroot_freq[0] = maj_third * root_freq;
            nonroot_freq[1] = min_sixth * root_freq;
            break;
        case 10: // sus4
            mute[2] = 0.0f;
            nonroot_freq[0] = fourth * root_freq;
            nonroot_freq[1] = maj_sixth * root_freq;
            break;
    }
}

// select   nonroot_freq[0]  [1]  [2]
//   0                   x    x    x
//   1                  -1    x    x
//   2                   x   -1    x
//   3                  -1    x   -1
//   4                  -1   -1   -1
//   5                  -1   +1   +1
//   6                  +1   -1   +1
//   7                  -2   +1   +2
//   8                  +1   -2   +2
//   9                  +1   +2    x
//  10                  +2   +1    x
void generateInversion(float select, float *nonroot_freq) {
    int iselect = (int) (select * 11.0f);
    switch (iselect) {
        case 0:
            break;
        case 1:
            nonroot_freq[0] /= 2.0;
            break;
        case 2:
            nonroot_freq[1] /= 2.0;
            break;
        case 3:
            nonroot_freq[0] /= 2.0;
            nonroot_freq[2] /= 2.0;
            break;
        case 4:
            nonroot_freq[0] /= 2.0;
            nonroot_freq[1] /= 2.0;
            nonroot_freq[2] /= 2.0;
            break;
        case 5:
            nonroot_freq[0] /= 2.0;
            nonroot_freq[1] *= 2.0;
            nonroot_freq[2] *= 2.0;
            break;
        case 6:
            nonroot_freq[0] *= 2.0;
            nonroot_freq[1] /= 2.0;
            nonroot_freq[2] *= 2.0;
            break;
        case 7:
            nonroot_freq[0] /= 4.0;
            nonroot_freq[1] *= 2.0;
            nonroot_freq[2] *= 4.0;
            break;
        case 8:
            nonroot_freq[0] *= 2.0;
            nonroot_freq[1] /= 4.0;
            nonroot_freq[2] *= 4.0;
            break;
        case 9:
            nonroot_freq[0] *= 2.0;
            nonroot_freq[1] *= 4.0;
            break;
        case 10:
            nonroot_freq[0] *= 4.0;
            nonroot_freq[1] *= 2.0;
            break;
    }
}

ringPolyType old_poly = RING_POLY_ONE;
float ringInput[torus::kMaxBlockSize];
float ringOutput[torus::kMaxBlockSize];
float ringAux[torus::kMaxBlockSize];

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	hw.ProcessAllControls();
	enc.Debounce();
	GenerateUiEvents();
    moduleState &VCOState = ProcessState();
    torus::PerformanceState performance_state; // Rings
    torus::Patch            patch;             // Rings
    bool                    easterEgg;         // Rings

    // Set frequency based on CV_1 (V/OCT) and CV_6 (manual tuning)
    #ifdef REV0P0
    float tune = u2b_middle_deadzone(u_flip(hw.GetAdcValue(CV_6), CV6_ADJ)) * 5.0f ;
    #elif defined(REV0P1)
    float tune = u2b_middle_deadzone(u_flip(hw.GetAdcValue(ADC_9), 0.0f)) * 5.0f ;
    #endif
    float voct = (hw.GetAdcValue(CV_1) + VCOState.voctOffset) * 5.0f ;
    float final_voct = voct + tune;
    if (VCOState.quantize)
        final_voct = quantizeFreq(final_voct);
    float freq = cv_to_freq(final_voct, VCOState.octave);

    // Get P1-P4
    #ifdef REV0P0
    float p1 = u2u_left_deadzone(u_flip(hw.GetAdcValue(ADC_9), 0.0f))  + (hw.GetAdcValue(CV_2) + VCOState.p1Offset) * VCOState.p1Attenuvert;
    #elif defined(REV0P1)
    float p1 = u2u_left_deadzone(u_flip(hw.GetAdcValue(CV_6), 0.0f))  + (hw.GetAdcValue(CV_2) + VCOState.p1Offset) * VCOState.p1Attenuvert;
    #endif
    float p2 = u2u_left_deadzone(u_flip(hw.GetAdcValue(ADC_10), 0.0f)) + (hw.GetAdcValue(CV_3) + VCOState.p2Offset) * VCOState.p2Attenuvert;
    float p3 = u2u_left_deadzone(u_flip(hw.GetAdcValue(ADC_11), 0.0f)) + (hw.GetAdcValue(CV_4) + VCOState.p3Offset) * VCOState.p3Attenuvert;
    float p4 = u2u_left_deadzone(u_flip(hw.GetAdcValue(ADC_12), 0.0f)) + (hw.GetAdcValue(CV_5) + VCOState.p4Offset) * VCOState.p4Attenuvert;

    // some voices use a gate/trigger
    bool gate = hw.gate_in_1.State();
    #ifdef REV0P1
        gateSwitch.Debounce();
        gate = gate || gateSwitch.Pressed();
        gateLed.Set(gate ? 1.0f : 0.0f);
        gateLed.Update();
    #endif
    bool trigger = gate && !gate_last;
    gate_last = gate;
    float ftrigger = trigger ? 1.0f : 0.0f;

    // some voices can use an output envelope
	env.SetTime(ADSR_SEG_ATTACK, VCOState.outputEnvValues.attack);
    env.SetTime(ADSR_SEG_DECAY, VCOState.outputEnvValues.decay);
	env.SetTime(ADSR_SEG_RELEASE, VCOState.outputEnvValues.release);
    env.SetSustainLevel(VCOState.outputEnvValues.sustain);
    float env_level = env.Process(gate);

    float mix1 = 0.0, mix2 = 1.0;
    float mute[3];
    for (int i=0; i<3; i++) mute[i] = 0.0f;
	float out1 = 0.0, out2 = 0.0;
	if (VCOState.voice == MODE_PAIR) {
        // p1: out1 parameter
        // p2: out2 parameter
        // p3: mix for out2
        // p4: freq shift for out2
   
        float freq2 = (1.0f + p4 * 0.5f) * freq;
        mix2 = p3;
        mix1 = (1 - p3);

		osc1.SetFreq(freq);
		osc2.SetFreq(freq2);

		vs_osc1.SetWaveshape(VCOState.pairWaveform[0] == PAIR_SQUARE ? 1.0f : 0.0f);
		vs_osc2.SetWaveshape(VCOState.pairWaveform[1] == PAIR_SQUARE ? 1.0f : 0.0f);
		vs_osc1.SetPW(p1);
		vs_osc2.SetPW(p2);
		vs_osc1.SetFreq(freq);
		vs_osc2.SetFreq(freq);
		vs_osc1.SetSyncFreq(freq);
		vs_osc2.SetSyncFreq(freq2);
        vs_osc2.SetSync(VCOState.pairSync);

        if (!VCOState.useOutputEnvelope)
	        env_level = 1.0f;
    }
    if (VCOState.voice == MODE_HARMONIC) {
        // p1: index of largest harmonic
        // p2: index of 2nd largest harmonic
        // p3: "Q" of harmonics (0 = narrow, 1 = wide)
        // p4: freq shift
        float freq1 = (1.0f + p4) * freq;
        int main_harmonic = (int) (((float) NUM_HARMONICS) * std::fmin(p1, 1.0f));
        int next_harmonic = (int) (((float) NUM_HARMONICS) * std::fmin(p2, 1.0f)); // left-most: none
        float main_peak = 0.9;
        float next_peak = 0.7;
        float q = fclamp(p3, 0.0f, 1.0f);

        harm_osc.SetFreq(freq1);

        float vals[NUM_HARMONICS];
        for (int i=0; i<NUM_HARMONICS; i++)
            vals[i] = 0.0f;

        vals[main_harmonic] += main_peak;
        float factor = main_peak;
        for (int d=1; d<NUM_HARMONICS; d++) {
            int dplus = main_harmonic + d;
            int dminus = main_harmonic - d;
            if ((dplus >= NUM_HARMONICS) && (dminus < 0))
                break;
            factor *= q;
            if (dplus < NUM_HARMONICS)
                vals[dplus] += factor;
            if (dminus >= 0)
                vals[dminus] += factor;
        }

        if (next_harmonic >= 0) {
            factor = next_peak;
            vals[next_harmonic] += next_peak;
            for (int d=1; d<NUM_HARMONICS; d++) {
                int dplus = next_harmonic + d;
                int dminus = next_harmonic - d;
                if ((dplus >= NUM_HARMONICS) && (dminus < 0))
                    break;
                factor *= q;
                if (dplus < NUM_HARMONICS)
                    vals[dplus] += factor;
                if (dminus >= 0)
                    vals[dminus] += factor;
            }
        }

        Normalize(vals, 12, 0.9f);
        for (int i=0; i<NUM_HARMONICS; i++)
            harm_osc.SetSingleAmp(vals[i], i);

        if (!VCOState.useOutputEnvelope)
	        env_level = 1.0f;
    }
    if (VCOState.voice == MODE_FM) {
        // p1: index of op1
        // p2: index of op2
        // p3: index of op3
        // p4: fedback of op3
        float p[4];
        p[0] = 1.f;
        p[1] = p1;
        p[2] = p2;
        p[3] = p3;
        fm_osc.SetAlgorithm(VCOState.fmAlogrithm);
        for (int op = 0; op < 4; op++) {
            fm_env[op].SetTime(ADSR_SEG_ATTACK, VCOState.fmOpSettings[op].opEnvValues.attack);
            fm_env[op].SetTime(ADSR_SEG_DECAY, VCOState.fmOpSettings[op].opEnvValues.decay);
	        fm_env[op].SetTime(ADSR_SEG_RELEASE, VCOState.fmOpSettings[op].opEnvValues.release);
            fm_env[op].SetSustainLevel(VCOState.fmOpSettings[op].opEnvValues.sustain);
            float elevel = fm_env[op].Process(gate);
            float level = VCOState.fmOpSettings[op].useOpEnvelope ? elevel : p[op];
            float index = level * VCOState.fmOpSettings[op].maxIndex / 100.f;
            
            fm_osc.SetFrequency(op, freq * VCOState.fmOpSettings[op].ratioN / VCOState.fmOpSettings[op].ratioD);
            fm_osc.SetIndex(op, index);
        }
        fm_osc.SetFB(p4 * VCOState.fmOpSettings[3].maxFB);
        if (!VCOState.useOutputEnvelope)
	        env_level = 1.0f;
    }
    if (VCOState.voice == MODE_FORMANT) {
        // p1: formant 1 frequency
        // p2: formant 2 frequency ratio
        // p3: shape
        // p4: freq shift
        float freq1 = (1.0f + p4) * freq;
        form_osc.SetFreq(freq1);
        float form_freq = cv_to_freq(p1 * 5.0f, 0.125f);
        form_osc.SetForm1Freq(form_freq);
        form_osc.SetForm2Freq(form_freq * p2 * 4.0);
        form_osc.SetShape(p3 * 2.0f - 1.0f);

        if (!VCOState.useOutputEnvelope)
	        env_level = 1.0f;
    }
    if (VCOState.voice == MODE_STRINGS) {
        // p1: degree of unison
        // p2:
        // p3: filter frequency
        // p4: freq shift
        float shifted = (1.0f + p4) * freq;
        for (int i =0; i<NUM_STRINGS; i++) {
            float sign = 0.0f;
            if (i < (NUM_STRINGS / 2))
                sign = -1.0;
            if (i > (NUM_STRINGS / 2))
                sign = 1.0;

            string_section[i].SetFreq(shifted * (1.0f + (((float) i) * sign * p1 * 0.005f)));
        }
        string_filter.SetFreq(std::fmin(shifted * (1.0f + (20.25f * p3 - 0.25f)), 22000.0f));
        string_filter.SetDrive(0.25f);
        string_filter.SetRes(0.0f);
        
        if (!VCOState.useOutputEnvelope)
	        env_level = 1.0f;
    }
    if (VCOState.voice == MODE_CHORD) {
        // p1: chord index
        // p2: inverstion index
        // p3: voice select
        // p4: freq shift
        float shifted = (1.0f + p4) * freq;
        chord_voice[0].SetFreq(shifted);
        float nonroot_freq[3];
        Chord2Freq(p1, shifted, nonroot_freq, mute);
        generateInversion(p2, nonroot_freq);
        for (int i=0; i<3; i++)
            chord_voice[i+1].SetFreq(nonroot_freq[i]);

        uint8_t waveform;
        float pw = 0.5f;
        switch ((int) (p3 * 6.0)) {
            case 0:
                waveform = Oscillator::WAVE_SIN;
                break;
            case 1:
                waveform = Oscillator::WAVE_POLYBLEP_SAW;
                break;
            case 2:
                waveform = Oscillator::WAVE_POLYBLEP_TRI;
                break;
            case 3:
                waveform = Oscillator::WAVE_POLYBLEP_SQUARE;
                pw = 0.5f;
                break;
            case 4:
                waveform = Oscillator::WAVE_POLYBLEP_SQUARE;
                pw = 0.25f;
                break;
            default:
                waveform = Oscillator::WAVE_POLYBLEP_SQUARE;
                pw = 0.125f;
                break;
        }
        for (int i=0; i<4; i++) {
            chord_voice[i].SetWaveform(waveform);
            chord_voice[i].SetPw(pw);
        }

        if (!VCOState.useOutputEnvelope)
	        env_level = 1.0f;
    }
    if (VCOState.voice == MODE_PLUCK) {
        // p1: Decay
        // p2: Damping
        // p4: freq shift
        float freq1 = (1.0f + p4) * freq;
        pluck.SetDecay(p1);
        pluck.SetDamp(p2);
        pluck.SetFreq(freq1);
        pluck.SetMode(VCOState.pluckMode);
    }
    if (VCOState.voice == MODE_BASS) {
        bassDrum.SetFreq(freq);
        bassDrum.SetTone(p1);
        bassDrum.SetDecay(p2);
        bassDrum.SetAttackFmAmount(p3);
        bassDrum.SetSelfFmAmount(p4);
        bassDrum.SetAccent(VCOState.bassAccent);
    }
    if (VCOState.voice == MODE_SNARE) {
        snareDrum.SetFreq(freq);
        snareDrum.SetTone(p1);
        snareDrum.SetDecay(p2);
        snareDrum.SetAccent(p3);
        snareDrum.SetSnappy(p4);
    }
    if (VCOState.voice == MODE_HIHAT) {
        hiHat.SetFreq(freq);
        hiHat.SetTone(p1);
        hiHat.SetDecay(p2);
        hiHat.SetAccent(p3);
        hiHat.SetNoisiness(p4);
    }
    if (VCOState.voice == MODE_RINGS) {
        // Rings (from Mutable Instruments)
        // p1: structure
        // p2: damping
        // p3: brightness
        // p4: position

        CONSTRAIN(p1, 0.0f, 1.0f);
        CONSTRAIN(p2, 0.0f, 1.0f);
        CONSTRAIN(p3, 0.0f, 1.0f);
        CONSTRAIN(p4, 0.0f, 1.0f);
        patch.structure = p1;
        patch.damping = p2;
        patch.brightness = p3;
        patch.position = p4;

        performance_state.strum = trigger;
        performance_state.internal_exciter = true;
        performance_state.internal_note = (VCOState.ringNormal == RING_NORMAL_NOTE);
        performance_state.internal_strum = (VCOState.ringNormal == RING_NORMAL_STRUM);
        performance_state.tonic = (tune + 1.0f) * 12.0f;
        performance_state.note = performance_state.internal_note ? 0.0f : (voct * 12.0f);
        performance_state.fm = 0.0f;

        float chord = p1;
        chord *= static_cast<float>(torus::kNumChords - 1);
        CONSTRAIN(chord, 0, torus::kNumChords - 1);
        performance_state.chord = chord;

        // polyphony setting
        if (old_poly != VCOState.ringPoly) {
            part.set_polyphony(0x01 << VCOState.ringPoly);
            string_synth.set_polyphony(0x01 << VCOState.ringPoly);
        }
        old_poly = VCOState.ringPoly;
    
        // model settings
        easterEgg = (VCOState.ringModel == RING_MODEL_EGG);
        if (!easterEgg)
            part.set_model((torus::ResonatorModel) VCOState.ringModel);
        string_synth.set_fx((torus::FxType) VCOState.ringFx);

        // inputs/outputs
        for(size_t i = 0; i < size; ++i) {
                ringInput[i] = 0.0f;
        }
        if(easterEgg) {
            strummer.Process(NULL, size, &performance_state);
            string_synth.Process(performance_state, patch, ringInput, ringOutput, ringAux, size);
        } else {
            strummer.Process(ringInput, size, &performance_state);
            part.Process(performance_state, patch, ringInput, ringOutput, ringAux, size);
        }
    }

	for (size_t i = 0; i < size; i++)
	{
        if (VCOState.voice == MODE_PAIR) {
            if (VCOState.pairWaveform[0] == PAIR_SINE)
			    out1 = osc1.Process() * env_level;
		    else
			    out1 = vs_osc1.Process() * env_level;

		    if (VCOState.pairWaveform[1] == PAIR_SINE)
			    out2 = osc2.Process() * env_level;
		    else
			    out2 = vs_osc2.Process() * env_level;
    
            out2 = out1 * mix1 + out2 * mix2;

            // this voice tends to be louder than the rest
            out1 *= 0.5f;
            out2 *= 0.5f;
        }
        if (VCOState.voice == MODE_HARMONIC) {
            out1 = harm_osc.Process() * env_level;
            out2 = white.Process();
        }
        if (VCOState.voice == MODE_FM) {
            out1 = fm_osc.Process() * env_level;
            out2 = white.Process();
        }
        if (VCOState.voice == MODE_FORMANT) {
            out1 = form_osc.Process() * env_level;
            out2 = white.Process();
        }
        if (VCOState.voice == MODE_STRINGS) {
            float section = 0;
            for (int i=0; i<NUM_STRINGS; i++)
                section += string_section[i].Process();
            string_filter.Process(section);
            float filtered = string_filter.Low();
            out1 = filtered * env_level;
            out2 = section;
        }
        if (VCOState.voice == MODE_CHORD) {
            float chord = 0.0f;
            chord += chord_voice[0].Process();
            for (int i=0; i<3; i++) {
                chord += chord_voice[i+1].Process() * mute[i];
            }
            out1 = chord * env_level;
            out2 = white.Process();
        }
        if (VCOState.voice == MODE_PLUCK) {
            out1 = pluck.Process(ftrigger);
            ftrigger = 0.0f;
            out2 = white.Process();
        }
        if (VCOState.voice == MODE_BASS) {
            out1 = bassDrum.Process(trigger);
            trigger = false;
            out2 = white.Process();
        }
        if (VCOState.voice == MODE_SNARE) {
            out1 = snareDrum.Process(trigger);
            trigger = false;
            out2 = white.Process();
        }
        if (VCOState.voice == MODE_HIHAT) {
            out1 = hiHat.Process(trigger);
            trigger = false;
            out2 = white.Process();
        }
        if (VCOState.voice == MODE_RINGS) {
            out1 = ringOutput[i];
            out2 = ringAux[i];
            trigger = false;
        }

		OUT_L[i] = out1 * VCOState.gain;
		OUT_R[i] = out2 * VCOState.gain;
	}
}

int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

	float sample_rate = hw.AudioSampleRate();
	float block_size = hw.AudioBlockSize();

	// Set parameters for envelope
    env.Init(sample_rate/block_size);

    // Sine oscillators
	osc1.Init(sample_rate);
    osc1.SetWaveform(osc1.WAVE_SIN);
    osc1.SetAmp(1.0);
	
	osc2.Init(sample_rate);
	osc2.SetWaveform(osc2.WAVE_SIN);
    osc2.SetAmp(1.0);

	// Square/Triangle/Saw oscillators
	vs_osc1.Init(sample_rate);
	vs_osc1.SetWaveshape(0.f);
	vs_osc1.SetPW(0.5f);
	vs_osc1.SetSyncFreq(440.f);
	vs_osc2.Init(sample_rate);
	vs_osc2.SetWaveshape(0.f);
	vs_osc2.SetPW(0.5f);
	vs_osc2.SetFreq(440.f);
	vs_osc2.SetSyncFreq(440.f);

	// Harmonic oscillator
	harm_osc.Init(sample_rate);
    harm_osc.SetFirstHarmIdx(1);

    // FM oscillator
    fm_osc.Init(sample_rate);
    for (int i=0; i<4; i++)
        fm_env[i].Init(sample_rate/block_size);

    // Formant oscillator
    form_osc.Init(sample_rate);

    // String section
    for (int i=0; i<NUM_STRINGS; i++) {
        string_section[i].Init(sample_rate);
        string_section[i].SetAmp(1.0f / (float) NUM_STRINGS);
        string_section[i].SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
        string_section[i].PhaseAdd(rand() / (float) RAND_MAX);
    }
    string_filter.Init(sample_rate);

    // Chord oscillators
    for (int i=0; i<4; i++) {
        chord_voice[i].Init(sample_rate);
        chord_voice[i].SetAmp(0.2f);
        chord_voice[i].SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
    }

    // Plucked oscillator
    float init_buff[256]; // buffer for Pluck impulse
    pluck.Init(sample_rate, init_buff, 256, PLUCK_MODE_RECURSIVE);
    pluck.SetAmp(1.0f);

    // Drums
    bassDrum.Init(sample_rate);
    snareDrum.Init(sample_rate);
    hiHat.Init(sample_rate);

    // Rings
    torus::InitResources();
    strummer.Init(0.01f, sample_rate / block_size);
    part.Init(ring_reverb_buffer);
    string_synth.Init(ring_reverb_buffer);

    // Noise
    white.Init();

    // Gate button and LED
    #ifdef REV0P1
    gateLed.Init(hw.D10, false);
    gateSwitch.Init(hw.D1, 0.0f, Switch::TYPE_MOMENTARY, Switch::POLARITY_INVERTED, Switch::PULL_NONE);
    #endif

	// factory initialize module state if needed
    moduleState defState;
	initFactoryState(&defState);
    SavedState.Init(defState);

    // re-initialize if version number changed
    moduleState &writtenState = SavedState.GetSettings();
    if (writtenState.version != defState.version) {
        SavedState.RestoreDefaults();
    }
    moduleState &currentState = SavedState.GetSettings();

    // offsets
    voctOffsetItem.Init(&voctOffset, CV_1, "V/OCT");
    p1OffsetItem.Init(&p1Offset, CV_2, "P1 CV");
    p2OffsetItem.Init(&p2Offset, CV_3, "P2 CV");
    p3OffsetItem.Init(&p3Offset, CV_4, "P3 CV");
    p4OffsetItem.Init(&p4Offset, CV_5, "P4 CV");

	// set up display
	MyOledDisplay::Config disp_cfg;
	display.Init(disp_cfg);

	// start UI
	InitUi();
	InitUiPages();
    State2Ui(currentState);
    //currentState.p3Attenuvert = 0.3f;
    attenuvertValueP3.Set(currentState.p3Attenuvert);
    
    hw.StartAudio(AudioCallback);
	ui.OpenPage(mainMenu);

    uint32_t last_save = 0;

	while(1)
    {
        ui.Process();

        // try to update flash if one second has passed (or on roll-over)
        uint32_t now = System::GetNow();
        if ((now > (last_save + 1000)) || (now < last_save)) {
            SavedState.Save(); // should only write flash if state has changed
            last_save = now;
        }
    }
}
