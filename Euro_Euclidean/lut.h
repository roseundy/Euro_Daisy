#ifndef LUT_H
#define LUT_H

#define LUT_SINE_SIZE 5121

//float DSY_SDRAM_BSS lut_sine[LUT_SINE_SIZE];
float lut_sine[LUT_SINE_SIZE];

void InitLUT() {
    // lut_sine
    for(int i = 0; i < LUT_SINE_SIZE; i++) {
        float t     = (float)i / 4096.f * 2.0 * PI_F;
        lut_sine[i] = sinf(t);
    }
}

float lookup_sin(float x) {
    bool neg = x < 0.0f;
    if (neg)
        x = -x;
    int32_t index = x * 4096.f / (2.0f * PI_F);
    index = index & 4095;
    return neg ? -lut_sine[index] : lut_sine[index];
}

float lookup_cos(float x) {
    return lookup_sin(x + PI_F / 2.0f);
}


#endif // LUT_H
