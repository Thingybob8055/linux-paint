/* Main window: ribbon (CPBFrame + MSPAINT_RIBBON), status bar (CAppletStatusBar
 * + CAppletZoomControl), commands (CPBView::On*), clipboard, thumbnail and
 * full screen views, settings (HKCU\...\Applets\Paint\View equivalents). */
#include "paint.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>

App app;

/* colorColorsDef as shown by CColorGallerySite::GetDynamicItemInfo */
static const uint32_t default_palette[20] = {
    0x000000, 0x7F7F7F, 0x880015, 0xED1C24, 0xFF7F27, 0xFFF200, 0x22B14C, 0x00A2E8, 0x3F48CC, 0xA349A4,
    0xFFFFFF, 0xC3C3C3, 0xB97A57, 0xFFAEC9, 0xFFC90E, 0xEFE4B0, 0xB5E61D, 0x99D9EA, 0x7092BE, 0xC8BFE7,
};

static RGB hex_rgb(uint32_t v) { return (RGB){ ((v >> 16) & 255) / 255.0, ((v >> 8) & 255) / 255.0, (v & 255) / 255.0 }; }

static GtkWidget *select_btn, *undo_btn, *redo_btn, *paste_btn, *cut_btn, *copy_btn, *crop_btn;
static GtkWidget *home_page, *view_page;
static GList *color_boxes;     /* drawing areas that show colour 1/2 (both tabs) */
static GList *palette_areas;   /* drawing areas of palette cells */
static GList *size_widgets, *brush_images, *shape_buttons;
static GtkWidget *brush_big_img_holder;
static GtkWidget *fullscreen_win;
static int default_w = 0, default_h = 0;

/* ------------------------------------------------------------------ */
/* small UI helpers                                                    */

static GtkWidget *big_button(const char *icon, const char *label, GCallback cb, gpointer data)
{
    GtkWidget *b = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(v), icon_widget(icon, 32), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(v), gtk_label_new_with_mnemonic(label), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(b), v);
    if (cb) g_signal_connect_swapped(b, "clicked", cb, data);
    gtk_widget_set_valign(b, GTK_ALIGN_START);
    return b;
}

static GtkWidget *small_button(const char *icon, const char *label, GCallback cb, gpointer data)
{
    GtkWidget *b = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(h), icon_widget(icon, 16), FALSE, FALSE, 0);
    if (label) gtk_box_pack_start(GTK_BOX(h), gtk_label_new_with_mnemonic(label), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(b), h);
    if (cb) g_signal_connect_swapped(b, "clicked", cb, data);
    return b;
}

static GtkWidget *group(const char *title, GtkWidget *content)
{
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(v), content, TRUE, TRUE, 0);
    GtkWidget *l = gtk_label_new(title);
    gtk_style_context_add_class(gtk_widget_get_style_context(l), "group-label");
    gtk_box_pack_end(GTK_BOX(v), l, FALSE, FALSE, 0);
    gtk_widget_set_margin_start(v, 4);
    gtk_widget_set_margin_end(v, 4);
    return v;
}

static void add_group(GtkWidget *bar, GtkWidget *g)
{
    gtk_box_pack_start(GTK_BOX(bar), g, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
}

static GtkWidget *menu_item(GtkWidget *menu, const char *icon, const char *label, GCallback cb, gpointer data)
{
    GtkWidget *it = gtk_menu_item_new();
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    if (icon) gtk_box_pack_start(GTK_BOX(h), icon_widget(icon, 16), FALSE, FALSE, 0);
    else gtk_box_pack_start(GTK_BOX(h), gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), FALSE, FALSE, 8);
    GtkWidget *l = gtk_label_new_with_mnemonic(label);
    gtk_box_pack_start(GTK_BOX(h), l, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(it), h);
    if (cb) g_signal_connect_swapped(it, "activate", cb, data);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), it);
    return it;
}

static GtkWidget *submenu(GtkWidget *menu, const char *icon, const char *label)
{
    GtkWidget *it = menu_item(menu, icon, label, NULL, NULL);
    GtkWidget *m = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(it), m);
    return m;
}

static void menu_sep(GtkWidget *menu)
{
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
}

static GtkWidget *menu_button(GtkWidget *child, GtkWidget *menu)
{
    GtkWidget *b = gtk_menu_button_new();
    gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
    gtk_container_remove(GTK_CONTAINER(b), gtk_bin_get_child(GTK_BIN(b)));
    gtk_container_add(GTK_CONTAINER(b), child);
    gtk_widget_show_all(menu);
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(b), menu);
    return b;
}

static GtkWidget *big_menu_button(const char *icon, const char *label, GtkWidget *menu)
{
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(v), icon_widget(icon, 32), FALSE, FALSE, 0);
    gchar *t = g_strdup_printf("%s ▾", label);
    gtk_box_pack_start(GTK_BOX(v), gtk_label_new_with_mnemonic(t), FALSE, FALSE, 0);
    g_free(t);
    GtkWidget *b = menu_button(v, menu);
    gtk_widget_set_valign(b, GTK_ALIGN_START);
    return b;
}

static GtkWidget *small_menu_button(const char *icon, const char *label, GtkWidget *menu)
{
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(h), icon_widget(icon, 16), FALSE, FALSE, 0);
    gchar *t = g_strdup_printf("%s ▾", label);
    gtk_box_pack_start(GTK_BOX(h), gtk_label_new_with_mnemonic(t), FALSE, FALSE, 0);
    g_free(t);
    return menu_button(h, menu);
}

/* ------------------------------------------------------------------ */
/* title / status                                                      */

void update_title(void)
{
    if (!app.window) return;
    gchar *base = g_path_get_basename(app.filename ? app.filename : "Untitled");
    gchar *t = g_strdup_printf("%s - %s", base, APP_NAME);
    gtk_window_set_title(GTK_WINDOW(app.window), t);
    g_free(t);
    g_free(base);
}

void mark_dirty(void)
{
    if (!app.dirty) {
        app.dirty = true;
        update_title();
    }
}

void update_status(void)
{
    if (!app.st_pos) return;
    char b[96];
    if (app.mouse_x >= 0) g_snprintf(b, sizeof b, "%d, %dpx", app.mouse_x, app.mouse_y);
    else b[0] = 0;
    gtk_label_set_text(GTK_LABEL(app.st_pos), b);
    if (app.sel.active) g_snprintf(b, sizeof b, "%d × %dpx", app.sel.w, app.sel.h);
    else b[0] = 0;
    gtk_label_set_text(GTK_LABEL(app.st_sel), b);
    g_snprintf(b, sizeof b, "%d × %dpx", app.w, app.h);
    gtk_label_set_text(GTK_LABEL(app.st_size), b);
    b[0] = 0;
    if (app.filename) {
        GStatBuf st;
        if (g_stat(app.filename, &st) == 0) {
            gchar *s = g_format_size(st.st_size);
            g_snprintf(b, sizeof b, "Size: %s", s);
            g_free(s);
        }
    }
    gtk_label_set_text(GTK_LABEL(app.st_file), b);
    g_snprintf(b, sizeof b, "%g%%", round(app.zoom * 1000) / 10);
    gtk_label_set_text(GTK_LABEL(app.zoom_label), b);
    app.updating_ui = true;
    gtk_range_set_value(GTK_RANGE(app.zoom_scale), log2(app.zoom));
    app.updating_ui = false;
}

/* ------------------------------------------------------------------ */
/* colours                                                             */

void update_color_ui(void)
{
    for (GList *l = color_boxes; l; l = l->next) gtk_widget_queue_draw(l->data);
    for (GList *l = palette_areas; l; l = l->next) gtk_widget_queue_draw(l->data);
}

void set_color(int which, RGB c)
{
    if (which == 2) app.color2 = c;
    else app.color1 = c;
    update_color_ui();
    shape_refresh();
    text_apply_attrs();
    canvas_redraw();
}

/* CColors::AddNewColor: custom colours fill the third palette row, oldest replaced */
void add_custom_color(RGB c)
{
    for (int i = 0; i < app.n_custom; i++)
        if (fabs(app.custom[i].r - c.r) < 1e-3 && fabs(app.custom[i].g - c.g) < 1e-3 && fabs(app.custom[i].b - c.b) < 1e-3)
            return;
    if (app.n_custom < N_CUSTOM_COLORS) app.custom[app.n_custom++] = c;
    else {
        memmove(app.custom, app.custom + 1, sizeof(RGB) * (N_CUSTOM_COLORS - 1));
        app.custom[N_CUSTOM_COLORS - 1] = c;
    }
    update_color_ui();
}

static gboolean colorbox_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
    int which = GPOINTER_TO_INT(d);
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    RGB c = which == 2 ? app.color2 : app.color1;
    if (app.active_color == which) {
        cairo_set_source_rgb(cr, 0.98, 0.85, 0.5);
        cairo_rectangle(cr, 0, 0, W, H);
        cairo_fill(cr);
    }
    double s = MIN(W, H) - 8;
    cairo_rectangle(cr, (W - s) / 2 + 0.5, 3.5, s, s);
    cairo_set_source_rgb(cr, c.r, c.g, c.b);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.4, 0.45, 0.5);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    return TRUE;
}

static gboolean colorbox_click(GtkWidget *w, GdkEventButton *ev, gpointer d)
{
    (void)w; (void)ev;
    app.active_color = GPOINTER_TO_INT(d);
    update_color_ui();
    return TRUE;
}

static GtkWidget *color_box(int which, const char *label)
{
    GtkWidget *eb = gtk_event_box_new();
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *da = gtk_drawing_area_new();
    gtk_widget_set_size_request(da, 44, 38);
    g_signal_connect(da, "draw", G_CALLBACK(colorbox_draw), GINT_TO_POINTER(which));
    color_boxes = g_list_prepend(color_boxes, da);
    gtk_box_pack_start(GTK_BOX(v), da, FALSE, FALSE, 0);
    GtkWidget *l = gtk_label_new(label);
    gtk_label_set_justify(GTK_LABEL(l), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(v), l, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(eb), v);
    g_signal_connect(eb, "button-press-event", G_CALLBACK(colorbox_click), GINT_TO_POINTER(which));
    gtk_widget_set_tooltip_text(eb, which == 1 ? "Color 1 (foreground color)\nClick here and then select a color from the palette. This color is used with the pencil and with brushes, as well as for shape outlines."
                                               : "Color 2 (background color)\nClick here and then select a color from the palette. This color is used with the eraser and for shape fills.");
    return eb;
}

static bool palette_get(int i, RGB *c)
{
    if (i < 20) { *c = hex_rgb(default_palette[i]); return true; }
    if (i - 20 < app.n_custom) { *c = app.custom[i - 20]; return true; }
    return false;
}

static gboolean palette_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
    int i = GPOINTER_TO_INT(d);
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    RGB c;
    cairo_rectangle(cr, 1.5, 1.5, W - 3, H - 3);
    if (palette_get(i, &c)) {
        cairo_set_source_rgb(cr, c.r, c.g, c.b);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0.55, 0.6, 0.65);
    } else {
        cairo_set_source_rgb(cr, 0.98, 0.98, 0.98);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0.8, 0.82, 0.85);
    }
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    return TRUE;
}

static gboolean palette_click(GtkWidget *w, GdkEventButton *ev, gpointer d)
{
    (void)w;
    RGB c;
    if (!palette_get(GPOINTER_TO_INT(d), &c)) return TRUE;
    set_color(ev->button == 3 ? 2 : app.active_color, c);
    return TRUE;
}

static GtkWidget *colors_group(void)
{
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(h), color_box(1, "Color\n1"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(h), color_box(2, "Color\n2"), FALSE, FALSE, 0);
    GtkWidget *g = gtk_grid_new();
    gtk_widget_set_valign(g, GTK_ALIGN_CENTER);
    for (int i = 0; i < 30; i++) {
        GtkWidget *eb = gtk_event_box_new();
        GtkWidget *da = gtk_drawing_area_new();
        gtk_widget_set_size_request(da, 19, 19);
        g_signal_connect(da, "draw", G_CALLBACK(palette_draw), GINT_TO_POINTER(i));
        gtk_container_add(GTK_CONTAINER(eb), da);
        g_signal_connect(eb, "button-press-event", G_CALLBACK(palette_click), GINT_TO_POINTER(i));
        palette_areas = g_list_prepend(palette_areas, da);
        gtk_grid_attach(GTK_GRID(g), eb, i % 10, i / 10, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(h), g, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(h), big_button("edit-colors", "Edit\ncolors", G_CALLBACK(cmd_edit_colors), NULL),
                       FALSE, FALSE, 0);
    return group("Colors", h);
}

/* ------------------------------------------------------------------ */
/* commands                                                            */

static int new_default_w(void) { return default_w; }
static int new_default_h(void) { return default_h; }

void cmd_new(void)
{
    if (!confirm_discard()) return;
    sel_clear(false);
    undo_clear();
    g_free(app.filename);
    app.filename = NULL;
    app.dpi_x = app.dpi_y = 96;
    app.monochrome = false;
    set_image(surface_new(new_default_w(), new_default_h(), (RGB){1, 1, 1}));
    app.dirty = false;
    update_title();
    update_ui();
}

static void add_filters(GtkFileChooser *fc, bool open)
{
    GtkFileFilter *all = gtk_file_filter_new();
    if (open) {
        gtk_file_filter_set_name(all, "All Picture Files");
        gtk_file_filter_add_pixbuf_formats(all);
        gtk_file_chooser_add_filter(fc, all);
    }
    struct { const char *name, *pat[4]; } f[] = {
        { "Bitmap Files (*.bmp;*.dib)", { "*.bmp", "*.dib", NULL } },
        { "JPEG (*.jpg;*.jpeg;*.jpe;*.jfif)", { "*.jpg", "*.jpeg", "*.jpe", "*.jfif" } },
        { "GIF (*.gif)", { "*.gif", NULL } },
        { "TIFF (*.tif;*.tiff)", { "*.tif", "*.tiff", NULL } },
        { "PNG (*.png)", { "*.png", NULL } },
        { "ICO (*.ico)", { "*.ico", NULL } },
    };
    for (unsigned i = 0; i < G_N_ELEMENTS(f); i++) {
        GtkFileFilter *ff = gtk_file_filter_new();
        gtk_file_filter_set_name(ff, f[i].name);
        for (int j = 0; j < 4 && f[i].pat[j]; j++) {
            gtk_file_filter_add_pattern(ff, f[i].pat[j]);
            gchar *up = g_ascii_strup(f[i].pat[j], -1);
            gtk_file_filter_add_pattern(ff, up);
            g_free(up);
        }
        gtk_file_chooser_add_filter(fc, ff);
    }
    if (open) {
        GtkFileFilter *any = gtk_file_filter_new();
        gtk_file_filter_set_name(any, "All Files (*.*)");
        gtk_file_filter_add_pattern(any, "*");
        gtk_file_chooser_add_filter(fc, any);
        gtk_file_chooser_set_filter(fc, all);
    }
}

static void open_path(const char *path)
{
    GError *err = NULL;
    if (!file_open(path, &err)) {
        gchar *b = g_path_get_basename(path);
        error_dialog("Paint cannot read this file.\n%s\n%s", b, err ? err->message : "");
        g_free(b);
        g_clear_error(&err);
    }
}

void cmd_open(void)
{
    if (!confirm_discard()) return;
    GtkWidget *d = gtk_file_chooser_dialog_new("Open", GTK_WINDOW(app.window), GTK_FILE_CHOOSER_ACTION_OPEN,
                                               "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
    add_filters(GTK_FILE_CHOOSER(d), true);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        gchar *p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
        gtk_widget_destroy(d);
        open_path(p);
        g_free(p);
        return;
    }
    gtk_widget_destroy(d);
}

static bool save_to(const char *path, const char *fmt)
{
    GError *err = NULL;
    if (!file_save(path, fmt, &err)) {
        error_dialog("Paint cannot save this file.\n%s", err ? err->message : "");
        g_clear_error(&err);
        return false;
    }
    return true;
}

static const struct { const char *label, *fmt, *ext; } save_types[] = {
    { "PNG (*.png)", "png", "png" },
    { "JPEG (*.jpg;*.jpeg;*.jpe;*.jfif)", "jpeg", "jpg" },
    { "24-bit Bitmap (*.bmp;*.dib)", "bmp24", "bmp" },
    { "256 Color Bitmap (*.bmp;*.dib)", "bmp8", "bmp" },
    { "16 Color Bitmap (*.bmp;*.dib)", "bmp4", "bmp" },
    { "Monochrome Bitmap (*.bmp;*.dib)", "bmp1", "bmp" },
    { "GIF (*.gif)", "gif", "gif" },
    { "TIFF (*.tif;*.tiff)", "tiff", "tif" },
};

/* CPBDoc::OnSaveAs{Png,Jpeg,Bmp,Gif,Other} */
bool cmd_save_as(const char *fmt)
{
    tool_commit_pending();
    GtkWidget *d = gtk_file_chooser_dialog_new("Save As", GTK_WINDOW(app.window), GTK_FILE_CHOOSER_ACTION_SAVE,
                                               "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d), TRUE);
    GtkWidget *combo = gtk_combo_box_text_new();
    int sel = 0;
    const char *want = fmt ? fmt : (app.filename ? format_from_path(app.filename) : "png");
    for (unsigned i = 0; i < G_N_ELEMENTS(save_types); i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), save_types[i].label);
        if (!strcmp(save_types[i].fmt, want)) sel = i;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), sel);
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(h), gtk_label_new("Save as type:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(h), combo, FALSE, FALSE, 0);
    gtk_widget_show_all(h);
    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(d), h);
    gchar *base;
    if (app.filename) {
        base = g_path_get_basename(app.filename);
        gchar *dir = g_path_get_dirname(app.filename);
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(d), dir);
        g_free(dir);
        char *dot = strrchr(base, '.');
        if (dot) *dot = 0;
    } else {
        base = g_strdup("Untitled");
        const char *pics = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
        if (pics) gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(d), pics);
    }
    gchar *nm = g_strdup_printf("%s.%s", base, save_types[sel].ext);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(d), nm);
    g_free(nm);
    g_free(base);
    bool ok = false;
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        gchar *p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
        int t = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
        /* add the extension if the user typed none */
        gchar *b = g_path_get_basename(p);
        if (!strchr(b, '.')) {
            gchar *np = g_strdup_printf("%s.%s", p, save_types[t].ext);
            g_free(p);
            p = np;
        }
        g_free(b);
        const char *f = save_types[t].fmt;
        if (!strcmp(format_from_path(p), "jpeg") && strcmp(f, "jpeg")) f = "jpeg";
        else if (!strcmp(format_from_path(p), "png") && strcmp(f, "png") && strchr(p, '.') &&
                 !g_str_has_suffix(p, save_types[t].ext)) f = "png";
        gtk_widget_destroy(d);
        ok = save_to(p, f);
        g_free(p);
        update_ui();
        return ok;
    }
    gtk_widget_destroy(d);
    return ok;
}

bool cmd_save(void)
{
    tool_commit_pending();
    if (!app.filename) return cmd_save_as(NULL);
    return save_to(app.filename, format_from_path(app.filename));
}

/* clipboard */
static GdkPixbuf *surface_to_pixbuf(cairo_surface_t *s)
{
    return gdk_pixbuf_get_from_surface(s, 0, 0, cairo_image_surface_get_width(s), cairo_image_surface_get_height(s));
}

void cmd_copy(void)
{
    if (!app.sel.active) return;
    cairo_surface_t *s = sel_get_pixels();
    GdkPixbuf *pb = surface_to_pixbuf(s);
    gtk_clipboard_set_image(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), pb);
    gtk_clipboard_store(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
    g_object_unref(pb);
    cairo_surface_destroy(s);
    update_ui();
}

void cmd_cut(void)
{
    if (!app.sel.active) return;
    cmd_copy();
    sel_delete();
}

static void paste_surface(cairo_surface_t *s)
{
    /* CImgWnd::PasteImageClip pastes at the top-left of the visible area */
    GtkAdjustment *ha = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(app.scroll));
    GtkAdjustment *va = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(app.scroll));
    double ix, iy;
    canvas_widget_to_image(gtk_adjustment_get_value(ha), gtk_adjustment_get_value(va), &ix, &iy);
    int x = MAX(0, (int)ceil(ix)), y = MAX(0, (int)ceil(iy));
    sel_from_surface(s, x, y);
}

void cmd_paste(void)
{
    if (text_active()) return;
    GdkPixbuf *pb = gtk_clipboard_wait_for_image(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
    if (!pb) {
        /* a file copied in a file manager */
        gchar **uris = gtk_clipboard_wait_for_uris(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
        if (uris && uris[0]) {
            gchar *p = g_filename_from_uri(uris[0], NULL, NULL);
            if (p) pb = gdk_pixbuf_new_from_file(p, NULL);
            g_free(p);
        }
        g_strfreev(uris);
    }
    if (!pb) return;
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    cairo_surface_t *s = surface_new(w, h, (RGB){1, 1, 1});
    cairo_t *cr = cairo_create(s);
    gdk_cairo_set_source_pixbuf(cr, pb, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    g_object_unref(pb);
    paste_surface(s);
    cairo_surface_destroy(s);
}

void cmd_paste_from(void)
{
    GtkWidget *d = gtk_file_chooser_dialog_new("Paste From", GTK_WINDOW(app.window), GTK_FILE_CHOOSER_ACTION_OPEN,
                                               "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
    add_filters(GTK_FILE_CHOOSER(d), true);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        gchar *p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
        gtk_widget_destroy(d);
        GError *err = NULL;
        cairo_surface_t *s = load_image_surface(p, &err);
        if (s) {
            paste_surface(s);
            cairo_surface_destroy(s);
        } else {
            error_dialog("Paint cannot read this file.\n%s", err ? err->message : "");
            g_clear_error(&err);
        }
        g_free(p);
        return;
    }
    gtk_widget_destroy(d);
}

/* Send in e-mail (MAPI in the original; xdg-email here) */
void cmd_email(void)
{
    if (!app.filename || app.dirty) {
        if (!cmd_save()) return;
    }
    gchar *argv[] = { "xdg-email", "--attach", app.filename, NULL };
    GError *err = NULL;
    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err)) {
        error_dialog("Could not start the e-mail program: %s", err->message);
        g_clear_error(&err);
    }
}

/* From scanner or camera (CWIAMgr) via SANE's scanimage */
void cmd_scan(void)
{
    if (!confirm_discard()) return;
    gchar *tmp = g_build_filename(g_get_tmp_dir(), "linpaint-scan.png", NULL);
    gchar *cmd = g_strdup_printf("scanimage --format=png -o '%s'", tmp);
    gchar *err_out = NULL;
    int status = 0;
    GError *err = NULL;
    GdkWindow *gw = gtk_widget_get_window(app.window);
    GdkCursor *wait = gdk_cursor_new_from_name(gdk_window_get_display(gw), "wait");
    gdk_window_set_cursor(gw, wait);
    while (gtk_events_pending()) gtk_main_iteration();
    bool ok = g_spawn_command_line_sync(cmd, NULL, &err_out, &status, &err) && status == 0;
    gdk_window_set_cursor(gw, NULL);
    g_object_unref(wait);
    if (ok) {
        open_path(tmp);
        g_free(app.filename);
        app.filename = NULL;
        app.dirty = true;
        update_title();
    } else {
        error_dialog("Paint could not acquire an image.\nNo scanner or camera was found (SANE 'scanimage' is required).\n%s",
                     err ? err->message : (err_out ? err_out : ""));
    }
    g_clear_error(&err);
    g_free(err_out);
    g_free(cmd);
    g_free(tmp);
}

static void cmd_exit(void)
{
    if (confirm_discard()) gtk_widget_destroy(app.window);
}

/* ------------------------------------------------------------------ */
/* full screen (CFullScreenThumbNailView) and thumbnail (CThumbNailView) */

static gboolean fs_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
    (void)d;
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_paint(cr);
    double s = MIN(1.0, MIN((double)W / app.w, (double)H / app.h));
    cairo_translate(cr, floor((W - app.w * s) / 2), floor((H - app.h * s) / 2));
    cairo_scale(cr, s, s);
    cairo_set_source_surface(cr, app.img, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), s < 1 ? CAIRO_FILTER_GOOD : CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
    return TRUE;
}

static gboolean fs_close(GtkWidget *w, GdkEvent *e, gpointer d)
{
    (void)e; (void)d;
    gtk_widget_destroy(w);
    fullscreen_win = NULL;
    return TRUE;
}

void cmd_fullscreen(void)
{
    tool_commit_pending();
    fullscreen_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_transient_for(GTK_WINDOW(fullscreen_win), GTK_WINDOW(app.window));
    GtkWidget *da = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(fullscreen_win), da);
    g_signal_connect(da, "draw", G_CALLBACK(fs_draw), NULL);
    gtk_widget_add_events(fullscreen_win, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
    g_signal_connect(fullscreen_win, "button-press-event", G_CALLBACK(fs_close), NULL);
    g_signal_connect(fullscreen_win, "key-press-event", G_CALLBACK(fs_close), NULL);
    gtk_widget_show_all(fullscreen_win);
    gtk_window_fullscreen(GTK_WINDOW(fullscreen_win));
}

static gboolean thumb_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
    (void)d;
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    cairo_set_source_rgb(cr, 0.788, 0.827, 0.886);
    cairo_paint(cr);
    double s = MIN((double)W / app.w, (double)H / app.h);
    double ox = (W - app.w * s) / 2, oy = (H - app.h * s) / 2;
    cairo_save(cr);
    cairo_translate(cr, ox, oy);
    cairo_scale(cr, s, s);
    cairo_set_source_surface(cr, app.img, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    sel_draw(cr);
    cairo_restore(cr);
    /* visible part of the canvas */
    GtkAdjustment *ha = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(app.scroll));
    GtkAdjustment *va = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(app.scroll));
    double x0, y0;
    canvas_widget_to_image(gtk_adjustment_get_value(ha), gtk_adjustment_get_value(va), &x0, &y0);
    double vw = gtk_adjustment_get_page_size(ha) / app.zoom, vh = gtk_adjustment_get_page_size(va) / app.zoom;
    if (vw < app.w || vh < app.h) {
        cairo_rectangle(cr, ox + MAX(0, x0) * s + 0.5, oy + MAX(0, y0) * s + 0.5, MIN(vw, app.w) * s, MIN(vh, app.h) * s);
        cairo_set_source_rgb(cr, 0.9, 0.1, 0.1);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
    }
    return TRUE;
}

static gboolean thumb_click(GtkWidget *w, GdkEventButton *ev, gpointer d)
{
    (void)d;
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double s = MIN((double)W / app.w, (double)H / app.h);
    double ix = (ev->x - (W - app.w * s) / 2) / s, iy = (ev->y - (H - app.h * s) / 2) / s;
    GtkAdjustment *ha = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(app.scroll));
    GtkAdjustment *va = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(app.scroll));
    double wx, wy;
    canvas_image_to_widget(ix, iy, &wx, &wy);
    gtk_adjustment_set_value(ha, wx - gtk_adjustment_get_page_size(ha) / 2);
    gtk_adjustment_set_value(va, wy - gtk_adjustment_get_page_size(va) / 2);
    return TRUE;
}

static gboolean thumb_closed(GtkWidget *w, GdkEvent *e, gpointer d)
{
    (void)w; (void)e; (void)d;
    app.thumb_win = NULL;
    app.thumb_area = NULL;
    update_ui();
    return FALSE;
}

void cmd_thumbnail(bool show)
{
    if (!show) {
        if (app.thumb_win) gtk_widget_destroy(app.thumb_win);
        app.thumb_win = app.thumb_area = NULL;
        return;
    }
    if (app.thumb_win) return;
    app.thumb_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.thumb_win), "Thumbnail");
    gtk_window_set_transient_for(GTK_WINDOW(app.thumb_win), GTK_WINDOW(app.window));
    gtk_window_set_type_hint(GTK_WINDOW(app.thumb_win), GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_window_set_default_size(GTK_WINDOW(app.thumb_win), 200, 150);
    app.thumb_area = gtk_drawing_area_new();
    gtk_widget_add_events(app.thumb_area, GDK_BUTTON_PRESS_MASK | GDK_BUTTON1_MOTION_MASK);
    g_signal_connect(app.thumb_area, "draw", G_CALLBACK(thumb_draw), NULL);
    g_signal_connect(app.thumb_area, "button-press-event", G_CALLBACK(thumb_click), NULL);
    g_signal_connect(app.thumb_area, "motion-notify-event", G_CALLBACK(thumb_click), NULL);
    gtk_container_add(GTK_CONTAINER(app.thumb_win), app.thumb_area);
    g_signal_connect(app.thumb_win, "delete-event", G_CALLBACK(thumb_closed), NULL);
    gtk_widget_show_all(app.thumb_win);
}

void thumb_refresh(void)
{
    if (app.thumb_area) gtk_widget_queue_draw(app.thumb_area);
}

/* ------------------------------------------------------------------ */
/* ribbon: Home tab                                                    */

static void tool_clicked(GtkToggleButton *b, gpointer d)
{
    if (app.updating_ui) return;
    if (!gtk_toggle_button_get_active(b)) { update_ui(); return; }
    tool_select(GPOINTER_TO_INT(d));
}

static GtkWidget *tool_toggle(ToolId t, const char *icon, const char *tip)
{
    GtkWidget *b = gtk_toggle_button_new();
    gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
    gtk_container_add(GTK_CONTAINER(b), icon_widget(icon, 16));
    gtk_widget_set_tooltip_text(b, tip);
    g_signal_connect(b, "toggled", G_CALLBACK(tool_clicked), GINT_TO_POINTER(t));
    app.tool_btn[t] = b;
    return b;
}

static void sel_tool_cmd(gpointer d)
{
    tool_select(GPOINTER_TO_INT(d));
}

static void transparent_toggled(GtkCheckMenuItem *m, gpointer d)
{
    (void)d;
    if (app.updating_ui) return;
    app.transparent_sel = gtk_check_menu_item_get_active(m);
    canvas_redraw();
}

static void rotate_cmd(gpointer d) { image_rotate(GPOINTER_TO_INT(d)); }
static void flip_cmd(gpointer d) { image_flip(GPOINTER_TO_INT(d)); }

/* brushes gallery */
static gboolean brush_img_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
    BrushKind k = GPOINTER_TO_INT(d);
    brush_preview_icon(cr, k, gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w));
    return TRUE;
}

static void brush_pick(GtkWidget *b, gpointer d)
{
    app.brush_kind = GPOINTER_TO_INT(d);
    GtkWidget *pop = gtk_widget_get_ancestor(b, GTK_TYPE_POPOVER);
    if (pop) gtk_widget_hide(pop);
    tool_select(TOOL_BRUSH);
    update_ui();
}

static GtkWidget *brush_gallery_button(void)
{
    GtkWidget *mb = gtk_menu_button_new();
    gtk_button_set_relief(GTK_BUTTON(mb), GTK_RELIEF_NONE);
    gtk_container_remove(GTK_CONTAINER(mb), gtk_bin_get_child(GTK_BIN(mb)));
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    brush_big_img_holder = gtk_drawing_area_new();
    gtk_widget_set_size_request(brush_big_img_holder, 40, 32);
    g_signal_connect(brush_big_img_holder, "draw", G_CALLBACK(brush_img_draw), GINT_TO_POINTER(-1));
    gtk_box_pack_start(GTK_BOX(v), icon_widget("brushes", 32), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(v), gtk_label_new("Brushes ▾"), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(mb), v);
    GtkWidget *pop = gtk_popover_new(mb);
    GtkWidget *g = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(g), 4);
    for (int i = 0; i < BRUSH_COUNT; i++) {
        GtkWidget *b = gtk_button_new();
        gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
        GtkWidget *da = gtk_drawing_area_new();
        gtk_widget_set_size_request(da, 64, 40);
        g_signal_connect(da, "draw", G_CALLBACK(brush_img_draw), GINT_TO_POINTER(i));
        gtk_container_add(GTK_CONTAINER(b), da);
        gtk_widget_set_tooltip_text(b, brush_name(i));
        g_signal_connect(b, "clicked", G_CALLBACK(brush_pick), GINT_TO_POINTER(i));
        gtk_grid_attach(GTK_GRID(g), b, i % 3, i / 3, 1, 1);
    }
    gtk_widget_show_all(g);
    gtk_container_add(GTK_CONTAINER(pop), g);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(mb), pop);
    gtk_widget_set_valign(mb, GTK_ALIGN_START);
    gtk_widget_set_tooltip_text(mb, "Brushes\nDraw with different types of brushes.");
    brush_images = g_list_prepend(brush_images, mb);
    return mb;
}

/* shapes gallery */
static gboolean shape_icon_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
    ShapeKind k = GPOINTER_TO_INT(d);
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double m = 3;
    cairo_set_line_width(cr, 1.2);
    cairo_set_source_rgb(cr, 0.12, 0.3, 0.6);
    if (k == SHAPE_LINE) {
        cairo_move_to(cr, m, H - m); cairo_line_to(cr, W - m, m);
    } else if (k == SHAPE_CURVE) {
        cairo_move_to(cr, m, H - m); cairo_curve_to(cr, W * 0.2, -H * 0.3, W * 0.8, H * 1.3, W - m, m);
    } else if (k == SHAPE_POLYGON) {
        cairo_move_to(cr, m, H * 0.4); cairo_line_to(cr, W * 0.45, m); cairo_line_to(cr, W - m, H * 0.35);
        cairo_line_to(cr, W * 0.7, H - m); cairo_line_to(cr, W * 0.2, H - m); cairo_close_path(cr);
    } else {
        shape_build_path(cr, k, m, m, W - m, H - m);
    }
    cairo_stroke(cr);
    return TRUE;
}

static void shape_clicked(GtkToggleButton *b, gpointer d)
{
    if (app.updating_ui) return;
    if (!gtk_toggle_button_get_active(b)) { update_ui(); return; }
    tool_commit_pending();
    app.shape_kind = GPOINTER_TO_INT(d);
    if (app.tool != TOOL_SHAPE) tool_select(TOOL_SHAPE);
    update_ui();
}

static void media_set(gpointer d)
{
    int v = GPOINTER_TO_INT(d);
    if (v >= 100) app.fill = v - 100;
    else app.outline = v;
    shape_refresh();
    update_ui();
}

static GtkWidget *media_menu(bool fill)
{
    GtkWidget *m = gtk_menu_new();
    for (int i = 0; i < MEDIA_COUNT; i++) {
        const char *n = media_name(i);
        if (fill && i == MEDIA_NONE) n = "No fill";
        menu_item(m, NULL, n, G_CALLBACK(media_set), GINT_TO_POINTER(fill ? 100 + i : i));
    }
    return m;
}

/* size gallery: rebuilt for the current tool each time it opens */
static gboolean size_line_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
    int px = GPOINTER_TO_INT(d);
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    double t = MIN(px, H - 2);
    cairo_rectangle(cr, 4, (H - t) / 2, W - 8, t);
    cairo_fill(cr);
    return TRUE;
}

static void size_pick(GtkWidget *b, gpointer d)
{
    GtkWidget *pop = gtk_widget_get_ancestor(b, GTK_TYPE_POPOVER);
    if (pop) gtk_widget_hide(pop);
    tool_set_size(GPOINTER_TO_INT(d));
}

static void size_popover_show(GtkWidget *pop, gpointer d)
{
    (void)d;
    GtkWidget *box = gtk_bin_get_child(GTK_BIN(pop));
    GList *ch = gtk_container_get_children(GTK_CONTAINER(box));
    for (GList *l = ch; l; l = l->next) gtk_widget_destroy(l->data);
    g_list_free(ch);
    int n;
    const int *list = tool_size_list(&n);
    for (int i = 0; i < n; i++) {
        GtkWidget *b = gtk_toggle_button_new();
        gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
        GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *da = gtk_drawing_area_new();
        gtk_widget_set_size_request(da, 110, MAX(14, list[i] + 4));
        g_signal_connect(da, "draw", G_CALLBACK(size_line_draw), GINT_TO_POINTER(list[i]));
        gtk_box_pack_start(GTK_BOX(h), da, FALSE, FALSE, 0);
        gchar *t = g_strdup_printf("%dpx", list[i]);
        gtk_box_pack_start(GTK_BOX(h), gtk_label_new(t), FALSE, FALSE, 0);
        g_free(t);
        gtk_container_add(GTK_CONTAINER(b), h);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b), list[i] == tool_cur_size());
        g_signal_connect(b, "clicked", G_CALLBACK(size_pick), GINT_TO_POINTER(list[i]));
        gtk_box_pack_start(GTK_BOX(box), b, FALSE, FALSE, 0);
    }
    gtk_widget_show_all(box);
}

static GtkWidget *size_button(void)
{
    GtkWidget *mb = gtk_menu_button_new();
    gtk_button_set_relief(GTK_BUTTON(mb), GTK_RELIEF_NONE);
    gtk_container_remove(GTK_CONTAINER(mb), gtk_bin_get_child(GTK_BIN(mb)));
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(v), icon_widget("size", 32), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(v), gtk_label_new("Size ▾"), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(mb), v);
    GtkWidget *pop = gtk_popover_new(mb);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(box), 4);
    gtk_container_add(GTK_CONTAINER(pop), box);
    g_signal_connect(pop, "show", G_CALLBACK(size_popover_show), NULL);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(mb), pop);
    gtk_widget_set_valign(mb, GTK_ALIGN_START);
    gtk_widget_set_tooltip_text(mb, "Size (Ctrl+Plus, Ctrl+Minus)\nSelect the width for the selected tool.");
    size_widgets = g_list_prepend(size_widgets, mb);
    return mb;
}

static GtkWidget *clipboard_group(void)
{
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget *pm = gtk_menu_new();
    menu_item(pm, "paste", "_Paste", G_CALLBACK(cmd_paste), NULL);
    menu_item(pm, "open", "Paste _from", G_CALLBACK(cmd_paste_from), NULL);
    GtkWidget *pb = big_menu_button("paste", "_Paste", pm);
    gtk_widget_set_tooltip_text(pb, "Paste (Ctrl+V)\nPaste the contents of the Clipboard.");
    paste_btn = pb;
    gtk_box_pack_start(GTK_BOX(h), pb, FALSE, FALSE, 0);
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    cut_btn = small_button("cut", "Cu_t", G_CALLBACK(cmd_cut), NULL);
    copy_btn = small_button("copy", "_Copy", G_CALLBACK(cmd_copy), NULL);
    gtk_widget_set_tooltip_text(cut_btn, "Cut (Ctrl+X)\nCut the selection from the canvas and put it on the Clipboard.");
    gtk_widget_set_tooltip_text(copy_btn, "Copy (Ctrl+C)\nCopy the selection from the canvas and put it on the Clipboard.");
    gtk_box_pack_start(GTK_BOX(v), cut_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(v), copy_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(h), v, FALSE, FALSE, 0);
    return group("Clipboard", h);
}

static void build_home(GtkWidget *bar)
{
    add_group(bar, clipboard_group());

    /* Image */
    GtkWidget *ih = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget *sm = gtk_menu_new();
    GtkWidget *hdr = gtk_menu_item_new_with_label("Selection shapes");
    gtk_widget_set_sensitive(hdr, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(sm), hdr);
    menu_item(sm, "select-rect", "_Rectangular selection", G_CALLBACK(sel_tool_cmd), GINT_TO_POINTER(TOOL_SELECT_RECT));
    menu_item(sm, "select-free", "_Free-form selection", G_CALLBACK(sel_tool_cmd), GINT_TO_POINTER(TOOL_SELECT_FREE));
    menu_sep(sm);
    menu_item(sm, "select-all", "Select _all", G_CALLBACK(sel_select_all), NULL);
    menu_item(sm, "invert-sel", "_Invert selection", G_CALLBACK(sel_invert), NULL);
    menu_item(sm, "delete", "_Delete", G_CALLBACK(sel_delete), NULL);
    app.transp_check = gtk_check_menu_item_new_with_mnemonic("_Transparent selection");
    g_signal_connect(app.transp_check, "toggled", G_CALLBACK(transparent_toggled), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(sm), app.transp_check);
    GtkWidget *selb = big_menu_button("select", "S_elect", sm);
    gtk_widget_set_tooltip_text(selb, "Select\nSelect a part of the picture.");
    select_btn = selb;
    gtk_box_pack_start(GTK_BOX(ih), selb, FALSE, FALSE, 0);
    GtkWidget *iv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    crop_btn = small_button("crop", "Cr_op", G_CALLBACK(image_crop), NULL);
    gtk_widget_set_tooltip_text(crop_btn, "Crop (Ctrl+Shift+X)\nCrop the picture so it only contains the current selection.");
    GtkWidget *rs = small_button("resize", "Re_size", G_CALLBACK(cmd_resize), NULL);
    gtk_widget_set_tooltip_text(rs, "Resize and skew (Ctrl+W)\nResize or skew the picture or selection.");
    GtkWidget *rm = gtk_menu_new();
    menu_item(rm, "rotate-r", "Rotate _right 90°", G_CALLBACK(rotate_cmd), GINT_TO_POINTER(1));
    menu_item(rm, "rotate-l", "Rotate _left 90°", G_CALLBACK(rotate_cmd), GINT_TO_POINTER(3));
    menu_item(rm, "rotate-180", "Rotate 1_80°", G_CALLBACK(rotate_cmd), GINT_TO_POINTER(2));
    menu_item(rm, "flip-v", "Flip _vertical", G_CALLBACK(flip_cmd), GINT_TO_POINTER(0));
    menu_item(rm, "flip-h", "Flip _horizontal", G_CALLBACK(flip_cmd), GINT_TO_POINTER(1));
    GtkWidget *rb = small_menu_button("rotate", "R_otate", rm);
    gtk_widget_set_tooltip_text(rb, "Rotate or flip\nRotate or flip the picture or selection.");
    gtk_box_pack_start(GTK_BOX(iv), crop_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(iv), rs, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(iv), rb, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ih), iv, FALSE, FALSE, 0);
    add_group(bar, group("Image", ih));

    /* Tools */
    GtkWidget *tg = gtk_grid_new();
    gtk_widget_set_valign(tg, GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(tg), tool_toggle(TOOL_PENCIL, "pencil", "Pencil\nDraw a free-form line with the selected line width."), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(tg), tool_toggle(TOOL_FILL, "fill", "Fill with color\nClick an area on the canvas to fill it with the foreground color, or right-click to fill it with the background color."), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(tg), tool_toggle(TOOL_TEXT, "text", "Text\nInsert text into the picture."), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(tg), tool_toggle(TOOL_ERASER, "eraser", "Eraser\nErase part of the picture and replace it with the background color."), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(tg), tool_toggle(TOOL_PICKER, "picker", "Color picker\nPick a color from the picture and use it for drawing."), 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(tg), tool_toggle(TOOL_ZOOM, "zoom", "Magnifier\nChange the magnification for a part of the picture."), 2, 1, 1, 1);
    add_group(bar, group("Tools", tg));

    /* Brushes */
    add_group(bar, group("", brush_gallery_button()));

    /* Shapes */
    GtkWidget *shh = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *sg = gtk_grid_new();
    GtkWidget *sgf = gtk_frame_new(NULL);
    gtk_container_add(GTK_CONTAINER(sgf), sg);
    gtk_widget_set_valign(sgf, GTK_ALIGN_CENTER);
    for (int i = 0; i < SHAPE_COUNT; i++) {
        GtkWidget *b = gtk_toggle_button_new();
        gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "shape-btn");
        GtkWidget *da = gtk_drawing_area_new();
        gtk_widget_set_size_request(da, 16, 16);
        g_signal_connect(da, "draw", G_CALLBACK(shape_icon_draw), GINT_TO_POINTER(i));
        gtk_container_add(GTK_CONTAINER(b), da);
        gtk_widget_set_tooltip_text(b, shape_name(i));
        g_signal_connect(b, "toggled", G_CALLBACK(shape_clicked), GINT_TO_POINTER(i));
        shape_buttons = g_list_append(shape_buttons, b);
        gtk_grid_attach(GTK_GRID(sg), b, i % 8, i / 8, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(shh), sgf, FALSE, FALSE, 0);
    GtkWidget *ov = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(ov, GTK_ALIGN_CENTER);
    app.outline_btn = small_menu_button("outline", "Outline", media_menu(false));
    app.fill_btn = small_menu_button("shapefill", "Fill", media_menu(true));
    gtk_widget_set_tooltip_text(app.outline_btn, "Shape outline\nSelect the outline material for the shape.");
    gtk_widget_set_tooltip_text(app.fill_btn, "Shape fill\nSelect the fill material for the shape.");
    gtk_box_pack_start(GTK_BOX(ov), app.outline_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ov), app.fill_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(shh), ov, FALSE, FALSE, 0);
    add_group(bar, group("Shapes", shh));

    /* Size */
    add_group(bar, group("", size_button()));

    /* Colors */
    add_group(bar, colors_group());
}

/* ------------------------------------------------------------------ */
/* ribbon: View tab                                                    */

static void view_toggle(GtkToggleButton *b, gpointer d)
{
    if (app.updating_ui) return;
    bool on = gtk_toggle_button_get_active(b);
    switch (GPOINTER_TO_INT(d)) {
    case 0: app.show_rulers = on; break;
    case 1: app.show_grid = on; break;
    case 2: app.show_status = on; break;
    case 3: cmd_thumbnail(on); break;
    }
    update_ui();
    canvas_redraw();
}

static void zoom_cmd(gpointer d)
{
    int v = GPOINTER_TO_INT(d);
    if (v == 0) canvas_set_zoom(1, -1, -1);
    else canvas_zoom_step(v);
}

static void build_view(GtkWidget *bar)
{
    GtkWidget *zh = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget *zi = big_button("zoom-in", "Zoom\n_in", G_CALLBACK(zoom_cmd), GINT_TO_POINTER(1));
    GtkWidget *zo = big_button("zoom-out", "Zoom\no_ut", G_CALLBACK(zoom_cmd), GINT_TO_POINTER(-1));
    GtkWidget *z1 = big_button("zoom-100", "100\n_%", G_CALLBACK(zoom_cmd), GINT_TO_POINTER(0));
    gtk_widget_set_tooltip_text(zi, "Zoom in (Ctrl+Page Up)");
    gtk_widget_set_tooltip_text(zo, "Zoom out (Ctrl+Page Down)");
    gtk_widget_set_tooltip_text(z1, "100%\nView the picture at 100% zoom.");
    gtk_box_pack_start(GTK_BOX(zh), zi, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(zh), zo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(zh), z1, FALSE, FALSE, 0);
    add_group(bar, group("Zoom", zh));

    GtkWidget *sv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(sv, GTK_ALIGN_CENTER);
    app.chk_rulers = gtk_check_button_new_with_mnemonic("_Rulers");
    app.chk_grid = gtk_check_button_new_with_mnemonic("_Gridlines");
    app.chk_status = gtk_check_button_new_with_mnemonic("_Status bar");
    gtk_widget_set_tooltip_text(app.chk_rulers, "Rulers (Ctrl+R)");
    gtk_widget_set_tooltip_text(app.chk_grid, "Gridlines (Ctrl+G)");
    g_signal_connect(app.chk_rulers, "toggled", G_CALLBACK(view_toggle), GINT_TO_POINTER(0));
    g_signal_connect(app.chk_grid, "toggled", G_CALLBACK(view_toggle), GINT_TO_POINTER(1));
    g_signal_connect(app.chk_status, "toggled", G_CALLBACK(view_toggle), GINT_TO_POINTER(2));
    gtk_box_pack_start(GTK_BOX(sv), app.chk_rulers, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sv), app.chk_grid, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sv), app.chk_status, FALSE, FALSE, 0);
    add_group(bar, group("Show or hide", sv));

    GtkWidget *dh = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget *fs = big_button("fullscreen", "Full\nscreen", G_CALLBACK(cmd_fullscreen), NULL);
    gtk_widget_set_tooltip_text(fs, "Full screen (F11)\nView the picture in full screen.");
    gtk_box_pack_start(GTK_BOX(dh), fs, FALSE, FALSE, 0);
    GtkWidget *tb = gtk_toggle_button_new();
    gtk_button_set_relief(GTK_BUTTON(tb), GTK_RELIEF_NONE);
    GtkWidget *tv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(tv), icon_widget("thumbnail", 32), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tv), gtk_label_new("Thumbnail"), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(tb), tv);
    gtk_widget_set_valign(tb, GTK_ALIGN_START);
    g_signal_connect(tb, "toggled", G_CALLBACK(view_toggle), GINT_TO_POINTER(3));
    gtk_widget_set_tooltip_text(tb, "Thumbnail\nShow or hide the Thumbnail window.");
    g_object_set_data(G_OBJECT(app.window), "thumb-toggle", tb);
    gtk_box_pack_start(GTK_BOX(dh), tb, FALSE, FALSE, 0);
    add_group(bar, group("Display", dh));
}

/* ------------------------------------------------------------------ */
/* ribbon: Text tab (shown while the text tool is editing)              */

static void font_changed(GtkComboBox *c, gpointer d)
{
    (void)d;
    if (app.updating_ui) return;
    gchar *f = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(c));
    if (f && *f) g_strlcpy(app.font_family, f, sizeof app.font_family);
    g_free(f);
    text_apply_attrs();
}

static void font_size_changed(GtkComboBox *c, gpointer d)
{
    (void)d;
    if (app.updating_ui) return;
    gchar *f = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(c));
    int v = f ? atoi(f) : 0;
    if (v > 0 && v <= 999) app.font_size = v;
    g_free(f);
    text_apply_attrs();
}

static void style_toggle(GtkToggleButton *b, gpointer d)
{
    if (app.updating_ui) return;
    bool on = gtk_toggle_button_get_active(b);
    switch (GPOINTER_TO_INT(d)) {
    case 0: app.bold = on; break;
    case 1: app.italic = on; break;
    case 2: app.underline = on; break;
    case 3: app.strike = on; break;
    case 4: app.text_opaque = on; break;
    case 5: app.text_opaque = !on; break;
    }
    update_ui();
    text_apply_attrs();
}

static GtkWidget *style_btn(const char *markup, int which, const char *tip)
{
    GtkWidget *b = gtk_toggle_button_new();
    GtkWidget *l = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(l), markup);
    gtk_container_add(GTK_CONTAINER(b), l);
    gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(b, tip);
    g_signal_connect(b, "toggled", G_CALLBACK(style_toggle), GINT_TO_POINTER(which));
    return b;
}

static int cmp_family(const void *a, const void *b)
{
    return g_utf8_collate(pango_font_family_get_name(*(PangoFontFamily **)a),
                          pango_font_family_get_name(*(PangoFontFamily **)b));
}

static void build_text_tab(GtkWidget *bar)
{
    add_group(bar, clipboard_group());

    GtkWidget *fv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *fr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    app.font_combo = gtk_combo_box_text_new();
    PangoFontMap *fm = pango_cairo_font_map_get_default();
    PangoFontFamily **fams;
    int nf;
    pango_font_map_list_families(fm, &fams, &nf);
    qsort(fams, nf, sizeof *fams, cmp_family);
    for (int i = 0; i < nf; i++)
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app.font_combo), pango_font_family_get_name(fams[i]),
                                  pango_font_family_get_name(fams[i]));
    /* Calibri (the original's default) may not be installed: fall back */
    bool have = false;
    for (int i = 0; i < nf && !have; i++) have = !g_ascii_strcasecmp(pango_font_family_get_name(fams[i]), app.font_family);
    for (int i = 0; i < nf && !have; i++) {
        const char *n = pango_font_family_get_name(fams[i]);
        if (!strcmp(n, "Carlito") || !strcmp(n, "DejaVu Sans") || !strcmp(n, "Sans")) {
            g_strlcpy(app.font_family, n, sizeof app.font_family);
            have = true;
        }
    }
    if (!have && nf) g_strlcpy(app.font_family, pango_font_family_get_name(fams[0]), sizeof app.font_family);
    g_free(fams);
    gtk_widget_set_size_request(app.font_combo, 170, -1);
    g_signal_connect(app.font_combo, "changed", G_CALLBACK(font_changed), NULL);
    app.font_size_combo = gtk_combo_box_text_new_with_entry();
    static const int sizes[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72 };
    for (unsigned i = 0; i < G_N_ELEMENTS(sizes); i++) {
        char b[8];
        g_snprintf(b, sizeof b, "%d", sizes[i]);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.font_size_combo), b);
    }
    gtk_entry_set_width_chars(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(app.font_size_combo))), 3);
    g_signal_connect(app.font_size_combo, "changed", G_CALLBACK(font_size_changed), NULL);
    gtk_box_pack_start(GTK_BOX(fr), app.font_combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(fr), app.font_size_combo, FALSE, FALSE, 0);
    GtkWidget *sr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    app.bold_btn = style_btn("<b>B</b>", 0, "Bold (Ctrl+B)");
    app.italic_btn = style_btn("<i>I</i>", 1, "Italic (Ctrl+I)");
    app.underline_btn = style_btn("<u>U</u>", 2, "Underline (Ctrl+U)");
    app.strike_btn = style_btn("<s>abc</s>", 3, "Strikethrough");
    gtk_box_pack_start(GTK_BOX(sr), app.bold_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sr), app.italic_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sr), app.underline_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sr), app.strike_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(fv), fr, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(fv), sr, FALSE, FALSE, 0);
    gtk_widget_set_valign(fv, GTK_ALIGN_CENTER);
    add_group(bar, group("Font", fv));

    GtkWidget *bv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(bv, GTK_ALIGN_CENTER);
    app.opaque_btn = gtk_toggle_button_new();
    app.transp_btn = gtk_toggle_button_new();
    GtkWidget *h1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4), *h2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(h1), icon_widget("opaque", 16), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(h1), gtk_label_new("Opaque"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(h2), icon_widget("transp", 16), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(h2), gtk_label_new("Transparent"), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(app.opaque_btn), h1);
    gtk_container_add(GTK_CONTAINER(app.transp_btn), h2);
    gtk_button_set_relief(GTK_BUTTON(app.opaque_btn), GTK_RELIEF_NONE);
    gtk_button_set_relief(GTK_BUTTON(app.transp_btn), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(app.opaque_btn, "Opaque\nMake the background of the text opaque (Color 2).");
    gtk_widget_set_tooltip_text(app.transp_btn, "Transparent\nMake the background of the text transparent.");
    g_signal_connect(app.opaque_btn, "toggled", G_CALLBACK(style_toggle), GINT_TO_POINTER(4));
    g_signal_connect(app.transp_btn, "toggled", G_CALLBACK(style_toggle), GINT_TO_POINTER(5));
    gtk_box_pack_start(GTK_BOX(bv), app.opaque_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bv), app.transp_btn, FALSE, FALSE, 0);
    add_group(bar, group("Background", bv));

    add_group(bar, colors_group());
}

void show_text_tab(bool show)
{
    GtkNotebook *nb = GTK_NOTEBOOK(app.ribbon_notebook);
    int idx = gtk_notebook_page_num(nb, app.text_tab);
    if (show) {
        gtk_widget_show(app.text_tab);
        gtk_notebook_set_current_page(nb, idx);
    } else if (gtk_widget_get_visible(app.text_tab)) {
        if (gtk_notebook_get_current_page(nb) == idx) gtk_notebook_set_current_page(nb, 0);
        gtk_widget_hide(app.text_tab);
    }
    update_ui();
}

/* ------------------------------------------------------------------ */
/* File menu                                                           */

static void open_recent(GtkRecentChooser *rc, gpointer d)
{
    (void)d;
    gchar *uri = gtk_recent_chooser_get_current_uri(rc);
    gchar *p = uri ? g_filename_from_uri(uri, NULL, NULL) : NULL;
    if (p && confirm_discard()) open_path(p);
    g_free(p);
    g_free(uri);
}

static void save_as_cmd(gpointer d) { cmd_save_as(d); }
static void print_cmd(gpointer d) { do_print(GPOINTER_TO_INT(d)); }
static void wallpaper_cmd(gpointer d) { set_wallpaper(d); }

static GtkWidget *file_menu(void)
{
    GtkWidget *m = gtk_menu_new();
    menu_item(m, "new", "_New\tCtrl+N", G_CALLBACK(cmd_new), NULL);
    menu_item(m, "open", "_Open\tCtrl+O", G_CALLBACK(cmd_open), NULL);
    menu_item(m, "save", "_Save\tCtrl+S", G_CALLBACK(cmd_save), NULL);
    GtkWidget *sa = submenu(m, "saveas", "Save _as");
    menu_item(sa, NULL, "_PNG picture", G_CALLBACK(save_as_cmd), "png");
    menu_item(sa, NULL, "_JPEG picture", G_CALLBACK(save_as_cmd), "jpeg");
    menu_item(sa, NULL, "_BMP picture", G_CALLBACK(save_as_cmd), "bmp24");
    menu_item(sa, NULL, "_GIF picture", G_CALLBACK(save_as_cmd), "gif");
    menu_item(sa, NULL, "_Other formats\tF12", G_CALLBACK(save_as_cmd), NULL);
    menu_sep(m);
    GtkWidget *pm = submenu(m, "print", "_Print");
    menu_item(pm, "print", "_Print\tCtrl+P", G_CALLBACK(print_cmd), GINT_TO_POINTER(0));
    menu_item(pm, NULL, "Page set_up", G_CALLBACK(do_page_setup), NULL);
    menu_item(pm, NULL, "Print pre_view", G_CALLBACK(print_cmd), GINT_TO_POINTER(1));
    menu_item(m, "scanner", "Fro_m scanner or camera", G_CALLBACK(cmd_scan), NULL);
    menu_item(m, "email", "Sen_d in e-mail", G_CALLBACK(cmd_email), NULL);
    GtkWidget *wm = submenu(m, "wallpaper", "Set as desktop _background");
    menu_item(wm, NULL, "_Fill", G_CALLBACK(wallpaper_cmd), "fill");
    menu_item(wm, NULL, "_Tile", G_CALLBACK(wallpaper_cmd), "tile");
    menu_item(wm, NULL, "_Center", G_CALLBACK(wallpaper_cmd), "center");
    menu_sep(m);
    menu_item(m, "properties", "Prop_erties\tCtrl+E", G_CALLBACK(cmd_properties), NULL);
    menu_item(m, "about", "Abou_t Paint", G_CALLBACK(cmd_about), NULL);
    menu_sep(m);
    /* Recent pictures (CWinApp::LoadStdProfileSettings(9) keeps 9) */
    GtkWidget *ri = gtk_menu_item_new_with_mnemonic("_Recent pictures");
    GtkWidget *rc = gtk_recent_chooser_menu_new();
    GtkRecentFilter *rf = gtk_recent_filter_new();
    gtk_recent_filter_add_pixbuf_formats(rf);
    gtk_recent_chooser_add_filter(GTK_RECENT_CHOOSER(rc), rf);
    gtk_recent_chooser_set_limit(GTK_RECENT_CHOOSER(rc), 9);
    gtk_recent_chooser_set_sort_type(GTK_RECENT_CHOOSER(rc), GTK_RECENT_SORT_MRU);
    gtk_recent_chooser_set_local_only(GTK_RECENT_CHOOSER(rc), TRUE);
    g_signal_connect(rc, "item-activated", G_CALLBACK(open_recent), NULL);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(ri), rc);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), ri);
    menu_sep(m);
    menu_item(m, "exit", "E_xit", G_CALLBACK(cmd_exit), NULL);
    return m;
}

/* right-click menu of the selection tools (CImgWnd::ShowContextMenu) */
void show_context_menu(GdkEventButton *ev)
{
    (void)ev;
    GtkWidget *m = gtk_menu_new();
    GtkWidget *it;
    it = menu_item(m, "cut", "Cu_t", G_CALLBACK(cmd_cut), NULL);
    gtk_widget_set_sensitive(it, app.sel.active);
    it = menu_item(m, "copy", "_Copy", G_CALLBACK(cmd_copy), NULL);
    gtk_widget_set_sensitive(it, app.sel.active);
    menu_item(m, "paste", "_Paste", G_CALLBACK(cmd_paste), NULL);
    menu_item(m, "crop", "Cr_op", G_CALLBACK(image_crop), NULL);
    menu_item(m, "select-all", "Select _all", G_CALLBACK(sel_select_all), NULL);
    menu_item(m, "invert-sel", "_Invert selection", G_CALLBACK(sel_invert), NULL);
    it = menu_item(m, "delete", "_Delete", G_CALLBACK(sel_delete), NULL);
    gtk_widget_set_sensitive(it, app.sel.active);
    GtkWidget *rm = submenu(m, "rotate", "_Rotate");
    menu_item(rm, "rotate-r", "Rotate _right 90°", G_CALLBACK(rotate_cmd), GINT_TO_POINTER(1));
    menu_item(rm, "rotate-l", "Rotate _left 90°", G_CALLBACK(rotate_cmd), GINT_TO_POINTER(3));
    menu_item(rm, "rotate-180", "Rotate 1_80°", G_CALLBACK(rotate_cmd), GINT_TO_POINTER(2));
    menu_item(rm, "flip-v", "Flip _vertical", G_CALLBACK(flip_cmd), GINT_TO_POINTER(0));
    menu_item(rm, "flip-h", "Flip _horizontal", G_CALLBACK(flip_cmd), GINT_TO_POINTER(1));
    menu_item(m, "resize", "Re_size", G_CALLBACK(cmd_resize), NULL);
    menu_item(m, NULL, "Invert _color", G_CALLBACK(image_invert), NULL);
    gtk_widget_show_all(m);
    gtk_menu_popup_at_pointer(GTK_MENU(m), NULL);
}

/* ------------------------------------------------------------------ */
/* update_ui: enable/disable and check states (CPBView::OnUpdate*)     */

void update_ui(void)
{
    if (!app.window || app.updating_ui) return;
    app.updating_ui = true;
    for (int t = 0; t < TOOL_COUNT; t++)
        if (app.tool_btn[t]) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.tool_btn[t]), (int)app.tool == t);
    int i = 0;
    for (GList *l = shape_buttons; l; l = l->next, i++)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l->data), app.tool == TOOL_SHAPE && (int)app.shape_kind == i);
    if (app.tool == TOOL_SELECT_RECT || app.tool == TOOL_SELECT_FREE)
        gtk_widget_set_state_flags(select_btn, GTK_STATE_FLAG_CHECKED, FALSE);
    else
        gtk_widget_unset_state_flags(select_btn, GTK_STATE_FLAG_CHECKED);
    for (GList *l = size_widgets; l; l = l->next) gtk_widget_set_sensitive(l->data, tool_has_size());
    gtk_widget_set_sensitive(app.outline_btn, app.tool == TOOL_SHAPE);
    gtk_widget_set_sensitive(app.fill_btn, app.tool == TOOL_SHAPE && app.shape_kind != SHAPE_LINE && app.shape_kind != SHAPE_CURVE);
    gtk_widget_set_sensitive(undo_btn, app.n_undo > 0 || app.undo_pending);
    gtk_widget_set_sensitive(redo_btn, app.n_redo > 0);
    gtk_widget_set_sensitive(cut_btn, app.sel.active);
    gtk_widget_set_sensitive(copy_btn, app.sel.active);
    gtk_widget_set_sensitive(crop_btn, app.sel.active);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(app.transp_check), app.transparent_sel);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.chk_rulers), app.show_rulers);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.chk_grid), app.show_grid);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.chk_status), app.show_status);
    GtkWidget *tb = g_object_get_data(G_OBJECT(app.window), "thumb-toggle");
    if (tb) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tb), app.thumb_win != NULL);
    gtk_widget_set_visible(app.hruler, app.show_rulers);
    gtk_widget_set_visible(app.vruler, app.show_rulers);
    gtk_widget_set_visible(app.ruler_corner, app.show_rulers);
    gtk_widget_set_visible(app.statusbar, app.show_status);
    /* text tab */
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(app.font_combo), app.font_family);
    char b[8];
    g_snprintf(b, sizeof b, "%d", app.font_size);
    gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(app.font_size_combo))), b);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.bold_btn), app.bold);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.italic_btn), app.italic);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.underline_btn), app.underline);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.strike_btn), app.strike);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.opaque_btn), app.text_opaque);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.transp_btn), !app.text_opaque);
    app.updating_ui = false;
    update_color_ui();
}

/* ------------------------------------------------------------------ */
/* keyboard (accelerators of the original + ribbon keytips)            */

static gboolean on_key(GtkWidget *w, GdkEventKey *ev, gpointer d)
{
    (void)w; (void)d;
    guint mods = ev->state & gtk_accelerator_get_default_mod_mask();
    bool ctrl = mods & GDK_CONTROL_MASK, shift = mods & GDK_SHIFT_MASK;
    guint k = gdk_keyval_to_lower(ev->keyval);

    /* let focused entries (font combo etc.) have their keys */
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(app.window));
    if (focus && GTK_IS_ENTRY(focus)) return FALSE;

    if (text_active()) {
        if (ctrl && (k == GDK_KEY_b || k == GDK_KEY_i || k == GDK_KEY_u)) {
            if (k == GDK_KEY_b) app.bold = !app.bold;
            if (k == GDK_KEY_i) app.italic = !app.italic;
            if (k == GDK_KEY_u) app.underline = !app.underline;
            update_ui();
            text_apply_attrs();
            return TRUE;
        }
        if (tool_key(ev)) return TRUE;
    }

    if (ctrl && !shift) {
        switch (k) {
        case GDK_KEY_n: cmd_new(); return TRUE;
        case GDK_KEY_o: cmd_open(); return TRUE;
        case GDK_KEY_s: cmd_save(); return TRUE;
        case GDK_KEY_p: do_print(false); return TRUE;
        case GDK_KEY_z: do_undo(); return TRUE;
        case GDK_KEY_y: do_redo(); return TRUE;
        case GDK_KEY_x: cmd_cut(); return TRUE;
        case GDK_KEY_c: cmd_copy(); return TRUE;
        case GDK_KEY_v: cmd_paste(); return TRUE;
        case GDK_KEY_a: sel_select_all(); return TRUE;
        case GDK_KEY_e: cmd_properties(); return TRUE;
        case GDK_KEY_w: cmd_resize(); return TRUE;
        case GDK_KEY_r: app.show_rulers = !app.show_rulers; update_ui(); return TRUE;
        case GDK_KEY_g: app.show_grid = !app.show_grid; update_ui(); canvas_redraw(); return TRUE;
        case GDK_KEY_Page_Up: canvas_zoom_step(1); return TRUE;
        case GDK_KEY_Page_Down: canvas_zoom_step(-1); return TRUE;
        case GDK_KEY_plus: case GDK_KEY_equal: case GDK_KEY_KP_Add: tool_size_step(1); return TRUE;
        case GDK_KEY_minus: case GDK_KEY_KP_Subtract: tool_size_step(-1); return TRUE;
        }
    }
    if (ctrl && shift) {
        switch (k) {
        case GDK_KEY_x: image_crop(); return TRUE;
        case GDK_KEY_i: image_invert(); return TRUE;
        case GDK_KEY_plus: case GDK_KEY_KP_Add: tool_size_step(1); return TRUE;
        }
    }
    if (!ctrl && !(mods & GDK_MOD1_MASK)) {
        switch (ev->keyval) {
        case GDK_KEY_F11: cmd_fullscreen(); return TRUE;
        case GDK_KEY_F12: cmd_save_as(NULL); return TRUE;
        case GDK_KEY_F1: cmd_about(); return TRUE;
        case GDK_KEY_Delete:
            if (app.sel.active) { sel_delete(); return TRUE; }
            break;
        }
    }
    if (mods & GDK_MOD1_MASK && ev->keyval == GDK_KEY_F4) { cmd_exit(); return TRUE; }
    return tool_key(ev);
}

static gboolean on_delete(GtkWidget *w, GdkEvent *e, gpointer d)
{
    (void)w; (void)e; (void)d;
    return !confirm_discard();
}

/* ------------------------------------------------------------------ */
/* status bar                                                          */

static void zoom_scale_changed(GtkRange *r, gpointer d)
{
    (void)d;
    if (app.updating_ui) return;
    double z = pow(2, gtk_range_get_value(r));
    z = round(z * 100) / 100;
    canvas_set_zoom(z, -1, -1);
}

static GtkWidget *status_cell(GtkWidget **label, const char *icon, int width)
{
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    if (icon) gtk_box_pack_start(GTK_BOX(h), icon_widget(icon, 16), FALSE, FALSE, 0);
    *label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(*label), 0);
    gtk_widget_set_size_request(*label, width, -1);
    gtk_box_pack_start(GTK_BOX(h), *label, FALSE, FALSE, 0);
    return h;
}

static GtkWidget *build_statusbar(void)
{
    GtkWidget *sb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(sb), "paint-status");
    gtk_container_set_border_width(GTK_CONTAINER(sb), 2);
    gtk_box_pack_start(GTK_BOX(sb), status_cell(&app.st_pos, "cur-pencil", 110), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sb), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sb), status_cell(&app.st_sel, "select", 110), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sb), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sb), status_cell(&app.st_size, "resize", 110), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sb), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sb), status_cell(&app.st_file, "save", 110), FALSE, FALSE, 0);
    GtkWidget *zh = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    app.zoom_label = gtk_label_new("100%");
    gtk_widget_set_size_request(app.zoom_label, 50, -1);
    GtkWidget *zo = gtk_button_new_with_label("−"), *zi = gtk_button_new_with_label("+");
    gtk_button_set_relief(GTK_BUTTON(zo), GTK_RELIEF_NONE);
    gtk_button_set_relief(GTK_BUTTON(zi), GTK_RELIEF_NONE);
    g_signal_connect_swapped(zo, "clicked", G_CALLBACK(zoom_cmd), GINT_TO_POINTER(-1));
    g_signal_connect_swapped(zi, "clicked", G_CALLBACK(zoom_cmd), GINT_TO_POINTER(1));
    app.zoom_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -3, 3, 0.01);
    gtk_scale_set_draw_value(GTK_SCALE(app.zoom_scale), FALSE);
    gtk_scale_add_mark(GTK_SCALE(app.zoom_scale), 0, GTK_POS_BOTTOM, NULL);
    gtk_widget_set_size_request(app.zoom_scale, 130, -1);
    g_signal_connect(app.zoom_scale, "value-changed", G_CALLBACK(zoom_scale_changed), NULL);
    gtk_box_pack_start(GTK_BOX(zh), app.zoom_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(zh), zo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(zh), app.zoom_scale, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(zh), zi, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(sb), zh, FALSE, FALSE, 0);
    return sb;
}

/* ------------------------------------------------------------------ */
/* settings                                                            */

static gchar *settings_path(void)
{
    return g_build_filename(g_get_user_config_dir(), "linpaint", "settings.ini", NULL);
}

static void load_settings(void)
{
    GKeyFile *kf = g_key_file_new();
    gchar *p = settings_path();
    bool ok = g_key_file_load_from_file(kf, p, G_KEY_FILE_NONE, NULL);
    g_free(p);
    app.show_rulers = false;
    app.show_status = true;
    app.show_grid = false;
    g_strlcpy(app.font_family, "Calibri", sizeof app.font_family);
    app.font_size = 11;
    if (!ok) { g_key_file_free(kf); return; }
    /* names follow the original's View registry values */
    if (g_key_file_has_key(kf, "View", "ShowRulers", NULL)) app.show_rulers = g_key_file_get_boolean(kf, "View", "ShowRulers", NULL);
    if (g_key_file_has_key(kf, "View", "ShowStatusBar", NULL)) app.show_status = g_key_file_get_boolean(kf, "View", "ShowStatusBar", NULL);
    if (g_key_file_has_key(kf, "View", "ShowGrid", NULL)) app.show_grid = g_key_file_get_boolean(kf, "View", "ShowGrid", NULL);
    int bw = g_key_file_get_integer(kf, "View", "BMPWidth", NULL), bh = g_key_file_get_integer(kf, "View", "BMPHeight", NULL);
    /* CPBApp::InitInstance ignores stored sizes above 5000 */
    if (bw > 0 && bh > 0 && bw <= 5000 && bh <= 5000) { default_w = bw; default_h = bh; }
    gchar *ff = g_key_file_get_string(kf, "Text", "TypeFaceName", NULL);
    if (ff) { g_strlcpy(app.font_family, ff, sizeof app.font_family); g_free(ff); }
    int fs = g_key_file_get_integer(kf, "Text", "PointSize", NULL);
    if (fs > 0) app.font_size = fs;
    app.bold = g_key_file_get_boolean(kf, "Text", "Bold", NULL);
    app.italic = g_key_file_get_boolean(kf, "Text", "Italic", NULL);
    app.underline = g_key_file_get_boolean(kf, "Text", "Underline", NULL);
    app.strike = g_key_file_get_boolean(kf, "Text", "Strikeout", NULL);
    gsize n = 0;
    gchar **cc = g_key_file_get_string_list(kf, "Colors", "Custom", &n, NULL);
    for (gsize i = 0; i < n && i < N_CUSTOM_COLORS; i++) {
        unsigned v = (unsigned)strtoul(cc[i], NULL, 16);
        app.custom[app.n_custom++] = hex_rgb(v);
    }
    g_strfreev(cc);
    g_key_file_free(kf);
}

static void save_settings(void)
{
    GKeyFile *kf = g_key_file_new();
    g_key_file_set_boolean(kf, "View", "ShowRulers", app.show_rulers);
    g_key_file_set_boolean(kf, "View", "ShowStatusBar", app.show_status);
    g_key_file_set_boolean(kf, "View", "ShowGrid", app.show_grid);
    g_key_file_set_integer(kf, "View", "BMPWidth", app.w);
    g_key_file_set_integer(kf, "View", "BMPHeight", app.h);
    g_key_file_set_string(kf, "Text", "TypeFaceName", app.font_family);
    g_key_file_set_integer(kf, "Text", "PointSize", app.font_size);
    g_key_file_set_boolean(kf, "Text", "Bold", app.bold);
    g_key_file_set_boolean(kf, "Text", "Italic", app.italic);
    g_key_file_set_boolean(kf, "Text", "Underline", app.underline);
    g_key_file_set_boolean(kf, "Text", "Strikeout", app.strike);
    gchar *cc[N_CUSTOM_COLORS + 1] = { 0 };
    for (int i = 0; i < app.n_custom; i++)
        cc[i] = g_strdup_printf("%06X", rgb_to_px(app.custom[i]) & 0xFFFFFF);
    g_key_file_set_string_list(kf, "Colors", "Custom", (const gchar *const *)cc, app.n_custom);
    for (int i = 0; i < app.n_custom; i++) g_free(cc[i]);
    gchar *p = settings_path();
    gchar *dir = g_path_get_dirname(p);
    g_mkdir_with_parents(dir, 0755);
    g_key_file_save_to_file(kf, p, NULL);
    g_free(dir);
    g_free(p);
    g_key_file_free(kf);
}

/* ------------------------------------------------------------------ */

static const char *css =
    "notebook.ribbon > header { background: #DFE9F5; }"
    "notebook.ribbon > stack { background: linear-gradient(#FFFFFF, #E8EFF8); }"
    "notebook.ribbon > header tab { padding: 2px 14px; }"
    ".group-label { color: #3B4F6B; font-size: 0.85em; }"
    "button.file-button { background: #1E5BB8; color: white; border-radius: 3px 3px 0 0; padding: 2px 12px; }"
    "button.file-button label { color: white; }"
    ".shape-btn { padding: 1px; min-height: 0; min-width: 0; }"
    "button.checked-big:checked, button:checked { background: #FCE4A8; }"
    ".paint-status { background: #DCE6F4; border-top: 1px solid #A8B8D0; }";

static void on_drop(GtkWidget *w, GdkDragContext *ctx, int x, int y, GtkSelectionData *sd, guint info, guint t, gpointer d)
{
    (void)w; (void)x; (void)y; (void)info; (void)d;
    gchar **uris = gtk_selection_data_get_uris(sd);
    if (uris && uris[0]) {
        gchar *p = g_filename_from_uri(uris[0], NULL, NULL);
        if (p && confirm_discard()) open_path(p);   /* CPBFrame::OnDropFiles */
        g_free(p);
    }
    g_strfreev(uris);
    gtk_drag_finish(ctx, TRUE, FALSE, t);
}

static void activate(GtkApplication *ga, gpointer d)
{
    char **argv = d;
    GtkCssProvider *cp = gtk_css_provider_new();
    gtk_css_provider_load_from_data(cp, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(cp),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    app.window = gtk_application_window_new(ga);
    gtk_window_set_icon_name(GTK_WINDOW(app.window), "applications-graphics");
    GdkRectangle wa;
    gdk_monitor_get_workarea(gdk_display_get_primary_monitor(gdk_display_get_default()), &wa);
    gtk_window_set_default_size(GTK_WINDOW(app.window), MIN(1100, wa.width * 0.8), MIN(800, wa.height * 0.8));
    if (!default_w) {
        /* no remembered size: pick a canvas that fits the initial window */
        default_w = MAX(320, (int)(MIN(1100, wa.width * 0.8) - 60));
        default_h = MAX(240, (int)(MIN(800, wa.height * 0.8) - 230));
    }

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(app.window), vbox);

    /* ribbon */
    app.ribbon_notebook = gtk_notebook_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(app.ribbon_notebook), "ribbon");
    GtkWidget *fb = gtk_menu_button_new();
    gtk_container_remove(GTK_CONTAINER(fb), gtk_bin_get_child(GTK_BIN(fb)));
    GtkWidget *fl = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(fl), icon_widget("file", 16), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(fl), gtk_label_new("File ▾"), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(fb), fl);
    gtk_style_context_add_class(gtk_widget_get_style_context(fb), "file-button");
    GtkWidget *fm = file_menu();
    gtk_widget_show_all(fm);
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(fb), fm);
    gtk_widget_show_all(fb);
    gtk_notebook_set_action_widget(GTK_NOTEBOOK(app.ribbon_notebook), fb, GTK_PACK_START);

    /* quick access toolbar */
    GtkWidget *qat = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *qs = small_button("save", NULL, G_CALLBACK(cmd_save), NULL);
    undo_btn = small_button("undo", NULL, G_CALLBACK(do_undo), NULL);
    redo_btn = small_button("redo", NULL, G_CALLBACK(do_redo), NULL);
    GtkWidget *qa = small_button("about", NULL, G_CALLBACK(cmd_about), NULL);
    gtk_widget_set_tooltip_text(qs, "Save (Ctrl+S)");
    gtk_widget_set_tooltip_text(undo_btn, "Undo (Ctrl+Z)");
    gtk_widget_set_tooltip_text(redo_btn, "Redo (Ctrl+Y)");
    gtk_widget_set_tooltip_text(qa, "About Paint (F1)");
    gtk_box_pack_start(GTK_BOX(qat), qs, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(qat), undo_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(qat), redo_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(qat), qa, FALSE, FALSE, 0);
    gtk_widget_show_all(qat);
    gtk_notebook_set_action_widget(GTK_NOTEBOOK(app.ribbon_notebook), qat, GTK_PACK_END);

    home_page = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    view_page = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    app.text_tab = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(home_page), 3);
    gtk_container_set_border_width(GTK_CONTAINER(view_page), 3);
    gtk_container_set_border_width(GTK_CONTAINER(app.text_tab), 3);
    build_home(home_page);
    build_view(view_page);
    build_text_tab(app.text_tab);
    gtk_notebook_append_page(GTK_NOTEBOOK(app.ribbon_notebook), home_page, gtk_label_new_with_mnemonic("_Home"));
    gtk_notebook_append_page(GTK_NOTEBOOK(app.ribbon_notebook), view_page, gtk_label_new_with_mnemonic("_View"));
    GtkWidget *tl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(tl), "<span foreground='#B8860B'><b>Text</b></span>");
    gtk_notebook_append_page(GTK_NOTEBOOK(app.ribbon_notebook), app.text_tab, tl);
    gtk_box_pack_start(GTK_BOX(vbox), app.ribbon_notebook, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), canvas_create(), TRUE, TRUE, 0);
    app.statusbar = build_statusbar();
    gtk_box_pack_start(GTK_BOX(vbox), app.statusbar, FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(app.statusbar, FALSE);

    g_signal_connect(app.window, "key-press-event", G_CALLBACK(on_key), NULL);
    g_signal_connect(app.window, "delete-event", G_CALLBACK(on_delete), NULL);
    gtk_drag_dest_set(app.window, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
    gtk_drag_dest_add_uri_targets(app.window);
    g_signal_connect(app.window, "drag-data-received", G_CALLBACK(on_drop), NULL);

    set_image(surface_new(default_w, default_h, (RGB){1, 1, 1}));
    app.dirty = false;
    update_title();
    gtk_widget_show_all(app.window);
    gtk_widget_hide(app.text_tab);
    update_ui();
    update_status();
    canvas_update_cursor();
    gtk_widget_grab_focus(app.canvas);

    selftest_schedule();
    if (argv && argv[1]) {
        GFile *gf = g_file_new_for_commandline_arg(argv[1]);
        gchar *p = g_file_get_path(gf);
        if (p) open_path(p);
        g_free(p);
        g_object_unref(gf);
    }
}

int main(int argc, char **argv)
{
    memset(&app, 0, sizeof app);
    app.zoom = 1;
    app.dpi_x = app.dpi_y = 96;
    app.color1 = hex_rgb(0x000000);
    app.color2 = hex_rgb(0xFFFFFF);
    app.active_color = 1;
    app.tool = app.prev_tool = TOOL_PENCIL;
    app.brush_kind = BRUSH_REGULAR;
    app.shape_kind = SHAPE_LINE;
    app.outline = MEDIA_SOLID;
    app.fill = MEDIA_NONE;
    app.antialias = true;
    app.text_opaque = false;
    app.mouse_x = app.mouse_y = -1;
    /* default sizes (index into each list as the original selects at startup) */
    app.size_px[TOOL_PENCIL] = 1;
    app.size_px[TOOL_ERASER] = 8;
    app.size_px[TOOL_SHAPE] = 1;
    app.brush_size_px[BRUSH_REGULAR] = 3;
    app.brush_size_px[BRUSH_CALLIG1] = 5;
    app.brush_size_px[BRUSH_CALLIG2] = 5;
    app.brush_size_px[BRUSH_AIRBRUSH] = 8;
    for (int i = BRUSH_OIL; i < BRUSH_COUNT; i++) app.brush_size_px[i] = 16;
    app.brush_size_px[BRUSH_NATPENCIL] = 6;

    load_settings();
    gtk_init(&argc, &argv);
    brush_init();
    tool_init();
    /* linpaint --convert IN OUT [FORMAT]: load and re-save without a window */
    if (argc >= 4 && !strcmp(argv[1], "--convert")) {
        GError *err = NULL;
        if (!file_open(argv[2], &err) ||
            !file_save(argv[3], argc >= 5 ? argv[4] : format_from_path(argv[3]), &err)) {
            g_printerr("linpaint: %s\n", err ? err->message : "failed");
            return 1;
        }
        return 0;
    }
    GtkApplication *ga = gtk_application_new("org.linpaint.Paint", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(ga, "activate", G_CALLBACK(activate), argv);
    int r = g_application_run(G_APPLICATION(ga), 1, argv);
    save_settings();
    g_object_unref(ga);
    return r;
}
