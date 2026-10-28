/* The drawing surface (CImgWnd), rulers (CRulerBar) and canvas resizing.
 *
 * Image pixels are shown at app.zoom with a fixed margin; the canvas can be
 * resized by dragging the three handles on its right/bottom edges exactly
 * as in mspaint (CImgWnd::ResizeMouseHandler). */
#include "paint.h"
#include <math.h>

#define MARGIN 5
#define HSZ 5          /* resize handle size */
#define RULER 22

/* CImgWnd::presetZooms (12.5% .. 800%) */
const double zoom_presets[] = { 0.125, 0.25, 0.5, 1, 2, 3, 4, 5, 6, 7, 8 };
const int n_zoom_presets = sizeof zoom_presets / sizeof zoom_presets[0];

static struct {
    int drag_handle;          /* 0 none, 1 right, 2 bottom, 3 corner */
    int new_w, new_h;
    bool tool_drag;
    int tool_button;
    double press_wx, press_wy;
} cv;

void canvas_image_to_widget(double ix, double iy, double *wx, double *wy)
{
    *wx = MARGIN + ix * app.zoom;
    *wy = MARGIN + iy * app.zoom;
}

void canvas_widget_to_image(double wx, double wy, double *ix, double *iy)
{
    *ix = (wx - MARGIN) / app.zoom;
    *iy = (wy - MARGIN) / app.zoom;
}

void canvas_update_size(void)
{
    if (!app.canvas) return;
    int w = (int)ceil(app.w * app.zoom) + 2 * MARGIN + 30;
    int h = (int)ceil(app.h * app.zoom) + 2 * MARGIN + 30;
    gtk_widget_set_size_request(app.canvas, w, h);
    rulers_redraw();
}

void canvas_redraw(void)
{
    if (app.canvas) gtk_widget_queue_draw(app.canvas);
}

void canvas_redraw_rect(double x, double y, double w, double h)
{
    if (!app.canvas) return;
    double wx, wy;
    canvas_image_to_widget(x, y, &wx, &wy);
    gtk_widget_queue_draw_area(app.canvas, (int)floor(wx) - 2, (int)floor(wy) - 2,
                               (int)ceil(w * app.zoom) + 5, (int)ceil(h * app.zoom) + 5);
}

static void draw_handle(cairo_t *cr, double x, double y)
{
    cairo_rectangle(cr, floor(x - HSZ / 2.0) + 0.5, floor(y - HSZ / 2.0) + 0.5, HSZ, HSZ);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.33, 0.4, 0.5);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
}

/* gridline spacing in image pixels (CImgWnd::DrawGrid picks coarser grids
 * when zoomed out so lines never get closer than a few screen pixels) */
static int grid_step(void)
{
    if (app.zoom >= 4) return 1;
    if (app.zoom >= 2) return 2;
    if (app.zoom >= 1) return 5;
    if (app.zoom >= 0.5) return 10;
    if (app.zoom >= 0.25) return 20;
    return 40;
}

static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
    (void)d;
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double iw = app.w * app.zoom, ih = app.h * app.zoom;

    /* workspace */
    cairo_set_source_rgb(cr, ui.workspace.r, ui.workspace.g, ui.workspace.b);
    cairo_paint(cr);
    /* soft shadow */
    cairo_set_source_rgba(cr, ui.shadow.r, ui.shadow.g, ui.shadow.b, ui.shadow_alpha);
    cairo_rectangle(cr, MARGIN + 2, MARGIN + 2, iw + 1, ih + 1);
    cairo_fill(cr);

    /* image */
    cairo_save(cr);
    cairo_rectangle(cr, MARGIN, MARGIN, iw, ih);
    cairo_clip(cr);
    cairo_translate(cr, MARGIN, MARGIN);
    cairo_scale(cr, app.zoom, app.zoom);
    cairo_set_source_surface(cr, app.img, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), app.zoom >= 1 ? CAIRO_FILTER_NEAREST : CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    sel_draw(cr);
    cairo_restore(cr);

    /* gridlines (halftone 0xC0C0C0 / 0x808080 in the original) */
    if (app.show_grid) {
        int step = grid_step();
        double clip_x0, clip_y0, clip_x1, clip_y1;
        cairo_clip_extents(cr, &clip_x0, &clip_y0, &clip_x1, &clip_y1);
        cairo_save(cr);
        cairo_set_line_width(cr, 1);
        double dash[] = { 1, 1 };
        cairo_set_dash(cr, dash, 2, 0);
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        int x0 = MAX(step, (int)((clip_x0 - MARGIN) / app.zoom) / step * step);
        for (int x = x0; x < app.w; x += step) {
            double wx = floor(MARGIN + x * app.zoom) + 0.5;
            if (wx > clip_x1) break;
            cairo_move_to(cr, wx, MARGIN);
            cairo_line_to(cr, wx, MARGIN + ih);
        }
        int y0 = MAX(step, (int)((clip_y0 - MARGIN) / app.zoom) / step * step);
        for (int y = y0; y < app.h; y += step) {
            double wy = floor(MARGIN + y * app.zoom) + 0.5;
            if (wy > clip_y1) break;
            cairo_move_to(cr, MARGIN, wy);
            cairo_line_to(cr, MARGIN + iw, wy);
        }
        cairo_stroke(cr);
        cairo_restore(cr);
    }

    tool_draw_overlay(cr);

    /* canvas resize handles */
    if (!cv.drag_handle) {
        draw_handle(cr, MARGIN + iw + 2, MARGIN + ih / 2);
        draw_handle(cr, MARGIN + iw / 2, MARGIN + ih + 2);
        draw_handle(cr, MARGIN + iw + 2, MARGIN + ih + 2);
    } else {
        double dash[] = { 2, 2 };
        cairo_set_dash(cr, dash, 2, 0);
        cairo_set_line_width(cr, 1);
        cairo_set_source_rgb(cr, ui.fg.r, ui.fg.g, ui.fg.b);
        cairo_rectangle(cr, MARGIN - 0.5, MARGIN - 0.5, cv.new_w * app.zoom + 1, cv.new_h * app.zoom + 1);
        cairo_stroke(cr);
    }
    (void)W; (void)H;
    return TRUE;
}

static int resize_handle_hit(double wx, double wy)
{
    double iw = app.w * app.zoom, ih = app.h * app.zoom, r = HSZ + 2;
    double rx = MARGIN + iw + 2, by = MARGIN + ih + 2;
    if (fabs(wx - rx) <= r && fabs(wy - by) <= r) return 3;
    if (fabs(wx - rx) <= r && fabs(wy - (MARGIN + ih / 2)) <= r) return 1;
    if (fabs(wx - (MARGIN + iw / 2)) <= r && fabs(wy - by) <= r) return 2;
    return 0;
}

static gboolean on_press(GtkWidget *w, GdkEventButton *ev, gpointer d)
{
    (void)d;
    /* taking focus back from a ribbon button makes the viewport scroll its
     * "newly focused" child into view, i.e. jump to the top-left corner */
    GtkAdjustment *ha = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(app.scroll));
    GtkAdjustment *va = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(app.scroll));
    double hv = gtk_adjustment_get_value(ha), vv = gtk_adjustment_get_value(va);
    gtk_widget_grab_focus(w);
    gtk_adjustment_set_value(ha, hv);
    gtk_adjustment_set_value(va, vv);
    if (ev->type != GDK_BUTTON_PRESS && ev->type != GDK_2BUTTON_PRESS) return TRUE;
    if (ev->button != 1 && ev->button != 3) return FALSE;
    if (ev->type == GDK_BUTTON_PRESS && ev->button == 1) {
        int h = resize_handle_hit(ev->x, ev->y);
        if (h) {
            tool_commit_pending();
            cv.drag_handle = h;
            cv.new_w = app.w;
            cv.new_h = app.h;
            canvas_redraw();
            return TRUE;
        }
    }
    if (cv.tool_drag && ev->type == GDK_BUTTON_PRESS && (int)ev->button != cv.tool_button) {
        /* pressing the other button during a drag cancels it (CImgWnd::CancelPainting) */
        tool_cancel();
        cv.tool_drag = false;
        return TRUE;
    }
    double ix, iy;
    canvas_widget_to_image(ev->x, ev->y, &ix, &iy);
    if (ev->type == GDK_BUTTON_PRESS) {
        cv.tool_drag = true;
        cv.tool_button = ev->button;
    }
    tool_press(ix, iy, ev->button, ev->state, ev->type == GDK_2BUTTON_PRESS ? 2 : 1);
    return TRUE;
}

static gboolean on_release(GtkWidget *w, GdkEventButton *ev, gpointer d)
{
    (void)w; (void)d;
    if (cv.drag_handle) {
        int nw = cv.new_w, nh = cv.new_h;
        cv.drag_handle = 0;
        image_set_size(nw, nh);
        canvas_redraw();
        update_status();
        return TRUE;
    }
    if (!cv.tool_drag || (int)ev->button != cv.tool_button) return TRUE;
    cv.tool_drag = false;
    double ix, iy;
    canvas_widget_to_image(ev->x, ev->y, &ix, &iy);
    tool_release(ix, iy, ev->button, ev->state);
    return TRUE;
}

static gboolean on_motion(GtkWidget *w, GdkEventMotion *ev, gpointer d)
{
    (void)d;
    double ix, iy;
    canvas_widget_to_image(ev->x, ev->y, &ix, &iy);
    if (cv.drag_handle) {
        if (cv.drag_handle & 1) cv.new_w = CLAMP((int)lround(ix), 1, MAX_IMAGE_DIM);
        if (cv.drag_handle & 2) cv.new_h = CLAMP((int)lround(iy), 1, MAX_IMAGE_DIM);
        app.mouse_x = -1;
        update_status();
        gchar *s = g_strdup_printf("%d × %dpx", cv.new_w, cv.new_h);
        gtk_label_set_text(GTK_LABEL(app.st_sel), s);
        g_free(s);
        canvas_redraw();
        return TRUE;
    }
    app.mouse_x = (int)floor(ix);
    app.mouse_y = (int)floor(iy);
    if (app.mouse_x < 0 || app.mouse_y < 0 || app.mouse_x >= app.w || app.mouse_y >= app.h)
        app.mouse_x = app.mouse_y = -1;
    tool_motion(ix, iy, ev->state);
    /* cursor feedback for the resize handles */
    GdkWindow *win = gtk_widget_get_window(w);
    int h = cv.tool_drag ? 0 : resize_handle_hit(ev->x, ev->y);
    if (h) {
        const char *n = h == 1 ? "ew-resize" : h == 2 ? "ns-resize" : "nwse-resize";
        GdkCursor *c = gdk_cursor_new_from_name(gdk_window_get_display(win), n);
        gdk_window_set_cursor(win, c);
        g_object_unref(c);
    } else {
        canvas_update_cursor();
    }
    update_status();
    rulers_redraw();
    gdk_event_request_motions(ev);
    return TRUE;
}

static gboolean on_leave(GtkWidget *w, GdkEventCrossing *ev, gpointer d)
{
    (void)w; (void)ev; (void)d;
    app.mouse_x = app.mouse_y = -1;
    update_status();
    rulers_redraw();
    canvas_redraw();
    return FALSE;
}

static gboolean on_scroll(GtkWidget *w, GdkEventScroll *ev, gpointer d)
{
    (void)w; (void)d;
    if (!(ev->state & GDK_CONTROL_MASK)) return FALSE;   /* let the scrolled window scroll */
    double ix, iy;
    canvas_widget_to_image(ev->x, ev->y, &ix, &iy);
    int dir = 0;
    if (ev->direction == GDK_SCROLL_UP) dir = 1;
    else if (ev->direction == GDK_SCROLL_DOWN) dir = -1;
    else if (ev->direction == GDK_SCROLL_SMOOTH) dir = ev->delta_y < 0 ? 1 : ev->delta_y > 0 ? -1 : 0;
    if (!dir) return TRUE;
    int i;
    for (i = 0; i < n_zoom_presets && zoom_presets[i] < app.zoom - 1e-9; i++) ;
    double z;
    if (dir > 0) z = (i < n_zoom_presets && fabs(zoom_presets[i] - app.zoom) < 1e-9) ?
                     zoom_presets[MIN(i + 1, n_zoom_presets - 1)] : zoom_presets[MIN(i, n_zoom_presets - 1)];
    else z = zoom_presets[MAX(i - 1, 0)];
    canvas_set_zoom(z, ix, iy);
    return TRUE;
}

void canvas_set_zoom(double z, double ax, double ay)
{
    z = CLAMP(z, zoom_presets[0], zoom_presets[n_zoom_presets - 1]);
    GtkAdjustment *ha = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(app.scroll));
    GtkAdjustment *va = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(app.scroll));
    double pw = gtk_adjustment_get_page_size(ha), ph = gtk_adjustment_get_page_size(va);
    /* screen position of the anchor point within the viewport */
    double sx, sy;
    if (ax < 0) {
        ax = (gtk_adjustment_get_value(ha) + pw / 2 - MARGIN) / app.zoom;
        ay = (gtk_adjustment_get_value(va) + ph / 2 - MARGIN) / app.zoom;
    }
    sx = MARGIN + ax * app.zoom - gtk_adjustment_get_value(ha);
    sy = MARGIN + ay * app.zoom - gtk_adjustment_get_value(va);
    app.zoom = z;
    canvas_update_size();
    /* make sure the adjustments know the new size before we scroll */
    double nw = app.w * z + 2 * MARGIN + 30, nh = app.h * z + 2 * MARGIN + 30;
    gtk_adjustment_set_upper(ha, MAX(nw, pw));
    gtk_adjustment_set_upper(va, MAX(nh, ph));
    gtk_adjustment_set_value(ha, MARGIN + ax * z - sx);
    gtk_adjustment_set_value(va, MARGIN + ay * z - sy);
    canvas_redraw();
    canvas_update_cursor();
    update_status();
    update_ui();
    thumb_refresh();
}

void canvas_zoom_step(int dir)
{
    int i;
    if (dir > 0) {
        for (i = 0; i < n_zoom_presets && zoom_presets[i] <= app.zoom + 1e-9; i++) ;
        if (i < n_zoom_presets) canvas_set_zoom(zoom_presets[i], -1, -1);
    } else {
        for (i = n_zoom_presets - 1; i >= 0 && zoom_presets[i] >= app.zoom - 1e-9; i--) ;
        if (i >= 0) canvas_set_zoom(zoom_presets[i], -1, -1);
    }
}

/* ------------------------------------------------------------------ */
/* rulers                                                              */

static gboolean ruler_draw(GtkWidget *w, cairo_t *cr, gpointer data)
{
    bool horiz = GPOINTER_TO_INT(data);
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    cairo_set_source_rgb(cr, ui.ruler_bg.r, ui.ruler_bg.g, ui.ruler_bg.b);
    cairo_paint(cr);
    cairo_set_source_rgb(cr, ui.ruler_line.r, ui.ruler_line.g, ui.ruler_line.b);
    cairo_set_line_width(cr, 1);
    if (horiz) { cairo_move_to(cr, 0, H - 0.5); cairo_line_to(cr, W, H - 0.5); }
    else { cairo_move_to(cr, W - 0.5, 0); cairo_line_to(cr, W - 0.5, H); }
    cairo_stroke(cr);

    GtkAdjustment *adj = horiz ? gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(app.scroll))
                               : gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(app.scroll));
    double off = gtk_adjustment_get_value(adj);
    double len = horiz ? W : H;
    /* major tick spacing: 100px at 100%, adjusted so labels stay readable */
    double major = 100;
    while (major * app.zoom > 160 && major > 1) major /= 2;
    while (major * app.zoom < 60) major *= 2;
    double minor = major / 10;
    if (minor * app.zoom < 4) minor = major / 2;
    cairo_set_source_rgb(cr, ui.ruler_text.r, ui.ruler_text.g, ui.ruler_text.b);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 9);
    double start = floor(((0 + off - MARGIN) / app.zoom) / minor) * minor;
    for (double v = MAX(0, start); ; v += minor) {
        double p = MARGIN + v * app.zoom - off;
        if (p > len) break;
        bool is_major = fmod(v + 1e-6, major) < 1e-3;
        double tl = is_major ? (horiz ? H : W) * 0.6 : (horiz ? H : W) * 0.25;
        double q = floor(p) + 0.5;
        if (horiz) { cairo_move_to(cr, q, H); cairo_line_to(cr, q, H - tl); }
        else { cairo_move_to(cr, W, q); cairo_line_to(cr, W - tl, q); }
        cairo_stroke(cr);
        if (is_major) {
            char buf[32];
            g_snprintf(buf, sizeof buf, "%d", (int)lround(v));
            if (horiz) { cairo_move_to(cr, q + 2, 9); cairo_show_text(cr, buf); }
            else {
                cairo_save(cr);
                cairo_move_to(cr, 9, q + 2);
                cairo_rotate(cr, M_PI / 2);
                cairo_show_text(cr, buf);
                cairo_restore(cr);
            }
        }
        if (v > app.w + app.h + major * 2) break;
    }
    /* mouse position marker */
    if (app.mouse_x >= 0) {
        double p = MARGIN + (horiz ? app.mouse_x + 0.5 : app.mouse_y + 0.5) * app.zoom - off;
        cairo_set_source_rgb(cr, 0.85, 0.1, 0.1);
        if (horiz) { cairo_move_to(cr, floor(p) + 0.5, 0); cairo_line_to(cr, floor(p) + 0.5, H); }
        else { cairo_move_to(cr, 0, floor(p) + 0.5); cairo_line_to(cr, W, floor(p) + 0.5); }
        cairo_stroke(cr);
    }
    return TRUE;
}

static gboolean corner_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
    (void)w; (void)d;
    cairo_set_source_rgb(cr, ui.ruler_bg.r, ui.ruler_bg.g, ui.ruler_bg.b);
    cairo_paint(cr);
    return TRUE;
}

void rulers_redraw(void)
{
    if (app.hruler) gtk_widget_queue_draw(app.hruler);
    if (app.vruler) gtk_widget_queue_draw(app.vruler);
}

static void adj_changed(GtkAdjustment *a, gpointer d)
{
    (void)a; (void)d;
    rulers_redraw();
    thumb_refresh();
}

GtkWidget *canvas_create(void)
{
    GtkWidget *grid = gtk_grid_new();
    app.canvas = gtk_drawing_area_new();
    gtk_widget_set_can_focus(app.canvas, TRUE);
    gtk_widget_add_events(app.canvas, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                                      GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
                                      GDK_LEAVE_NOTIFY_MASK | GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK |
                                      GDK_KEY_PRESS_MASK);
    g_signal_connect(app.canvas, "draw", G_CALLBACK(on_draw), NULL);
    g_signal_connect(app.canvas, "button-press-event", G_CALLBACK(on_press), NULL);
    g_signal_connect(app.canvas, "button-release-event", G_CALLBACK(on_release), NULL);
    g_signal_connect(app.canvas, "motion-notify-event", G_CALLBACK(on_motion), NULL);
    g_signal_connect(app.canvas, "leave-notify-event", G_CALLBACK(on_leave), NULL);
    g_signal_connect(app.canvas, "scroll-event", G_CALLBACK(on_scroll), NULL);

    app.scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_hexpand(app.scroll, TRUE);
    gtk_widget_set_vexpand(app.scroll, TRUE);
    gtk_container_add(GTK_CONTAINER(app.scroll), app.canvas);
    g_signal_connect(gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(app.scroll)), "value-changed",
                     G_CALLBACK(adj_changed), NULL);
    g_signal_connect(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(app.scroll)), "value-changed",
                     G_CALLBACK(adj_changed), NULL);

    app.hruler = gtk_drawing_area_new();
    app.vruler = gtk_drawing_area_new();
    app.ruler_corner = gtk_drawing_area_new();
    g_signal_connect(app.ruler_corner, "draw", G_CALLBACK(corner_draw), NULL);
    gtk_widget_set_size_request(app.hruler, -1, RULER);
    gtk_widget_set_size_request(app.vruler, RULER, -1);
    gtk_widget_set_size_request(app.ruler_corner, RULER, RULER);
    g_signal_connect(app.hruler, "draw", G_CALLBACK(ruler_draw), GINT_TO_POINTER(1));
    g_signal_connect(app.vruler, "draw", G_CALLBACK(ruler_draw), GINT_TO_POINTER(0));
    gtk_grid_attach(GTK_GRID(grid), app.ruler_corner, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), app.hruler, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), app.vruler, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), app.scroll, 1, 1, 1, 1);
    gtk_widget_set_no_show_all(app.hruler, TRUE);
    gtk_widget_set_no_show_all(app.vruler, TRUE);
    gtk_widget_set_no_show_all(app.ruler_corner, TRUE);
    return grid;
}
