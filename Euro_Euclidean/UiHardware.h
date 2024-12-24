#pragma once

#include "daisy_patch_sm.h"
#include "daisy.h"

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

extern DaisyPatchSM 			hw;

/** To make it easier to refer to specific buttons in the 
 *  code, we use this enum to give each button ID a specific name.
 */
enum ButtonIds
{
    bttnEncoder = 0,
    bttnSelA = 1,
    bttnSelB = 2,
    bttnSelC = 3,
    bttnSelD = 4,
    NUM_BUTTONS
};

/** To make it easier to refer to specific encoders in the 
 *  code, we use this enum to give each encoder ID a specific name.
 */
enum EncoderIds
{
    encoderMain = 0,
    // We don't have any more encoders on the Patch, but if there were more, you'd add them here
    NUM_ENCODERS
};