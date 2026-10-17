/* Light / dark appearance.
 *
 * Windows 7 Paint has no dark mode; this is a Linux addition.  The mode is
 * Auto (follow the desktop), Light or Dark.  Forced modes switch GTK to
 * Adwaita / Adwaita-dark so every stock widget matches; Auto keeps the
 * user's GTK theme and, on GNOME, honours org.gnome.desktop.interface
 * color-scheme.  Whether the result is dark is decided from the theme's
 * actual foreground colour, so themes like Breeze-Dark are detected too.
 */
#include "paint.h"
#include <string.h>

UiPalette ui;

static GtkCssProvider *provider;
static GSettings *iface;            /* org.gnome.desktop.interface, if installed */
static gchar *sys_theme;            /* theme name chosen by the desktop */
static gboolean sys_prefer_dark;
static bool applying;

static const char *css_common =
    "notebook.ribbon > header tab { padding: 2px 14px; }"
    ".group-label { font-size: 0.85em; }"
    "button.file-button { background: #1E5BB8; color: white; border-radius: 3px 3px 0 0; padding: 2px 12px; }"
    "button.file-button label { color: white; }"
    ".shape-btn { padding: 1px; min-height: 0; min-width: 0; }";

static const char *css_light =
    "notebook.ribbon > header { background: #DFE9F5; }"
    "notebook.ribbon > stack { background: linear-gradient(#FFFFFF, #E8EFF8); }"
    ".group-label { color: #3B4F6B; }"
    "button:checked { background: #FCE4A8; }"
    ".paint-status { background: #DCE6F4; border-top: 1px solid #A8B8D0; }";

static const char *css_dark =
    "notebook.ribbon > header { background: #262A30; }"
    "notebook.ribbon > stack { background: linear-gradient(#383D45, #2E3238); }"
    ".group-label { color: #9FB0C8; }"
    "button:checked { background: #5C4B1E; }"
    ".paint-status { background: #262A30; border-top: 1px solid #454B55; }";

static RGB hex(uint32_t v) { return (RGB){ ((v >> 16) & 255) / 255.0, ((v >> 8) & 255) / 255.0, (v & 255) / 255.0 }; }

static void set_palette(bool dark)
{
    ui.dark = dark;
    if (!dark) {
        ui.workspace = hex(0xC9D3E2);     /* Win7 Paint's workspace blue-grey */
        ui.shadow = hex(0x73859E);
        ui.shadow_alpha = 0.35;
        ui.ruler_bg = hex(0xF5F7FC);
        ui.ruler_line = hex(0x99A8BF);
        ui.ruler_text = hex(0x40404D);
        ui.cell_border = hex(0x8C99A6);
        ui.cell_empty = hex(0xFAFAFA);
        ui.cell_empty_border = hex(0xCCD1D9);
        ui.highlight = hex(0xFAD980);
        ui.fg = hex(0x1A1A1A);
        ui.accent = hex(0x1F4D99);
    } else {
        ui.workspace = hex(0x1E2228);
        ui.shadow = hex(0x000000);
        ui.shadow_alpha = 0.55;
        ui.ruler_bg = hex(0x2B2F36);
        ui.ruler_line = hex(0x5A6270);
        ui.ruler_text = hex(0xC8CED6);
        ui.cell_border = hex(0x6A7380);
        ui.cell_empty = hex(0x353A42);
        ui.cell_empty_border = hex(0x4A505A);
        ui.highlight = hex(0x7A6225);
        ui.fg = hex(0xE6E9EE);
        ui.accent = hex(0x8CB4F0);
    }
}

/* the theme's own text colour tells us whether it is a dark theme */
static bool theme_is_dark(void)
{
    GtkWidgetPath *path = gtk_widget_path_new();
    gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
    GtkStyleContext *sc = gtk_style_context_new();
    gtk_style_context_set_path(sc, path);
    gtk_style_context_set_screen(sc, gdk_screen_get_default());
    GdkRGBA fg;
    gtk_style_context_get_color(sc, GTK_STATE_FLAG_NORMAL, &fg);
    g_object_unref(sc);
    gtk_widget_path_unref(path);
    return 0.299 * fg.red + 0.587 * fg.green + 0.114 * fg.blue > 0.5;
}

static bool gnome_prefers_dark(void)
{
    if (!iface) return false;
    gchar *cs = g_settings_get_string(iface, "color-scheme");
    bool d = cs && !strcmp(cs, "prefer-dark");
    g_free(cs);
    return d;
}

static void refresh_widgets(void)
{
    if (!app.window) return;
    GdkWindow *w = gtk_widget_get_window(app.window);
    if (w) gdk_window_invalidate_rect(w, NULL, TRUE);
    gtk_widget_queue_draw(app.window);
    canvas_redraw();
    rulers_redraw();
    thumb_refresh();
    update_ui();
}

void theme_apply(void)
{
    GtkSettings *gs = gtk_settings_get_default();
    applying = true;
    switch (app.theme_mode) {
    case THEME_LIGHT:
        g_object_set(gs, "gtk-theme-name", "Adwaita", "gtk-application-prefer-dark-theme", FALSE, NULL);
        break;
    case THEME_DARK:
        g_object_set(gs, "gtk-theme-name", "Adwaita", "gtk-application-prefer-dark-theme", TRUE, NULL);
        break;
    default:
        g_object_set(gs, "gtk-theme-name", sys_theme,
                     "gtk-application-prefer-dark-theme", sys_prefer_dark || gnome_prefers_dark(), NULL);
        break;
    }
    applying = false;
    bool dark = app.theme_mode == THEME_DARK ? true : app.theme_mode == THEME_LIGHT ? false : theme_is_dark();
    set_palette(dark);
    gchar *css = g_strconcat(css_common, dark ? css_dark : css_light, NULL);
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    g_free(css);
    refresh_widgets();
}

void theme_set_mode(int mode)
{
    if (mode == app.theme_mode) return;
    app.theme_mode = mode;
    theme_apply();
}

/* the desktop changed its theme: remember it, then re-apply our mode */
static void system_theme_changed(GObject *o, GParamSpec *ps, gpointer d)
{
    (void)d;
    if (applying) return;
    /* read only the property that changed: the other may hold our forced value */
    if (!strcmp(ps->name, "gtk-theme-name")) {
        g_free(sys_theme);
        g_object_get(o, "gtk-theme-name", &sys_theme, NULL);
        if (!sys_theme) sys_theme = g_strdup("Adwaita");
    } else {
        g_object_get(o, "gtk-application-prefer-dark-theme", &sys_prefer_dark, NULL);
    }
    theme_apply();
}

static void color_scheme_changed(GSettings *s, const char *key, gpointer d)
{
    (void)s; (void)key; (void)d;
    if (app.theme_mode == THEME_AUTO) theme_apply();
}

void theme_init(void)
{
    GtkSettings *gs = gtk_settings_get_default();
    g_object_get(gs, "gtk-theme-name", &sys_theme, "gtk-application-prefer-dark-theme", &sys_prefer_dark, NULL);
    if (!sys_theme) sys_theme = g_strdup("Adwaita");

    GSettingsSchemaSource *src = g_settings_schema_source_get_default();
    GSettingsSchema *sch = src ? g_settings_schema_source_lookup(src, "org.gnome.desktop.interface", TRUE) : NULL;
    if (sch) {
        if (g_settings_schema_has_key(sch, "color-scheme")) {
            iface = g_settings_new("org.gnome.desktop.interface");
            g_signal_connect(iface, "changed::color-scheme", G_CALLBACK(color_scheme_changed), NULL);
        }
        g_settings_schema_unref(sch);
    }
    g_signal_connect(gs, "notify::gtk-theme-name", G_CALLBACK(system_theme_changed), NULL);
    g_signal_connect(gs, "notify::gtk-application-prefer-dark-theme", G_CALLBACK(system_theme_changed), NULL);

    provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    theme_apply();
}
