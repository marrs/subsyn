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
float plsWavetable[WAVETABLE_SIZE];
float triWavetable[WAVETABLE_SIZE];

void init_wavetables()
{
    loop(x, WAVETABLE_SIZE) {
        sawWavetable[x] = (float)x / WAVETABLE_SIZE;
    }

    loop(x, WAVETABLE_SIZE/2) {
        plsWavetable[x] = 0.0f;
    }
    oloop(x, WAVETABLE_SIZE/2, WAVETABLE_SIZE) {
        plsWavetable[x] = 1.0f;
    }

    loop(x, WAVETABLE_SIZE/2) {
        triWavetable[x] = (float)x / WAVETABLE_SIZE*2;
    }
    oloop(x, WAVETABLE_SIZE/2, WAVETABLE_SIZE) {
        triWavetable[x] = 1.0f - (float)(x-WAVETABLE_SIZE /2) / WAVETABLE_SIZE*2;
    }

    loop(x, WAVETABLE_SIZE) {
        printf("x%d: %f\n", x, triWavetable[x]);
    }

}
