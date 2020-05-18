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
#include "signal-processors.c"

XcbState xcbState;
const int gridGutterSize = 10;
const int gridColumnWidth = WAVETABLE_SIZE;

int grid_col(int pos)
{
    return (pos * gridGutterSize) + ((pos -1) * gridColumnWidth);
}

void plot_tdomain(int col, int row, TDomain signal)
{
    PlotParams params;
    params.height = 128;
    loop(x, signal.length) {
        params.xOffset = grid_col(col);
        params.yOffset = (row * 128) + (row * gridGutterSize) + gridGutterSize;
        plot_sample(xcbState, x, signal.samples[x] + 0.5, &params);
    }
}

void plot_fdomain(int row, FDomain ft)
{
    PlotParams params;
    params.height = 100;

    loop (x, ft.length) {
        params.xOffset = grid_col(2);
        params.yOffset = (row * 128) + (row * gridGutterSize) + gridGutterSize;
        plot_sample(xcbState, x, ft.re[x], &params);
    }
    loop (x, ft.length) {
        params.xOffset = grid_col(2) + gridGutterSize + (gridColumnWidth /2);
        params.yOffset = (row * 128) + (row * gridGutterSize) + gridGutterSize;
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
                       grid_col(4), // width
                       600,                           /*  height       */
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

    float sourceSignalSamples[WAVETABLE_SIZE];
    TDomain sourceSignal;
    sourceSignal.samples = sourceSignalSamples;
    sourceSignal.length = WAVETABLE_SIZE;

    float processedSignalSamples[WAVETABLE_SIZE];
    TDomain processedSignal;
    processedSignal.samples = processedSignalSamples;
    processedSignal.length = WAVETABLE_SIZE;

    loop(x, WAVETABLE_SIZE) { sourceSignalSamples[x] = sinWavetable[x]; }
    dft(dftSin, sourceSignal);

    loop(x, WAVETABLE_SIZE) { sourceSignalSamples[x] = sawWavetable[x]; }
    dft(dftSaw, sourceSignal);

    loop(x, WAVETABLE_SIZE) { sourceSignalSamples[x] = pulWavetable[x]; }
    dft(dftPul, sourceSignal);

    loop(x, WAVETABLE_SIZE) { sourceSignalSamples[x] = triWavetable[x]; }
    dft(dftTri, sourceSignal);

    PlotParams params;
    params.height = 100;

    for (;;) {
        sleep(1);
        if (isShuttingDown) {
            xcb_disconnect (xcbState.connection);
            return 0;
        }

        // Sine wave, time and freq domains.
        sourceSignal.samples = sinWavetable;
        plot_tdomain(1, 0, sourceSignal);
        plot_fdomain(0, dftSin);
        idft(processedSignal, dftSin);
        plot_tdomain(3, 0, processedSignal);

        // Saw wave, time and freq domains.
        sourceSignal.samples = sawWavetable;
        plot_tdomain(1, 1, sourceSignal);
        plot_fdomain(1, dftSaw);
        idft(processedSignal, dftSaw);
        plot_tdomain(3, 1, processedSignal);

        // Pulse wave, time and freq domains.
        sourceSignal.samples = pulWavetable;
        plot_tdomain(1, 2, sourceSignal);
        plot_fdomain(2, dftPul);
        idft(processedSignal, dftPul);
        plot_tdomain(3, 2, processedSignal);

        // Triangle wave, time and freq domains.
        sourceSignal.samples = triWavetable;
        plot_tdomain(1, 3, sourceSignal);
        plot_fdomain(3, dftTri);
        idft(processedSignal, dftTri);
        plot_tdomain(3, 3, processedSignal);

        xcb_flush (xcbState.connection);
    }

    return 0;
}
