/* Whole-image operations and undo history.
 * Mirrors CBitmapObj / CUndoBmObj / CImgWnd::CmdRot etc. */
#include "paint.h"
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* helpers                                                            */

uint32_t rgb_to_px(RGB c)
{
    int r = (int)lround(c.r * 255), g = (int)lround(c.g * 255), b = (int)lround(c.b * 255);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

RGB px_to_rgb(uint32_t p)
{
    unsigned a = p >> 24;
    if (a == 0) return (RGB){1, 1, 1};
    RGB c = { ((p >> 16) & 255) / 255.0, ((p >> 8) & 255) / 255.0, (p & 255) / 255.0 };
    if (a != 255) { c.r = c.r * 255 / a; c.g = c.g * 255 / a; c.b = c.b * 255 / a; }
    return c;
}

cairo_surface_t *surface_new(int w, int h, RGB fill)
{
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t *cr = cairo_create(s);
    cairo_set_source_rgb(cr, fill.r, fill.g, fill.b);
    cairo_paint(cr);
    cairo_destroy(cr);
    return s;
}

cairo_surface_t *surface_copy(cairo_surface_t *s)
{
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    cairo_surface_t *d = cairo_image_surface_create(cairo_image_surface_get_format(s), w, h);
    cairo_surface_flush(s);
    int ss = cairo_image_surface_get_stride(s), ds = cairo_image_surface_get_stride(d);
    unsigned char *sp = cairo_image_surface_get_data(s), *dp = cairo_image_surface_get_data(d);
    for (int y = 0; y < h; y++) memcpy(dp + y * ds, sp + y * ss, MIN(ss, ds));
    cairo_surface_mark_dirty(d);
    return d;
}

void set_image(cairo_surface_t *s)
{
    if (app.img && app.img != s) cairo_surface_destroy(app.img);
    app.img = s;
    app.w = cairo_image_surface_get_width(s);
    app.h = cairo_image_surface_get_height(s);
    canvas_update_size();
    canvas_redraw();
    thumb_refresh();
    update_status();
}

/* ------------------------------------------------------------------ */
/* undo: each step stores the changed rectangle before & after, or whole
 * images when the canvas size changed.                                */

struct UndoStep {
    int x, y;
    cairo_surface_t *before, *after;    /* region pixels */
    bool full;                          /* before/after are whole images */
};

static void step_free(UndoStep *s)
{
    if (!s) return;
    if (s->before) cairo_surface_destroy(s->before);
    if (s->after) cairo_surface_destroy(s->after);
    g_free(s);
}

static cairo_surface_t *sub_copy(cairo_surface_t *src, int x, int y, int w, int h)
{
    cairo_surface_t *d = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t *cr = cairo_create(d);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr, src, -x, -y);
    cairo_paint(cr);
    cairo_destroy(cr);
    return d;
}

static void push_step(UndoStep *st)
{
    if (g_getenv("LINPAINT_DEBUG"))
        g_printerr("push step full=%d x=%d y=%d %dx%d\n", st->full, st->x, st->y,
                   cairo_image_surface_get_width(st->before), cairo_image_surface_get_height(st->before));
    for (int i = 0; i < app.n_redo; i++) step_free(app.redo[i]);
    app.n_redo = 0;
    if (app.n_undo == UNDO_LEVELS) {
        step_free(app.undo[0]);
        memmove(app.undo, app.undo + 1, sizeof(UndoStep *) * (UNDO_LEVELS - 1));
        app.n_undo--;
    }
    app.undo[app.n_undo++] = st;
    mark_dirty();
    update_ui();
}

void undo_begin(void)
{
    if (app.undo_pending) cairo_surface_destroy(app.undo_pending);
    app.undo_pending = surface_copy(app.img);
}

void undo_cancel(void)
{
    if (app.undo_pending) {
        cairo_surface_destroy(app.undo_pending);
        app.undo_pending = NULL;
    }
}

void undo_commit(void)
{
    cairo_surface_t *b = app.undo_pending;
    if (!b) return;
    app.undo_pending = NULL;
    if (cairo_image_surface_get_width(b) != app.w || cairo_image_surface_get_height(b) != app.h) {
        undo_push_full(b);
        return;
    }
    cairo_surface_flush(app.img);
    uint32_t *p0 = (uint32_t *)cairo_image_surface_get_data(b);
    uint32_t *p1 = (uint32_t *)cairo_image_surface_get_data(app.img);
    int st0 = cairo_image_surface_get_stride(b) / 4, st1 = cairo_image_surface_get_stride(app.img) / 4;
    int minx = app.w, miny = app.h, maxx = -1, maxy = -1;
    for (int y = 0; y < app.h; y++) {
        uint32_t *r0 = p0 + y * st0, *r1 = p1 + y * st1;
        if (!memcmp(r0, r1, app.w * 4)) continue;
        if (y < miny) miny = y;
        maxy = y;
        for (int x = 0; x < app.w; x++)
            if (r0[x] != r1[x]) { if (x < minx) minx = x; break; }
        for (int x = app.w - 1; x >= 0; x--)
            if (r0[x] != r1[x]) { if (x > maxx) maxx = x; break; }
    }
    if (maxx < 0) { cairo_surface_destroy(b); return; }  /* nothing changed */
    UndoStep *s = g_new0(UndoStep, 1);
    s->x = minx; s->y = miny;
    s->before = sub_copy(b, minx, miny, maxx - minx + 1, maxy - miny + 1);
    s->after = sub_copy(app.img, minx, miny, maxx - minx + 1, maxy - miny + 1);
    cairo_surface_destroy(b);
    push_step(s);
}

void undo_push_full(cairo_surface_t *before)
{
    UndoStep *s = g_new0(UndoStep, 1);
    s->full = true;
    s->before = before;
    s->after = surface_copy(app.img);
    push_step(s);
}

static void apply_step(UndoStep *s, bool undo)
{
    if (g_getenv("LINPAINT_DEBUG"))
        g_printerr("apply %s full=%d x=%d y=%d\n", undo ? "undo" : "redo", s->full, s->x, s->y);
    cairo_surface_t *src = undo ? s->before : s->after;
    if (s->full) {
        set_image(surface_copy(src));
        return;
    }
    cairo_t *cr = cairo_create(app.img);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr, src, s->x, s->y);
    /* SOURCE + paint would clear everything outside the region */
    cairo_rectangle(cr, s->x, s->y, cairo_image_surface_get_width(src), cairo_image_surface_get_height(src));
    cairo_fill(cr);
    cairo_destroy(cr);
    canvas_redraw();
    thumb_refresh();
}

void do_undo(void)
{
    tool_commit_pending();
    if (app.n_undo == 0) return;
    UndoStep *s = app.undo[--app.n_undo];
    apply_step(s, true);
    app.redo[app.n_redo++] = s;
    app.dirty = true;
    update_title();
    update_ui();
}

void do_redo(void)
{
    tool_commit_pending();
    if (app.n_redo == 0) return;
    UndoStep *s = app.redo[--app.n_redo];
    apply_step(s, false);
    app.undo[app.n_undo++] = s;
    app.dirty = true;
    update_title();
    update_ui();
}

void undo_clear(void)
{
    for (int i = 0; i < app.n_undo; i++) step_free(app.undo[i]);
    for (int i = 0; i < app.n_redo; i++) step_free(app.redo[i]);
    app.n_undo = app.n_redo = 0;
    undo_cancel();
}

/* ------------------------------------------------------------------ */
/* pure surface transforms                                             */

cairo_surface_t *surface_rotate(cairo_surface_t *s, int q)
{
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    q = ((q % 4) + 4) % 4;
    int nw = (q % 2) ? h : w, nh = (q % 2) ? w : h;
    cairo_surface_t *d = cairo_image_surface_create(cairo_image_surface_get_format(s), nw, nh);
    cairo_t *cr = cairo_create(d);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_translate(cr, nw / 2.0, nh / 2.0);
    cairo_rotate(cr, q * M_PI / 2);
    cairo_translate(cr, -w / 2.0, -h / 2.0);
    cairo_set_source_surface(cr, s, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
    cairo_destroy(cr);
    return d;
}

cairo_surface_t *surface_flip(cairo_surface_t *s, bool horiz)
{
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    cairo_surface_t *d = cairo_image_surface_create(cairo_image_surface_get_format(s), w, h);
    cairo_t *cr = cairo_create(d);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    if (horiz) { cairo_translate(cr, w, 0); cairo_scale(cr, -1, 1); }
    else { cairo_translate(cr, 0, h); cairo_scale(cr, 1, -1); }
    cairo_set_source_surface(cr, s, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
    cairo_destroy(cr);
    return d;
}

void surface_invert(cairo_surface_t *s)
{
    cairo_surface_flush(s);
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    int st = cairo_image_surface_get_stride(s) / 4;
    uint32_t *p = (uint32_t *)cairo_image_surface_get_data(s);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint32_t v = p[y * st + x];
            uint32_t a = v >> 24;
            /* premultiplied: inverted channel = a - c */
            uint32_t r = a - ((v >> 16) & 255), g = a - ((v >> 8) & 255), b = a - (v & 255);
            p[y * st + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    cairo_surface_mark_dirty(s);
}

/* Stretch to nw x nh then skew; skew angles in degrees (-89..89).
 * Matches CStretchSkewDlg + CImgWnd::CmdSkewBrush: skew enlarges the
 * bounding box and the uncovered area takes the background colour. */
cairo_surface_t *surface_stretch_skew(cairo_surface_t *s, int nw, int nh, double skh, double skv,
                                      RGB bg, bool transparent_bg)
{
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    double th = tan(skh * M_PI / 180), tv = tan(skv * M_PI / 180);
    int ow = nw + (int)ceil(fabs(th) * nh), oh = nh + (int)ceil(fabs(tv) * nw);
    if (ow < 1) ow = 1;
    if (oh < 1) oh = 1;
    cairo_surface_t *d = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ow, oh);
    cairo_t *cr = cairo_create(d);
    if (!transparent_bg) {
        cairo_set_source_rgb(cr, bg.r, bg.g, bg.b);
        cairo_paint(cr);
    }
    /* x' = x + th*y ; y' = y + tv*x, shifted so everything is positive */
    double ox = th < 0 ? -th * nh : 0, oy = tv < 0 ? -tv * nw : 0;
    cairo_matrix_t m;
    cairo_matrix_init(&m, 1, tv, th, 1, ox, oy);
    cairo_transform(cr, &m);
    cairo_scale(cr, (double)nw / w, (double)nh / h);
    cairo_set_source_surface(cr, s, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr),
                             (nw >= w && nh >= h && skh == 0 && skv == 0) ? CAIRO_FILTER_NEAREST
                                                                          : CAIRO_FILTER_GOOD);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_fill(cr);
    cairo_destroy(cr);
    return d;
}

/* ------------------------------------------------------------------ */
/* image-level commands (apply to selection when one exists)           */

static void replace_image_with_undo(cairo_surface_t *n)
{
    cairo_surface_t *before = surface_copy(app.img);
    set_image(n);
    undo_push_full(before);
}

void image_rotate(int q)
{
    if (tool_transform_live(q == 1 ? SEL_OP_ROT_R : q == 3 ? SEL_OP_ROT_L : SEL_OP_ROT_180)) return;
    if (!app.sel.active) tool_commit_pending();
    if (app.sel.active) {
        sel_apply_transform(q == 1 ? SEL_OP_ROT_R : q == 3 ? SEL_OP_ROT_L : SEL_OP_ROT_180);
        return;
    }
    replace_image_with_undo(surface_rotate(app.img, q));
}

void image_flip(bool horiz)
{
    if (tool_transform_live(horiz ? SEL_OP_FLIP_H : SEL_OP_FLIP_V)) return;
    if (!app.sel.active) tool_commit_pending();
    if (app.sel.active) {
        sel_apply_transform(horiz ? SEL_OP_FLIP_H : SEL_OP_FLIP_V);
        return;
    }
    undo_begin();
    cairo_surface_t *n = surface_flip(app.img, horiz);
    cairo_t *cr = cairo_create(app.img);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr, n, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(n);
    undo_commit();
    canvas_redraw();
    thumb_refresh();
}

void image_invert(void)
{
    if (!app.sel.active) tool_commit_pending();
    if (app.sel.active) {
        sel_apply_transform(SEL_OP_INVERT);
        return;
    }
    undo_begin();
    surface_invert(app.img);
    undo_commit();
    canvas_redraw();
    thumb_refresh();
}

void image_crop(void)
{
    if (!app.sel.active) return;
    if (!app.sel.floating) sel_lift();
    cairo_surface_t *px = sel_get_pixels();
    /* CImgWnd::CmdCrop: canvas becomes the selection, with the (unselected
     * part of a free-form selection) filled with the secondary colour */
    cairo_surface_t *n = surface_new(cairo_image_surface_get_width(px),
                                     cairo_image_surface_get_height(px), app.color2);
    cairo_t *cr = cairo_create(n);
    cairo_set_source_surface(cr, px, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(px);
    sel_clear(false);
    /* undo snapshot must be the image before the hole was cut */
    cairo_surface_t *before = app.undo_pending ? app.undo_pending : surface_copy(app.img);
    app.undo_pending = NULL;
    set_image(n);
    undo_push_full(before);
}

void image_set_size(int w, int h)
{
    tool_commit_pending();
    if (w == app.w && h == app.h) return;
    cairo_surface_t *n = surface_new(w, h, app.color2);
    cairo_t *cr = cairo_create(n);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr, app.img, 0, 0);
    cairo_rectangle(cr, 0, 0, MIN(w, app.w), MIN(h, app.h));
    cairo_fill(cr);
    cairo_destroy(cr);
    replace_image_with_undo(n);
}

void image_resize_skew(double sxp, double syp, double skh, double skv)
{
    if (!app.sel.active) tool_commit_pending();
    if (app.sel.active) {
        if (!app.sel.floating) sel_lift();
        cairo_surface_t *px = sel_get_pixels();
        int nw = MAX(1, (int)lround(app.sel.w * sxp / 100)), nh = MAX(1, (int)lround(app.sel.h * syp / 100));
        cairo_surface_t *n = surface_stretch_skew(px, nw, nh, skh, skv, app.color2, true);
        cairo_surface_destroy(px);
        int x = app.sel.x, y = app.sel.y;
        bool paste = app.sel.from_paste, hole = app.sel.hole_made;
        if (app.sel.pix) cairo_surface_destroy(app.sel.pix);
        if (app.sel.mask) cairo_surface_destroy(app.sel.mask);
        app.sel.pix = n;
        app.sel.mask = NULL;
        app.sel.x = x; app.sel.y = y;
        app.sel.w = cairo_image_surface_get_width(n);
        app.sel.h = cairo_image_surface_get_height(n);
        app.sel.from_paste = paste; app.sel.hole_made = hole;
        canvas_redraw();
        return;
    }
    int nw = MAX(1, (int)lround(app.w * sxp / 100)), nh = MAX(1, (int)lround(app.h * syp / 100));
    if (nw > MAX_IMAGE_DIM || nh > MAX_IMAGE_DIM) {
        error_dialog("The image is too large. Choose smaller values.");
        return;
    }
    replace_image_with_undo(surface_stretch_skew(app.img, nw, nh, skh, skv, app.color2, false));
}

void image_to_monochrome(void)
{
    /* CImageAttr: switching to "Black and white" is a 1bpp conversion with a
     * 50% luminance threshold; it cannot be undone (the dialog warns). */
    cairo_surface_flush(app.img);
    int st = cairo_image_surface_get_stride(app.img) / 4;
    uint32_t *p = (uint32_t *)cairo_image_surface_get_data(app.img);
    for (int y = 0; y < app.h; y++)
        for (int x = 0; x < app.w; x++) {
            RGB c = px_to_rgb(p[y * st + x]);
            double l = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
            p[y * st + x] = l >= 0.5 ? 0xFFFFFFFF : 0xFF000000;
        }
    cairo_surface_mark_dirty(app.img);
    undo_clear();
    app.monochrome = true;
    mark_dirty();
    canvas_redraw();
    thumb_refresh();
}
