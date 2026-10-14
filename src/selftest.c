/* Developer self-test: LINPAINT_SELFTEST=<dir> runs a scripted sequence of
 * image operations through the real code paths and writes a PNG after each
 * step, then quits.  Used to verify behaviour without driving the GUI. */
#include "paint.h"
#include <math.h>

static int step;
static const char *outdir;

static void snap(const char *name)
{
    tool_commit_pending();
    gchar *p = g_strdup_printf("%s/%02d-%s.png", outdir, step++, name);
    cairo_surface_write_to_png(app.img, p);
    g_print("selftest: %s %dx%d undo=%d redo=%d\n", p, app.w, app.h, app.n_undo, app.n_redo);
    g_free(p);
}

static void draw_test_pattern(void)
{
    undo_begin();
    cairo_t *cr = cairo_create(app.img);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    cairo_set_source_rgb(cr, 0.93, 0.11, 0.14);
    cairo_rectangle(cr, 20, 20, 80, 50);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.13, 0.69, 0.30);
    cairo_arc(cr, 160, 60, 35, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_move_to(cr, 20, 120);
    cairo_set_font_size(cr, 28);
    cairo_show_text(cr, "Paint F");
    cairo_destroy(cr);
    undo_commit();
}

static void select_rect(int x, int y, int w, int h)
{
    tool_select(TOOL_SELECT_RECT);
    tool_press(x, y, 1, 0, 1);
    tool_motion(x + w, y + h, GDK_BUTTON1_MASK);
    tool_release(x + w, y + h, 1, 0);
}

static gboolean run(gpointer d)
{
    (void)d;
    set_image(surface_new(240, 160, (RGB){1, 1, 1}));
    draw_test_pattern();
    snap("pattern");

    select_rect(10, 10, 100, 70);
    image_rotate(1);                    /* rotate selection */
    snap("sel-rot90");
    do_undo();
    snap("undo-sel-rot");

    select_rect(10, 10, 100, 70);
    image_resize_skew(50, 50, 20, 0);   /* resize+skew selection */
    snap("sel-resize-skew");
    do_undo();

    image_flip(true);
    snap("flip-h");
    image_flip(false);
    snap("flip-v");
    do_undo();
    do_undo();
    snap("undo-flips");

    image_invert();
    snap("invert");
    do_undo();

    image_rotate(1);
    snap("rot-right");
    do_undo();
    snap("undo-rot");
    do_redo();
    snap("redo-rot");
    do_undo();

    image_resize_skew(150, 100, 0, 15);
    snap("resize-skew-v");
    do_undo();

    select_rect(120, 20, 80, 80);
    image_crop();
    snap("crop");
    do_undo();
    snap("undo-crop");

    image_set_size(300, 200);
    snap("canvas-grow");
    do_undo();

    /* free-form + transparent selection moved by keyboard */
    tool_select(TOOL_SELECT_FREE);
    tool_press(20, 20, 1, 0, 1);
    double pts[][2] = { { 110, 15 }, { 110, 80 }, { 60, 100 }, { 15, 70 }, { 20, 20 } };
    for (unsigned i = 0; i < G_N_ELEMENTS(pts); i++) tool_motion(pts[i][0], pts[i][1], GDK_BUTTON1_MASK);
    tool_release(20, 20, 1, 0);
    app.transparent_sel = true;
    GdkEventKey ev = { 0 };
    ev.type = GDK_KEY_PRESS;
    ev.keyval = GDK_KEY_Right;
    for (int i = 0; i < 30; i++) tool_key(&ev);
    snap("freeform-moved");
    app.transparent_sel = false;
    do_undo();

    /* select all + delete, then undo */
    sel_select_all();
    sel_delete();
    snap("deleted");
    do_undo();
    snap("undo-delete");

    /* paste a surface bigger than the canvas grows it */
    cairo_surface_t *big = surface_new(300, 60, (RGB){0.2, 0.4, 0.9});
    sel_from_surface(big, 0, 0);
    cairo_surface_destroy(big);
    snap("paste-grow");

    /* every shape outline+fill */
    set_image(surface_new(560, 220, (RGB){1, 1, 1}));
    undo_clear();
    tool_select(TOOL_SHAPE);
    app.fill = MEDIA_SOLID;
    app.outline = MEDIA_SOLID;
    app.size_px[TOOL_SHAPE] = 3;
    app.color1 = (RGB){0.25, 0.28, 0.8};
    app.color2 = (RGB){1, 0.95, 0};
    for (int k = SHAPE_OVAL; k < SHAPE_COUNT; k++) {
        if (k == SHAPE_POLYGON) continue;
        app.shape_kind = k;
        int i = k - SHAPE_OVAL, x = 10 + (i % 7) * 78, y = 10 + (i / 7) * 70;
        tool_press(x, y, 1, 0, 1);
        tool_motion(x + 60, y + 55, GDK_BUTTON1_MASK);
        tool_release(x + 60, y + 55, 1, 0);
        tool_commit_pending();
    }
    snap("all-shapes");

    /* every outline medium on a star */
    set_image(surface_new(560, 100, (RGB){1, 1, 1}));
    app.shape_kind = SHAPE_STAR5;
    app.fill = MEDIA_NONE;
    app.size_px[TOOL_SHAPE] = 5;
    for (int m = MEDIA_SOLID; m < MEDIA_COUNT; m++) {
        app.outline = m;
        int x = 10 + (m - 1) * 90;
        tool_press(x, 10, 1, 0, 1);
        tool_motion(x + 80, 90, GDK_BUTTON1_MASK);
        tool_release(x + 80, 90, 1, 0);
        tool_commit_pending();
    }
    snap("outline-media");
    for (int m = MEDIA_SOLID; m < MEDIA_COUNT; m++) {
        app.fill = m;
        app.outline = MEDIA_NONE;
        int x = 10 + (m - 1) * 90;
        tool_press(x, 10, 1, 0, 1);
        tool_motion(x + 80, 90, GDK_BUTTON1_MASK);
        tool_release(x + 80, 90, 1, 0);
        tool_commit_pending();
    }
    snap("fill-media");

    app.dirty = false;
    gtk_widget_destroy(app.window);
    return G_SOURCE_REMOVE;
}

void selftest_schedule(void)
{
    outdir = g_getenv("LINPAINT_SELFTEST");
    if (!outdir) return;
    g_mkdir_with_parents(outdir, 0755);
    g_timeout_add(500, run, NULL);
}
