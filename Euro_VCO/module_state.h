//
// module state

#ifndef MODULE_VCOSTATE_H_
#define MODULE_VCOSTATE_H_

namespace VCO {

enum voiceType {
	MODE_PAIR = 0,
	MODE_HARMONIC = 1,
    MODE_FM = 2,
    MODE_FORMANT = 3,
    MODE_STRINGS = 4,
	MODE_CHORD = 5,
	MODE_PLUCK = 6,
	MODE_BASS = 7,
	MODE_SNARE = 8,
	MODE_HIHAT = 9,
    MODE_RINGS = 10,
};

enum pairWaveformType {
    PAIR_SINE = 0,
    PAIR_SQUARE = 1,
    PAIR_TRIANGLE = 2,
};

enum ringPolyType {
    RING_POLY_ONE = 0,
    RING_POLY_TWO = 1,
    RING_POLY_FOUR = 2,
};

enum ringModelType {
    RING_MODEL_MODAL = 0,
    RING_MODEL_SYMPATHETIC = 1,
    RING_MODEL_INHARMONIC = 2,
    RING_MODEL_FM = 3,
    RING_MODEL_QUANTIZED = 4,
    RING_MODEL_REVERB = 5,
    RING_MODEL_EGG = 6,
};

// For Easter Egg
enum ringFxType {
    RING_FX_FORMANT = 6,
    RING_FX_CHORUS = 7,
    RING_FX_REVERB = 8,
    RING_FX_FORMANT2 = 9,
    RING_FX_ENSEMBLE = 10,
    RING_FX_REVERB2 = 11,
};

enum ringNormalType {
    RING_NORMAL_NONE = 0,
    RING_NORMAL_NOTE = 1,
    RING_NORMAL_STRUM = 2,
};

struct envelopeState {
    float               attack;
    float               decay;
    int                 sustain;
    float               release;

    bool operator== (const envelopeState &rhs) const {
    return ((this->attack == rhs.attack) &&
            (this->decay == rhs.decay) &&
            (this->sustain == rhs.sustain) &&
            (this->release == rhs.release));
    };
};

struct fmOpState {
    int                 ratioN;
    int                 ratioD;
    int                 maxIndex;
    int                 maxFB;
    bool                useOpEnvelope;
    envelopeState       opEnvValues;

    bool operator== (const fmOpState &rhs) const {
    return ((this->ratioN == rhs.ratioN) &&
            (this->ratioD == rhs.ratioD) &&
            (this->maxIndex == rhs.maxIndex) &&
            (this->maxFB == rhs.maxFB) &&
            (this->useOpEnvelope == rhs.useOpEnvelope) &&
            (this->opEnvValues == rhs.opEnvValues));
    };
};

struct moduleState
{
    // Version
    int                 version;

    // Voice
    voiceType	        voice;

    // Voice:Pair
    pairWaveformType    pairWaveform[2];
    bool                pairSync;

    // Voice:FM
    int                 fmAlogrithm;
    fmOpState           fmOpSettings[4];

    // Voice:Plucked
    int32_t             pluckMode;

    // Voice:Bass Drum
    float               bassAccent;

    // Voice: Rings
    ringPolyType        ringPoly;
    ringModelType       ringModel;
    ringFxType          ringFx;
    ringNormalType      ringNormal;

    // Attenuvert
    float               p1Attenuvert;
    float               p2Attenuvert;
    float               p3Attenuvert;
    float               p4Attenuvert;

    // Envelope
    bool                useOutputEnvelope;
    envelopeState       outputEnvValues;

    // Quantize
    bool                quantize;

    // Octave
    uint32_t            octInt;
    float               octave;

    // Gain
    float               gain;

    // Offsets
    float               voctOffset;
    float               p1Offset;
    float               p2Offset;
    float               p3Offset;
    float               p4Offset;

    bool operator!= (const moduleState &rhs) const {
    return !((this->version == rhs.version) &&
             (this->voice == rhs.voice) &&
             (this->pairWaveform[0] == rhs.pairWaveform[0]) &&
             (this->pairWaveform[1] == rhs.pairWaveform[1]) &&
             (this->pairSync == rhs.pairSync) &&
             (this->useOutputEnvelope == rhs.useOutputEnvelope) &&
             (this->fmAlogrithm == rhs.fmAlogrithm) &&
             (this->fmOpSettings[0] == rhs.fmOpSettings[0]) &&
             (this->fmOpSettings[1] == rhs.fmOpSettings[1]) &&
             (this->fmOpSettings[2] == rhs.fmOpSettings[2]) &&
             (this->fmOpSettings[3] == rhs.fmOpSettings[3]) &&
             (this->pluckMode == rhs.pluckMode) &&
             (this->bassAccent == rhs.bassAccent) &&
             (this->ringPoly == rhs.ringPoly) &&
             (this->ringModel == rhs.ringModel) &&
             (this->ringFx == rhs.ringFx) &&
             (this->ringNormal == rhs.ringNormal) &&
             (this->p1Attenuvert == rhs.p1Attenuvert) &&
             (this->p2Attenuvert == rhs.p2Attenuvert) &&
             (this->p3Attenuvert == rhs.p3Attenuvert) &&
             (this->p4Attenuvert == rhs.p4Attenuvert) &&
             (this->outputEnvValues == rhs.outputEnvValues) &&
             (this->octInt == rhs.octInt) &&
             (this->gain == rhs.gain) &&
             (this->voctOffset == rhs.voctOffset) &&
             (this->p1Offset == rhs.p1Offset) &&
             (this->p2Offset == rhs.p2Offset) &&
             (this->p3Offset == rhs.p3Offset) &&
             (this->p4Offset == rhs.p4Offset));
    };
};


} // namespace VCO

#endif // MODULE_VCOSTATE_H_