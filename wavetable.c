#include <math.h>
#include "macro-utils.h"
#include "signal-processors.h"
#define WAVETABLE_SIZE 256

typedef struct WavetableSample {
    float val;
    float offset;
} WavetableSample;

void scan_wavetable(TDomain wavetable, float wavelength, WavetableSample *sample)
{
    sample->offset += (float)wavetable.length / wavelength;
    if (sample->offset >= wavetable.length) {
        sample->offset -= wavetable.length;
    }
    int integerPart = (int)sample->offset;
    int decimalPart = integerPart - sample->offset;

    float lowerBound = wavetable.samples[integerPart];
    float upperBound = integerPart > 255? wavetable.samples[0] : wavetable.samples[integerPart +1];

    sample->val = lowerBound + (decimalPart * (upperBound - lowerBound));
}

float _sinWavetable[WAVETABLE_SIZE];
float _sawWavetable[WAVETABLE_SIZE];
float _pulWavetable[WAVETABLE_SIZE];
float _triWavetable[WAVETABLE_SIZE];

TDomain sinWavetable = {WAVETABLE_SIZE, _sinWavetable};
TDomain sawWavetable = {WAVETABLE_SIZE, _sawWavetable};
TDomain pulWavetable = {WAVETABLE_SIZE, _pulWavetable};
TDomain triWavetable = {WAVETABLE_SIZE, _triWavetable};

float _dftSinRe[WAVETABLE_SIZE/2];
float _dftSinIm[WAVETABLE_SIZE/2];
float _dftSawRe[WAVETABLE_SIZE/2];
float _dftSawIm[WAVETABLE_SIZE/2];
float _dftPulRe[WAVETABLE_SIZE/2];
float _dftPulIm[WAVETABLE_SIZE/2];
float _dftTriRe[WAVETABLE_SIZE/2];
float _dftTriIm[WAVETABLE_SIZE/2];

FDomain dftSin;
FDomain dftSaw;
FDomain dftPul;
FDomain dftTri;

// These wavetables are to be generated
// from an inverse Fourier transform.
float _idftSinWavetable[WAVETABLE_SIZE];
float _idftSawWavetable[WAVETABLE_SIZE];
float _idftPulWavetable[WAVETABLE_SIZE];
float _idftTriWavetable[WAVETABLE_SIZE];

TDomain idftSinWavetable = {WAVETABLE_SIZE, _idftSinWavetable};
TDomain idftSawWavetable = {WAVETABLE_SIZE, _idftPulWavetable};
TDomain idftPulWavetable = {WAVETABLE_SIZE, _idftSawWavetable};
TDomain idftTriWavetable = {WAVETABLE_SIZE, _idftTriWavetable};

void init_wavetables()
{
    loop(x, sinWavetable.length) {
        sinWavetable.samples[x] = sin(((float)x / sinWavetable.length) * TWO_PI) * 0.5f;
    }

    loop(x, sawWavetable.length) {
        sawWavetable.samples[x] = ((float)x / sinWavetable.length) - 0.5f;
    }

    loop(x, pulWavetable.length /2) {
        pulWavetable.samples[x] = -0.5f;
    }
    oloop(x, pulWavetable.length /2, pulWavetable.length) {
        pulWavetable.samples[x] = 0.5f;
    }

    loop(x, triWavetable.length /2) {
        triWavetable.samples[x] = ((float)x / triWavetable.length *2) - 0.5f;
    }
    oloop(x, triWavetable.length/2, triWavetable.length) {
        triWavetable.samples[x] = 0.5f - (float)(x - triWavetable.length /2) / triWavetable.length *2;
    }


    dft(dftSin, sinWavetable);
    dft(dftSaw, sawWavetable);
    dft(dftPul, pulWavetable);
    dft(dftTri, triWavetable);

    idft(idftSinWavetable, dftSin);
    idft(idftSawWavetable, dftSaw);
    idft(idftPulWavetable, dftPul);
    idft(idftTriWavetable, dftTri);
}
