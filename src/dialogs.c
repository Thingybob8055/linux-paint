/* Dialogs: Image Properties (CImageAttr), Resize and Skew (CStretchSkewDlg),
 * Edit Colors (CColors::EditColor), About, save-changes prompt. */
#include "paint.h"
#include <math.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>

void error_dialog(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    gchar *msg = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(app.window), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
                                          GTK_BUTTONS_OK, "%s", msg);
    gtk_window_set_title(GTK_WINDOW(d), APP_NAME);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    g_free(msg);
}

/* CPBDoc::SaveModified */
bool confirm_discard(void)
{
    tool_commit_pending();
    if (!app.dirty) return true;
    gchar *base = g_path_get_basename(app.filename ? app.filename : "Untitled");
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(app.window), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
                                          GTK_BUTTONS_NONE, "Do you want to save changes to %s?", base);
    g_free(base);
    gtk_window_set_title(GTK_WINDOW(d), APP_NAME);
    gtk_dialog_add_buttons(GTK_DIALOG(d), "_Save", 1, "Do_n't Save", 2, "_Cancel", 0, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(d), 1);
    int r = gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    if (r == 1) return cmd_save();
    return r == 2;
}

/* ------------------------------------------------------------------ */
/* Image Properties                                                    */

typedef struct {
    GtkWidget *w, *h, *in, *cm, *px, *bw, *col;
    int unit;           /* 0 inch, 1 cm, 2 px (CImageAttr::OnInches/OnCentimeters/OnPixels) */
    double cur_w, cur_h;  /* pixels */
} AttrDlg;

static double px_to_unit(AttrDlg *a, double px, double dpi)
{
    if (a->unit == 0) return px / dpi;
    if (a->unit == 1) return px / dpi * 2.54;
    return px;
}

static double unit_to_px(AttrDlg *a, double v, double dpi)
{
    if (a->unit == 0) return v * dpi;
    if (a->unit == 1) return v / 2.54 * dpi;
    return v;
}

static void attr_show(AttrDlg *a)
{
    int digits = a->unit == 2 ? 0 : 2;
    for (int i = 0; i < 2; i++) {
        GtkSpinButton *sb = GTK_SPIN_BUTTON(i ? a->h : a->w);
        double dpi = i ? app.dpi_y : app.dpi_x;
        gtk_spin_button_set_digits(sb, digits);
        gtk_spin_button_set_increments(sb, a->unit == 2 ? 1 : 0.01, a->unit == 2 ? 10 : 1);
        gtk_spin_button_set_range(sb, a->unit == 2 ? 1 : 0.01, px_to_unit(a, MAX_IMAGE_DIM, dpi));
        gtk_spin_button_set_value(sb, px_to_unit(a, i ? a->cur_h : a->cur_w, dpi));
    }
}

static void attr_unit(GtkToggleButton *t, AttrDlg *a)
{
    if (!gtk_toggle_button_get_active(t)) return;
    a->cur_w = unit_to_px(a, gtk_spin_button_get_value(GTK_SPIN_BUTTON(a->w)), app.dpi_x);
    a->cur_h = unit_to_px(a, gtk_spin_button_get_value(GTK_SPIN_BUTTON(a->h)), app.dpi_y);
    a->unit = GTK_WIDGET(t) == a->in ? 0 : GTK_WIDGET(t) == a->cm ? 1 : 2;
    attr_show(a);
}

static GtkWidget *frame_box(const char *title, GtkWidget *child)
{
    GtkWidget *f = gtk_frame_new(title);
    gtk_container_set_border_width(GTK_CONTAINER(child), 6);
    gtk_container_add(GTK_CONTAINER(f), child);
    return f;
}

void cmd_properties(void)
{
    tool_commit_pending();
    static int saved_unit = 2;
    AttrDlg a = { 0 };
    a.unit = saved_unit;
    a.cur_w = app.w;
    a.cur_h = app.h;
    GtkWidget *d = gtk_dialog_new_with_buttons("Image Properties", GTK_WINDOW(app.window), GTK_DIALOG_MODAL,
                                               "_Default", 2, "_Cancel", GTK_RESPONSE_CANCEL, "_OK", GTK_RESPONSE_OK,
                                               NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
    GtkWidget *ca = gtk_dialog_get_content_area(GTK_DIALOG(d));
    gtk_container_set_border_width(GTK_CONTAINER(ca), 10);
    gtk_box_set_spacing(GTK_BOX(ca), 8);

    /* file attributes */
    GtkWidget *fg = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(fg), 16);
    gtk_grid_set_row_spacing(GTK_GRID(fg), 3);
    char saved[64] = "Not Available", ondisk[64] = "Not Available", res[64];
    struct stat stbuf;
    if (app.filename && stat(app.filename, &stbuf) == 0) {
        strftime(saved, sizeof saved, "%x %X", localtime(&stbuf.st_mtime));
        gchar *s = g_format_size_full(stbuf.st_size, G_FORMAT_SIZE_IEC_UNITS);
        g_strlcpy(ondisk, s, sizeof ondisk);
        g_free(s);
    }
    g_snprintf(res, sizeof res, "%d DPI", (int)lround(app.dpi_x));
    const char *labels[] = { "Last Saved:", "Size on disk:", "Resolution:" };
    const char *vals[] = { saved, ondisk, res };
    for (int i = 0; i < 3; i++) {
        GtkWidget *l = gtk_label_new(labels[i]);
        gtk_widget_set_halign(l, GTK_ALIGN_START);
        GtkWidget *v = gtk_label_new(vals[i]);
        gtk_widget_set_halign(v, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(fg), l, 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(fg), v, 1, i, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(ca), frame_box("File attributes", fg), FALSE, FALSE, 0);

    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *ub = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    a.in = gtk_radio_button_new_with_mnemonic(NULL, "_Inches");
    a.cm = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(a.in), "C_entimeters");
    a.px = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(a.in), "_Pixels");
    gtk_box_pack_start(GTK_BOX(ub), a.in, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ub), a.cm, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ub), a.px, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), frame_box("Units", ub), TRUE, TRUE, 0);
    GtkWidget *cb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    a.bw = gtk_radio_button_new_with_mnemonic(NULL, "_Black and white");
    a.col = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(a.bw), "Co_lor");
    gtk_box_pack_start(GTK_BOX(cb), a.bw, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cb), a.col, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), frame_box("Colors", cb), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(ca), row, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.monochrome ? a.bw : a.col), TRUE);

    GtkWidget *sg = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(sg), 8);
    a.w = gtk_spin_button_new_with_range(1, MAX_IMAGE_DIM, 1);
    a.h = gtk_spin_button_new_with_range(1, MAX_IMAGE_DIM, 1);
    gtk_entry_set_activates_default(GTK_ENTRY(a.w), TRUE);
    gtk_entry_set_activates_default(GTK_ENTRY(a.h), TRUE);
    gtk_grid_attach(GTK_GRID(sg), gtk_label_new_with_mnemonic("_Width:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(sg), a.w, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(sg), gtk_label_new_with_mnemonic("_Height:"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(sg), a.h, 3, 0, 1, 1);
    gtk_box_pack_start(GTK_BOX(ca), sg, FALSE, FALSE, 4);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(a.unit == 0 ? a.in : a.unit == 1 ? a.cm : a.px), TRUE);
    attr_show(&a);
    g_signal_connect(a.in, "toggled", G_CALLBACK(attr_unit), &a);
    g_signal_connect(a.cm, "toggled", G_CALLBACK(attr_unit), &a);
    g_signal_connect(a.px, "toggled", G_CALLBACK(attr_unit), &a);
    gtk_widget_show_all(d);

    for (;;) {
        int r = gtk_dialog_run(GTK_DIALOG(d));
        if (r == 2) {      /* CImageAttr::OnDefault */
            GdkRectangle wa;
            gdk_monitor_get_workarea(gdk_display_get_primary_monitor(gdk_display_get_default()), &wa);
            a.cur_w = MAX(1, wa.width * 0.6);
            a.cur_h = MAX(1, wa.height * 0.6);
            attr_show(&a);
            continue;
        }
        if (r == GTK_RESPONSE_OK) {
            gtk_spin_button_update(GTK_SPIN_BUTTON(a.w));
            gtk_spin_button_update(GTK_SPIN_BUTTON(a.h));
            int nw = (int)lround(unit_to_px(&a, gtk_spin_button_get_value(GTK_SPIN_BUTTON(a.w)), app.dpi_x));
            int nh = (int)lround(unit_to_px(&a, gtk_spin_button_get_value(GTK_SPIN_BUTTON(a.h)), app.dpi_y));
            nw = CLAMP(nw, 1, MAX_IMAGE_DIM);
            nh = CLAMP(nh, 1, MAX_IMAGE_DIM);
            bool to_bw = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(a.bw)) && !app.monochrome;
            if (to_bw) {
                GtkWidget *q = gtk_message_dialog_new(GTK_WINDOW(d), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
                                                      GTK_BUTTONS_OK_CANCEL,
                                                      "Converting to black and white is permanent and cannot be undone. Continue?");
                int qr = gtk_dialog_run(GTK_DIALOG(q));
                gtk_widget_destroy(q);
                if (qr != GTK_RESPONSE_OK) continue;
            }
            if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(a.bw))) app.monochrome = false;
            saved_unit = a.unit;
            gtk_widget_destroy(d);
            image_set_size(nw, nh);
            if (to_bw) image_to_monochrome();
            update_status();
            return;
        }
        break;
    }
    gtk_widget_destroy(d);
}

/* ------------------------------------------------------------------ */
/* Resize and Skew                                                     */

typedef struct {
    GtkWidget *pct, *pix, *hz, *vt, *keep, *skh, *skv;
    int bw, bh;
    bool busy;
} RsDlg;

static void rs_mode(GtkToggleButton *t, RsDlg *r)
{
    if (!gtk_toggle_button_get_active(t)) return;
    bool pct = GTK_WIDGET(t) == r->pct;
    double h = gtk_spin_button_get_value(GTK_SPIN_BUTTON(r->hz)), v = gtk_spin_button_get_value(GTK_SPIN_BUTTON(r->vt));
    r->busy = true;
    if (pct) {   /* pixels -> percent */
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(r->hz), 1, 500);
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(r->vt), 1, 500);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(r->hz), round(h * 100.0 / r->bw));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(r->vt), round(v * 100.0 / r->bh));
    } else {
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(r->hz), 1, MAX_IMAGE_DIM);
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(r->vt), 1, MAX_IMAGE_DIM);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(r->hz), round(h * r->bw / 100.0));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(r->vt), round(v * r->bh / 100.0));
    }
    r->busy = false;
}

/* CStretchSkewDlg::OnChangeStretchHorz / Vert with "Maintain aspect ratio" */
static void rs_changed(GtkSpinButton *s, RsDlg *r)
{
    if (r->busy || !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(r->keep))) return;
    bool pct = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(r->pct));
    /* use the typed text so the partner updates while typing (CStretchSkewDlg::OnChangeStretch*) */
    double v = g_ascii_strtod(gtk_entry_get_text(GTK_ENTRY(s)), NULL);
    if (v <= 0) return;
    r->busy = true;
    if (GTK_WIDGET(s) == r->hz)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(r->vt), pct ? v : round(v * r->bh / r->bw));
    else
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(r->hz), pct ? v : round(v * r->bw / r->bh));
    r->busy = false;
}

void cmd_resize(void)
{
    if (!app.sel.active) tool_commit_pending();
    RsDlg r = { 0 };
    r.bw = app.sel.active ? app.sel.w : app.w;
    r.bh = app.sel.active ? app.sel.h : app.h;
    GtkWidget *d = gtk_dialog_new_with_buttons("Resize and Skew", GTK_WINDOW(app.window), GTK_DIALOG_MODAL,
                                               "_Cancel", GTK_RESPONSE_CANCEL, "_OK", GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
    GtkWidget *ca = gtk_dialog_get_content_area(GTK_DIALOG(d));
    gtk_container_set_border_width(GTK_CONTAINER(ca), 10);
    gtk_box_set_spacing(GTK_BOX(ca), 8);

    GtkWidget *g = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(g), 8);
    gtk_grid_set_row_spacing(GTK_GRID(g), 4);
    r.pct = gtk_radio_button_new_with_mnemonic(NULL, "P_ercentage");
    r.pix = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(r.pct), "Pi_xels");
    GtkWidget *by = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(by), gtk_label_new("By:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(by), r.pct, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(by), r.pix, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(g), by, 0, 0, 2, 1);
    r.hz = gtk_spin_button_new_with_range(1, 500, 1);
    r.vt = gtk_spin_button_new_with_range(1, 500, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(r.hz), 100);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(r.vt), 100);
    gtk_entry_set_activates_default(GTK_ENTRY(r.hz), TRUE);
    gtk_entry_set_activates_default(GTK_ENTRY(r.vt), TRUE);
    GtkWidget *l1 = gtk_label_new_with_mnemonic("_Horizontal:"), *l2 = gtk_label_new_with_mnemonic("_Vertical:");
    gtk_widget_set_halign(l1, GTK_ALIGN_START);
    gtk_widget_set_halign(l2, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(g), l1, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(g), r.hz, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(g), l2, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(g), r.vt, 1, 2, 1, 1);
    r.keep = gtk_check_button_new_with_mnemonic("_Maintain aspect ratio");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(r.keep), TRUE);
    gtk_grid_attach(GTK_GRID(g), r.keep, 0, 3, 2, 1);
    gtk_box_pack_start(GTK_BOX(ca), frame_box("Resize", g), FALSE, FALSE, 0);

    GtkWidget *g2 = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(g2), 8);
    gtk_grid_set_row_spacing(GTK_GRID(g2), 4);
    r.skh = gtk_spin_button_new_with_range(-89, 89, 1);
    r.skv = gtk_spin_button_new_with_range(-89, 89, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(r.skh), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(r.skv), 0);
    gtk_entry_set_activates_default(GTK_ENTRY(r.skh), TRUE);
    gtk_entry_set_activates_default(GTK_ENTRY(r.skv), TRUE);
    GtkWidget *l3 = gtk_label_new_with_mnemonic("H_orizontal:"), *l4 = gtk_label_new_with_mnemonic("V_ertical:");
    gtk_widget_set_halign(l3, GTK_ALIGN_START);
    gtk_widget_set_halign(l4, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(g2), l3, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g2), r.skh, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g2), l4, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(g2), r.skv, 1, 1, 1, 1);
    gtk_box_pack_start(GTK_BOX(ca), frame_box("Skew (Degrees)", g2), FALSE, FALSE, 0);

    g_signal_connect(r.pct, "toggled", G_CALLBACK(rs_mode), &r);
    g_signal_connect(r.pix, "toggled", G_CALLBACK(rs_mode), &r);
    g_signal_connect(r.hz, "value-changed", G_CALLBACK(rs_changed), &r);
    g_signal_connect(r.vt, "value-changed", G_CALLBACK(rs_changed), &r);
    g_signal_connect(r.hz, "changed", G_CALLBACK(rs_changed), &r);
    g_signal_connect(r.vt, "changed", G_CALLBACK(rs_changed), &r);
    gtk_widget_show_all(d);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) {
        gtk_spin_button_update(GTK_SPIN_BUTTON(r.hz));
        gtk_spin_button_update(GTK_SPIN_BUTTON(r.vt));
        gtk_spin_button_update(GTK_SPIN_BUTTON(r.skh));
        gtk_spin_button_update(GTK_SPIN_BUTTON(r.skv));
        double h = gtk_spin_button_get_value(GTK_SPIN_BUTTON(r.hz)), v = gtk_spin_button_get_value(GTK_SPIN_BUTTON(r.vt));
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(r.pix))) {
            h = h * 100.0 / r.bw;
            v = v * 100.0 / r.bh;
        }
        double sh = gtk_spin_button_get_value(GTK_SPIN_BUTTON(r.skh)), sv = gtk_spin_button_get_value(GTK_SPIN_BUTTON(r.skv));
        gtk_widget_destroy(d);
        if (h != 100 || v != 100 || sh != 0 || sv != 0) image_resize_skew(h, v, sh, sv);
        return;
    }
    gtk_widget_destroy(d);
}

/* ------------------------------------------------------------------ */

void cmd_edit_colors(void)
{
    GtkWidget *d = gtk_color_chooser_dialog_new("Edit Colors", GTK_WINDOW(app.window));
    RGB c = app.active_color == 2 ? app.color2 : app.color1;
    GdkRGBA rgba = { c.r, c.g, c.b, 1 };
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(d), FALSE);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(d), &rgba);
    g_object_set(d, "show-editor", TRUE, NULL);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) {
        gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(d), &rgba);
        RGB n = { rgba.red, rgba.green, rgba.blue };
        set_color(app.active_color, n);
        add_custom_color(n);
    }
    gtk_widget_destroy(d);
}

void cmd_about(void)
{
    const char *authors[] = { "Thingybob8055", NULL };
    gtk_show_about_dialog(GTK_WINDOW(app.window),
                          "program-name", APP_NAME,
                          "version", "6.1-linux",
                          "comments", "A Linux re-implementation of Windows 7 Paint,\nbuilt by studying the original mspaint.exe.",
                          "authors", authors,
                          "logo-icon-name", "applications-graphics",
                          NULL);
}
