#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <jack/jack.h>
#include <xcb/xcb.h>

#define die_if(pred, action) if (pred) { (action); shutdown_app(EXIT_FAILURE); }

#include "wavetable.c"

typedef struct JackState {
    jack_port_t *leftPort, *rightPort;
    jack_client_t *client;
} JackState;

typedef struct XcbState {
    xcb_connection_t *connection;
    xcb_window_t window;
    xcb_gcontext_t fgContext;
    xcb_gcontext_t bgContext;
} XcbState;

typedef enum Generator {
    GEN_Noise = 0,
    GEN_Sawtooth,
    GEN_Pulse,
    GEN_Sine
} Generator;

Generator uiSelectedGenerator = GEN_Noise;
int samplerateHz;
WavetableSample wtsample;

JackState jackState;
XcbState xcbState;

FILE *debugLog;

int isShuttingDown = 0;

int process(jack_nframes_t nframes, void *arg)
{
    jack_default_audio_sample_t *leftChannel, *rightChannel;

    leftChannel = (jack_default_audio_sample_t *)jack_port_get_buffer(jackState.leftPort, nframes);
    rightChannel = (jack_default_audio_sample_t *)jack_port_get_buffer(jackState.rightPort, nframes);

    float val = 0.0f;
    float pitchHz = 440;
    float wavelengthHz = samplerateHz / pitchHz;
    for (int i = 0; i < nframes; ++i) {
        if (uiSelectedGenerator == GEN_Noise) {
            val = (float)rand() / RAND_MAX;
        } else if (uiSelectedGenerator == GEN_Pulse) {
            scan_wavetable(pulseWavetable, wavelengthHz, &wtsample);
            val = wtsample.val;
        } else {
            scan_wavetable(sawWavetable, wavelengthHz, &wtsample);
            val = wtsample.val;
        }
        leftChannel[i] = val;
        rightChannel[i] = val;
        fprintf(debugLog, "%d %f\n", i%256 * 2, leftChannel[i]);
        xcb_point_t line[2];

        line[0].x = i;
        line[0].y = 0;
        line[1].x = i;
        line[1].y = 200*val;

        xcb_poly_line (xcbState.connection,
                       XCB_COORD_MODE_ORIGIN,
                       xcbState.window,
                       xcbState.fgContext,
                       2,
                       line);

        line[0].x = i;
        line[0].y = 200*val;
        line[1].x = i;
        line[1].y = 255;

        xcb_poly_line (xcbState.connection,
                       XCB_COORD_MODE_ORIGIN,
                       xcbState.window,
                       xcbState.bgContext,
                       2,
                       line);
    }
    xcb_flush (xcbState.connection);
    return 0;
}

void *ui_thread(void *arg)
{
    XcbState *xcbState = (XcbState *)arg;
    xcb_generic_event_t *event;
    while (event = xcb_wait_for_event (xcbState->connection)) {
        switch (event->response_type & ~0x80) {
            case XCB_KEY_RELEASE: {
                xcb_key_release_event_t *kr = (xcb_key_release_event_t *)event;
                //print_modifiers(kr->state);
                printf ("Key released in window %"PRIu32"\n",
                        kr->event);
                switch(uiSelectedGenerator) {
                    case GEN_Noise: uiSelectedGenerator = GEN_Sawtooth; break;
                    case GEN_Sawtooth: uiSelectedGenerator = GEN_Pulse; break;
                    case GEN_Pulse: uiSelectedGenerator = GEN_Noise; break;
                }
            } break;
        }
    }
}

void jack_shutdown (void *arg)
{
    jackState.client = NULL;
}

void sigterm_action(int signum)
{
    isShuttingDown = 1;
}

void shutdown_app(int exitStatus)
{
    if (NULL != xcbState.connection) {
        xcb_disconnect (xcbState.connection);
    }
    if (NULL != jackState.client) {
        jack_client_close(jackState.client);
    }
    fclose(debugLog);
    exit(exitStatus);
}

int main()
{
    // TODO: Get samplerate from Jack server.
    samplerateHz = 48000;
    /* Open the connection to the X server */
    xcbState.connection = xcb_connect (NULL, NULL);
    uint32_t xcbvals[2];
    wtsample.val = 0.0f;
    wtsample.offset = 0.0f;

    debugLog = fopen ("log","w");

    /* Get the first screen */
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
                       256, 256,                      /* width, height       */
                       10,                            /* border_width        */
                       XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class               */
                       screen->root_visual,           /* visual              */
                       XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
                       xcbvals );


    /* Map the window on the screen */
    xcb_map_window (xcbState.connection, xcbState.window);


    /* Make sure commands are sent before we pause so that the window gets shown */
    xcb_flush (xcbState.connection);

    pthread_t thread[1];
    int threadReturnCode = pthread_create(thread, NULL, ui_thread, (void *)&xcbState);



    const char *jcName = "testclient";
    const char *jcServerName =NULL;
    jack_options_t jcOptions = JackNullOption;
	jack_status_t jcStatus;

    
	jackState.client = jack_client_open(jcName, jcOptions, &jcStatus, jcServerName);
    jack_client_t *jackClient = jackState.client;

    die_if(NULL == jackClient, {
		fprintf (stderr, "jack_client_open() failed, "
			 "status = 0x%2.0x\n", jcStatus);
		if (jcStatus & JackServerFailed) {
			fprintf (stderr, "Unable to connect to JACK server\n");
		}
    });
	if (jcStatus & JackServerStarted) {
		fprintf (stderr, "JACK server started\n");
	}
	if (jcStatus & JackNameNotUnique) {
		jcName = jack_get_client_name(jackClient);
		fprintf (stderr, "unique name `%s' assigned\n", jcName);
	}

    jack_set_process_callback(jackClient, process, &jackState);

    jackState.leftPort = jack_port_register(jackClient,
                                            "leftChannel",
                                            JACK_DEFAULT_AUDIO_TYPE,
                                            JackPortIsOutput, 0);

    jackState.rightPort = jack_port_register(jackClient,
                                             "rightChannel",
                                             JACK_DEFAULT_AUDIO_TYPE,
                                             JackPortIsOutput, 0);

    die_if(NULL == jackState.leftPort || NULL == jackState.rightPort,
            fprintf(stderr, "Could not connect to Jack")
    );

    die_if(jack_activate(jackClient),
            fprintf(stderr, "Could not activate jack client"));

	const char **ports = jack_get_ports(jackClient, NULL, NULL,
				JackPortIsPhysical|JackPortIsInput);
	die_if(ports == NULL, fprintf(stderr, "no physical capture ports\n"));

	if (jack_connect(jackClient, jack_port_name(jackState.leftPort), ports[1])) {
		fprintf (stderr, "cannot connect left port\n");
	}
	if (jack_connect(jackClient, jack_port_name(jackState.rightPort), ports[0])) {
		fprintf (stderr, "cannot connect right port\n");
	}

	free (ports);

    init_wavetables();

    struct sigaction sigTermHandler;
    sigTermHandler.sa_handler = sigterm_action;
    sigaction(SIGTERM, &sigTermHandler, NULL);

    for (;;) {
        sleep(1);
        if (isShuttingDown) shutdown_app(EXIT_SUCCESS);
    }

    

    jack_on_shutdown(jackClient, jack_shutdown, &jackState);


    return 0;
}