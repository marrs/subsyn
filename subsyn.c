#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <jack/jack.h>
#include <xcb/xcb.h>
#include <gtk/gtk.h>

#include "macro-utils.h"
#define die_if(pred, action) if (pred) { (action); shutdown_app(EXIT_FAILURE); }

#include "xcb.h"
#include "signal-processors.h"
#include "signal-processors.c"
#include "wavetable.c"

#include "chart.c"

typedef struct JackState {
    jack_port_t *leftPort, *rightPort;
    jack_client_t *client;
} JackState;

Generator uiSelectedGenerator = GEN_Noise;
float uiVolume = 1.0f;
float uiPitch = 440.0f;
float uiCutoff = 20000.0f;
FDomain *uiCleanDft = &dftSaw;
TDomain *uiWavetable = &sawWavetable; // Unsused for now. Keeping for when we test FFT.
int samplerateHz;
WavetableSample wtsample;
float wavetableSamples[WAVETABLE_SIZE];
WavetableType uiWavetableType = WAVT_Wavetable;
TDomain processedWavetable;


float _processedDftRe[128];
float _processedDftIm[128];
FDomain processedDft;

JackState jackState;
XcbState xcbState;

FILE *debugLog;

int isShuttingDown = 0;

int process_audio(jack_nframes_t nframes, void *arg)
{
    jack_default_audio_sample_t *leftChannel, *rightChannel;

    leftChannel = (jack_default_audio_sample_t *)jack_port_get_buffer(jackState.leftPort, nframes);
    rightChannel = (jack_default_audio_sample_t *)jack_port_get_buffer(jackState.rightPort, nframes);

    float val = 0.0f;
    // TODO: Just use ui versions if we can.
    float pitchHz = uiPitch;
    float cutoffHz = uiCutoff;
    float wavelengthHz = samplerateHz / pitchHz;
    loop(i, nframes) {
        if (uiSelectedGenerator == GEN_Noise) {
            val = (float)rand() / RAND_MAX;
        } else {
            filter_cliffedge(uiCleanDft, &processedDft, pitchHz, cutoffHz);
            TDomain wavetable;
            wavetable.length = WAVETABLE_SIZE;
            wavetable.samples = wavetableSamples;
            idft(&wavetable, processedDft);
            //idft(&wavetable, *uiCleanDft);
            //scan_wavetable(&wtsample, wavelengthHz, &sawWavetable);
            scan_wavetable(&wtsample, wavelengthHz, &wavetable);
            val = 0.5 + wtsample.val;
        }
        val *= uiVolume;
        leftChannel[i] = val;
        rightChannel[i] = val;
        fprintf(debugLog, "%d %f\n", i%256 * 2, val);

        plot_sample(xcbState, i, val, NULL);

    }
    xcb_flush (xcbState.connection);
    return 0;
}

void *xcb_thread(void *arg)
{
    XcbState *xcbState = (XcbState *)arg;
    xcb_generic_event_t *event;
    while (event = xcb_wait_for_event (xcbState->connection)) {
        switch (event->response_type & ~0x80) {
            case XCB_KEY_RELEASE: {
                xcb_key_release_event_t *kr = (xcb_key_release_event_t *)event;
                printf ("Key released in window %"PRIu32"\n",
                        kr->event);
                cycle_generator(&uiSelectedGenerator);
                select_dft(&uiCleanDft, uiSelectedGenerator);
            } break;
        }
    }
}

void jack_shutdown (void *arg)
{
    jackState.client = NULL;
}

void on_shutdown()
{
    if (NULL != xcbState.connection) {
        xcb_disconnect (xcbState.connection);
    }
    if (NULL != jackState.client) {
        jack_client_close(jackState.client);
    }
    fclose(debugLog);
}

void shutdown_app(int exitStatus)
{
    on_shutdown();
    exit(exitStatus);
}

gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (event->keyval == GDK_KEY_space){
        cycle_generator(&uiSelectedGenerator);
        select_dft(&uiCleanDft, uiSelectedGenerator);
        return TRUE;
    }
    return FALSE;
}

void on_volume_change(GtkRange *widget, gpointer data)
{
    uiVolume = gtk_range_get_value(widget) / 10;
}

void on_pitch_change(GtkRange *widget, gpointer data)
{
    uiPitch = gtk_range_get_value(widget);
}

void on_cutoff_change(GtkRange *widget, gpointer data)
{
    uiCutoff = gtk_range_get_value(widget);
}

/*
void on_wavesource_toggle(GtkSwitch *widget, gpointer data)
{
    uiWavetableType = (WavetableType)gtk_switch_get_active(widget);
    select_wavetable(&uiWavetable,
                     uiSelectedGenerator,
                     uiWavetableType);
}
*/

void on_activate(GtkApplication *app)
{
    processedDft.re = _processedDftRe;
    processedDft.im = _processedDftIm;

    GtkWidget *window = gtk_application_window_new(app);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(box));

    gtk_widget_add_events(window, GDK_KEY_PRESS_MASK);
    g_signal_connect(G_OBJECT(window),
                     "key_press_event",
                     G_CALLBACK(on_key_press),
                     NULL);

    GtkWidget *volumeScale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL,
                                           gtk_adjustment_new(
                                                uiVolume * 10 // val
                                              , 0  // lower
                                              , 11 // upper
                                              , 1 // step inc
                                              , 1 // page inc
                                              , 1 // page size
                                           ));
    g_signal_connect(G_OBJECT(volumeScale)
                   , "value-changed"
                   , G_CALLBACK(on_volume_change)
                   , NULL);

    // For testing of DFT implementations
    /*
    GtkWidget *wavesourceToggle = gtk_switch_new();
    g_signal_connect(G_OBJECT(wavesourceToggle)
                   , "state-set"
                   , G_CALLBACK(on_wavesource_toggle)
                   , NULL);
   */

    GtkWidget *pitchScale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL,
                                           gtk_adjustment_new(
                                                uiPitch // val
                                              , 20  // lower
                                              , 2000 // upper
                                              , 1 // step inc
                                              , 1 // page inc
                                              , 1 // page size
                                           ));
    g_signal_connect(G_OBJECT(pitchScale)
                   , "value-changed"
                   , G_CALLBACK(on_pitch_change)
                   , NULL);

    GtkWidget *cutoffScale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL,
                                           gtk_adjustment_new(
                                                uiCutoff // val
                                              , 20  // lower
                                              , 20000 // upper
                                              , 1 // step inc
                                              , 1 // page inc
                                              , 1 // page size
                                           ));
    g_signal_connect(G_OBJECT(cutoffScale)
                   , "value-changed"
                   , G_CALLBACK(on_cutoff_change)
                   , NULL);

    gtk_box_pack_end(GTK_BOX(box), pitchScale, TRUE, TRUE, 2);
    gtk_box_pack_end(GTK_BOX(box), volumeScale, TRUE, TRUE, 2);
    gtk_box_pack_end(GTK_BOX(box), cutoffScale, TRUE, TRUE, 2);
    //gtk_box_pack_end(GTK_BOX(box), wavesourceToggle, FALSE, FALSE, 2);

    gtk_widget_show_all (window);

    // TODO: Get samplerate from Jack server.
    samplerateHz = 48000;
    /* Open the connection to the X server */
    xcbState.connection = xcb_connect(NULL, NULL);
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
    int threadReturnCode = pthread_create(thread, NULL, xcb_thread, (void *)&xcbState);



    const char *jcName = "testclient";
    const char *jcServerName =NULL;
    jack_options_t jcOptions = JackNullOption;
	jack_status_t jcStatus;

    
	jackState.client = jack_client_open(jcName, jcOptions, &jcStatus, jcServerName);
    jack_client_t *jackClient = jackState.client;

    jack_set_buffer_size(jackClient, 2048 * 2);

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

    jack_set_process_callback(jackClient, process_audio, &jackState);

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

    

    jack_on_shutdown(jackClient, jack_shutdown, &jackState);


}

int main(int argc, char **argv)
{
     GtkApplication *app = gtk_application_new ("com.example.GtkApplication",
                                             G_APPLICATION_FLAGS_NONE);
     g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);
     g_signal_connect (app, "shutdown", G_CALLBACK (on_shutdown), NULL);
    return g_application_run (G_APPLICATION (app), argc, argv);
}
