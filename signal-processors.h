#ifndef __SIGNAL_PROCESSORS_H__
typedef struct FDomain {
    int length;
    float *re;
    float *im;
} FDomain;

typedef struct TDomain {
    int length;
    float *samples;
} TDomain;

void dft(FDomain fDomain, TDomain signal);
void idft(TDomain signal, FDomain dft);
#define __SIGNAL_PROCESSORS_H__
#endif
