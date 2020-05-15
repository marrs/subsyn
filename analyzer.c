#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <xcb/xcb.h>
#include "xcb.h"
#include "macro-utils.h"

#include "chart.c"
#include "wavetable.c"

XcbState xcbState;
int gutterSize = 10;

typedef struct FDomain {
    float *re;
    float *im;
    int length;
} FDomain;

typedef struct TDomain {
    float *samples;
    int length;
} TDomain;

void dft(FDomain fDomain, TDomain signal)
{
    int totalPoints = signal.length;

    loop(x, fDomain.length) {
        fDomain.re[x] = 0.0f;
        fDomain.im[x] = 0.0f;
    }

    loop(x, fDomain.length) {
        loop(xx, signal.length) {
            fDomain.re[x] = fDomain.re[x] + signal.samples[xx] * cos(2 * M_PI * x * xx / totalPoints);
            fDomain.im[x] = fDomain.im[x] + signal.samples[xx] * sin(2 * M_PI * x * xx / totalPoints);
        }
        // XXX Is it correct to take the absolute value?
        fDomain.re[x] = fabsf(fDomain.re[x] / ((fDomain.length + 1) / 2));
        fDomain.im[x] = fabsf(fDomain.im[x] / ((fDomain.length + 1) / 2));
    }
}

void plot_tdomain(int row, TDomain signal)
{
    PlotParams params;
    params.height = 128;
    loop(x, signal.length) {
        params.xOffset = gutterSize;
        params.yOffset = (row * 128) + (row * gutterSize) + gutterSize;
        plot_sample(xcbState, x, signal.samples[x] + 0.5, &params);
    }
}

void plot_fdomain(int row, FDomain ft)
{
    PlotParams params;
    params.height = 100;

    loop (x, ft.length) {
        params.xOffset = 2 * gutterSize + WAVETABLE_SIZE;
        params.yOffset = (row * 128) + (row * gutterSize) + gutterSize;
        plot_sample(xcbState, x, ft.re[x], &params);
    }
    loop (x, ft.length) {
        params.xOffset = 3 * gutterSize +  WAVETABLE_SIZE + ft.length;
        params.yOffset = (row * 128) + (row * gutterSize) + gutterSize;
        plot_sample(xcbState, x, ft.im[x], &params);
    }
}

int isShuttingDown = 0;

void sigterm_action(int signum)
{
    isShuttingDown = 1;
}

int main()
{
    xcbState.connection = xcb_connect (NULL, NULL);
    uint32_t xcbvals[2];

    const xcb_setup_t      *setup  = xcb_get_setup(xcbState.connection);
    xcb_screen_iterator_t   iter   = xcb_setup_roots_iterator (setup);
    xcb_screen_t           *screen = iter.data;

    xcbState.fgContext = xcb_generate_id(xcbState.connection);
    xcbvals[0]  = screen->white_pixel;
    xcbvals[1] = 0;

    xcb_create_gc (xcbState.connection,
                   xcbState.fgContext,
                   screen->root,
                   XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES,
                   xcbvals);

    xcbState.bgContext = xcb_generate_id(xcbState.connection);
    xcbvals[0]  = screen->black_pixel;
    xcbvals[1] = 0;

    xcb_create_gc (xcbState.connection,
                   xcbState.bgContext,
                   screen->root,
                   XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES,
                   xcbvals);

    xcbvals[0] = screen->black_pixel;
    xcbvals[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_RELEASE;

    /* Create the window */
    xcbState.window = xcb_generate_id (xcbState.connection);
    xcb_create_window (xcbState.connection,           /* Connection          */
                       XCB_COPY_FROM_PARENT,          /* depth (same as root)*/
                       xcbState.window,               /* window Id           */
                       screen->root,                  /* parent window       */
                       0, 0,                          /* x, y                */
                       gutterSize*3  + WAVETABLE_SIZE*2, // width
                       512,                           /*  height       */
                       10,                            /* border_width        */
                       XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class               */
                       screen->root_visual,           /* visual              */
                       XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
                       xcbvals );

    struct sigaction sigTermHandler;
    sigTermHandler.sa_handler = sigterm_action;
    sigaction(SIGTERM, &sigTermHandler, NULL);

    xcb_map_window (xcbState.connection, xcbState.window);
    xcb_flush (xcbState.connection);

    init_wavetables();

    FDomain dftSin;
    float dftSinRe[127];
    float dftSinIm[127];
    dftSin.re = dftSinRe;
    dftSin.im = dftSinIm;
    dftSin.length = 127;

    FDomain dftSaw;
    float dftSawRe[127];
    float dftSawIm[127];
    dftSaw.re = dftSawRe;
    dftSaw.im = dftSawIm;
    dftSaw.length = 127;

    FDomain dftPul;
    float dftPulRe[127];
    float dftPulIm[127];
    dftPul.re = dftPulRe;
    dftPul.im = dftPulIm;
    dftPul.length = 127;

    FDomain dftTri;
    float dftTriRe[127];
    float dftTriIm[127];
    dftTri.re = dftTriRe;
    dftTri.im = dftTriIm;
    dftTri.length = 127;

    float signalSamples[WAVETABLE_SIZE];
    TDomain signal;
    signal.samples = signalSamples;
    signal.length = WAVETABLE_SIZE;

    loop(x, WAVETABLE_SIZE) { signalSamples[x] = sinWavetable[x]; }
    dft(dftSin, signal);

    loop(x, WAVETABLE_SIZE) { signalSamples[x] = sawWavetable[x]; }
    dft(dftSaw, signal);

    loop(x, WAVETABLE_SIZE) { signalSamples[x] = pulWavetable[x]; }
    dft(dftPul, signal);

    loop(x, WAVETABLE_SIZE) { signalSamples[x] = triWavetable[x]; }
    dft(dftTri, signal);

    PlotParams params;
    params.height = 100;

    for (;;) {
        sleep(1);
        if (isShuttingDown) {
            xcb_disconnect (xcbState.connection);
            return 0;
        }

        // Sine wave, time and freq domains.
        signal.samples = sinWavetable;
        plot_tdomain(0, signal);
        plot_fdomain(0, dftSin);

        // Saw wave, time and freq domains.
        signal.samples = sawWavetable;
        plot_tdomain(1, signal);
        plot_fdomain(1, dftSaw);

        // Pulse wave, time and freq domains.
        signal.samples = pulWavetable;
        plot_tdomain(2, signal);
        plot_fdomain(2, dftPul);
        xcb_flush (xcbState.connection);

        // Triangle wave, time and freq domains.
        signal.samples = triWavetable;
        plot_tdomain(3, signal);
        plot_fdomain(3, dftTri);
        xcb_flush (xcbState.connection);
    }

    return 0;
}
