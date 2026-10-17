/* linpaint - a Linux re-implementation of Windows 7 Paint (mspaint.exe 6.1).
 *
 * Behaviour, constants and feature set were recovered from the original
 * binary (PE32+ mspaint.exe, public PDB 811B1B09BA4247838A859E3C334EA8832);
 * class names from that binary are noted next to the code that mirrors them.
 */
#ifndef PAINT_H
#define PAINT_H

#include <gtk/gtk.h>
#include <cairo.h>
#include <stdint.h>
#include <stdbool.h>

#define APP_NAME "Paint"

/* ---- limits recovered from the binary ---- */
#define MAX_IMAGE_DIM   0xFFFF          /* CImageAttr::OnOK upper bound */
#define UNDO_LEVELS     50              /* CUndoBmObj ring size */
#define N_CUSTOM_COLORS 10

/* ---- tools (CImgTool / CPaintTool subclasses) ---- */
typedef enum {
    TOOL_SELECT_RECT, TOOL_SELECT_FREE,
    TOOL_PENCIL, TOOL_FILL, TOOL_TEXT, TOOL_ERASER, TOOL_PICKER, TOOL_ZOOM,
    TOOL_BRUSH,     /* uses app.brush_kind */
    TOOL_SHAPE,     /* uses app.shape_kind */
    TOOL_COUNT
} ToolId;

/* Brushes gallery order (CPaintToolManager::OnChange*Thickness) */
typedef enum {
    BRUSH_REGULAR, BRUSH_CALLIG1, BRUSH_CALLIG2, BRUSH_AIRBRUSH, BRUSH_OIL,
    BRUSH_CRAYON, BRUSH_MARKER, BRUSH_NATPENCIL, BRUSH_WATERCOLOR,
    BRUSH_COUNT
} BrushKind;

/* Shapes gallery order (CShapeStore::CreateShapes) */
typedef enum {
    SHAPE_LINE, SHAPE_CURVE, SHAPE_OVAL, SHAPE_RECT, SHAPE_ROUNDRECT,
    SHAPE_POLYGON, SHAPE_TRIANGLE, SHAPE_RTRIANGLE, SHAPE_DIAMOND,
    SHAPE_PENTAGON, SHAPE_HEXAGON, SHAPE_RARROW, SHAPE_LARROW, SHAPE_UARROW,
    SHAPE_DARROW, SHAPE_STAR4, SHAPE_STAR5, SHAPE_STAR6, SHAPE_RRCALLOUT,
    SHAPE_OVALCALLOUT, SHAPE_CLOUDCALLOUT, SHAPE_HEART, SHAPE_LIGHTNING,
    SHAPE_COUNT
} ShapeKind;

/* Outline / Fill media (CShapeModesGallery) */
typedef enum {
    MEDIA_NONE, MEDIA_SOLID, MEDIA_CRAYON, MEDIA_MARKER, MEDIA_OIL,
    MEDIA_NATPENCIL, MEDIA_WATERCOLOR, MEDIA_COUNT
} MediaKind;

typedef struct { double r, g, b; } RGB;

/* appearance (theme.c) */
typedef enum { THEME_AUTO, THEME_LIGHT, THEME_DARK } ThemeMode;
typedef struct {
    bool dark;
    RGB workspace, shadow, ruler_bg, ruler_line, ruler_text;
    RGB cell_border, cell_empty, cell_empty_border, highlight, fg, accent;
    double shadow_alpha;
} UiPalette;
extern UiPalette ui;

/* Floating selection (CImgWnd selection / CImgBrush) */
typedef struct {
    bool active;          /* a selection exists */
    bool floating;        /* pixels lifted from canvas into `pix` */
    int x, y, w, h;       /* position/size on canvas (w,h may differ from pix when stretched) */
    cairo_surface_t *pix; /* lifted pixels, ARGB32 */
    cairo_surface_t *mask;/* A8 mask for free-form selections, NULL = rect */
    bool from_paste;      /* came from clipboard/file -> no hole left behind */
    bool hole_made;       /* original area already erased */
} Selection;

typedef struct UndoStep UndoStep;

typedef struct {
    /* document (CPBDoc / CBitmapObj) */
    cairo_surface_t *img;
    int w, h;
    char *filename;
    bool dirty;
    double dpi_x, dpi_y;
    bool monochrome;

    /* tool state */
    ToolId tool, prev_tool;
    BrushKind brush_kind;
    ShapeKind shape_kind;
    MediaKind outline, fill;
    int size_px[TOOL_COUNT];           /* current size (px) of each tool */
    int brush_size_px[BRUSH_COUNT];
    RGB color1, color2;
    int active_color;                  /* 1 or 2: which colour box is selected */
    RGB custom[N_CUSTOM_COLORS];
    int n_custom;
    bool transparent_sel;
    bool antialias;

    /* text attributes */
    char font_family[128];
    int font_size;
    bool bold, italic, underline, strike, text_opaque;

    /* view */
    double zoom;
    bool show_rulers, show_grid, show_status;
    int theme_mode;                    /* ThemeMode */

    Selection sel;

    /* undo */
    UndoStep *undo[UNDO_LEVELS + 1];
    int n_undo, n_redo;
    UndoStep *redo[UNDO_LEVELS + 1];
    cairo_surface_t *undo_pending;     /* snapshot taken at op start */

    /* widgets */
    GtkWidget *window, *canvas, *scroll, *hruler, *vruler, *ruler_corner;
    GtkWidget *statusbar, *st_pos, *st_sel, *st_size, *st_file, *zoom_label, *zoom_scale;
    GtkWidget *ribbon, *ribbon_notebook, *text_tab;
    GtkWidget *tool_btn[TOOL_COUNT];
    GtkWidget *brush_btn, *shape_grid, *size_btn, *outline_btn, *fill_btn;
    GtkWidget *color1_btn, *color2_btn, *palette_btns[30];
    GtkWidget *font_combo, *font_size_combo;
    GtkWidget *bold_btn, *italic_btn, *underline_btn, *strike_btn, *opaque_btn, *transp_btn;
    GtkWidget *thumb_win, *thumb_area;
    GtkWidget *chk_rulers, *chk_grid, *chk_status, *transp_check;
    GtkWidget *theme_radio[3];
    bool updating_ui;
    int mouse_x, mouse_y;              /* image coords, or -1 */
} App;

extern App app;

/* ---- undo (image.c) ---- */
void undo_begin(void);                 /* snapshot whole image */
void undo_commit(void);                /* store diff since undo_begin */
void undo_cancel(void);
void undo_push_full(cairo_surface_t *before);   /* for size-changing ops */
void do_undo(void);
void do_redo(void);
void undo_clear(void);

/* ---- image ops (image.c) ---- */
cairo_surface_t *surface_new(int w, int h, RGB fill);
cairo_surface_t *surface_copy(cairo_surface_t *s);
void set_image(cairo_surface_t *s);     /* takes ownership */
void image_rotate(int quarter_turns);   /* 1=90 right, 2=180, 3=90 left */
void image_flip(bool horizontal);
void image_invert(void);
void image_crop(void);
void image_resize_skew(double sx_pct, double sy_pct, double skew_h_deg, double skew_v_deg);
void image_set_size(int w, int h);
void image_to_monochrome(void);
cairo_surface_t *surface_rotate(cairo_surface_t *s, int quarter);
cairo_surface_t *surface_flip(cairo_surface_t *s, bool horizontal);
void surface_invert(cairo_surface_t *s);
cairo_surface_t *surface_stretch_skew(cairo_surface_t *s, int nw, int nh, double skh, double skv,
                                      RGB bg, bool transparent_bg);
uint32_t rgb_to_px(RGB c);
RGB px_to_rgb(uint32_t p);

/* ---- selection (select.c) ---- */
void sel_clear(bool commit);
void sel_commit(void);
void sel_select_all(void);
void sel_invert(void);
void sel_delete(void);
void sel_lift(void);
void sel_from_surface(cairo_surface_t *s, int x, int y);
cairo_surface_t *sel_get_pixels(void);   /* new ref: current selection or whole image */
void sel_draw(cairo_t *cr);
bool sel_hit(double ix, double iy);
void sel_rect_in_canvas(double *x, double *y, double *w, double *h);
void sel_apply_transform(int what);      /* see SEL_OP_* */
enum { SEL_OP_ROT_R, SEL_OP_ROT_L, SEL_OP_ROT_180, SEL_OP_FLIP_H, SEL_OP_FLIP_V, SEL_OP_INVERT };

/* ---- canvas (canvas.c) ---- */
GtkWidget *canvas_create(void);
void canvas_update_size(void);
void canvas_redraw(void);
void canvas_redraw_rect(double x, double y, double w, double h);
void canvas_set_zoom(double z, double anchor_ix, double anchor_iy);
void canvas_zoom_step(int dir);
void canvas_image_to_widget(double ix, double iy, double *wx, double *wy);
void canvas_widget_to_image(double wx, double wy, double *ix, double *iy);
void canvas_update_cursor(void);
extern const double zoom_presets[];
extern const int n_zoom_presets;

/* ---- tools (tools.c) ---- */
void tool_select(ToolId t);
void tool_press(double ix, double iy, int button, guint state, int n_press);
void tool_motion(double ix, double iy, guint state);
void tool_release(double ix, double iy, int button, guint state);
bool tool_key(GdkEventKey *ev);
void tool_cancel(void);
void tool_commit_pending(void);          /* commit live shape / text / selection */
void tool_draw_overlay(cairo_t *cr);     /* in widget space */
int  tool_size_count(void);
int  tool_size_value(int idx);
int  tool_cur_size(void);
void tool_size_step(int dir);
void tool_set_size(int px);
bool tool_has_size(void);
bool text_active(void);
void tool_init(void);
void show_context_menu(GdkEventButton *ev);
const int *tool_size_list(int *n);
void tool_draw_cursor_preview(cairo_t *cr);
void text_apply_attrs(void);
void shape_refresh(void);
bool tool_transform_live(int op);

/* ---- brushes (brush.c) ---- */
void brush_init(void);
void brush_stroke_begin(BrushKind k, RGB c, int size, double x, double y);
void brush_stroke_to(double x, double y, double pressure);
void brush_stroke_end(void);
void brush_airbrush_tick(void);
void media_stroke_path(cairo_t *cr_img, MediaKind m, RGB c, int size, cairo_path_t *flat, bool closed);
void media_fill_path(cairo_t *cr_img, MediaKind m, RGB c, cairo_path_t *path);
void brush_preview_icon(cairo_t *cr, BrushKind k, int w, int h);
cairo_surface_t *media_texture(MediaKind m);

/* ---- shapes (shapes.c) ---- */
void shape_build_path(cairo_t *cr, ShapeKind k, double x0, double y0, double x1, double y1);
const char *shape_name(ShapeKind k);
const char *brush_name(BrushKind k);
const char *media_name(MediaKind m);

/* ---- file io (fileio.c) ---- */
bool file_open(const char *path, GError **err);
bool file_save(const char *path, const char *fmt, GError **err);
cairo_surface_t *load_image_surface(const char *path, GError **err);
const char *format_from_path(const char *path);
bool save_gif(cairo_surface_t *s, const char *path, GError **err);
void do_print(bool preview);
void do_page_setup(void);
void set_wallpaper(const char *mode);

/* ---- UI / commands (main.c, dialogs.c) ---- */
void update_title(void);
void update_status(void);
void update_ui(void);
void update_color_ui(void);
void mark_dirty(void);
void show_text_tab(bool show);
bool confirm_discard(void);
void cmd_new(void);
void cmd_open(void);
bool cmd_save(void);
bool cmd_save_as(const char *fmt);
void cmd_properties(void);
void cmd_resize(void);
void cmd_edit_colors(void);
void cmd_about(void);
void cmd_fullscreen(void);
void cmd_thumbnail(bool show);
void cmd_cut(void);
void cmd_copy(void);
void cmd_paste(void);
void cmd_paste_from(void);
void cmd_email(void);
void cmd_scan(void);
void cmd_copy_to(void);
void thumb_refresh(void);
void rulers_redraw(void);
void set_color(int which, RGB c);
void add_custom_color(RGB c);
void error_dialog(const char *fmt, ...);

void selftest_schedule(void);

/* ---- appearance (theme.c) ---- */
void theme_init(void);
void theme_apply(void);
void theme_set_mode(int mode);

/* ---- icons (icons.c) ---- */
GtkWidget *icon_widget(const char *name, int size);
void draw_icon(cairo_t *cr, const char *name, double size);
void draw_icon_mode(cairo_t *cr, const char *name, double size, bool dark);

#endif
