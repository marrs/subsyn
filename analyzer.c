#include <stdio.h>
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
    xcb_create_window (xcbState.connection,        /* Connection          */
                       XCB_COPY_FROM_PARENT,          /* depth (same as root)*/
                       xcbState.window,            /* window Id           */
                       screen->root,                  /* parent window       */
                       0, 0,                          /* x, y                */
                       512, 512,                      /* width, height       */
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

    PlotParams params;
    params.xOffset = 0;
    params.height = 100;

    for (;;) {
        sleep(1);
        if (isShuttingDown) {
            xcb_disconnect (xcbState.connection);
            return 0;
        }

        loop (x, WAVETABLE_SIZE) {
            params.yOffset = 0;
            plot_sample(xcbState, x, sawWavetable[x], &params);
        }

        loop (x, WAVETABLE_SIZE) {
            params.yOffset = 128;
            plot_sample(xcbState, x, plsWavetable[x], &params);
        }
        xcb_flush (xcbState.connection);
    }

    return 0;
}
