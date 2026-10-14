/* Vector icons for the ribbon, drawn with cairo on a 16x16 grid.
 * (The original's ribbon bitmaps are Microsoft artwork and are not reused.) */
#include "paint.h"
#include <math.h>
#include <string.h>

static void rgb(cairo_t *cr, int c) { cairo_set_source_rgb(cr, ((c >> 16) & 255) / 255.0, ((c >> 8) & 255) / 255.0, (c & 255) / 255.0); }

static void box(cairo_t *cr, double x, double y, double w, double h, int fill, int line)
{
    cairo_rectangle(cr, x + 0.5, y + 0.5, w, h);
    if (fill >= 0) { rgb(cr, fill); cairo_fill_preserve(cr); }
    rgb(cr, line);
    cairo_stroke(cr);
}

static void page(cairo_t *cr, double x, double y, double w, double h)
{
    cairo_move_to(cr, x + 0.5, y + 0.5);
    cairo_line_to(cr, x + w - 3 + 0.5, y + 0.5);
    cairo_line_to(cr, x + w + 0.5, y + 3.5);
    cairo_line_to(cr, x + w + 0.5, y + h + 0.5);
    cairo_line_to(cr, x + 0.5, y + h + 0.5);
    cairo_close_path(cr);
    rgb(cr, 0xFFFFFF);
    cairo_fill_preserve(cr);
    rgb(cr, 0x6A7B95);
    cairo_stroke(cr);
}

static void pencil_shape(cairo_t *cr, double s)
{
    /* diagonal pencil from bottom-left to top-right */
    cairo_save(cr);
    cairo_translate(cr, 8, 8);
    cairo_rotate(cr, -M_PI / 4);
    cairo_scale(cr, s, s);
    cairo_rectangle(cr, -4, -1.8, 8, 3.6);
    rgb(cr, 0xF2C230); cairo_fill_preserve(cr); rgb(cr, 0x8A6A10); cairo_stroke(cr);
    cairo_rectangle(cr, 4, -1.8, 2, 3.6);
    rgb(cr, 0xE58A9B); cairo_fill_preserve(cr); rgb(cr, 0x8A6A10); cairo_stroke(cr);
    cairo_move_to(cr, -4, -1.8); cairo_line_to(cr, -7, 0); cairo_line_to(cr, -4, 1.8); cairo_close_path(cr);
    rgb(cr, 0xF5DEB3); cairo_fill_preserve(cr); rgb(cr, 0x8A6A10); cairo_stroke(cr);
    cairo_move_to(cr, -6, -0.6); cairo_line_to(cr, -7, 0); cairo_line_to(cr, -6, 0.6); cairo_close_path(cr);
    rgb(cr, 0x333333); cairo_fill(cr);
    cairo_restore(cr);
}

static void magnifier(cairo_t *cr, double cx, double cy, double r)
{
    cairo_set_line_width(cr, 2.2);
    rgb(cr, 0x7A5A2A);
    cairo_move_to(cr, cx + r * 0.7, cy + r * 0.7);
    cairo_line_to(cr, cx + r * 1.9, cy + r * 1.9);
    cairo_stroke(cr);
    cairo_set_line_width(cr, 1.2);
    cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
    cairo_set_source_rgba(cr, 0.8, 0.92, 1, 0.9);
    cairo_fill_preserve(cr);
    rgb(cr, 0x40608A);
    cairo_stroke(cr);
    cairo_set_line_width(cr, 1);
}

static void arrow_curve(cairo_t *cr, bool cw)
{
    cairo_set_line_width(cr, 1.8);
    rgb(cr, 0x2D6FC4);
    if (cw) cairo_arc(cr, 8, 9, 4.5, M_PI, 2 * M_PI * 0.95);
    else cairo_arc_negative(cr, 8, 9, 4.5, 0, M_PI * 1.05);
    cairo_stroke(cr);
    double ex = cw ? 12.4 : 3.6;
    cairo_move_to(cr, ex - 2.5, 7.5); cairo_line_to(cr, ex + 2.5, 7.5); cairo_line_to(cr, ex, 11);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_set_line_width(cr, 1);
}

static void bucket(cairo_t *cr)
{
    cairo_save(cr);
    cairo_translate(cr, 7, 8);
    cairo_rotate(cr, -0.6);
    cairo_rectangle(cr, -4, -3, 8, 8);
    rgb(cr, 0xDDE6F2); cairo_fill_preserve(cr); rgb(cr, 0x50607A); cairo_stroke(cr);
    cairo_restore(cr);
    cairo_move_to(cr, 12, 9);
    cairo_curve_to(cr, 15, 11, 15, 14, 13.5, 14.5);
    cairo_curve_to(cr, 12, 14.5, 11.5, 12, 12, 9);
    rgb(cr, 0x2D6FC4); cairo_fill(cr);
}

static void picker(cairo_t *cr)
{
    cairo_save(cr);
    cairo_translate(cr, 8, 8);
    cairo_rotate(cr, -M_PI / 4);
    cairo_rectangle(cr, -6.5, -0.8, 8, 1.6);
    rgb(cr, 0xC8D8EE); cairo_fill_preserve(cr); rgb(cr, 0x40608A); cairo_stroke(cr);
    cairo_rectangle(cr, 1.5, -2.2, 5, 4.4);
    rgb(cr, 0x404040); cairo_fill(cr);
    cairo_restore(cr);
}

static void eraser(cairo_t *cr)
{
    cairo_save(cr);
    cairo_translate(cr, 8, 9);
    cairo_rotate(cr, -0.5);
    cairo_rectangle(cr, -6, -3, 6, 6);
    rgb(cr, 0xF4F4F4); cairo_fill_preserve(cr); rgb(cr, 0x6A7B95); cairo_stroke(cr);
    cairo_rectangle(cr, 0, -3, 6, 6);
    rgb(cr, 0xE0607E); cairo_fill_preserve(cr); rgb(cr, 0x8A3040); cairo_stroke(cr);
    cairo_restore(cr);
}

static void brushes(cairo_t *cr)
{
    cairo_save(cr);
    cairo_translate(cr, 9, 7);
    cairo_rotate(cr, -M_PI / 4);
    cairo_rectangle(cr, -1, -1.3, 9, 2.6);
    rgb(cr, 0xA0522D); cairo_fill(cr);
    cairo_rectangle(cr, -3, -1.6, 2.2, 3.2);
    rgb(cr, 0xB0B8C0); cairo_fill(cr);
    cairo_move_to(cr, -3, -1.6); cairo_curve_to(cr, -6, -1.6, -8, 0, -9, 0);
    cairo_curve_to(cr, -8, 0.5, -6, 1.6, -3, 1.6); cairo_close_path(cr);
    rgb(cr, 0x2D6FC4); cairo_fill(cr);
    cairo_restore(cr);
}

void draw_icon(cairo_t *cr, const char *n, double size)
{
    cairo_save(cr);
    cairo_scale(cr, size / 16.0, size / 16.0);
    cairo_set_line_width(cr, 1);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    if (!strcmp(n, "paste")) {
        box(cr, 2, 2, 10, 12, 0xC69C5D, 0x7A5A2A);
        box(cr, 4.5, 1, 5, 2.5, 0xC0C8D4, 0x50607A);
        page(cr, 7, 6, 7, 9);
    } else if (!strcmp(n, "cut")) {
        cairo_set_line_width(cr, 1.3);
        rgb(cr, 0x50607A);
        cairo_move_to(cr, 5, 1); cairo_line_to(cr, 10, 10); cairo_stroke(cr);
        cairo_move_to(cr, 11, 1); cairo_line_to(cr, 6, 10); cairo_stroke(cr);
        rgb(cr, 0x2D6FC4);
        cairo_arc(cr, 5, 12.5, 2.3, 0, 2 * M_PI); cairo_stroke(cr);
        cairo_arc(cr, 11, 12.5, 2.3, 0, 2 * M_PI); cairo_stroke(cr);
    } else if (!strcmp(n, "copy")) {
        page(cr, 1.5, 1.5, 8, 10);
        page(cr, 6, 4.5, 8, 10);
    } else if (!strcmp(n, "select") || !strcmp(n, "select-rect")) {
        double d[] = { 1.5, 1.5 };
        cairo_set_dash(cr, d, 2, 0);
        rgb(cr, 0x2050A0);
        cairo_rectangle(cr, 2.5, 3.5, 11, 9);
        cairo_stroke(cr);
    } else if (!strcmp(n, "select-free")) {
        double d[] = { 1.5, 1.5 };
        cairo_set_dash(cr, d, 2, 0);
        rgb(cr, 0x2050A0);
        cairo_move_to(cr, 3, 6); cairo_curve_to(cr, 3, 1, 12, 1, 13, 5);
        cairo_curve_to(cr, 14, 9, 9, 8, 10, 12); cairo_curve_to(cr, 10, 15, 3, 14, 3, 6);
        cairo_stroke(cr);
    } else if (!strcmp(n, "select-all")) {
        box(cr, 2, 2, 12, 12, 0xD6E6FA, 0x2050A0);
    } else if (!strcmp(n, "invert-sel")) {
        box(cr, 2, 2, 12, 12, 0xD6E6FA, 0x2050A0);
        box(cr, 5, 5, 6, 6, 0xFFFFFF, 0x2050A0);
    } else if (!strcmp(n, "delete")) {
        cairo_set_line_width(cr, 2.2);
        rgb(cr, 0xC0392B);
        cairo_move_to(cr, 4, 4); cairo_line_to(cr, 12, 12);
        cairo_move_to(cr, 12, 4); cairo_line_to(cr, 4, 12);
        cairo_stroke(cr);
    } else if (!strcmp(n, "transparent")) {
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++) {
                cairo_rectangle(cr, 2 + x * 3, 2 + y * 3, 3, 3);
                rgb(cr, (x + y) & 1 ? 0xC8C8C8 : 0xFFFFFF);
                cairo_fill(cr);
            }
        box(cr, 2, 2, 12, 12, -1, 0x6A7B95);
    } else if (!strcmp(n, "crop")) {
        cairo_set_line_width(cr, 2);
        rgb(cr, 0x50607A);
        cairo_move_to(cr, 4, 1); cairo_line_to(cr, 4, 12); cairo_line_to(cr, 15, 12);
        cairo_move_to(cr, 1, 4); cairo_line_to(cr, 12, 4); cairo_line_to(cr, 12, 15);
        cairo_stroke(cr);
    } else if (!strcmp(n, "resize")) {
        box(cr, 1, 5, 8, 8, 0xD6E6FA, 0x50607A);
        box(cr, 4, 2, 11, 11, -1, 0x2D6FC4);
        cairo_move_to(cr, 9, 7); cairo_line_to(cr, 13, 3); cairo_stroke(cr);
    } else if (!strcmp(n, "rotate") || !strcmp(n, "rotate-r")) {
        box(cr, 4, 6, 8, 8, 0xD6E6FA, 0x50607A);
        arrow_curve(cr, true);
    } else if (!strcmp(n, "rotate-l")) {
        box(cr, 4, 6, 8, 8, 0xD6E6FA, 0x50607A);
        arrow_curve(cr, false);
    } else if (!strcmp(n, "rotate-180")) {
        box(cr, 4, 6, 8, 8, 0xD6E6FA, 0x50607A);
        cairo_set_line_width(cr, 1.6);
        rgb(cr, 0x2D6FC4);
        cairo_arc(cr, 8, 8, 6, M_PI * 1.1, M_PI * 2.6);
        cairo_stroke(cr);
    } else if (!strcmp(n, "flip-v")) {
        cairo_move_to(cr, 3, 7); cairo_line_to(cr, 13, 7); cairo_line_to(cr, 8, 1.5); cairo_close_path(cr);
        rgb(cr, 0x2D6FC4); cairo_fill(cr);
        cairo_move_to(cr, 3, 9); cairo_line_to(cr, 13, 9); cairo_line_to(cr, 8, 14.5); cairo_close_path(cr);
        rgb(cr, 0x50607A); cairo_stroke(cr);
    } else if (!strcmp(n, "flip-h")) {
        cairo_move_to(cr, 7, 3); cairo_line_to(cr, 7, 13); cairo_line_to(cr, 1.5, 8); cairo_close_path(cr);
        rgb(cr, 0x2D6FC4); cairo_fill(cr);
        cairo_move_to(cr, 9, 3); cairo_line_to(cr, 9, 13); cairo_line_to(cr, 14.5, 8); cairo_close_path(cr);
        rgb(cr, 0x50607A); cairo_stroke(cr);
    } else if (!strcmp(n, "pencil") || !strcmp(n, "cur-pencil")) {
        pencil_shape(cr, 1.0);
    } else if (!strcmp(n, "fill") || !strcmp(n, "cur-fill")) {
        bucket(cr);
    } else if (!strcmp(n, "text")) {
        cairo_select_font_face(cr, "Serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 14);
        rgb(cr, 0x2050A0);
        cairo_move_to(cr, 2.5, 13.5);
        cairo_show_text(cr, "A");
    } else if (!strcmp(n, "eraser")) {
        eraser(cr);
    } else if (!strcmp(n, "picker") || !strcmp(n, "cur-picker")) {
        picker(cr);
    } else if (!strcmp(n, "zoom") || !strcmp(n, "cur-zoom")) {
        magnifier(cr, 6.5, 6.5, 4.2);
    } else if (!strcmp(n, "zoom-in") || !strcmp(n, "zoom-out")) {
        magnifier(cr, 6.5, 6.5, 4.6);
        cairo_set_line_width(cr, 1.5);
        rgb(cr, 0x1A3A6A);
        cairo_move_to(cr, 4.3, 6.5); cairo_line_to(cr, 8.7, 6.5);
        if (n[5] == 'i') { cairo_move_to(cr, 6.5, 4.3); cairo_line_to(cr, 6.5, 8.7); }
        cairo_stroke(cr);
    } else if (!strcmp(n, "zoom-100")) {
        magnifier(cr, 6.5, 6.5, 4.8);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 4.5);
        rgb(cr, 0x1A3A6A);
        cairo_move_to(cr, 2.7, 8.1);
        cairo_show_text(cr, "100");
    } else if (!strcmp(n, "brushes")) {
        brushes(cr);
    } else if (!strcmp(n, "outline")) {
        cairo_set_line_width(cr, 1.5);
        rgb(cr, 0x2D6FC4);
        cairo_rectangle(cr, 2.5, 3.5, 11, 9);
        cairo_stroke(cr);
    } else if (!strcmp(n, "shapefill")) {
        box(cr, 2, 3, 11, 9, 0x6FA8E8, 0x2050A0);
    } else if (!strcmp(n, "size")) {
        rgb(cr, 0x202020);
        cairo_rectangle(cr, 2, 2.5, 12, 1); cairo_fill(cr);
        cairo_rectangle(cr, 2, 5.5, 12, 2); cairo_fill(cr);
        cairo_rectangle(cr, 2, 9.5, 12, 4); cairo_fill(cr);
    } else if (!strcmp(n, "edit-colors")) {
        for (int i = 0; i < 6; i++) {
            double a = i * M_PI / 3;
            static const int cols[] = { 0xED1C24, 0xFFF200, 0x22B14C, 0x00A2E8, 0x3F48CC, 0xA349A4 };
            cairo_arc(cr, 8 + cos(a) * 3.3, 8 + sin(a) * 3.3, 2.8, 0, 2 * M_PI);
            rgb(cr, cols[i]);
            cairo_fill(cr);
        }
    } else if (!strcmp(n, "save")) {
        box(cr, 1.5, 1.5, 13, 13, 0x4A7CC0, 0x1A3A6A);
        box(cr, 4, 1.5, 8, 5, 0xFFFFFF, 0x1A3A6A);
        box(cr, 4, 9.5, 8, 5, 0xD0D8E4, 0x1A3A6A);
    } else if (!strcmp(n, "undo") || !strcmp(n, "redo")) {
        bool u = n[0] == 'u';
        cairo_set_line_width(cr, 2);
        rgb(cr, 0x2D6FC4);
        if (u) { cairo_arc(cr, 9, 9, 4.5, M_PI * 1.15, M_PI * 2.5); }
        else { cairo_arc_negative(cr, 7, 9, 4.5, -M_PI * 0.15, -M_PI * 1.5); }
        cairo_stroke(cr);
        double x = u ? 3 : 13;
        cairo_move_to(cr, x, 3); cairo_line_to(cr, x + (u ? 5 : -5), 6.5); cairo_line_to(cr, x, 9);
        cairo_close_path(cr);
        cairo_fill(cr);
    } else if (!strcmp(n, "new")) {
        page(cr, 3, 1.5, 10, 13);
    } else if (!strcmp(n, "open")) {
        cairo_move_to(cr, 1.5, 4.5); cairo_line_to(cr, 6, 4.5); cairo_line_to(cr, 7, 5.5); cairo_line_to(cr, 13.5, 5.5);
        cairo_line_to(cr, 13.5, 13.5); cairo_line_to(cr, 1.5, 13.5); cairo_close_path(cr);
        rgb(cr, 0xF0C860); cairo_fill_preserve(cr); rgb(cr, 0x8A6A10); cairo_stroke(cr);
    } else if (!strcmp(n, "saveas")) {
        box(cr, 1.5, 1.5, 11, 11, 0x4A7CC0, 0x1A3A6A);
        box(cr, 3.5, 1.5, 7, 4, 0xFFFFFF, 0x1A3A6A);
        pencil_shape(cr, 0.55);
    } else if (!strcmp(n, "print")) {
        box(cr, 4, 1.5, 8, 5, 0xFFFFFF, 0x50607A);
        box(cr, 1.5, 6.5, 13, 6, 0xB0BCCC, 0x50607A);
        box(cr, 4, 10.5, 8, 4, 0xFFFFFF, 0x50607A);
    } else if (!strcmp(n, "scanner")) {
        box(cr, 1.5, 7.5, 13, 5, 0xB0BCCC, 0x50607A);
        cairo_move_to(cr, 2, 7); cairo_line_to(cr, 13, 3); rgb(cr, 0x50607A); cairo_stroke(cr);
    } else if (!strcmp(n, "email")) {
        box(cr, 1.5, 3.5, 13, 9, 0xFFFFFF, 0x50607A);
        cairo_move_to(cr, 2, 4); cairo_line_to(cr, 8, 9); cairo_line_to(cr, 14, 4); cairo_stroke(cr);
    } else if (!strcmp(n, "wallpaper")) {
        box(cr, 1.5, 2.5, 13, 9, 0x6FA8E8, 0x50607A);
        cairo_move_to(cr, 2, 11); cairo_line_to(cr, 6, 6); cairo_line_to(cr, 9, 9); cairo_line_to(cr, 11, 7);
        cairo_line_to(cr, 14, 11); cairo_close_path(cr); rgb(cr, 0x22B14C); cairo_fill(cr);
        box(cr, 6, 12, 4, 2, 0xB0BCCC, 0x50607A);
    } else if (!strcmp(n, "properties")) {
        page(cr, 2, 1.5, 11, 13);
        rgb(cr, 0x2D6FC4);
        for (int i = 0; i < 4; i++) { cairo_rectangle(cr, 4, 4 + i * 2.5, 7, 1); cairo_fill(cr); }
    } else if (!strcmp(n, "about")) {
        cairo_arc(cr, 8, 8, 6.5, 0, 2 * M_PI);
        rgb(cr, 0x2D6FC4); cairo_fill(cr);
        cairo_select_font_face(cr, "Serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 10);
        rgb(cr, 0xFFFFFF);
        cairo_move_to(cr, 6.3, 12);
        cairo_show_text(cr, "i");
    } else if (!strcmp(n, "exit")) {
        box(cr, 1.5, 1.5, 9, 13, 0xE8ECF2, 0x50607A);
        cairo_set_line_width(cr, 1.6);
        rgb(cr, 0xC0392B);
        cairo_move_to(cr, 7, 8); cairo_line_to(cr, 15, 8); cairo_stroke(cr);
        cairo_move_to(cr, 12, 5); cairo_line_to(cr, 15, 8); cairo_line_to(cr, 12, 11); cairo_stroke(cr);
    } else if (!strcmp(n, "rulers")) {
        box(cr, 1.5, 1.5, 13, 3, 0xF5E6B0, 0x7A5A2A);
        box(cr, 1.5, 1.5, 3, 13, 0xF5E6B0, 0x7A5A2A);
    } else if (!strcmp(n, "grid")) {
        rgb(cr, 0x6A7B95);
        for (int i = 0; i < 5; i++) {
            cairo_move_to(cr, 1.5 + i * 3.25, 1.5); cairo_line_to(cr, 1.5 + i * 3.25, 14.5);
            cairo_move_to(cr, 1.5, 1.5 + i * 3.25); cairo_line_to(cr, 14.5, 1.5 + i * 3.25);
        }
        cairo_stroke(cr);
    } else if (!strcmp(n, "statusbar")) {
        box(cr, 1.5, 1.5, 13, 13, 0xFFFFFF, 0x6A7B95);
        box(cr, 1.5, 11.5, 13, 3, 0xC8D4E6, 0x6A7B95);
    } else if (!strcmp(n, "fullscreen")) {
        box(cr, 1.5, 2.5, 13, 11, 0x202020, 0x50607A);
        box(cr, 4, 5, 8, 6, 0x6FA8E8, 0x6FA8E8);
    } else if (!strcmp(n, "thumbnail")) {
        box(cr, 1.5, 1.5, 13, 13, 0xFFFFFF, 0x6A7B95);
        box(cr, 7.5, 7.5, 6, 6, 0x6FA8E8, 0x2050A0);
    } else if (!strcmp(n, "opaque")) {
        box(cr, 1.5, 2.5, 13, 11, 0xFFFFFF, 0x6A7B95);
        cairo_select_font_face(cr, "Serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 10); rgb(cr, 0x2050A0);
        cairo_move_to(cr, 4.5, 11.5); cairo_show_text(cr, "A");
    } else if (!strcmp(n, "transp")) {
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++) {
                cairo_rectangle(cr, 2 + x * 3, 3 + y * 2.5, 3, 2.5);
                rgb(cr, (x + y) & 1 ? 0xD0D0D0 : 0xFFFFFF);
                cairo_fill(cr);
            }
        cairo_select_font_face(cr, "Serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 10); rgb(cr, 0x2050A0);
        cairo_move_to(cr, 4.5, 11.5); cairo_show_text(cr, "A");
    } else if (!strcmp(n, "file")) {
        page(cr, 2, 1.5, 9, 12);
        rgb(cr, 0x2D6FC4);
        cairo_rectangle(cr, 9, 8, 6, 6); cairo_fill(cr);
    } else {
        box(cr, 2, 2, 12, 12, 0xEEEEEE, 0x999999);
    }
    cairo_restore(cr);
}

GtkWidget *icon_widget(const char *name, int size)
{
    int scale = 1;
    GdkScreen *scr = gdk_screen_get_default();
    if (scr) {
        GdkMonitor *m = gdk_display_get_primary_monitor(gdk_screen_get_display(scr));
        if (m) scale = gdk_monitor_get_scale_factor(m);
    }
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size * scale, size * scale);
    cairo_surface_set_device_scale(s, scale, scale);
    cairo_t *cr = cairo_create(s);
    draw_icon(cr, name, size);
    cairo_destroy(cr);
    GtkWidget *img = gtk_image_new_from_surface(s);
    cairo_surface_destroy(s);
    return img;
}
