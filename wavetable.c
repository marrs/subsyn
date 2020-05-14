#include <math.h>
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

float sinWavetable[WAVETABLE_SIZE];
float sawWavetable[WAVETABLE_SIZE];
float pulWavetable[WAVETABLE_SIZE];
float triWavetable[WAVETABLE_SIZE];

void init_wavetables()
{
    loop(x, WAVETABLE_SIZE) {
        sinWavetable[x] = sin(((float)x / WAVETABLE_SIZE) * TWO_PI) * 0.5f;
    }

    loop(x, WAVETABLE_SIZE) {
        sawWavetable[x] = ((float)x / WAVETABLE_SIZE) - 0.5f;
    }

    loop(x, WAVETABLE_SIZE/2) {
        pulWavetable[x] = -0.5f;
    }
    oloop(x, WAVETABLE_SIZE/2, WAVETABLE_SIZE) {
        pulWavetable[x] = 0.5f;
    }

    loop(x, WAVETABLE_SIZE/2) {
        triWavetable[x] = ((float)x / WAVETABLE_SIZE*2) - 0.5f;
    }
    oloop(x, WAVETABLE_SIZE/2, WAVETABLE_SIZE) {
        triWavetable[x] = 0.5f - (float)(x-WAVETABLE_SIZE /2) / WAVETABLE_SIZE*2;
    }
}
