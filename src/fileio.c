/* File I/O, printing and desktop integration (CPBDoc / CBmpStream /
 * C_PrintDialogEx / CPBView::SetTheWallpaper). */
#include "paint.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/* surface <-> pixbuf                                                  */

static cairo_surface_t *surface_from_pixbuf(GdkPixbuf *pb)
{
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    cairo_surface_t *s = surface_new(w, h, (RGB){1, 1, 1});
    cairo_t *cr = cairo_create(s);
    gdk_cairo_set_source_pixbuf(cr, pb, 0, 0);    /* alpha is flattened onto white */
    cairo_paint(cr);
    cairo_destroy(cr);
    return s;
}

static GdkPixbuf *pixbuf_from_surface(cairo_surface_t *s)
{
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    cairo_surface_flush(s);
    GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
    int ps = gdk_pixbuf_get_rowstride(pb), ss = cairo_image_surface_get_stride(s);
    guchar *pd = gdk_pixbuf_get_pixels(pb);
    unsigned char *sd = cairo_image_surface_get_data(s);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            RGB c = px_to_rgb(*(uint32_t *)(sd + y * ss + x * 4));
            guchar *q = pd + y * ps + x * 3;
            q[0] = (guchar)lround(c.r * 255); q[1] = (guchar)lround(c.g * 255); q[2] = (guchar)lround(c.b * 255);
        }
    return pb;
}

cairo_surface_t *load_image_surface(const char *path, GError **err)
{
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, err);
    if (!pb) return NULL;
    GdkPixbuf *o = gdk_pixbuf_apply_embedded_orientation(pb);
    cairo_surface_t *s = surface_from_pixbuf(o);
    g_object_unref(o);
    g_object_unref(pb);
    return s;
}

const char *format_from_path(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot) return "png";
    gchar *e = g_ascii_strdown(dot + 1, -1);
    const char *f = "png";
    if (!strcmp(e, "jpg") || !strcmp(e, "jpeg") || !strcmp(e, "jpe") || !strcmp(e, "jfif")) f = "jpeg";
    else if (!strcmp(e, "bmp") || !strcmp(e, "dib")) f = "bmp24";
    else if (!strcmp(e, "gif")) f = "gif";
    else if (!strcmp(e, "tif") || !strcmp(e, "tiff")) f = "tiff";
    else if (!strcmp(e, "ico")) f = "ico";
    g_free(e);
    return f;
}

bool file_open(const char *path, GError **err)
{
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, err);
    if (!pb) return false;
    const char *xd = gdk_pixbuf_get_option(pb, "x-dpi"), *yd = gdk_pixbuf_get_option(pb, "y-dpi");
    app.dpi_x = xd ? atof(xd) : 96;
    app.dpi_y = yd ? atof(yd) : 96;
    if (app.dpi_x <= 0) app.dpi_x = 96;
    if (app.dpi_y <= 0) app.dpi_y = 96;
    GdkPixbuf *o = gdk_pixbuf_apply_embedded_orientation(pb);
    cairo_surface_t *s = surface_from_pixbuf(o);
    g_object_unref(o);
    g_object_unref(pb);
    sel_clear(false);
    undo_clear();
    app.monochrome = false;
    set_image(s);
    g_free(app.filename);
    app.filename = g_strdup(path);
    app.dirty = false;
    gtk_recent_manager_add_item(gtk_recent_manager_get_default(), g_filename_to_uri(path, NULL, NULL));
    update_title();
    update_ui();
    return true;
}

/* ------------------------------------------------------------------ */
/* colour quantisation (median cut) for GIF and palette BMPs            */

typedef struct { int lo[3], hi[3]; int count; } Box;

static uint32_t *hist;   /* 32x32x32 histogram */

static void box_shrink(Box *b)
{
    int lo[3] = { 31, 31, 31 }, hi[3] = { 0, 0, 0 };
    b->count = 0;
    for (int r = b->lo[0]; r <= b->hi[0]; r++)
        for (int g = b->lo[1]; g <= b->hi[1]; g++)
            for (int bl = b->lo[2]; bl <= b->hi[2]; bl++) {
                uint32_t n = hist[(r << 10) | (g << 5) | bl];
                if (!n) continue;
                b->count += n;
                if (r < lo[0]) lo[0] = r;
                if (r > hi[0]) hi[0] = r;
                if (g < lo[1]) lo[1] = g;
                if (g > hi[1]) hi[1] = g;
                if (bl < lo[2]) lo[2] = bl;
                if (bl > hi[2]) hi[2] = bl;
            }
    if (b->count) { memcpy(b->lo, lo, sizeof lo); memcpy(b->hi, hi, sizeof hi); }
}

/* returns palette size; idx receives one index per pixel */
static int quantize(cairo_surface_t *s, int maxc, uint8_t pal[256][3], uint8_t *idx)
{
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    int st = cairo_image_surface_get_stride(s) / 4;
    cairo_surface_flush(s);
    uint32_t *p = (uint32_t *)cairo_image_surface_get_data(s);

    /* exact palette if the image has few enough colours */
    GHashTable *exact = g_hash_table_new(g_direct_hash, g_direct_equal);
    int n = 0;
    bool fits = true;
    for (int y = 0; y < h && fits; y++)
        for (int x = 0; x < w; x++) {
            uint32_t v = p[y * st + x] | 0xFF000000;
            if (!g_hash_table_contains(exact, GUINT_TO_POINTER(v))) {
                if (n == maxc) { fits = false; break; }
                pal[n][0] = (v >> 16) & 255; pal[n][1] = (v >> 8) & 255; pal[n][2] = v & 255;
                g_hash_table_insert(exact, GUINT_TO_POINTER(v), GINT_TO_POINTER(n));
                n++;
            }
        }
    if (fits) {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                idx[y * w + x] = GPOINTER_TO_INT(g_hash_table_lookup(exact, GUINT_TO_POINTER(p[y * st + x] | 0xFF000000)));
        g_hash_table_destroy(exact);
        return MAX(n, 2);
    }
    g_hash_table_destroy(exact);

    hist = g_new0(uint32_t, 32768);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint32_t v = p[y * st + x];
            hist[(((v >> 19) & 31) << 10) | (((v >> 11) & 31) << 5) | ((v >> 3) & 31)]++;
        }
    Box boxes[256];
    int nb = 1;
    boxes[0] = (Box){ { 0, 0, 0 }, { 31, 31, 31 }, 0 };
    box_shrink(&boxes[0]);
    while (nb < maxc) {
        int best = -1, bestv = 0, axis = 0;
        for (int i = 0; i < nb; i++)
            for (int a = 0; a < 3; a++) {
                int v = (boxes[i].hi[a] - boxes[i].lo[a]) * (boxes[i].count > 1 ? 1 : 0);
                if (v > bestv) { bestv = v; best = i; axis = a; }
            }
        if (best < 0) break;
        Box *b = &boxes[best];
        /* split at the median along the longest axis */
        uint64_t half = b->count / 2, acc = 0, proj[32] = { 0 };
        for (int r = b->lo[0]; r <= b->hi[0]; r++)
            for (int g = b->lo[1]; g <= b->hi[1]; g++)
                for (int bl = b->lo[2]; bl <= b->hi[2]; bl++) {
                    int cc[3] = { r, g, bl };
                    proj[cc[axis]] += hist[(r << 10) | (g << 5) | bl];
                }
        int cut = b->lo[axis];
        for (int c = b->lo[axis]; c < b->hi[axis]; c++) {
            acc += proj[c];
            cut = c;
            if (acc >= half) break;
        }
        Box nbx = *b;
        b->hi[axis] = cut;
        nbx.lo[axis] = cut + 1;
        box_shrink(b);
        box_shrink(&nbx);
        boxes[nb++] = nbx;
    }
    for (int i = 0; i < nb; i++) {
        double sr = 0, sg = 0, sb = 0, cnt = 0;
        Box *b = &boxes[i];
        for (int r = b->lo[0]; r <= b->hi[0]; r++)
            for (int g = b->lo[1]; g <= b->hi[1]; g++)
                for (int bl = b->lo[2]; bl <= b->hi[2]; bl++) {
                    uint32_t c = hist[(r << 10) | (g << 5) | bl];
                    sr += c * (r * 8 + 4); sg += c * (g * 8 + 4); sb += c * (bl * 8 + 4); cnt += c;
                }
        if (cnt == 0) cnt = 1;
        pal[i][0] = (uint8_t)(sr / cnt); pal[i][1] = (uint8_t)(sg / cnt); pal[i][2] = (uint8_t)(sb / cnt);
    }
    /* nearest-colour lookup cached per 15-bit bucket */
    int16_t *lut = g_new(int16_t, 32768);
    for (int i = 0; i < 32768; i++) lut[i] = -1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint32_t v = p[y * st + x];
            int key = (((v >> 19) & 31) << 10) | (((v >> 11) & 31) << 5) | ((v >> 3) & 31);
            if (lut[key] < 0) {
                int r = (v >> 16) & 255, g = (v >> 8) & 255, b = v & 255, bi = 0, bd = 1 << 30;
                for (int i = 0; i < nb; i++) {
                    int d = (r - pal[i][0]) * (r - pal[i][0]) + (g - pal[i][1]) * (g - pal[i][1]) +
                            (b - pal[i][2]) * (b - pal[i][2]);
                    if (d < bd) { bd = d; bi = i; }
                }
                lut[key] = bi;
            }
            idx[y * w + x] = (uint8_t)lut[key];
        }
    g_free(lut);
    g_free(hist);
    return nb;
}

/* ------------------------------------------------------------------ */
/* BMP writer: 1, 4, 8 and 24 bpp (the four "Save as type" BMP choices) */

static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
static void put32(FILE *f, unsigned v) { put16(f, v & 0xFFFF); put16(f, v >> 16); }

static bool save_bmp(cairo_surface_t *s, const char *path, int bpp, GError **err)
{
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    int st = cairo_image_surface_get_stride(s) / 4;
    cairo_surface_flush(s);
    uint32_t *p = (uint32_t *)cairo_image_surface_get_data(s);
    uint8_t pal[256][3];
    int ncol = 0;
    uint8_t *idx = NULL;
    if (bpp == 1) {
        ncol = 2;
        pal[0][0] = pal[0][1] = pal[0][2] = 0;
        pal[1][0] = pal[1][1] = pal[1][2] = 255;
        idx = g_malloc((size_t)w * h);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                RGB c = px_to_rgb(p[y * st + x]);
                idx[y * w + x] = (0.299 * c.r + 0.587 * c.g + 0.114 * c.b) >= 0.5;
            }
    } else if (bpp == 4 || bpp == 8) {
        idx = g_malloc((size_t)w * h);
        ncol = quantize(s, bpp == 4 ? 16 : 256, pal, idx);
        ncol = 1 << bpp;
    }
    int rowbytes = ((w * bpp + 31) / 32) * 4;
    unsigned off = 14 + 40 + ncol * 4, size = off + rowbytes * h;
    FILE *f = fopen(path, "wb");
    if (!f) {
        g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno), "%s", g_strerror(errno));
        g_free(idx);
        return false;
    }
    fputc('B', f); fputc('M', f);
    put32(f, size); put32(f, 0); put32(f, off);
    put32(f, 40); put32(f, w); put32(f, h); put16(f, 1); put16(f, bpp); put32(f, 0);
    put32(f, rowbytes * h);
    put32(f, (unsigned)lround(app.dpi_x / 0.0254)); put32(f, (unsigned)lround(app.dpi_y / 0.0254));
    put32(f, ncol); put32(f, 0);
    for (int i = 0; i < ncol; i++) {
        fputc(pal[i][2], f); fputc(pal[i][1], f); fputc(pal[i][0], f); fputc(0, f);
    }
    uint8_t *row = g_malloc0(rowbytes);
    for (int y = h - 1; y >= 0; y--) {
        memset(row, 0, rowbytes);
        for (int x = 0; x < w; x++) {
            if (bpp == 24) {
                RGB c = px_to_rgb(p[y * st + x]);
                row[x * 3] = (uint8_t)lround(c.b * 255);
                row[x * 3 + 1] = (uint8_t)lround(c.g * 255);
                row[x * 3 + 2] = (uint8_t)lround(c.r * 255);
            } else if (bpp == 8) row[x] = idx[y * w + x];
            else if (bpp == 4) row[x / 2] |= idx[y * w + x] << ((x & 1) ? 0 : 4);
            else if (idx[y * w + x]) row[x / 8] |= 0x80 >> (x & 7);
        }
        fwrite(row, 1, rowbytes, f);
    }
    g_free(row);
    g_free(idx);
    bool ok = fclose(f) == 0;
    if (!ok) g_set_error(err, G_FILE_ERROR, G_FILE_ERROR_IO, "Write failed");
    return ok;
}

/* ------------------------------------------------------------------ */
/* GIF writer (GIF89a, single frame, LZW)                               */

typedef struct { FILE *f; uint8_t buf[256]; int n; uint32_t acc; int bits; } BitOut;

static void bo_flush_block(BitOut *b)
{
    if (!b->n) return;
    fputc(b->n, b->f);
    fwrite(b->buf, 1, b->n, b->f);
    b->n = 0;
}

static void bo_put(BitOut *b, int code, int size)
{
    b->acc |= (uint32_t)code << b->bits;
    b->bits += size;
    while (b->bits >= 8) {
        b->buf[b->n++] = b->acc & 255;
        if (b->n == 255) bo_flush_block(b);
        b->acc >>= 8;
        b->bits -= 8;
    }
}

bool save_gif(cairo_surface_t *s, const char *path, GError **err)
{
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    uint8_t pal[256][3];
    uint8_t *idx = g_malloc((size_t)w * h);
    int n = quantize(s, 256, pal, idx);
    int depth = 1;
    while ((1 << depth) < n) depth++;
    FILE *f = fopen(path, "wb");
    if (!f) {
        g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno), "%s", g_strerror(errno));
        g_free(idx);
        return false;
    }
    fwrite("GIF89a", 1, 6, f);
    put16(f, w); put16(f, h);
    fputc(0x80 | ((depth - 1) << 4) | (depth - 1), f);
    fputc(0, f); fputc(0, f);
    for (int i = 0; i < (1 << depth); i++) {
        fputc(i < n ? pal[i][0] : 0, f); fputc(i < n ? pal[i][1] : 0, f); fputc(i < n ? pal[i][2] : 0, f);
    }
    fputc(0x2C, f); put16(f, 0); put16(f, 0); put16(f, w); put16(f, h); fputc(0, f);
    int min = depth < 2 ? 2 : depth;
    fputc(min, f);
    int clear = 1 << min, eoi = clear + 1, next = eoi + 1, size = min + 1;
    /* dictionary: (prefix << 8 | byte) -> code */
    GHashTable *dict = g_hash_table_new(g_direct_hash, g_direct_equal);
    BitOut b = { f, { 0 }, 0, 0, 0 };
    bo_put(&b, clear, size);
    int prefix = idx[0];
    for (long i = 1; i < (long)w * h; i++) {
        int c = idx[i];
        gpointer key = GINT_TO_POINTER(((prefix + 1) << 8) | c);
        gpointer v = g_hash_table_lookup(dict, key);
        if (v) { prefix = GPOINTER_TO_INT(v); continue; }
        bo_put(&b, prefix, size);
        if (next < 4096) {
            g_hash_table_insert(dict, key, GINT_TO_POINTER(next));
            if (next == (1 << size) && size < 12) size++;
            next++;
        } else {
            bo_put(&b, clear, size);
            g_hash_table_remove_all(dict);
            next = eoi + 1;
            size = min + 1;
        }
        prefix = c;
    }
    bo_put(&b, prefix, size);
    bo_put(&b, eoi, size);
    if (b.bits) { b.buf[b.n++] = b.acc & 255; }
    bo_flush_block(&b);
    fputc(0, f);
    fputc(0x3B, f);
    g_hash_table_destroy(dict);
    g_free(idx);
    bool ok = fclose(f) == 0;
    if (!ok) g_set_error(err, G_FILE_ERROR, G_FILE_ERROR_IO, "Write failed");
    return ok;
}

/* ------------------------------------------------------------------ */

bool file_save(const char *path, const char *fmt, GError **err)
{
    tool_commit_pending();
    bool ok;
    if (!strcmp(fmt, "bmp24")) ok = save_bmp(app.img, path, 24, err);
    else if (!strcmp(fmt, "bmp8")) ok = save_bmp(app.img, path, 8, err);
    else if (!strcmp(fmt, "bmp4")) ok = save_bmp(app.img, path, 4, err);
    else if (!strcmp(fmt, "bmp1")) ok = save_bmp(app.img, path, 1, err);
    else if (!strcmp(fmt, "gif")) ok = save_gif(app.img, path, err);
    else {
        GdkPixbuf *pb = pixbuf_from_surface(app.img);
        char dx[32], dy[32];
        g_snprintf(dx, sizeof dx, "%d", (int)lround(app.dpi_x));
        g_snprintf(dy, sizeof dy, "%d", (int)lround(app.dpi_y));
        if (!strcmp(fmt, "jpeg"))
            ok = gdk_pixbuf_save(pb, path, "jpeg", err, "quality", "95", "x-dpi", dx, "y-dpi", dy, NULL);
        else if (!strcmp(fmt, "png"))
            ok = gdk_pixbuf_save(pb, path, "png", err, "x-dpi", dx, "y-dpi", dy, NULL);
        else
            ok = gdk_pixbuf_save(pb, path, fmt, err, NULL);
        g_object_unref(pb);
    }
    if (ok) {
        if (!app.filename || strcmp(app.filename, path)) {
            g_free(app.filename);
            app.filename = g_strdup(path);
        }
        app.dirty = false;
        gchar *uri = g_filename_to_uri(path, NULL, NULL);
        if (uri) gtk_recent_manager_add_item(gtk_recent_manager_get_default(), uri);
        g_free(uri);
        update_title();
        update_status();
    }
    return ok;
}

/* ------------------------------------------------------------------ */
/* printing (CPBView::OnFilePrint / OnFilePrintPreview / page setup)    */

static GtkPageSetup *page_setup;
static GtkPrintSettings *print_settings;

static void draw_page(GtkPrintOperation *op, GtkPrintContext *ctx, int nr, gpointer d)
{
    (void)op; (void)nr; (void)d;
    cairo_t *cr = gtk_print_context_get_cairo_context(ctx);
    double pw = gtk_print_context_get_width(ctx), ph = gtk_print_context_get_height(ctx);
    double dpi = gtk_print_context_get_dpi_x(ctx);
    /* actual size at the image resolution, shrunk to fit like "Fit to 1 page" */
    double sx = dpi / app.dpi_x, sy = gtk_print_context_get_dpi_y(ctx) / app.dpi_y;
    double fit = MIN(pw / (app.w * sx), ph / (app.h * sy));
    if (fit < 1) { sx *= fit; sy *= fit; }
    double ox = (pw - app.w * sx) / 2;
    cairo_translate(cr, ox > 0 ? ox : 0, 0);
    cairo_scale(cr, sx, sy);
    cairo_set_source_surface(cr, app.img, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
}

void do_print(bool preview)
{
    tool_commit_pending();
    GtkPrintOperation *op = gtk_print_operation_new();
    gtk_print_operation_set_n_pages(op, 1);
    gtk_print_operation_set_unit(op, GTK_UNIT_POINTS);
    if (page_setup) gtk_print_operation_set_default_page_setup(op, page_setup);
    if (print_settings) gtk_print_operation_set_print_settings(op, print_settings);
    gchar *title = g_path_get_basename(app.filename ? app.filename : "Untitled");
    gtk_print_operation_set_job_name(op, title);
    g_free(title);
    g_signal_connect(op, "draw-page", G_CALLBACK(draw_page), NULL);
    GError *err = NULL;
    GtkPrintOperationResult r = gtk_print_operation_run(
        op, preview ? GTK_PRINT_OPERATION_ACTION_PREVIEW : GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
        GTK_WINDOW(app.window), &err);
    if (r == GTK_PRINT_OPERATION_RESULT_APPLY) {
        if (print_settings) g_object_unref(print_settings);
        print_settings = g_object_ref(gtk_print_operation_get_print_settings(op));
    } else if (r == GTK_PRINT_OPERATION_RESULT_ERROR) {
        error_dialog("%s", err ? err->message : "Printing failed.");
        g_clear_error(&err);
    }
    g_object_unref(op);
}

void do_page_setup(void)
{
    if (!print_settings) print_settings = gtk_print_settings_new();
    GtkPageSetup *n = gtk_print_run_page_setup_dialog(GTK_WINDOW(app.window), page_setup, print_settings);
    if (page_setup) g_object_unref(page_setup);
    page_setup = n;
}

/* CPBView::SetTheWallpaper - Fill / Tile / Center */
void set_wallpaper(const char *mode)
{
    if (!app.filename || app.dirty) {
        error_dialog("Save the picture before setting it as the desktop background.");
        if (!cmd_save()) return;
    }
    gchar *uri = g_filename_to_uri(app.filename, NULL, NULL);
    const char *opt = !strcmp(mode, "tile") ? "wallpaper" : !strcmp(mode, "center") ? "centered" : "zoom";
    gchar *argv1[] = { "gsettings", "set", "org.gnome.desktop.background", "picture-uri", uri, NULL };
    gchar *argv2[] = { "gsettings", "set", "org.gnome.desktop.background", "picture-uri-dark", uri, NULL };
    gchar *argv3[] = { "gsettings", "set", "org.gnome.desktop.background", "picture-options", (gchar *)opt, NULL };
    GError *err = NULL;
    int status = 0;
    bool ok = g_spawn_sync(NULL, argv1, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL,
                           NULL, NULL, &status, &err) && status == 0;
    if (ok) {
        g_spawn_sync(NULL, argv2, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        g_spawn_sync(NULL, argv3, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    } else {
        error_dialog("Could not set the desktop background%s%s",
                     err ? ": " : ".", err ? err->message : "");
    }
    g_clear_error(&err);
    g_free(uri);
}
