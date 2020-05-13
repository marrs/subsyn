typedef struct PlotParams {
    int xOffset, yOffset, height;
} PlotParams;

void plot_sample(XcbState xcbState, int x, float y, PlotParams *params)
{
    int height = 255;
    int xOffset = 0;
    int yOffset = 0;

    if (NULL != params) {
        height = params->height;
        xOffset = params->xOffset;
        yOffset = params->yOffset;
    }

    xcb_point_t line[2];

    line[0].x = xOffset + x;
    line[0].y = yOffset;
    line[1].x = xOffset + x;
    line[1].y = yOffset + height*y;

    xcb_poly_line (xcbState.connection,
            XCB_COORD_MODE_ORIGIN,
            xcbState.window,
            xcbState.fgContext,
            2,
            line);

    line[0].x = xOffset + x;
    line[0].y = yOffset + height*y;
    line[1].x = xOffset + x;
    line[1].y = yOffset + 255;

    xcb_poly_line(xcbState.connection,
                  XCB_COORD_MODE_ORIGIN,
                  xcbState.window,
                  xcbState.bgContext,
                  2,
                  line);
}
