/* Selections: rectangular and free-form, floating pixels, transparent mode.
 * Mirrors CSelectTool / CFreehandSelectTool / CImgBrush / CImgWnd::Cmd*. */
#include "paint.h"
#include <math.h>
#include <string.h>

/* Erase the selected area of the canvas to colour 2, honouring the mask */
static void erase_area(void)
{
    Selection *s = &app.sel;
    cairo_t *cr = cairo_create(app.img);
    cairo_set_source_rgb(cr, app.color2.r, app.color2.g, app.color2.b);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    if (s->mask) {
        cairo_mask_surface(cr, s->mask, s->x, s->y);
    } else {
        cairo_rectangle(cr, s->x, s->y, s->w, s->h);
        cairo_fill(cr);
    }
    cairo_destroy(cr);
}

/* copy the canvas pixels under the selection into a new surface;
 * outside a free-form mask becomes transparent */
static cairo_surface_t *grab_pixels(void)
{
    Selection *s = &app.sel;
    cairo_surface_t *p = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, s->w, s->h);
    cairo_t *cr = cairo_create(p);
    cairo_set_source_surface(cr, app.img, -s->x, -s->y);
    if (s->mask) cairo_mask_surface(cr, s->mask, 0, 0);
    else cairo_paint(cr);
    cairo_destroy(cr);
    return p;
}

void sel_lift(void)
{
    Selection *s = &app.sel;
    if (!s->active || s->floating) return;
    if (!app.undo_pending) undo_begin();
    s->pix = grab_pixels();
    s->floating = true;
    if (!s->from_paste && !s->hole_made) {
        erase_area();
        s->hole_made = true;
    }
}

/* Render the floating pixels as they will land on the canvas (stretched,
 * and with colour 2 knocked out when "Transparent selection" is on). */
static cairo_surface_t *floating_render(void)
{
    Selection *s = &app.sel;
    int pw = cairo_image_surface_get_width(s->pix), ph = cairo_image_surface_get_height(s->pix);
    cairo_surface_t *o = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, MAX(1, s->w), MAX(1, s->h));
    cairo_t *cr = cairo_create(o);
    cairo_scale(cr, (double)s->w / pw, (double)s->h / ph);
    cairo_set_source_surface(cr, s->pix, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
    cairo_destroy(cr);
    if (app.transparent_sel) {
        cairo_surface_flush(o);
        uint32_t key = rgb_to_px(app.color2);
        uint32_t *p = (uint32_t *)cairo_image_surface_get_data(o);
        int st = cairo_image_surface_get_stride(o) / 4;
        for (int y = 0; y < s->h; y++)
            for (int x = 0; x < s->w; x++)
                if (p[y * st + x] == key) p[y * st + x] = 0;
        cairo_surface_mark_dirty(o);
    }
    return o;
}

static void stamp_floating(void)
{
    Selection *s = &app.sel;
    cairo_surface_t *o = floating_render();
    cairo_t *cr = cairo_create(app.img);
    cairo_set_source_surface(cr, o, s->x, s->y);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(o);
}

void sel_commit(void)
{
    Selection *s = &app.sel;
    if (s->active && s->floating && s->pix) stamp_floating();
    if (app.undo_pending) undo_commit();
}

void sel_clear(bool commit)
{
    Selection *s = &app.sel;
    if (commit) sel_commit();
    if (s->pix) cairo_surface_destroy(s->pix);
    if (s->mask) cairo_surface_destroy(s->mask);
    memset(s, 0, sizeof *s);
    canvas_redraw();
    thumb_refresh();
    update_status();
    update_ui();
}

void sel_select_all(void)
{
    tool_commit_pending();
    sel_clear(true);
    if (app.tool != TOOL_SELECT_RECT && app.tool != TOOL_SELECT_FREE) tool_select(TOOL_SELECT_RECT);
    app.sel.active = true;
    app.sel.x = 0; app.sel.y = 0; app.sel.w = app.w; app.sel.h = app.h;
    canvas_redraw();
    update_status();
    update_ui();
}

/* CSelectTool::InvertSelectRect - everything except the current selection */
void sel_invert(void)
{
    Selection *s = &app.sel;
    if (!s->active || s->floating) {
        if (s->floating) sel_clear(true);
        else sel_select_all();
        return;
    }
    cairo_surface_t *m = cairo_image_surface_create(CAIRO_FORMAT_A8, app.w, app.h);
    cairo_t *cr = cairo_create(m);
    cairo_set_source_rgba(cr, 0, 0, 0, 1);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_DEST_OUT);
    if (s->mask) cairo_mask_surface(cr, s->mask, s->x, s->y);
    else { cairo_rectangle(cr, s->x, s->y, s->w, s->h); cairo_fill(cr); }
    cairo_destroy(cr);
    if (s->mask) cairo_surface_destroy(s->mask);
    s->mask = m;
    s->x = 0; s->y = 0; s->w = app.w; s->h = app.h;
    canvas_redraw();
    update_status();
}

void sel_delete(void)
{
    Selection *s = &app.sel;
    if (!s->active) return;
    if (s->floating) {
        /* drop the floating pixels; the hole (if any) stays */
        if (s->pix) cairo_surface_destroy(s->pix);
        s->pix = NULL;
        s->floating = false;
        if (app.undo_pending) undo_commit();
    } else {
        undo_begin();
        erase_area();
        undo_commit();
    }
    sel_clear(false);
}

void sel_from_surface(cairo_surface_t *src, int x, int y)
{
    tool_commit_pending();
    sel_clear(true);
    if (app.tool != TOOL_SELECT_RECT && app.tool != TOOL_SELECT_FREE) tool_select(TOOL_SELECT_RECT);
    int w = cairo_image_surface_get_width(src), h = cairo_image_surface_get_height(src);
    /* CImgWnd::PasteImageClip: grow the canvas when the pasted image is larger */
    if (x + w > app.w || y + h > app.h) {
        int nw = MAX(app.w, x + w), nh = MAX(app.h, y + h);
        cairo_surface_t *before = surface_copy(app.img);
        cairo_surface_t *n = surface_new(nw, nh, app.color2);
        cairo_t *cr = cairo_create(n);
        cairo_set_source_surface(cr, app.img, 0, 0);
        cairo_paint(cr);
        cairo_destroy(cr);
        set_image(n);
        undo_push_full(before);
    }
    Selection *s = &app.sel;
    s->active = true;
    s->floating = true;
    s->from_paste = true;
    s->x = x; s->y = y; s->w = w; s->h = h;
    s->pix = surface_copy(src);
    undo_begin();
    canvas_redraw();
    update_status();
    update_ui();
}

cairo_surface_t *sel_get_pixels(void)
{
    Selection *s = &app.sel;
    if (!s->active) return surface_copy(app.img);
    if (s->floating) {
        cairo_surface_t *o = floating_render();
        /* fill transparent bits (outside free-form) with white like the clipboard DIB */
        cairo_surface_t *r = surface_new(s->w, s->h, (RGB){1, 1, 1});
        cairo_t *cr = cairo_create(r);
        if (!s->mask) {
            cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        }
        cairo_set_source_surface(cr, o, 0, 0);
        cairo_paint(cr);
        cairo_destroy(cr);
        cairo_surface_destroy(o);
        return r;
    }
    cairo_surface_t *p = grab_pixels();
    cairo_surface_t *r = surface_new(s->w, s->h, (RGB){1, 1, 1});
    cairo_t *cr = cairo_create(r);
    cairo_set_source_surface(cr, p, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(p);
    return r;
}

void sel_apply_transform(int op)
{
    Selection *s = &app.sel;
    if (!s->active) return;
    sel_lift();
    cairo_surface_t *n = NULL;
    /* bake any stretching first */
    int pw = cairo_image_surface_get_width(s->pix), ph = cairo_image_surface_get_height(s->pix);
    if (pw != s->w || ph != s->h) {
        cairo_surface_t *t = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, s->w, s->h);
        cairo_t *cr = cairo_create(t);
        cairo_scale(cr, (double)s->w / pw, (double)s->h / ph);
        cairo_set_source_surface(cr, s->pix, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
        cairo_paint(cr);
        cairo_destroy(cr);
        cairo_surface_destroy(s->pix);
        s->pix = t;
    }
    switch (op) {
    case SEL_OP_ROT_R: n = surface_rotate(s->pix, 1); break;
    case SEL_OP_ROT_L: n = surface_rotate(s->pix, 3); break;
    case SEL_OP_ROT_180: n = surface_rotate(s->pix, 2); break;
    case SEL_OP_FLIP_H: n = surface_flip(s->pix, true); break;
    case SEL_OP_FLIP_V: n = surface_flip(s->pix, false); break;
    case SEL_OP_INVERT:
        surface_invert(s->pix);
        break;
    }
    if (n) {
        cairo_surface_destroy(s->pix);
        s->pix = n;
        s->w = cairo_image_surface_get_width(n);
        s->h = cairo_image_surface_get_height(n);
    }
    canvas_redraw();
    update_status();
}

/* draw floating pixels; cr is in image coordinates */
void sel_draw(cairo_t *cr)
{
    Selection *s = &app.sel;
    if (!s->active || !s->floating || !s->pix) return;
    cairo_surface_t *o = floating_render();
    cairo_save(cr);
    cairo_set_source_surface(cr, o, s->x, s->y);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
    cairo_restore(cr);
    cairo_surface_destroy(o);
}

bool sel_hit(double ix, double iy)
{
    Selection *s = &app.sel;
    return s->active && ix >= s->x && iy >= s->y && ix < s->x + s->w && iy < s->y + s->h;
}

void sel_rect_in_canvas(double *x, double *y, double *w, double *h)
{
    double x0, y0, x1, y1;
    canvas_image_to_widget(app.sel.x, app.sel.y, &x0, &y0);
    canvas_image_to_widget(app.sel.x + app.sel.w, app.sel.y + app.sel.h, &x1, &y1);
    *x = x0; *y = y0; *w = x1 - x0; *h = y1 - y0;
}
