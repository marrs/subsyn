#include <math.h>
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
        fDomain.re[x] = fDomain.re[x] / ((fDomain.length + 1) / 2);
        fDomain.im[x] = fDomain.im[x] / ((fDomain.length + 1) / 2);
    }
}

void idft(TDomain signal, FDomain dft)
{
    loop(x, signal.length) {
        signal.samples[x] = 0.0f;
        loop(xx, dft.length) {
            signal.samples[x] = signal.samples[x] + dft.re[xx] * cos(2 * M_PI * xx * x / signal.length);
            signal.samples[x] = signal.samples[x] + dft.im[xx] * sin(2 * M_PI * xx * x / signal.length);
        }
        signal.samples[x] /= 2;
    }
}
