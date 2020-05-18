typedef struct XcbState {
    xcb_connection_t *connection;
    xcb_window_t window;
    xcb_gcontext_t fgContext;
    xcb_gcontext_t bgContext;
} XcbState;
