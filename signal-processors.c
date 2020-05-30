#include <math.h>
#include <stdio.h>
#include "macro-utils.h"
#include "signal-processors.h"

void dft(FDomain fDomain, TDomain signal)
{
    loop(x, fDomain.length) {
        fDomain.re[x] = 0.0f;
        fDomain.im[x] = 0.0f;
        loop(xx, signal.length) {
            fDomain.re[x] = fDomain.re[x] + signal.samples[xx] * cos(2 * M_PI * x * xx / signal.length);
            fDomain.im[x] = fDomain.im[x] + signal.samples[xx] * sin(2 * M_PI * x * xx / signal.length);
        }
        // XXX Is this correct? Divisor can be a half value.
        fDomain.re[x] = fDomain.re[x] / ((fDomain.length + 1) / 2);
        fDomain.im[x] = fDomain.im[x] / ((fDomain.length + 1) / 2);
    }
}

void idft(TDomain *signal, FDomain dft)
{
    loop(x, signal->length) {
        signal->samples[x] = 0.0f;
        loop(xx, dft.length) {
            signal->samples[x] = signal->samples[x] + dft.re[xx] * cos(2 * M_PI * xx * x / signal->length);
            signal->samples[x] = signal->samples[x] + dft.im[xx] * sin(2 * M_PI * xx * x / signal->length);
        }
        signal->samples[x] /= 2;
    }
}

void filter_cliffedge(FDomain *cleanSignal, FDomain *processedSignal, const float freq, const float cutoff)
{
    float cutoffHarmonic = cutoff/freq;
    processedSignal->length = 256/2;
    loop (x, cleanSignal->length) {
        if (x + 1 > cutoffHarmonic) {
            processedSignal->re[x] = processedSignal->im[x] = 0;
        } else {
            processedSignal->re[x] = cleanSignal->re[x];
            processedSignal->im[x] = cleanSignal->im[x];
        }
    }
}
