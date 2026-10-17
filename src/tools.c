/* Tools: pencil, fill, text, eraser, colour picker, magnifier, selection,
 * brushes and shapes.  Each section notes the mspaint class it mirrors. */
#include "paint.h"
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* size lists recovered from CPBApp::AddTools / tool constructors       */

static const int sz_pencil[] = { 1, 2, 3, 4 };
static const int sz_brush[] = { 1, 3, 5, 8 };
static const int sz_callig[] = { 3, 5, 8, 10 };
static const int sz_air[] = { 4, 8, 16, 24 };
static const int sz_media[] = { 8, 16, 30, 40 };
static const int sz_natpencil[] = { 4, 6, 8, 10 };
static const int sz_eraser[] = { 4, 6, 8, 10 };
static const int sz_shape[] = { 1, 3, 5, 8 };

const int *tool_size_list(int *n)
{
    *n = 4;
    switch (app.tool) {
    case TOOL_PENCIL: return sz_pencil;
    case TOOL_ERASER: return sz_eraser;
    case TOOL_SHAPE: return sz_shape;
    case TOOL_BRUSH:
        switch (app.brush_kind) {
        case BRUSH_REGULAR: return sz_brush;
        case BRUSH_CALLIG1: case BRUSH_CALLIG2: return sz_callig;
        case BRUSH_AIRBRUSH: return sz_air;
        case BRUSH_NATPENCIL: return sz_natpencil;
        default: return sz_media;
        }
    default:
        *n = 0;
        return NULL;
    }
}

bool tool_has_size(void)
{
    int n;
    tool_size_list(&n);
    return n > 0;
}

int tool_cur_size(void)
{
    if (app.tool == TOOL_BRUSH) return app.brush_size_px[app.brush_kind];
    return app.size_px[app.tool];
}

void shape_refresh(void);

void tool_set_size(int px)
{
    px = CLAMP(px, 1, 50);   /* CImgTool::SetStrokeWidth rejects >= 0x33 */
    if (app.tool == TOOL_BRUSH) app.brush_size_px[app.brush_kind] = px;
    else app.size_px[app.tool] = px;
    shape_refresh();
    canvas_redraw();
    update_ui();
}

/* CPBView::OnLargerbrush / OnSmallerbrush (Ctrl +/-) */
void tool_size_step(int dir)
{
    if (!tool_has_size()) return;
    tool_set_size(tool_cur_size() + dir);
}

/* ------------------------------------------------------------------ */
/* generic tracker (CImgWnd::DrawTracker / AdjustTrackerRect)           */

typedef struct { double x, y, w, h; } Rect;

static void handle_pos(Rect r, int i, double *hx, double *hy)
{
    double xs[] = { r.x, r.x + r.w / 2, r.x + r.w, r.x + r.w, r.x + r.w, r.x + r.w / 2, r.x, r.x };
    double ys[] = { r.y, r.y, r.y, r.y + r.h / 2, r.y + r.h, r.y + r.h, r.y + r.h, r.y + r.h / 2 };
    *hx = xs[i];
    *hy = ys[i];
}

/* r in image coords; returns handle 0..7, 8 inside, -1 outside */
static int tracker_hit(Rect r, double ix, double iy)
{
    double tol = 4 / app.zoom;
    for (int i = 0; i < 8; i++) {
        double hx, hy;
        handle_pos(r, i, &hx, &hy);
        if (fabs(ix - hx) <= tol && fabs(iy - hy) <= tol) return i;
    }
    if (ix >= r.x && iy >= r.y && ix <= r.x + r.w && iy <= r.y + r.h) return 8;
    return -1;
}

static Rect tracker_apply(int h, Rect r0, double dx, double dy)
{
    double x0 = r0.x, y0 = r0.y, x1 = r0.x + r0.w, y1 = r0.y + r0.h;
    switch (h) {
    case 0: x0 += dx; y0 += dy; break;
    case 1: y0 += dy; break;
    case 2: x1 += dx; y0 += dy; break;
    case 3: x1 += dx; break;
    case 4: x1 += dx; y1 += dy; break;
    case 5: y1 += dy; break;
    case 6: x0 += dx; y1 += dy; break;
    case 7: x0 += dx; break;
    case 8: x0 += dx; x1 += dx; y0 += dy; y1 += dy; break;
    }
    return (Rect){ x0, y0, x1 - x0, y1 - y0 };
}

static Rect rect_norm(Rect r)
{
    if (r.w < 0) { r.x += r.w; r.w = -r.w; }
    if (r.h < 0) { r.y += r.h; r.h = -r.h; }
    return r;
}

static void draw_tracker(cairo_t *cr, Rect r, bool handles)
{
    double x0, y0, x1, y1;
    canvas_image_to_widget(r.x, r.y, &x0, &y0);
    canvas_image_to_widget(r.x + r.w, r.y + r.h, &x1, &y1);
    double dash[] = { 3, 3 };
    cairo_save(cr);
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, floor(x0) - 0.5, floor(y0) - 0.5, floor(x1) - floor(x0) + 1, floor(y1) - floor(y0) + 1);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_stroke_preserve(cr);
    cairo_set_dash(cr, dash, 2, 0);
    cairo_set_source_rgb(cr, 0.2, 0.4, 0.8);
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0);
    if (handles) {
        Rect w = { x0, y0, x1 - x0, y1 - y0 };
        for (int i = 0; i < 8; i++) {
            double hx, hy;
            handle_pos(w, i, &hx, &hy);
            cairo_rectangle(cr, floor(hx) - 2.5, floor(hy) - 2.5, 5, 5);
            cairo_set_source_rgb(cr, 1, 1, 1);
            cairo_fill_preserve(cr);
            cairo_set_source_rgb(cr, 0.33, 0.4, 0.5);
            cairo_stroke(cr);
        }
    }
    cairo_restore(cr);
}

/* ------------------------------------------------------------------ */
/* pixel helpers                                                       */

static void plot_square(uint32_t *p, int stride, int cx, int cy, int size, uint32_t col,
                        bool replace_only, uint32_t match)
{
    int x0 = cx - (size - 1) / 2, y0 = cy - (size - 1) / 2;
    for (int y = MAX(0, y0); y < MIN(app.h, y0 + size); y++)
        for (int x = MAX(0, x0); x < MIN(app.w, x0 + size); x++) {
            uint32_t *q = &p[y * stride + x];
            if (!replace_only || *q == match) *q = col;
        }
}

/* Bresenham line with a square nib (DrawImgLine) */
static void plot_line(int x0, int y0, int x1, int y1, int size, uint32_t col, bool replace_only, uint32_t match)
{
    cairo_surface_flush(app.img);
    uint32_t *p = (uint32_t *)cairo_image_surface_get_data(app.img);
    int stride = cairo_image_surface_get_stride(app.img) / 4;
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        plot_square(p, stride, x0, y0, size, col, replace_only, match);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
    cairo_surface_mark_dirty(app.img);
    int r = size + 1;
    canvas_redraw_rect(MIN(x0, x1) - r, MIN(y0, y1) - r, abs(x1 - x0) + 2 * r, abs(y1 - y0) + 2 * r);
}

/* CFloodTool: ExtFloodFill(FLOODFILLSURFACE) - exact colour match, 4-connected */
static void flood_fill(int sx, int sy, uint32_t col)
{
    if (sx < 0 || sy < 0 || sx >= app.w || sy >= app.h) return;
    cairo_surface_flush(app.img);
    uint32_t *p = (uint32_t *)cairo_image_surface_get_data(app.img);
    int st = cairo_image_surface_get_stride(app.img) / 4;
    uint32_t target = p[sy * st + sx];
    if (target == col) return;
    GArray *stack = g_array_new(FALSE, FALSE, sizeof(int) * 2);
    int seed[2] = { sx, sy };
    g_array_append_val(stack, seed);
    while (stack->len) {
        int *s = &g_array_index(stack, int, (stack->len - 1) * 2);
        int x = s[0], y = s[1];
        g_array_set_size(stack, stack->len - 1);
        uint32_t *row = p + y * st;
        if (row[x] != target) continue;
        int l = x, r = x;
        while (l > 0 && row[l - 1] == target) l--;
        while (r < app.w - 1 && row[r + 1] == target) r++;
        for (int i = l; i <= r; i++) row[i] = col;
        for (int dy = -1; dy <= 1; dy += 2) {
            int ny = y + dy;
            if (ny < 0 || ny >= app.h) continue;
            uint32_t *nr = p + ny * st;
            bool in = false;
            for (int i = l; i <= r; i++) {
                if (nr[i] == target) {
                    if (!in) { int pt[2] = { i, ny }; g_array_append_val(stack, pt); in = true; }
                } else in = false;
            }
        }
    }
    g_array_free(stack, TRUE);
    cairo_surface_mark_dirty(app.img);
    canvas_redraw();
}

static RGB pick_color(int x, int y)
{
    if (x < 0 || y < 0 || x >= app.w || y >= app.h) return app.color1;
    cairo_surface_flush(app.img);
    uint32_t *p = (uint32_t *)cairo_image_surface_get_data(app.img);
    return px_to_rgb(p[y * (cairo_image_surface_get_stride(app.img) / 4) + x]);
}

/* ------------------------------------------------------------------ */
/* live shapes (CShapeTool states: Init -> Creating -> Modifying)       */

#define MAXPTS 4096
static struct {
    bool active;
    bool creating;
    bool closed_poly;
    ShapeKind kind;
    double x0, y0, x1, y1;           /* template rect (unnormalised => flipped) */
    int rot;                         /* quarter turns */
    double pts[MAXPTS][2];           /* line/curve/polygon, absolute */
    int npts;
    int curve_stage;                 /* 0 creating, 1 waiting bend 1, 2 waiting bend 2 */
    bool swap;                       /* drawn with right button */
    int handle;                      /* drag state */
    Rect r0;
    double pts0[MAXPTS][2];
    double px, py;
} sh;

static bool is_point_shape(ShapeKind k) { return k == SHAPE_LINE || k == SHAPE_CURVE || k == SHAPE_POLYGON; }

static Rect shape_bbox(void)
{
    if (!is_point_shape(sh.kind)) {
        double w = sh.x1 - sh.x0, h = sh.y1 - sh.y0;
        if (sh.rot & 1) {
            double cx = (sh.x0 + sh.x1) / 2, cy = (sh.y0 + sh.y1) / 2;
            return rect_norm((Rect){ cx - h / 2, cy - w / 2, h, w });
        }
        return rect_norm((Rect){ sh.x0, sh.y0, w, h });
    }
    double a = 1e9, b = 1e9, c = -1e9, d = -1e9;
    for (int i = 0; i < sh.npts; i++) {
        a = MIN(a, sh.pts[i][0]); b = MIN(b, sh.pts[i][1]);
        c = MAX(c, sh.pts[i][0]); d = MAX(d, sh.pts[i][1]);
    }
    return (Rect){ a, b, c - a, d - b };
}

static void shape_path(cairo_t *cr)
{
    cairo_new_path(cr);
    double o = (tool_cur_size() & 1) ? 0.5 : 0;   /* pixel-centre alignment for odd widths */
    if (sh.kind == SHAPE_LINE) {
        cairo_move_to(cr, sh.pts[0][0] + o, sh.pts[0][1] + o);
        cairo_line_to(cr, sh.pts[1][0] + o, sh.pts[1][1] + o);
    } else if (sh.kind == SHAPE_CURVE) {
        cairo_move_to(cr, sh.pts[0][0] + o, sh.pts[0][1] + o);
        cairo_curve_to(cr, sh.pts[1][0] + o, sh.pts[1][1] + o, sh.pts[2][0] + o, sh.pts[2][1] + o,
                       sh.pts[3][0] + o, sh.pts[3][1] + o);
    } else if (sh.kind == SHAPE_POLYGON) {
        for (int i = 0; i < sh.npts; i++)
            if (i == 0) cairo_move_to(cr, sh.pts[i][0] + o, sh.pts[i][1] + o);
            else cairo_line_to(cr, sh.pts[i][0] + o, sh.pts[i][1] + o);
        if (sh.closed_poly) cairo_close_path(cr);
    } else {
        double cx = (sh.x0 + sh.x1) / 2, cy = (sh.y0 + sh.y1) / 2;
        cairo_save(cr);
        cairo_translate(cr, cx + o, cy + o);
        cairo_rotate(cr, sh.rot * M_PI / 2);
        cairo_translate(cr, -cx, -cy);
        shape_build_path(cr, sh.kind, sh.x0, sh.y0, sh.x1, sh.y1);
        cairo_restore(cr);
    }
}

static void shape_render(void)
{
    if (!sh.active || !app.undo_pending) return;
    cairo_t *cr = cairo_create(app.img);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr, app.undo_pending, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_antialias(cr, app.antialias ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE);
    shape_path(cr);
    cairo_path_t *path = cairo_copy_path(cr);
    cairo_new_path(cr);
    RGB oc = sh.swap ? app.color2 : app.color1, fc = sh.swap ? app.color1 : app.color2;
    bool open = sh.kind == SHAPE_LINE || sh.kind == SHAPE_CURVE || (sh.kind == SHAPE_POLYGON && !sh.closed_poly);
    if (!open) media_fill_path(cr, app.fill, fc, path);
    MediaKind om = app.outline;
    if (open && om == MEDIA_NONE) om = MEDIA_SOLID;
    media_stroke_path(cr, om, oc, tool_cur_size(), path, !open);
    cairo_path_destroy(path);
    cairo_destroy(cr);
    canvas_redraw();
}

void shape_refresh(void)
{
    if (sh.active) shape_render();
}

static void shape_commit(void)
{
    if (!sh.active) return;
    shape_render();
    sh.active = false;
    sh.creating = false;
    undo_commit();
    canvas_redraw();
    thumb_refresh();
}

static void shape_cancel(void)
{
    if (!sh.active) return;
    if (app.undo_pending) {
        cairo_t *cr = cairo_create(app.img);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface(cr, app.undo_pending, 0, 0);
        cairo_paint(cr);
        cairo_destroy(cr);
    }
    undo_cancel();
    sh.active = false;
    canvas_redraw();
}

/* rotate / flip the live shape (CShapeToolModifyingState::OnRotate/OnFlip*) */
static bool shape_transform(int op)
{
    if (!sh.active || sh.creating) return false;
    Rect b = shape_bbox();
    double cx = b.x + b.w / 2, cy = b.y + b.h / 2;
    if (is_point_shape(sh.kind)) {
        for (int i = 0; i < sh.npts; i++) {
            double x = sh.pts[i][0] - cx, y = sh.pts[i][1] - cy, nx = x, ny = y;
            switch (op) {
            case SEL_OP_ROT_R: nx = -y; ny = x; break;
            case SEL_OP_ROT_L: nx = y; ny = -x; break;
            case SEL_OP_ROT_180: nx = -x; ny = -y; break;
            case SEL_OP_FLIP_H: nx = -x; break;
            case SEL_OP_FLIP_V: ny = -y; break;
            }
            sh.pts[i][0] = cx + nx;
            sh.pts[i][1] = cy + ny;
        }
    } else {
        switch (op) {
        case SEL_OP_ROT_R: sh.rot = (sh.rot + 1) & 3; break;
        case SEL_OP_ROT_L: sh.rot = (sh.rot + 3) & 3; break;
        case SEL_OP_ROT_180: sh.rot = (sh.rot + 2) & 3; break;
        case SEL_OP_FLIP_H:
            if (sh.rot & 1) { double t = sh.y0; sh.y0 = sh.y1; sh.y1 = t; }
            else { double t = sh.x0; sh.x0 = sh.x1; sh.x1 = t; }
            break;
        case SEL_OP_FLIP_V:
            if (sh.rot & 1) { double t = sh.x0; sh.x0 = sh.x1; sh.x1 = t; }
            else { double t = sh.y0; sh.y0 = sh.y1; sh.y1 = t; }
            break;
        default: return false;
        }
    }
    shape_render();
    return true;
}

/* resize the live shape so its bounding box becomes r (from r0) */
static void shape_set_bbox(Rect r0, Rect r)
{
    if (is_point_shape(sh.kind)) {
        for (int i = 0; i < sh.npts; i++) {
            double u = r0.w ? (sh.pts0[i][0] - r0.x) / r0.w : 0, v = r0.h ? (sh.pts0[i][1] - r0.y) / r0.h : 0;
            sh.pts[i][0] = r.x + u * r.w;
            sh.pts[i][1] = r.y + v * r.h;
        }
        return;
    }
    if (sh.rot & 1) {
        double cx = r.x + r.w / 2, cy = r.y + r.h / 2;
        bool fx = sh.x1 < sh.x0, fy = sh.y1 < sh.y0;
        double hw = fabs(r.h) / 2, hh = fabs(r.w) / 2;
        sh.x0 = cx - (fx ? -hw : hw); sh.x1 = cx + (fx ? -hw : hw);
        sh.y0 = cy - (fy ? -hh : hh); sh.y1 = cy + (fy ? -hh : hh);
        return;
    }
    bool fx = sh.x1 < sh.x0, fy = sh.y1 < sh.y0;
    sh.x0 = fx ? r.x + r.w : r.x; sh.x1 = fx ? r.x : r.x + r.w;
    sh.y0 = fy ? r.y + r.h : r.y; sh.y1 = fy ? r.y : r.y + r.h;
}

/* ------------------------------------------------------------------ */
/* text box (CTextTool / CTedit)                                        */

static struct {
    bool active;
    Rect r;
    GString *s;
    int cur, anchor;
    bool creating;
    int handle;
    Rect r0;
    double px, py;
    bool dragsel;
} tx;
static GtkIMContext *im;

bool text_active(void) { return tx.active; }

static PangoLayout *text_layout(cairo_t *cr)
{
    PangoLayout *l = pango_cairo_create_layout(cr);
    PangoFontDescription *fd = pango_font_description_new();
    pango_font_description_set_family(fd, app.font_family);
    pango_font_description_set_weight(fd, app.bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
    pango_font_description_set_style(fd, app.italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
    /* point size at the image's 96 dpi */
    pango_font_description_set_absolute_size(fd, app.font_size * 96.0 / 72.0 * PANGO_SCALE);
    pango_layout_set_font_description(l, fd);
    pango_font_description_free(fd);
    PangoAttrList *al = pango_attr_list_new();
    if (app.underline) pango_attr_list_insert(al, pango_attr_underline_new(PANGO_UNDERLINE_SINGLE));
    if (app.strike) pango_attr_list_insert(al, pango_attr_strikethrough_new(TRUE));
    pango_layout_set_attributes(l, al);
    pango_attr_list_unref(al);
    pango_layout_set_wrap(l, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_width(l, MAX(1, (int)(tx.r.w - 4)) * PANGO_SCALE);
    pango_layout_set_text(l, tx.s->str, -1);
    return l;
}

/* the box grows downward to fit its text, like the RichEdit control */
static void text_fit(void)
{
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1);
    cairo_t *cr = cairo_create(s);
    PangoLayout *l = text_layout(cr);
    int lw, lh;
    pango_layout_get_pixel_size(l, &lw, &lh);
    double line = app.font_size * 96.0 / 72.0 * 1.3;
    double need = MAX(lh, line) + 4;
    if (tx.r.h < need) tx.r.h = need;
    g_object_unref(l);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
}

static void text_render(cairo_t *cr, bool with_bg)
{
    if (with_bg && app.text_opaque) {
        cairo_set_source_rgb(cr, app.color2.r, app.color2.g, app.color2.b);
        cairo_rectangle(cr, tx.r.x, tx.r.y, tx.r.w, tx.r.h);
        cairo_fill(cr);
    }
    cairo_save(cr);
    cairo_rectangle(cr, tx.r.x, tx.r.y, tx.r.w, tx.r.h);
    cairo_clip(cr);
    PangoLayout *l = text_layout(cr);
    cairo_set_source_rgb(cr, app.color1.r, app.color1.g, app.color1.b);
    cairo_move_to(cr, tx.r.x + 2, tx.r.y + 2);
    pango_cairo_show_layout(cr, l);
    g_object_unref(l);
    cairo_restore(cr);
}

static void text_commit(void)
{
    if (!tx.active) return;
    tx.active = false;
    if (tx.s->len) {
        undo_begin();
        cairo_t *cr = cairo_create(app.img);
        text_render(cr, true);
        cairo_destroy(cr);
        undo_commit();
    }
    g_string_truncate(tx.s, 0);
    gtk_im_context_focus_out(im);
    show_text_tab(false);
    canvas_redraw();
    thumb_refresh();
}

void text_apply_attrs(void)
{
    if (tx.active) {
        text_fit();
        canvas_redraw();
    }
}

static void text_delete_sel(void)
{
    int a = MIN(tx.cur, tx.anchor), b = MAX(tx.cur, tx.anchor);
    if (a == b) return;
    g_string_erase(tx.s, a, b - a);
    tx.cur = tx.anchor = a;
}

static void text_insert(const char *str)
{
    text_delete_sel();
    g_string_insert(tx.s, tx.cur, str);
    tx.cur += strlen(str);
    tx.anchor = tx.cur;
    text_fit();
    canvas_redraw();
}

static void im_commit(GtkIMContext *c, const char *str, gpointer d)
{
    (void)c; (void)d;
    if (tx.active) text_insert(str);
}

static int utf8_prev(int i)
{
    if (i <= 0) return 0;
    const char *p = g_utf8_prev_char(tx.s->str + i);
    return p - tx.s->str;
}

static int utf8_next(int i)
{
    if (i >= (int)tx.s->len) return tx.s->len;
    return g_utf8_next_char(tx.s->str + i) - tx.s->str;
}

static int text_index_at(double ix, double iy)
{
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1);
    cairo_t *cr = cairo_create(s);
    PangoLayout *l = text_layout(cr);
    int idx, trailing;
    pango_layout_xy_to_index(l, (int)((ix - tx.r.x - 2) * PANGO_SCALE), (int)((iy - tx.r.y - 2) * PANGO_SCALE),
                             &idx, &trailing);
    while (trailing-- > 0) idx = g_utf8_next_char(tx.s->str + idx) - tx.s->str;
    g_object_unref(l);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
    return MIN(idx, (int)tx.s->len);
}

static int text_vertical(int dir)
{
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1);
    cairo_t *cr = cairo_create(s);
    PangoLayout *l = text_layout(cr);
    int ni, nt;
    pango_layout_move_cursor_visually(l, TRUE, tx.cur, 0, 0, &ni, &nt);
    PangoRectangle pos;
    pango_layout_get_cursor_pos(l, tx.cur, &pos, NULL);
    int line, x;
    pango_layout_index_to_line_x(l, tx.cur, FALSE, &line, &x);
    int target = line + dir, res = tx.cur;
    if (target >= 0 && target < pango_layout_get_line_count(l)) {
        PangoLayoutLine *pl = pango_layout_get_line_readonly(l, target);
        int idx, tr;
        pango_layout_line_x_to_index(pl, x, &idx, &tr);
        while (tr-- > 0) idx = g_utf8_next_char(tx.s->str + idx) - tx.s->str;
        res = idx;
    }
    g_object_unref(l);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
    return res;
}

static bool text_key(GdkEventKey *ev)
{
    if (gtk_im_context_filter_keypress(im, ev)) return true;
    bool ctrl = ev->state & GDK_CONTROL_MASK, shift = ev->state & GDK_SHIFT_MASK;
    int old = tx.cur;
    switch (ev->keyval) {
    case GDK_KEY_BackSpace:
        if (tx.cur == tx.anchor) tx.anchor = utf8_prev(tx.cur);
        text_delete_sel();
        break;
    case GDK_KEY_Delete:
        if (tx.cur == tx.anchor) tx.anchor = utf8_next(tx.cur);
        text_delete_sel();
        break;
    case GDK_KEY_Return: case GDK_KEY_KP_Enter:
        text_insert("\n");
        return true;
    case GDK_KEY_Left: tx.cur = utf8_prev(tx.cur); break;
    case GDK_KEY_Right: tx.cur = utf8_next(tx.cur); break;
    case GDK_KEY_Up: tx.cur = text_vertical(-1); break;
    case GDK_KEY_Down: tx.cur = text_vertical(1); break;
    case GDK_KEY_Home:
        if (ctrl) tx.cur = 0;
        else { while (tx.cur > 0 && tx.s->str[tx.cur - 1] != '\n') tx.cur--; }
        break;
    case GDK_KEY_End:
        if (ctrl) tx.cur = tx.s->len;
        else { while (tx.cur < (int)tx.s->len && tx.s->str[tx.cur] != '\n') tx.cur++; }
        break;
    case GDK_KEY_Escape:
        text_commit();
        return true;
    case GDK_KEY_a: case GDK_KEY_A:
        if (!ctrl) return false;
        tx.anchor = 0; tx.cur = tx.s->len;
        canvas_redraw();
        return true;
    case GDK_KEY_c: case GDK_KEY_C: case GDK_KEY_x: case GDK_KEY_X: {
        if (!ctrl) return false;
        int a = MIN(tx.cur, tx.anchor), b = MAX(tx.cur, tx.anchor);
        if (a != b) {
            GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
            gtk_clipboard_set_text(cb, tx.s->str + a, b - a);
            if (ev->keyval == GDK_KEY_x || ev->keyval == GDK_KEY_X) text_delete_sel();
        }
        break;
    }
    case GDK_KEY_v: case GDK_KEY_V: {
        if (!ctrl) return false;
        gchar *t = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
        if (t) { text_insert(t); g_free(t); }
        return true;
    }
    default:
        return false;
    }
    if (old != tx.cur && !shift && ev->keyval != GDK_KEY_BackSpace && ev->keyval != GDK_KEY_Delete)
        tx.anchor = tx.cur;
    if (ev->keyval == GDK_KEY_BackSpace || ev->keyval == GDK_KEY_Delete) tx.anchor = tx.cur;
    text_fit();
    canvas_redraw();
    return true;
}

static void text_draw_overlay(cairo_t *cr)
{
    cairo_save(cr);
    cairo_translate(cr, 5, 5);
    cairo_scale(cr, app.zoom, app.zoom);
    /* selection highlight */
    PangoLayout *l = text_layout(cr);
    if (tx.cur != tx.anchor) {
        int a = MIN(tx.cur, tx.anchor), b = MAX(tx.cur, tx.anchor);
        int range[2] = { a, b };
        cairo_region_t *rg = gdk_pango_layout_get_clip_region(l, 0, 0, range, 1);
        int n = cairo_region_num_rectangles(rg);
        cairo_set_source_rgba(cr, 0.2, 0.45, 0.9, 0.35);
        for (int i = 0; i < n; i++) {
            cairo_rectangle_int_t rr;
            cairo_region_get_rectangle(rg, i, &rr);
            cairo_rectangle(cr, tx.r.x + 2 + rr.x, tx.r.y + 2 + rr.y, rr.width, rr.height);
        }
        cairo_fill(cr);
        cairo_region_destroy(rg);
    }
    text_render(cr, true);
    /* caret */
    PangoRectangle pos;
    pango_layout_get_cursor_pos(l, tx.cur, &pos, NULL);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 1 / app.zoom);
    double cx = tx.r.x + 2 + pos.x / (double)PANGO_SCALE;
    cairo_move_to(cr, cx, tx.r.y + 2 + pos.y / (double)PANGO_SCALE);
    cairo_rel_line_to(cr, 0, pos.height / (double)PANGO_SCALE);
    cairo_stroke(cr);
    g_object_unref(l);
    cairo_restore(cr);
    draw_tracker(cr, tx.r, true);
}

/* ------------------------------------------------------------------ */
/* selection tool state (CSelectTool / CFreehandSelectTool)             */

enum { SM_NONE, SM_CREATE, SM_FREE, SM_MOVE, SM_SIZE };
static struct {
    int mode;
    double x0, y0, x1, y1;
    GArray *free;          /* double pairs */
    int handle;
    Rect r0;
    double px, py;
    bool smear;
} sl;

static void sel_finish_free(void)
{
    int n = sl.free->len / 2;
    double *p = (double *)sl.free->data;
    if (n < 3) return;
    double a = 1e9, b = 1e9, c = -1e9, d = -1e9;
    for (int i = 0; i < n; i++) {
        a = MIN(a, p[2 * i]); b = MIN(b, p[2 * i + 1]);
        c = MAX(c, p[2 * i]); d = MAX(d, p[2 * i + 1]);
    }
    int x0 = CLAMP((int)floor(a), 0, app.w), y0 = CLAMP((int)floor(b), 0, app.h);
    int x1 = CLAMP((int)ceil(c) + 1, 0, app.w), y1 = CLAMP((int)ceil(d) + 1, 0, app.h);
    if (x1 - x0 < 1 || y1 - y0 < 1) return;
    cairo_surface_t *m = cairo_image_surface_create(CAIRO_FORMAT_A8, x1 - x0, y1 - y0);
    cairo_t *cr = cairo_create(m);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    for (int i = 0; i < n; i++) cairo_line_to(cr, p[2 * i] - x0 + 0.5, p[2 * i + 1] - y0 + 0.5);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 1);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    cairo_destroy(cr);
    app.sel.active = true;
    app.sel.x = x0; app.sel.y = y0; app.sel.w = x1 - x0; app.sel.h = y1 - y0;
    app.sel.mask = m;
}

/* ------------------------------------------------------------------ */
/* common drag state                                                   */

static struct {
    bool down;
    int button;
    double sx, sy, lx, ly;
    guint state;
    guint air_timer;
    bool picker_active;
    RGB picked;
    double zx, zy;       /* magnifier hover */
} dr;

static RGB btn_color(int button) { return button == 3 ? app.color2 : app.color1; }

static gboolean air_tick(gpointer d)
{
    (void)d;
    brush_airbrush_tick();
    return G_SOURCE_CONTINUE;
}

void tool_init(void)
{
    sl.free = g_array_new(FALSE, FALSE, sizeof(double));
    tx.s = g_string_new("");
    im = gtk_im_context_simple_new();
    g_signal_connect(im, "commit", G_CALLBACK(im_commit), NULL);
}

void tool_commit_pending(void)
{
    if (sh.active) shape_commit();
    if (tx.active) text_commit();
    if (app.sel.active) sel_clear(true);
}

void tool_cancel(void)
{
    if (dr.air_timer) { g_source_remove(dr.air_timer); dr.air_timer = 0; }
    if (dr.down && (app.tool == TOOL_PENCIL || app.tool == TOOL_BRUSH || app.tool == TOOL_ERASER)) {
        brush_stroke_end();
        if (app.undo_pending) {
            cairo_t *cr = cairo_create(app.img);
            cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_surface(cr, app.undo_pending, 0, 0);
            cairo_paint(cr);
            cairo_destroy(cr);
            undo_cancel();
        }
    }
    if (sh.active && sh.creating) shape_cancel();
    if (sl.mode == SM_CREATE || sl.mode == SM_FREE) sl.mode = SM_NONE;
    dr.down = false;
    canvas_redraw();
}

void tool_select(ToolId t)
{
    if (t != app.tool) {
        tool_commit_pending();
        if (t != TOOL_PICKER) app.prev_tool = (app.tool == TOOL_PICKER) ? app.prev_tool : app.tool;
        else app.prev_tool = app.tool;
        app.tool = t;
    }
    show_text_tab(false);
    canvas_update_cursor();
    canvas_redraw();
    update_ui();
}

/* ------------------------------------------------------------------ */
/* press / motion / release                                            */

void tool_press(double ix, double iy, int button, guint state, int n_press)
{
    int px = (int)floor(ix), py = (int)floor(iy);
    bool shift = state & GDK_SHIFT_MASK, ctrl = state & GDK_CONTROL_MASK;

    /* ---- polygon in progress: each click adds a vertex ---- */
    if (app.tool == TOOL_SHAPE && sh.active && sh.creating && sh.kind == SHAPE_POLYGON) {
        if (n_press == 2) {
            sh.closed_poly = true;
            sh.creating = false;
            shape_render();
            return;
        }
        if (sh.npts < MAXPTS - 1) {
            sh.pts[sh.npts][0] = px; sh.pts[sh.npts][1] = py;
            sh.npts++;
        }
        dr.down = true; dr.button = button;
        shape_render();
        return;
    }
    /* ---- curve bends ---- */
    if (app.tool == TOOL_SHAPE && sh.active && sh.kind == SHAPE_CURVE && sh.curve_stage > 0) {
        dr.down = true; dr.button = button;
        if (sh.curve_stage == 1) { sh.pts[1][0] = sh.pts[2][0] = px; sh.pts[1][1] = sh.pts[2][1] = py; }
        else { sh.pts[2][0] = px; sh.pts[2][1] = py; }
        shape_render();
        return;
    }
    if (n_press == 2) return;

    dr.down = true;
    dr.button = button;
    dr.sx = dr.lx = ix;
    dr.sy = dr.ly = iy;
    dr.state = state;

    switch (app.tool) {
    case TOOL_PENCIL:
        undo_begin();
        plot_line(px, py, px, py, tool_cur_size(), rgb_to_px(btn_color(button)), false, 0);
        break;
    case TOOL_ERASER:
        undo_begin();
        if (button == 1) plot_line(px, py, px, py, tool_cur_size(), rgb_to_px(app.color2), false, 0);
        else plot_line(px, py, px, py, tool_cur_size(), rgb_to_px(app.color2), true, rgb_to_px(app.color1));
        break;
    case TOOL_FILL:
        undo_begin();
        flood_fill(px, py, rgb_to_px(btn_color(button)));
        undo_commit();
        thumb_refresh();
        dr.down = false;
        break;
    case TOOL_PICKER:
        dr.picker_active = true;
        dr.picked = pick_color(px, py);
        break;
    case TOOL_ZOOM:
        dr.down = false;
        if (button == 1) {
            int i;
            for (i = 0; i < n_zoom_presets && zoom_presets[i] <= app.zoom + 1e-9; i++) ;
            if (i < n_zoom_presets) canvas_set_zoom(zoom_presets[i], ix, iy);
        } else {
            int i;
            for (i = n_zoom_presets - 1; i >= 0 && zoom_presets[i] >= app.zoom - 1e-9; i--) ;
            if (i >= 0) canvas_set_zoom(zoom_presets[i], ix, iy);
        }
        break;
    case TOOL_BRUSH:
        undo_begin();
        brush_stroke_begin(app.brush_kind, btn_color(button), tool_cur_size(), ix, iy);
        if (app.brush_kind == BRUSH_AIRBRUSH) dr.air_timer = g_timeout_add(15, air_tick, NULL);
        break;
    case TOOL_TEXT:
        if (tx.active) {
            int h = tracker_hit(tx.r, ix, iy);
            if (h >= 0 && h < 8) { tx.handle = h; tx.r0 = tx.r; tx.px = ix; tx.py = iy; return; }
            if (h == 8) {
                tx.handle = -1;
                tx.cur = tx.anchor = text_index_at(ix, iy);
                tx.dragsel = true;
                canvas_redraw();
                return;
            }
            text_commit();
            dr.down = false;
            return;
        }
        if (button != 1) { dr.down = false; return; }
        tx.creating = true;
        tx.r = (Rect){ px, py, 0, 0 };
        break;
    case TOOL_SELECT_RECT:
    case TOOL_SELECT_FREE: {
        if (button == 3) {
            dr.down = false;
            show_context_menu(NULL);
            return;
        }
        Rect sr = { app.sel.x, app.sel.y, app.sel.w, app.sel.h };
        int h = app.sel.active ? tracker_hit(sr, ix, iy) : -1;
        if (h >= 0 && h < 8) {
            sel_lift();
            sl.mode = SM_SIZE; sl.handle = h; sl.r0 = sr; sl.px = ix; sl.py = iy;
            return;
        }
        if (h == 8) {
            if (ctrl) {
                if (app.sel.floating) {
                    /* Ctrl-drag of a floating selection leaves a copy behind */
                    cairo_surface_t *keep = surface_copy(app.sel.pix);
                    int kw = app.sel.w, kh = app.sel.h;
                    sel_commit();
                    undo_begin();
                    app.sel.pix = keep;
                    app.sel.w = kw; app.sel.h = kh;
                    app.sel.floating = true;
                    app.sel.from_paste = true;
                } else {
                    app.sel.from_paste = true;   /* copy: no hole */
                    sel_lift();
                }
            } else {
                sel_lift();
            }
            sl.mode = SM_MOVE; sl.r0 = sr; sl.px = ix; sl.py = iy;
            sl.smear = shift;
            return;
        }
        sel_clear(true);
        if (app.tool == TOOL_SELECT_RECT) {
            sl.mode = SM_CREATE;
            sl.x0 = sl.x1 = CLAMP(px, 0, app.w);
            sl.y0 = sl.y1 = CLAMP(py, 0, app.h);
        } else {
            sl.mode = SM_FREE;
            g_array_set_size(sl.free, 0);
            double pt[2] = { ix, iy };
            g_array_append_vals(sl.free, pt, 2);
        }
        break;
    }
    case TOOL_SHAPE: {
        if (sh.active) {
            Rect b = shape_bbox();
            if (sh.kind == SHAPE_LINE) {
                double tol = 4 / app.zoom;
                for (int i = 0; i < 2; i++)
                    if (fabs(ix - sh.pts[i][0]) <= tol && fabs(iy - sh.pts[i][1]) <= tol) {
                        sh.handle = 10 + i;
                        return;
                    }
            }
            int h = tracker_hit((Rect){ b.x - 2 / app.zoom, b.y - 2 / app.zoom, b.w + 4 / app.zoom, b.h + 4 / app.zoom }, ix, iy);
            if (h >= 0) {
                sh.handle = h; sh.r0 = b; sh.px = ix; sh.py = iy;
                memcpy(sh.pts0, sh.pts, sizeof(double) * 2 * sh.npts);
                return;
            }
            shape_commit();
        }
        undo_begin();
        sh.active = true;
        sh.creating = true;
        sh.kind = app.shape_kind;
        sh.rot = 0;
        sh.swap = button == 3;
        sh.closed_poly = false;
        sh.curve_stage = 0;
        sh.handle = -1;
        sh.x0 = sh.x1 = px;
        sh.y0 = sh.y1 = py;
        sh.npts = 0;
        if (is_point_shape(sh.kind)) {
            sh.pts[0][0] = sh.pts[1][0] = px;
            sh.pts[0][1] = sh.pts[1][1] = py;
            sh.npts = 2;
        }
        break;
    }
    default:
        break;
    }
}

static void constrain(double sx, double sy, double *x, double *y, bool angles)
{
    double dx = *x - sx, dy = *y - sy;
    if (angles) {
        /* CImgTool::AdjustPointsForConstraint: snap to 0/45/90 degrees */
        double a = atan2(dy, dx), d = hypot(dx, dy);
        a = round(a / (M_PI / 4)) * (M_PI / 4);
        *x = sx + round(cos(a) * d);
        *y = sy + round(sin(a) * d);
    } else {
        double m = MAX(fabs(dx), fabs(dy));
        *x = sx + (dx < 0 ? -m : m);
        *y = sy + (dy < 0 ? -m : m);
    }
}

void tool_motion(double ix, double iy, guint state)
{
    int px = (int)floor(ix), py = (int)floor(iy);
    bool shift = state & GDK_SHIFT_MASK;
    dr.zx = ix; dr.zy = iy;

    if (!dr.down) {
        if (app.tool == TOOL_BRUSH || app.tool == TOOL_ERASER || app.tool == TOOL_ZOOM || app.tool == TOOL_PENCIL)
            canvas_redraw();
        if (sh.active && sh.creating && sh.kind == SHAPE_POLYGON && sh.npts >= 1) {
            /* rubber band from last vertex */
            sh.pts[sh.npts - 1][0] = px; sh.pts[sh.npts - 1][1] = py;
        }
        return;
    }

    switch (app.tool) {
    case TOOL_PENCIL:
        plot_line((int)floor(dr.lx), (int)floor(dr.ly), px, py, tool_cur_size(),
                  rgb_to_px(btn_color(dr.button)), false, 0);
        break;
    case TOOL_ERASER:
        if (dr.button == 1)
            plot_line((int)floor(dr.lx), (int)floor(dr.ly), px, py, tool_cur_size(), rgb_to_px(app.color2), false, 0);
        else
            plot_line((int)floor(dr.lx), (int)floor(dr.ly), px, py, tool_cur_size(), rgb_to_px(app.color2), true,
                      rgb_to_px(app.color1));
        break;
    case TOOL_PICKER:
        dr.picked = pick_color(px, py);
        break;
    case TOOL_BRUSH:
        brush_stroke_to(ix, iy, 1);
        break;
    case TOOL_TEXT:
        if (tx.creating) {
            tx.r = rect_norm((Rect){ floor(dr.sx), floor(dr.sy), px - floor(dr.sx), py - floor(dr.sy) });
            canvas_redraw();
        } else if (tx.active && tx.dragsel) {
            tx.cur = text_index_at(ix, iy);
            canvas_redraw();
        } else if (tx.active && tx.handle >= 0) {
            Rect r = rect_norm(tracker_apply(tx.handle, tx.r0, round(ix - tx.px), round(iy - tx.py)));
            if (r.w < 8) r.w = 8;
            tx.r = r;
            text_fit();
            canvas_redraw();
        }
        break;
    case TOOL_SELECT_RECT:
    case TOOL_SELECT_FREE:
        if (sl.mode == SM_CREATE) {
            sl.x1 = CLAMP(px, 0, app.w);
            sl.y1 = CLAMP(py, 0, app.h);
            canvas_redraw();
        } else if (sl.mode == SM_FREE) {
            double pt[2] = { ix, iy };
            g_array_append_vals(sl.free, pt, 2);
            canvas_redraw();
        } else if (sl.mode == SM_MOVE) {
            if (sl.smear) {
                /* Shift-drag stamps copies along the way */
                cairo_surface_t *keep = surface_copy(app.sel.pix);
                sel_commit();
                undo_begin();
                app.sel.pix = keep;
                app.sel.floating = true;
            }
            app.sel.x = (int)lround(sl.r0.x + ix - sl.px);
            app.sel.y = (int)lround(sl.r0.y + iy - sl.py);
            canvas_redraw();
        } else if (sl.mode == SM_SIZE) {
            Rect r = tracker_apply(sl.handle, sl.r0, round(ix - sl.px), round(iy - sl.py));
            r = rect_norm(r);
            app.sel.x = (int)r.x; app.sel.y = (int)r.y;
            app.sel.w = MAX(1, (int)r.w); app.sel.h = MAX(1, (int)r.h);
            canvas_redraw();
        }
        break;
    case TOOL_SHAPE:
        if (!sh.active) break;
        if (sh.creating) {
            double x = px, y = py;
            if (sh.kind == SHAPE_LINE || sh.kind == SHAPE_POLYGON || sh.kind == SHAPE_CURVE) {
                int k = sh.kind == SHAPE_POLYGON ? sh.npts - 1 : (sh.kind == SHAPE_CURVE ? 1 : 1);
                double ax = sh.pts[k - 1 >= 0 ? k - 1 : 0][0], ay = sh.pts[k - 1 >= 0 ? k - 1 : 0][1];
                if (sh.kind == SHAPE_CURVE) { ax = sh.pts[0][0]; ay = sh.pts[0][1]; }
                if (shift) constrain(ax, ay, &x, &y, true);
                if (sh.kind == SHAPE_POLYGON) { sh.pts[sh.npts - 1][0] = x; sh.pts[sh.npts - 1][1] = y; }
                else { sh.pts[1][0] = x; sh.pts[1][1] = y; }
            } else {
                if (shift) constrain(sh.x0, sh.y0, &x, &y, false);
                sh.x1 = x; sh.y1 = y;
            }
            shape_render();
        } else if (sh.kind == SHAPE_CURVE && sh.curve_stage > 0) {
            if (sh.curve_stage == 1) { sh.pts[1][0] = sh.pts[2][0] = px; sh.pts[1][1] = sh.pts[2][1] = py; }
            else { sh.pts[2][0] = px; sh.pts[2][1] = py; }
            shape_render();
        } else if (sh.handle >= 10) {
            double x = px, y = py;
            if (shift) constrain(sh.pts[11 - sh.handle][0], sh.pts[11 - sh.handle][1], &x, &y, true);
            sh.pts[sh.handle - 10][0] = x; sh.pts[sh.handle - 10][1] = y;
            shape_render();
        } else if (sh.handle >= 0) {
            Rect r = tracker_apply(sh.handle, sh.r0, round(ix - sh.px), round(iy - sh.py));
            if (sh.handle != 8) {
                if (r.w < 0) r.w = 0;
                if (r.h < 0) r.h = 0;
            }
            shape_set_bbox(sh.r0, r);
            shape_render();
        }
        break;
    default:
        break;
    }
    dr.lx = ix;
    dr.ly = iy;
}

void tool_release(double ix, double iy, int button, guint state)
{
    (void)state;
    int px = (int)floor(ix), py = (int)floor(iy);
    if (!dr.down) return;
    dr.down = false;
    switch (app.tool) {
    case TOOL_PENCIL:
    case TOOL_ERASER:
        undo_commit();
        thumb_refresh();
        break;
    case TOOL_BRUSH:
        if (dr.air_timer) { g_source_remove(dr.air_timer); dr.air_timer = 0; }
        brush_stroke_end();
        undo_commit();
        thumb_refresh();
        break;
    case TOOL_PICKER:
        /* CPickColorTool::OnEndDrag: set the colour and return to the previous tool */
        dr.picker_active = false;
        set_color(button == 3 ? 2 : 1, pick_color(px, py));
        tool_select(app.prev_tool == TOOL_PICKER ? TOOL_PENCIL : app.prev_tool);
        break;
    case TOOL_TEXT:
        if (tx.creating) {
            tx.creating = false;
            Rect r = tx.r;
            /* a click (no drag) creates a default-sized box */
            if (r.w < 10 || r.h < 10) {
                r.w = MAX(r.w, MIN(200, app.w - r.x));
                r.h = MAX(r.h, app.font_size * 96.0 / 72.0 * 1.3 + 4);
            }
            if (r.w < 8) r.w = 8;
            tx.r = r;
            tx.active = true;
            g_string_truncate(tx.s, 0);
            tx.cur = tx.anchor = 0;
            tx.handle = -1;
            gtk_im_context_focus_in(im);
            text_fit();
            show_text_tab(true);
            canvas_redraw();
        }
        tx.dragsel = false;
        tx.handle = -1;
        break;
    case TOOL_SELECT_RECT:
        if (sl.mode == SM_CREATE) {
            sl.mode = SM_NONE;
            int x0 = (int)MIN(sl.x0, sl.x1), y0 = (int)MIN(sl.y0, sl.y1);
            int x1 = (int)MAX(sl.x0, sl.x1), y1 = (int)MAX(sl.y0, sl.y1);
            if (x1 - x0 >= 1 && y1 - y0 >= 1) {
                app.sel.active = true;
                app.sel.x = x0; app.sel.y = y0; app.sel.w = x1 - x0; app.sel.h = y1 - y0;
            }
            canvas_redraw();
            update_ui();
            break;
        }
        /* fallthrough */
    case TOOL_SELECT_FREE:
        if (sl.mode == SM_FREE) {
            sl.mode = SM_NONE;
            sel_finish_free();
            canvas_redraw();
            update_ui();
            break;
        }
        sl.mode = SM_NONE;
        thumb_refresh();
        break;
    case TOOL_SHAPE:
        if (!sh.active) break;
        if (sh.creating) {
            if (sh.kind == SHAPE_POLYGON) {
                /* start next segment; clicking on the first vertex closes */
                int n = sh.npts;
                if (n >= 3 && fabs(sh.pts[n - 1][0] - sh.pts[0][0]) <= 3 / app.zoom + 1 &&
                    fabs(sh.pts[n - 1][1] - sh.pts[0][1]) <= 3 / app.zoom + 1) {
                    sh.npts--;
                    sh.closed_poly = true;
                    sh.creating = false;
                    shape_render();
                    break;
                }
                if (sh.npts < MAXPTS - 1) {
                    sh.pts[sh.npts][0] = sh.pts[sh.npts - 1][0];
                    sh.pts[sh.npts][1] = sh.pts[sh.npts - 1][1];
                    sh.npts++;
                }
                break;
            }
            if (sh.kind == SHAPE_CURVE) {
                /* turn the dragged line into a bezier awaiting two bends */
                double x0 = sh.pts[0][0], y0 = sh.pts[0][1], x1 = sh.pts[1][0], y1 = sh.pts[1][1];
                sh.pts[3][0] = x1; sh.pts[3][1] = y1;
                sh.pts[1][0] = x0; sh.pts[1][1] = y0;
                sh.pts[2][0] = x1; sh.pts[2][1] = y1;
                sh.npts = 4;
                sh.curve_stage = 1;
                sh.creating = false;
                shape_render();
                break;
            }
            sh.creating = false;
            Rect b = shape_bbox();
            if (b.w < 1 && b.h < 1) { shape_cancel(); break; }
            shape_render();
        } else if (sh.kind == SHAPE_CURVE && sh.curve_stage > 0) {
            sh.curve_stage++;
            if (sh.curve_stage > 2) {
                sh.curve_stage = 0;
                shape_commit();
            }
        }
        sh.handle = -1;
        break;
    default:
        break;
    }
    update_status();
}

bool tool_key(GdkEventKey *ev)
{
    if (tx.active) return text_key(ev);
    bool ctrl = ev->state & GDK_CONTROL_MASK;
    if (ev->keyval == GDK_KEY_Escape) {
        if (dr.down) { tool_cancel(); return true; }
        if (sh.active) {
            if (sh.creating && sh.kind == SHAPE_POLYGON && sh.npts > 2) {
                sh.npts--;
                sh.creating = false;
                shape_render();
            }
            shape_commit();
            return true;
        }
        if (app.sel.active) { sel_clear(true); return true; }
        return false;
    }
    /* arrow keys nudge the selection or live shape (CImgWnd::MoveSelectionOnKeyPress) */
    int dx = 0, dy = 0;
    switch (ev->keyval) {
    case GDK_KEY_Left: dx = -1; break;
    case GDK_KEY_Right: dx = 1; break;
    case GDK_KEY_Up: dy = -1; break;
    case GDK_KEY_Down: dy = 1; break;
    default: return false;
    }
    if (ctrl) return false;
    if (app.sel.active) {
        sel_lift();
        app.sel.x += dx;
        app.sel.y += dy;
        canvas_redraw();
        return true;
    }
    if (sh.active && !sh.creating) {
        Rect b = shape_bbox();
        memcpy(sh.pts0, sh.pts, sizeof(double) * 2 * sh.npts);
        shape_set_bbox(b, (Rect){ b.x + dx, b.y + dy, b.w, b.h });
        shape_render();
        return true;
    }
    return false;
}

/* Rotate / flip / invert commands go to the live shape when there is one */
bool tool_transform_live(int op);
bool tool_transform_live(int op)
{
    if (op == SEL_OP_INVERT) return false;
    return shape_transform(op);
}

/* ------------------------------------------------------------------ */
/* overlay (widget coordinates)                                        */

void tool_draw_overlay(cairo_t *cr)
{
    /* selection */
    if (sl.mode == SM_CREATE) {
        Rect r = rect_norm((Rect){ sl.x0, sl.y0, sl.x1 - sl.x0, sl.y1 - sl.y0 });
        draw_tracker(cr, r, false);
    } else if (sl.mode == SM_FREE && sl.free->len >= 4) {
        double *p = (double *)sl.free->data;
        cairo_save(cr);
        cairo_set_line_width(cr, 1);
        for (guint i = 0; i < sl.free->len / 2; i++) {
            double wx, wy;
            canvas_image_to_widget(p[2 * i], p[2 * i + 1], &wx, &wy);
            cairo_line_to(cr, wx, wy);
        }
        cairo_set_source_rgb(cr, 0, 0, 0);
        double dash[] = { 2, 2 };
        cairo_set_dash(cr, dash, 2, 0);
        cairo_stroke(cr);
        cairo_restore(cr);
    } else if (app.sel.active) {
        draw_tracker(cr, (Rect){ app.sel.x, app.sel.y, app.sel.w, app.sel.h }, true);
    }

    /* live shape */
    if (sh.active && !sh.creating) {
        if (sh.kind == SHAPE_LINE) {
            for (int i = 0; i < 2; i++) {
                double wx, wy;
                canvas_image_to_widget(sh.pts[i][0] + 0.5, sh.pts[i][1] + 0.5, &wx, &wy);
                cairo_rectangle(cr, floor(wx) - 2.5, floor(wy) - 2.5, 5, 5);
                cairo_set_source_rgb(cr, 1, 1, 1);
                cairo_fill_preserve(cr);
                cairo_set_source_rgb(cr, 0.33, 0.4, 0.5);
                cairo_set_line_width(cr, 1);
                cairo_stroke(cr);
            }
        } else if (!(sh.kind == SHAPE_CURVE && sh.curve_stage > 0)) {
            draw_tracker(cr, shape_bbox(), true);
        }
    }

    if (tx.creating) draw_tracker(cr, tx.r, false);
    if (tx.active) text_draw_overlay(cr);

    /* cursor visuals (CPaintTool::ShowCursorVisual / CEraserTool::OnShowDragger) */
    if (app.mouse_x >= 0 && !tx.active) {
        int s = tool_cur_size();
        double cx, cy;
        if (app.tool == TOOL_ERASER) {
            int x0 = app.mouse_x - (s - 1) / 2, y0 = app.mouse_y - (s - 1) / 2;
            double wx0, wy0, wx1, wy1;
            canvas_image_to_widget(x0, y0, &wx0, &wy0);
            canvas_image_to_widget(x0 + s, y0 + s, &wx1, &wy1);
            cairo_rectangle(cr, floor(wx0) + 0.5, floor(wy0) + 0.5, floor(wx1 - wx0) - 1, floor(wy1 - wy0) - 1);
            cairo_set_source_rgb(cr, app.color2.r, app.color2.g, app.color2.b);
            cairo_fill_preserve(cr);
            cairo_set_source_rgb(cr, 0, 0, 0);
            cairo_set_line_width(cr, 1);
            cairo_stroke(cr);
        } else if (app.tool == TOOL_BRUSH && !dr.down && s * app.zoom >= 3) {
            canvas_image_to_widget(dr.zx, dr.zy, &cx, &cy);
            double r = s * app.zoom / 2;
            cairo_set_line_width(cr, 1);
            if (app.brush_kind == BRUSH_CALLIG1 || app.brush_kind == BRUSH_CALLIG2) {
                double a = app.brush_kind == BRUSH_CALLIG1 ? M_PI / 4 : 3 * M_PI / 4;
                cairo_move_to(cr, cx - cos(a) * r, cy + sin(a) * r);
                cairo_line_to(cr, cx + cos(a) * r, cy - sin(a) * r);
            } else if (app.brush_kind == BRUSH_MARKER) {
                cairo_rectangle(cr, cx - r, cy - r, 2 * r, 2 * r);
            } else {
                cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
            }
            cairo_set_source_rgb(cr, 0, 0, 0);
            cairo_stroke(cr);
        } else if (app.tool == TOOL_ZOOM) {
            /* CZoomTool::InvertZoomRect: preview of the area the next zoom shows */
            int i;
            for (i = 0; i < n_zoom_presets && zoom_presets[i] <= app.zoom + 1e-9; i++) ;
            if (i < n_zoom_presets) {
                GtkAdjustment *ha = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(app.scroll));
                GtkAdjustment *va = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(app.scroll));
                double vw = gtk_adjustment_get_page_size(ha) / zoom_presets[i];
                double vh = gtk_adjustment_get_page_size(va) / zoom_presets[i];
                vw = MIN(vw, app.w); vh = MIN(vh, app.h);
                Rect r = { CLAMP(dr.zx - vw / 2, 0, app.w - vw), CLAMP(dr.zy - vh / 2, 0, app.h - vh), vw, vh };
                double x0, y0, x1, y1;
                canvas_image_to_widget(r.x, r.y, &x0, &y0);
                canvas_image_to_widget(r.x + r.w, r.y + r.h, &x1, &y1);
                cairo_rectangle(cr, floor(x0) + 0.5, floor(y0) + 0.5, floor(x1 - x0), floor(y1 - y0));
                cairo_set_operator(cr, CAIRO_OPERATOR_DIFFERENCE);
                cairo_set_source_rgb(cr, 1, 1, 1);
                cairo_set_line_width(cr, 1);
                cairo_stroke(cr);
                cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
            }
        }
    }
    if (dr.picker_active) {
        /* picked colour preview next to the pointer */
        double wx, wy;
        canvas_image_to_widget(dr.lx, dr.ly, &wx, &wy);
        cairo_rectangle(cr, floor(wx) + 12.5, floor(wy) + 12.5, 22, 22);
        cairo_set_source_rgb(cr, dr.picked.r, dr.picked.g, dr.picked.b);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
    }
}

/* ------------------------------------------------------------------ */
/* cursors                                                             */

static GdkCursor *cursors[TOOL_COUNT + 4];

static GdkCursor *make_cursor(const char *icon, int hx, int hy)
{
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 24, 24);
    cairo_t *cr = cairo_create(s);
    draw_icon_mode(cr, icon, 24, false);   /* cursors sit on the canvas: always light */
    cairo_destroy(cr);
    GdkCursor *c = gdk_cursor_new_from_surface(gdk_display_get_default(), s, hx, hy);
    cairo_surface_destroy(s);
    return c;
}

void canvas_update_cursor(void)
{
    if (!app.canvas || !gtk_widget_get_realized(app.canvas)) return;
    GdkWindow *win = gtk_widget_get_window(app.canvas);
    GdkDisplay *dpy = gdk_window_get_display(win);
    if (!cursors[0]) {
        cursors[TOOL_PENCIL] = make_cursor("cur-pencil", 2, 21);
        cursors[TOOL_FILL] = make_cursor("cur-fill", 3, 19);
        cursors[TOOL_PICKER] = make_cursor("cur-picker", 2, 21);
        cursors[TOOL_ZOOM] = make_cursor("cur-zoom", 9, 9);
        cursors[TOOL_TEXT] = gdk_cursor_new_from_name(dpy, "text");
        cursors[0] = gdk_cursor_new_from_name(dpy, "crosshair");
        cursors[TOOL_COUNT] = gdk_cursor_new_from_name(dpy, "move");
        cursors[TOOL_COUNT + 1] = gdk_cursor_new_from_name(dpy, "nwse-resize");
        cursors[TOOL_COUNT + 2] = gdk_cursor_new_from_name(dpy, "nesw-resize");
        cursors[TOOL_COUNT + 3] = gdk_cursor_new_from_name(dpy, "ew-resize");
    }
    GdkCursor *c = cursors[0];
    double ix = dr.zx, iy = dr.zy;
    int h = -1;
    if (app.sel.active && (app.tool == TOOL_SELECT_RECT || app.tool == TOOL_SELECT_FREE))
        h = tracker_hit((Rect){ app.sel.x, app.sel.y, app.sel.w, app.sel.h }, ix, iy);
    else if (sh.active && !sh.creating && app.tool == TOOL_SHAPE)
        h = tracker_hit(shape_bbox(), ix, iy);
    else if (tx.active)
        h = tracker_hit(tx.r, ix, iy);
    switch (app.tool) {
    case TOOL_PENCIL: case TOOL_FILL: case TOOL_PICKER: case TOOL_ZOOM: case TOOL_TEXT:
        c = cursors[app.tool];
        break;
    default:
        break;
    }
    if (h == 8 && !tx.active) c = cursors[TOOL_COUNT];
    else if (h == 0 || h == 4) c = cursors[TOOL_COUNT + 1];
    else if (h == 2 || h == 6) c = cursors[TOOL_COUNT + 2];
    else if (h == 3 || h == 7) c = cursors[TOOL_COUNT + 3];
    else if (h == 1 || h == 5) c = gdk_cursor_new_from_name(dpy, "ns-resize");
    gdk_window_set_cursor(win, c);
}
