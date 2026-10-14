/* Brushes and artistic media.
 *
 * mspaint 6.1 renders its brushes through the "PGS" layer (PGSSolidBrush,
 * PGSTextureBrush, PGSTextureSkeletalStrokeBrush) using 512x512 media
 * textures stored as IMAGE resources.  We do not ship Microsoft's textures;
 * equivalent procedural ones are generated at start-up.
 *
 * Every stroke accumulates into a coverage mask; the visible result is
 * recomposited from the pre-stroke image each time, so translucent media
 * (marker, watercolor) do not build up within one stroke - the same
 * behaviour the original gets from its skeletal-stroke operations.
 */
#include "paint.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TEX 256

static cairo_surface_t *tex[MEDIA_COUNT];

static double frand(void) { return rand() / (double)RAND_MAX; }

/* simple value noise */
static float *noise_layer(int cell, unsigned seed)
{
    srand(seed);
    int g = TEX / cell + 1;
    float *grid = g_new(float, g * g);
    for (int i = 0; i < g * g; i++) grid[i] = (float)frand();
    float *out = g_new(float, TEX * TEX);
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) {
            int gx = x / cell, gy = y / cell;
            float fx = (x % cell) / (float)cell, fy = (y % cell) / (float)cell;
            fx = fx * fx * (3 - 2 * fx);
            fy = fy * fy * (3 - 2 * fy);
            int gx1 = (gx + 1) % (g - 1), gy1 = (gy + 1) % (g - 1);
            float a = grid[gy * g + gx], b = grid[gy * g + gx1];
            float c = grid[gy1 * g + gx], d = grid[gy1 * g + gx1];
            out[y * TEX + x] = (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy;
        }
    g_free(grid);
    return out;
}

static cairo_surface_t *make_tex(MediaKind m)
{
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_A8, TEX, TEX);
    unsigned char *p = cairo_image_surface_get_data(s);
    int st = cairo_image_surface_get_stride(s);
    float *n1 = noise_layer(4, 11 + m), *n2 = noise_layer(16, 23 + m), *n3 = noise_layer(2, 37 + m);
    srand(99 + m);
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) {
            float v = 1;
            int i = y * TEX + x;
            switch (m) {
            case MEDIA_CRAYON: {      /* paper grain: coarse speckles */
                float g = n3[i] * 0.6f + n1[i] * 0.4f;
                v = g > 0.42f ? 1.0f : g / 0.42f * 0.35f;
                break;
            }
            case MEDIA_NATPENCIL: {   /* fine graphite grain, lighter */
                float g = n3[i] * 0.7f + n1[i] * 0.3f;
                v = 0.25f + 0.6f * (g > 0.5f ? 1.0f : g * 1.2f);
                if (v > 0.85f) v = 0.85f;
                break;
            }
            case MEDIA_OIL: {         /* horizontal bristle streaks */
                float streak = n1[((y % TEX) * TEX + (x / 8) % TEX)];
                float row = n3[y * TEX + (x / 32) % TEX];
                v = 0.75f + 0.25f * (streak * 0.6f + row * 0.4f);
                break;
            }
            case MEDIA_WATERCOLOR:    /* soft blotches, transparent */
                v = 0.30f + 0.25f * n2[i] + 0.1f * n1[i];
                break;
            case MEDIA_MARKER:
                v = 0.55f;
                break;
            default:
                v = 1;
            }
            if (v < 0) v = 0;
            if (v > 1) v = 1;
            p[y * st + x] = (unsigned char)(v * 255);
        }
    g_free(n1); g_free(n2); g_free(n3);
    cairo_surface_mark_dirty(s);
    return s;
}

void brush_init(void)
{
    for (int m = 0; m < MEDIA_COUNT; m++) tex[m] = make_tex(m);
}

cairo_surface_t *media_texture(MediaKind m) { return tex[m]; }

static MediaKind brush_media(BrushKind k)
{
    switch (k) {
    case BRUSH_OIL: return MEDIA_OIL;
    case BRUSH_CRAYON: return MEDIA_CRAYON;
    case BRUSH_MARKER: return MEDIA_MARKER;
    case BRUSH_NATPENCIL: return MEDIA_NATPENCIL;
    case BRUSH_WATERCOLOR: return MEDIA_WATERCOLOR;
    default: return MEDIA_SOLID;
    }
}

/* Composite colour * (mask & texture) over base into target, within a rect */
static void composite(cairo_surface_t *target, cairo_surface_t *base, cairo_surface_t *mask,
                      MediaKind m, RGB c, int rx, int ry, int rw, int rh)
{
    if (rw <= 0 || rh <= 0) return;
    cairo_t *cr = cairo_create(target);
    cairo_rectangle(cr, rx, ry, rw, rh);
    cairo_clip(cr);
    if (base) {
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface(cr, base, 0, 0);
        cairo_paint(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    }
    cairo_set_source_rgb(cr, c.r, c.g, c.b);
    if (m == MEDIA_SOLID) {
        cairo_mask_surface(cr, mask, 0, 0);
    } else {
        cairo_surface_t *tmp = cairo_image_surface_create(CAIRO_FORMAT_A8, rw, rh);
        cairo_t *t = cairo_create(tmp);
        cairo_pattern_t *pt = cairo_pattern_create_for_surface(tex[m]);
        cairo_pattern_set_extend(pt, CAIRO_EXTEND_REPEAT);
        cairo_matrix_t mx;
        cairo_matrix_init_translate(&mx, rx, ry);
        cairo_pattern_set_matrix(pt, &mx);
        cairo_set_source(t, pt);
        cairo_paint(t);
        cairo_pattern_destroy(pt);
        cairo_set_operator(t, CAIRO_OPERATOR_DEST_IN);
        cairo_set_source_surface(t, mask, -rx, -ry);
        cairo_paint(t);
        cairo_destroy(t);
        cairo_mask_surface(cr, tmp, rx, ry);
        cairo_surface_destroy(tmp);
    }
    cairo_destroy(cr);
}

/* ------------------------------------------------------------------ */
/* freehand stroke engine                                              */

static struct {
    bool active;
    BrushKind kind;
    MediaKind media;
    RGB color;
    int size;
    double lx, ly;
    cairo_surface_t *target, *base, *mask;
    bool own_base;
    int tw, th;
} st;

static void stroke_setup(cairo_surface_t *target, cairo_surface_t *base, BrushKind k, RGB c, int size,
                         double x, double y)
{
    st.active = true;
    st.kind = k;
    st.media = brush_media(k);
    st.color = c;
    st.size = size;
    st.lx = x; st.ly = y;
    st.target = target;
    st.base = base;
    st.tw = cairo_image_surface_get_width(target);
    st.th = cairo_image_surface_get_height(target);
    st.mask = cairo_image_surface_create(CAIRO_FORMAT_A8, st.tw, st.th);
}

/* nib for the calligraphy brushes: a slanted line segment of length `size` */
static void callig_vec(double *nx, double *ny)
{
    double a = (st.kind == BRUSH_CALLIG1) ? M_PI / 4 : 3 * M_PI / 4;
    *nx = cos(a) * st.size / 2.0;
    *ny = -sin(a) * st.size / 2.0;
}

static void stroke_segment(double x0, double y0, double x1, double y1)
{
    cairo_t *cr = cairo_create(st.mask);
    double r = st.size / 2.0;
    double pad = st.size + 2;
    int bx = (int)floor(MIN(x0, x1) - pad), by = (int)floor(MIN(y0, y1) - pad);
    int bw = (int)ceil(fabs(x1 - x0) + 2 * pad) + 1, bh = (int)ceil(fabs(y1 - y0) + 2 * pad) + 1;
    cairo_set_source_rgba(cr, 0, 0, 0, 1);

    switch (st.kind) {
    case BRUSH_CALLIG1:
    case BRUSH_CALLIG2: {
        double nx, ny;
        callig_vec(&nx, &ny);
        cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
        cairo_move_to(cr, x0 + nx, y0 + ny);
        cairo_line_to(cr, x1 + nx, y1 + ny);
        cairo_line_to(cr, x1 - nx, y1 - ny);
        cairo_line_to(cr, x0 - nx, y0 - ny);
        cairo_close_path(cr);
        cairo_fill_preserve(cr);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
        break;
    }
    case BRUSH_MARKER:
        /* chisel marker: square-ish nib */
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
        cairo_set_line_width(cr, st.size);
        cairo_move_to(cr, x0, y0);
        cairo_line_to(cr, x1, y1);
        cairo_stroke(cr);
        break;
    case BRUSH_WATERCOLOR: {
        /* soft round nib stamped along the segment */
        double d = hypot(x1 - x0, y1 - y0), step = MAX(1.0, r / 3);
        int n = (int)(d / step) + 1;
        for (int i = 0; i <= n; i++) {
            double t = n ? (double)i / n : 0, x = x0 + (x1 - x0) * t, y = y0 + (y1 - y0) * t;
            cairo_pattern_t *g = cairo_pattern_create_radial(x, y, 0, x, y, r);
            cairo_pattern_add_color_stop_rgba(g, 0, 0, 0, 0, 1);
            cairo_pattern_add_color_stop_rgba(g, 0.7, 0, 0, 0, 0.9);
            cairo_pattern_add_color_stop_rgba(g, 1, 0, 0, 0, 0);
            cairo_set_source(cr, g);
            cairo_arc(cr, x, y, r, 0, 2 * M_PI);
            cairo_fill(cr);
            cairo_pattern_destroy(g);
        }
        break;
    }
    case BRUSH_NATPENCIL:
    case BRUSH_CRAYON:
    case BRUSH_OIL:
    case BRUSH_REGULAR:
    default:
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_set_line_width(cr, st.size);
        cairo_move_to(cr, x0, y0);
        cairo_line_to(cr, x1, y1);
        cairo_stroke(cr);
        break;
    }
    cairo_destroy(cr);

    if (bx < 0) { bw += bx; bx = 0; }
    if (by < 0) { bh += by; by = 0; }
    if (bx + bw > st.tw) bw = st.tw - bx;
    if (by + bh > st.th) bh = st.th - by;
    composite(st.target, st.base, st.mask, st.media, st.color, bx, by, bw, bh);
    if (st.target == app.img) canvas_redraw_rect(bx, by, bw, bh);
}

void brush_stroke_begin(BrushKind k, RGB c, int size, double x, double y)
{
    if (k == BRUSH_AIRBRUSH) {
        st.active = true;
        st.kind = k;
        st.color = c;
        st.size = size;
        st.lx = x; st.ly = y;
        st.target = app.img;
        brush_airbrush_tick();
        return;
    }
    stroke_setup(app.img, app.undo_pending, k, c, size, x, y);
    stroke_segment(x, y, x, y);
}

void brush_stroke_to(double x, double y, double pressure)
{
    (void)pressure;
    if (!st.active) return;
    if (st.kind == BRUSH_AIRBRUSH) {
        st.lx = x; st.ly = y;
        brush_airbrush_tick();
        return;
    }
    stroke_segment(st.lx, st.ly, x, y);
    st.lx = x; st.ly = y;
}

void brush_stroke_end(void)
{
    if (!st.active) return;
    st.active = false;
    if (st.mask) cairo_surface_destroy(st.mask);
    st.mask = NULL;
}

/* CAirBrushTool::OnDrag: 10 single pixels per tick, uniformly inside a
 * circle of diameter (size+1)&~1 (minimum 4). */
void brush_airbrush_tick(void)
{
    if (!st.active || st.kind != BRUSH_AIRBRUSH) return;
    int d = (st.size + 1) & ~1;
    if (d < 4) d = 4;
    int r = d / 2;
    cairo_surface_flush(app.img);
    uint32_t *p = (uint32_t *)cairo_image_surface_get_data(app.img);
    int stride = cairo_image_surface_get_stride(app.img) / 4;
    uint32_t px = rgb_to_px(st.color);
    int cx = (int)floor(st.lx), cy = (int)floor(st.ly);
    for (int i = 0; i < 10; i++) {
        int dx, dy;
        do {
            dx = rand() % (d + 1) - r;
            dy = rand() % (d + 1) - r;
        } while (dx * dx + dy * dy > r * r);
        int x = cx + dx, y = cy + dy;
        if (x >= 0 && y >= 0 && x < app.w && y < app.h) p[y * stride + x] = px;
    }
    cairo_surface_mark_dirty(app.img);
    canvas_redraw_rect(cx - r - 1, cy - r - 1, d + 3, d + 3);
}

/* ------------------------------------------------------------------ */
/* media on paths (shape outline / fill)                               */

static void path_bbox(cairo_t *cr, double pad, int w, int h, int *bx, int *by, int *bw, int *bh)
{
    double x0, y0, x1, y1;
    cairo_path_extents(cr, &x0, &y0, &x1, &y1);
    *bx = MAX(0, (int)floor(x0 - pad));
    *by = MAX(0, (int)floor(y0 - pad));
    *bw = MIN(w, (int)ceil(x1 + pad)) - *bx;
    *bh = MIN(h, (int)ceil(y1 + pad)) - *by;
}

void media_stroke_path(cairo_t *cr_img, MediaKind m, RGB c, int size, cairo_path_t *path, bool closed)
{
    (void)closed;
    if (m == MEDIA_NONE) return;
    cairo_surface_t *target = cairo_get_target(cr_img);
    int w = cairo_image_surface_get_width(target), h = cairo_image_surface_get_height(target);
    if (m == MEDIA_SOLID) {
        cairo_save(cr_img);
        cairo_new_path(cr_img);
        cairo_append_path(cr_img, path);
        cairo_set_source_rgb(cr_img, c.r, c.g, c.b);
        cairo_set_line_width(cr_img, size);
        cairo_set_line_join(cr_img, CAIRO_LINE_JOIN_MITER);
        cairo_set_line_cap(cr_img, CAIRO_LINE_CAP_ROUND);
        cairo_stroke(cr_img);
        cairo_restore(cr_img);
        return;
    }
    /* textured outline: artistic media use a fatter skeletal stroke */
    int width = (m == MEDIA_NATPENCIL) ? MAX(2, size) : MAX(4, size * 2);
    cairo_surface_t *mask = cairo_image_surface_create(CAIRO_FORMAT_A8, w, h);
    cairo_t *mc = cairo_create(mask);
    cairo_append_path(mc, path);
    cairo_set_line_width(mc, width);
    cairo_set_line_join(mc, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(mc, CAIRO_LINE_CAP_ROUND);
    int bx, by, bw, bh;
    path_bbox(mc, width + 2, w, h, &bx, &by, &bw, &bh);
    cairo_stroke(mc);
    cairo_destroy(mc);
    cairo_surface_flush(target);
    composite(target, NULL, mask, m, c, bx, by, bw, bh);
    cairo_surface_destroy(mask);
}

void media_fill_path(cairo_t *cr_img, MediaKind m, RGB c, cairo_path_t *path)
{
    if (m == MEDIA_NONE) return;
    cairo_surface_t *target = cairo_get_target(cr_img);
    int w = cairo_image_surface_get_width(target), h = cairo_image_surface_get_height(target);
    if (m == MEDIA_SOLID) {
        cairo_save(cr_img);
        cairo_new_path(cr_img);
        cairo_append_path(cr_img, path);
        cairo_set_source_rgb(cr_img, c.r, c.g, c.b);
        cairo_fill(cr_img);
        cairo_restore(cr_img);
        return;
    }
    cairo_surface_t *mask = cairo_image_surface_create(CAIRO_FORMAT_A8, w, h);
    cairo_t *mc = cairo_create(mask);
    cairo_append_path(mc, path);
    int bx, by, bw, bh;
    path_bbox(mc, 2, w, h, &bx, &by, &bw, &bh);
    cairo_fill(mc);
    cairo_destroy(mc);
    cairo_surface_flush(target);
    composite(target, NULL, mask, m, c, bx, by, bw, bh);
    cairo_surface_destroy(mask);
}

/* Gallery preview: an S-shaped stroke drawn with the real engine */
void brush_preview_icon(cairo_t *cr, BrushKind k, int w, int h)
{
    cairo_surface_t *s = surface_new(w, h, (RGB){1, 1, 1});
    int size = (k == BRUSH_AIRBRUSH) ? 8 : (k == BRUSH_CALLIG1 || k == BRUSH_CALLIG2) ? 6 :
               (k == BRUSH_REGULAR) ? 4 : 8;
    RGB col = {0.15, 0.15, 0.15};
    if (k == BRUSH_AIRBRUSH) {
        srand(5);
        uint32_t *p = (uint32_t *)cairo_image_surface_get_data(s);
        int stride = cairo_image_surface_get_stride(s) / 4;
        for (int i = 0; i < 40; i++) {
            double t = i / 39.0, x = 4 + t * (w - 8), y = h / 2.0 + sin(t * 2 * M_PI) * h / 4;
            for (int j = 0; j < 6; j++) {
                int dx = rand() % 9 - 4, dy = rand() % 9 - 4;
                if (dx * dx + dy * dy > 16) continue;
                int px = (int)x + dx, py = (int)y + dy;
                if (px >= 0 && py >= 0 && px < w && py < h) p[py * stride + px] = 0xFF262626;
            }
        }
        cairo_surface_mark_dirty(s);
    } else {
        cairo_surface_t *base = surface_copy(s);
        bool was = st.active;
        if (!was) {
            stroke_setup(s, base, k, col, size, 6, h / 2.0);
            double px = 6, py = h / 2.0;
            for (int i = 1; i <= 30; i++) {
                double t = i / 30.0, x = 6 + t * (w - 12), y = h / 2.0 + sin(t * 2 * M_PI) * h / 4;
                stroke_segment(px, py, x, y);
                px = x; py = y;
            }
            cairo_surface_destroy(st.mask);
            st.mask = NULL;
            st.active = false;
        }
        cairo_surface_destroy(base);
    }
    cairo_set_source_surface(cr, s, 0, 0);
    cairo_paint(cr);
    cairo_surface_destroy(s);
}
