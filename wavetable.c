#define WAVETABLE_SIZE 256

typedef struct WavetableSample {
    float val;
    float offset;
} WavetableSample;

void scan_wavetable(float *wavetable, float wavelength, WavetableSample *sample)
{
    sample->offset += (float)WAVETABLE_SIZE / wavelength;
    if (sample->offset >= 256) {
        sample->offset -= 256;
    }
    int integerPart = (int)sample->offset;
    int decimalPart = integerPart - sample->offset;

    float lowerBound = wavetable[integerPart];
    float upperBound = integerPart > 255? wavetable[0] : wavetable[integerPart +1];

    sample->val = lowerBound + (decimalPart * (upperBound - lowerBound));
}

float sawWavetable[WAVETABLE_SIZE];
float pulseWavetable[WAVETABLE_SIZE];

void init_wavetables()
{
    for (int x = 0; x < WAVETABLE_SIZE; ++x) {
        sawWavetable[x] = (float)x / WAVETABLE_SIZE;
    }

    for (int x = 0; x < WAVETABLE_SIZE /2; ++x) {
        pulseWavetable[x] = 0.0f;
    }
    for (int x = WAVETABLE_SIZE /2; x < WAVETABLE_SIZE; ++x) {
        pulseWavetable[x] = 1.0f;
    }
}
