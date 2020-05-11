#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <jack/jack.h>
#include <xcb/xcb.h>

#define die_if(pred, appState, action) if (pred) { (action); shutdown_app(appState, EXIT_FAILURE); }

typedef struct {
    jack_port_t *leftPort, *rightPort;
    jack_client_t *jackClient;
    xcb_connection_t *xcbConnection;
} AppState;

int isShuttingDown = 0;

int process(jack_nframes_t nframes, void *arg)
{
    AppState *appState = (AppState *)arg;
    jack_default_audio_sample_t *leftChannel, *rightChannel;

    leftChannel = (jack_default_audio_sample_t *)jack_port_get_buffer(appState->leftPort, nframes);
    rightChannel = (jack_default_audio_sample_t *)jack_port_get_buffer(appState->rightPort, nframes);

    FILE * pFile;
    pFile = fopen ("log","wa");
    float val = 0.0f;
    for (int i = 0; i < nframes; ++i) {
        val = (float)rand() / RAND_MAX;
        leftChannel[i] = val;
        rightChannel[i] = val;
        fprintf(pFile, "%f\n", leftChannel[i]);
    }
    fclose (pFile);
    return 0;
}

void jack_shutdown (void *arg)
{
    AppState *appState = (AppState *)arg;
    appState->jackClient = NULL;
}

void sigterm_action(int signum)
{
    isShuttingDown = 1;
}

void shutdown_app(AppState *appState, int exitStatus)
{
    if (NULL != appState->xcbConnection) {
        xcb_disconnect (appState->xcbConnection);
    }
    if (NULL != appState->jackClient) {
        jack_client_close (appState->jackClient);
    }
    exit(exitStatus);
}

int main()
{
    AppState appState;
    /* Open the connection to the X server */
    appState.xcbConnection = NULL;
    appState.xcbConnection = xcb_connect (NULL, NULL);


    /* Get the first screen */
    const xcb_setup_t      *setup  = xcb_get_setup(appState.xcbConnection);
    xcb_screen_iterator_t   iter   = xcb_setup_roots_iterator (setup);
    xcb_screen_t           *screen = iter.data;


    /* Create the window */
    xcb_window_t window = xcb_generate_id (appState.xcbConnection);
    xcb_create_window (appState.xcbConnection,        /* Connection          */
                       XCB_COPY_FROM_PARENT,          /* depth (same as root)*/
                       window,                        /* window Id           */
                       screen->root,                  /* parent window       */
                       0, 0,                          /* x, y                */
                       150, 150,                      /* width, height       */
                       10,                            /* border_width        */
                       XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class               */
                       screen->root_visual,           /* visual              */
                       0, NULL );                     /* masks, not used yet */


    /* Map the window on the screen */
    xcb_map_window (appState.xcbConnection, window);


    /* Make sure commands are sent before we pause so that the window gets shown */
    xcb_flush (appState.xcbConnection);



    const char *jcName = "testclient";
    const char *jcServerName =NULL;
    jack_options_t jcOptions = JackNullOption;
	jack_status_t jcStatus;

    
	appState.jackClient = jack_client_open(jcName, jcOptions, &jcStatus, jcServerName);
    jack_client_t *jackClient = appState.jackClient;

    die_if(NULL == jackClient, &appState, {
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

    jack_set_process_callback(jackClient, process, &appState);

    appState.leftPort = jack_port_register(jackClient,
                                             "leftChannel",
					                         JACK_DEFAULT_AUDIO_TYPE,
					                         JackPortIsOutput, 0);

    appState.rightPort = jack_port_register(jackClient,
                                              "rightChannel",
					                          JACK_DEFAULT_AUDIO_TYPE,
					                          JackPortIsOutput, 0);

    die_if(NULL == appState.leftPort || NULL == appState.rightPort, &appState,
            fprintf(stderr, "Could not connect to Jack")
    );

    die_if(jack_activate(jackClient), &appState,
            fprintf(stderr, "Could not activate jack client"));

	const char **ports = jack_get_ports(jackClient, NULL, NULL,
				JackPortIsPhysical|JackPortIsInput);
	die_if(ports == NULL, &appState, fprintf(stderr, "no physical capture ports\n"));

	if (jack_connect(jackClient, jack_port_name(appState.leftPort), ports[1])) {
		fprintf (stderr, "cannot connect left port\n");
	}
	if (jack_connect(jackClient, jack_port_name(appState.rightPort), ports[0])) {
		fprintf (stderr, "cannot connect right port\n");
	}

	free (ports);

    struct sigaction sigTermHandler;
    sigTermHandler.sa_handler = sigterm_action;
    sigaction(SIGTERM, &sigTermHandler, NULL);

    for (;;) {
        sleep(1);
        if (isShuttingDown) shutdown_app(&appState, EXIT_SUCCESS);
    }

    

    jack_on_shutdown(jackClient, jack_shutdown, &appState);


    return 0;
}
