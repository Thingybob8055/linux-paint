/* Shape geometry.
 *
 * All template shapes are expressed, exactly as in mspaint's CShapeStore,
 * as a list of primitives (line / cubic bezier / elliptical arc / ellipse /
 * rounded rect) in a -50..50 box, then mapped onto the dragged rectangle by
 * CDrawShapeOperation::CreateWorldTransformMatrix (scale -> rotate -> flip ->
 * translate).  The tables below were captured by emulating
 * CShapeStore::Create* from the original binary.
 */
#include "paint.h"
#include <math.h>

typedef enum { P_LINE, P_BEZ, P_ARC, P_ELLIPSE, P_RRECT } PrimType;
typedef struct { PrimType t; double v[8]; } Prim;

#define L(a, b, c, d) { P_LINE, { a, b, c, d } }
#define BZ(a, b, c, d, e, f, g, h) { P_BEZ, { a, b, c, d, e, f, g, h } }
#define ARC(l, t, r, b, s, w) { P_ARC, { l, t, r, b, s, w } }

static const Prim p_rect[] = { L(-50,-50,50,-50), L(50,-50,50,50), L(50,50,-50,50), L(-50,50,-50,-50) };
static const Prim p_oval[] = { { P_ELLIPSE, { -50, -50, 50, 50 } } };
static const Prim p_rrect[] = { { P_RRECT, { -50, -50, 50, 50 } } };
static const Prim p_tri[] = { L(0,-50,50,50), L(50,50,-50,50), L(-50,50,0,-50) };
static const Prim p_rtri[] = { L(-50,-50,50,50), L(50,50,-50,50), L(-50,50,-50,-50) };
static const Prim p_diamond[] = { L(0,-50,50,0), L(50,0,0,50), L(0,50,-50,0), L(-50,0,0,-50) };
static const Prim p_pent[] = { L(0,-50,-50,-12), L(-50,-12,-31,50), L(-31,50,31,50), L(31,50,50,-12), L(50,-12,0,-50) };
static const Prim p_hex[] = { L(0,-50,-50,-25), L(-50,-25,-50,25), L(-50,25,0,50), L(0,50,50,25), L(50,25,50,-25), L(50,-25,0,-50) };
static const Prim p_rarrow[] = { L(0,-50,0,-25), L(0,-25,-50,-25), L(-50,-25,-50,25), L(-50,25,0,25), L(0,25,0,50), L(0,50,50,0), L(50,0,0,-50) };
static const Prim p_larrow[] = { L(-50,0,0,50), L(0,50,0,25), L(0,25,50,25), L(50,25,50,-25), L(50,-25,0,-25), L(0,-25,0,-50), L(0,-50,-50,0) };
static const Prim p_uarrow[] = { L(0,-50,-50,0), L(-50,0,-25,0), L(-25,0,-25,50), L(-25,50,25,50), L(25,50,25,0), L(25,0,50,0), L(50,0,0,-50) };
static const Prim p_darrow[] = { L(-50,0,0,50), L(0,50,50,0), L(50,0,25,0), L(25,0,25,-50), L(25,-50,-25,-50), L(-25,-50,-25,0), L(-25,0,-50,0) };
static const Prim p_star4[] = { L(-50,0,-12,-12), L(-12,-12,0,-50), L(0,-50,12,-12), L(12,-12,50,0), L(50,0,12,12), L(12,12,0,50), L(0,50,-12,12), L(-12,12,-50,0) };
static const Prim p_star5[] = { L(0,-50,-12,-12), L(-12,-12,-50,-11), L(-50,-11,-20,12), L(-20,12,-31,50), L(-31,50,0,28),
                                L(0,28,31,50), L(31,50,20,12), L(20,12,50,-11), L(50,-11,12,-12), L(12,-12,0,-50) };
static const Prim p_star6[] = { L(0,-50,18,-25), L(18,-25,50,-25), L(50,-25,34,0), L(34,0,50,25), L(50,25,18,25), L(18,25,0,50),
                                L(0,50,-18,25), L(-18,25,-50,25), L(-50,25,-34,0), L(-34,0,-50,-25), L(-50,-25,-18,-25), L(-18,-25,0,-50) };
static const Prim p_rrcallout[] = { L(-34,-50,34,-50), ARC(18,-50,50,-18,270,90), L(50,-34,50,22), ARC(18,4,50,38,0,90),
                                    L(34,38,-10,38), L(-10,38,-22,50), L(-22,50,-34,38), ARC(-50,4,-18,38,90,90),
                                    L(-50,22,-50,-34), ARC(-50,-50,-18,-18,180,90) };
static const Prim p_ovalcallout[] = { ARC(-50,-50,50,38,127,334), L(-10,37,-28,50), L(-28,50,-30,29) };
static const Prim p_cloud[] = {
    ARC(-39,0,-7,31,103,54), ARC(-47,-3,-27,21,97,-30), ARC(-47,-3,-27,21,67,75), ARC(-47,-3,-27,21,142,75),
    ARC(-50,-21,-30,2,110,-20), ARC(-50,-21,-30,2,90,90), ARC(-50,-21,-30,2,180,89), ARC(-40,-42,-10,-5,162,145),
    ARC(-40,-42,-10,-5,307,-9), ARC(-19,-47,6,-14,213,95), ARC(-1,-50,22,-17,220,-5), ARC(-1,-50,22,-17,215,99),
    ARC(15,-50,39,-17,229,-7), ARC(15,-50,39,-17,222,122), ARC(19,-40,47,-10,292,117), ARC(19,-40,47,-10,49,-15),
    ARC(22,-25,49,9,320,123), ARC(10,-6,36,25,351,-60), ARC(10,-6,36,25,291,100), ARC(10,-6,36,25,31,85),
    ARC(-14,2,17,36,0,160), ARC(-14,2,17,36,160,-10), ARC(-39,0,-7,31,51,52), ARC(-36,30,-23,42,280,100),
    ARC(-36,30,-23,42,30,100), ARC(-39,40,-32,47,320,95), ARC(-39,40,-32,47,55,95), ARC(-43,45,-37,50,300,95),
    ARC(-43,45,-37,50,35,95), ARC(-43,45,-37,50,130,85), ARC(-43,45,-37,50,215,85), ARC(-39,40,-32,47,150,85),
    ARC(-39,40,-32,47,235,85), ARC(-36,30,-23,42,120,80), ARC(-36,30,-23,42,200,80) };
static const Prim p_heart[] = { BZ(-45,2,-65,-48,-17,-71,0,-24), BZ(0,-24,17,-71,65,-48,45,2),
                                BZ(45,2,32,35,0,50,0,50), BZ(0,50,0,50,-32,35,-45,2) };
static const Prim p_lightning[] = { L(-11,-50,9,-22), L(9,-22,2,-18), L(2,-18,26,5), L(26,5,18,9), L(18,9,50,50), L(50,50,-3,19),
                                    L(-3,19,6,14), L(6,14,-26,-5), L(-26,-5,-15,-11), L(-15,-11,-50,-31), L(-50,-31,-11,-50) };

#define N(a) (int)(sizeof(a) / sizeof(a[0]))
static const struct { const Prim *p; int n; } templates[SHAPE_COUNT] = {
    [SHAPE_OVAL] = { p_oval, N(p_oval) },
    [SHAPE_RECT] = { p_rect, N(p_rect) },
    [SHAPE_ROUNDRECT] = { p_rrect, N(p_rrect) },
    [SHAPE_TRIANGLE] = { p_tri, N(p_tri) },
    [SHAPE_RTRIANGLE] = { p_rtri, N(p_rtri) },
    [SHAPE_DIAMOND] = { p_diamond, N(p_diamond) },
    [SHAPE_PENTAGON] = { p_pent, N(p_pent) },
    [SHAPE_HEXAGON] = { p_hex, N(p_hex) },
    [SHAPE_RARROW] = { p_rarrow, N(p_rarrow) },
    [SHAPE_LARROW] = { p_larrow, N(p_larrow) },
    [SHAPE_UARROW] = { p_uarrow, N(p_uarrow) },
    [SHAPE_DARROW] = { p_darrow, N(p_darrow) },
    [SHAPE_STAR4] = { p_star4, N(p_star4) },
    [SHAPE_STAR5] = { p_star5, N(p_star5) },
    [SHAPE_STAR6] = { p_star6, N(p_star6) },
    [SHAPE_RRCALLOUT] = { p_rrcallout, N(p_rrcallout) },
    [SHAPE_OVALCALLOUT] = { p_ovalcallout, N(p_ovalcallout) },
    [SHAPE_CLOUDCALLOUT] = { p_cloud, N(p_cloud) },
    [SHAPE_HEART] = { p_heart, N(p_heart) },
    [SHAPE_LIGHTNING] = { p_lightning, N(p_lightning) },
};

static void seg_start(cairo_t *cr, double x, double y)
{
    if (cairo_has_current_point(cr)) cairo_line_to(cr, x, y);
    else cairo_move_to(cr, x, y);
}

/* Build the path for a template shape in the rectangle (x0,y0)-(x1,y1).
 * Dragging "backwards" flips the shape, as with the original's flip flags. */
void shape_build_path(cairo_t *cr, ShapeKind k, double x0, double y0, double x1, double y1)
{
    if (k == SHAPE_LINE || k == SHAPE_CURVE || k == SHAPE_POLYGON || !templates[k].p) return;
    double cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    double sx = (x1 - x0) / 100.0, sy = (y1 - y0) / 100.0;   /* negative = flipped */
    if (fabs(sx) < 1e-6) sx = sx < 0 ? -1e-6 : 1e-6;
    if (fabs(sy) < 1e-6) sy = sy < 0 ? -1e-6 : 1e-6;

    /* CPrimitiveRoundedRect: corner radius is min(16px, w/2, h/2) in device units */
    if (k == SHAPE_ROUNDRECT) {
        double l = MIN(x0, x1), t = MIN(y0, y1), w = fabs(x1 - x0), h = fabs(y1 - y0);
        double r = MIN(16, MIN(w / 2, h / 2));
        cairo_new_sub_path(cr);
        cairo_arc(cr, l + w - r, t + r, r, -M_PI / 2, 0);
        cairo_arc(cr, l + w - r, t + h - r, r, 0, M_PI / 2);
        cairo_arc(cr, l + r, t + h - r, r, M_PI / 2, M_PI);
        cairo_arc(cr, l + r, t + r, r, M_PI, 3 * M_PI / 2);
        cairo_close_path(cr);
        return;
    }

    cairo_matrix_t saved;
    cairo_get_matrix(cr, &saved);
    cairo_new_sub_path(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, sx, sy);
    for (int i = 0; i < templates[k].n; i++) {
        const Prim *p = &templates[k].p[i];
        const double *v = p->v;
        switch (p->t) {
        case P_LINE:
            seg_start(cr, v[0], v[1]);
            cairo_line_to(cr, v[2], v[3]);
            break;
        case P_BEZ:
            seg_start(cr, v[0], v[1]);
            cairo_curve_to(cr, v[2], v[3], v[4], v[5], v[6], v[7]);
            break;
        case P_ARC: {
            double ax = (v[0] + v[2]) / 2, ay = (v[1] + v[3]) / 2;
            double rx = (v[2] - v[0]) / 2, ry = (v[3] - v[1]) / 2;
            double a0 = v[4] * M_PI / 180, a1 = (v[4] + v[5]) * M_PI / 180;
            cairo_save(cr);
            cairo_translate(cr, ax, ay);
            cairo_scale(cr, rx, ry);
            if (v[5] >= 0) cairo_arc(cr, 0, 0, 1, a0, a1);
            else cairo_arc_negative(cr, 0, 0, 1, a0, a1);
            cairo_restore(cr);
            break;
        }
        case P_ELLIPSE:
            cairo_save(cr);
            cairo_scale(cr, 50, 50);
            cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
            cairo_restore(cr);
            break;
        case P_RRECT:
            break;
        }
    }
    cairo_close_path(cr);
    cairo_set_matrix(cr, &saved);
}

const char *shape_name(ShapeKind k)
{
    static const char *n[SHAPE_COUNT] = {
        "Line", "Curve", "Oval", "Rectangle", "Rounded rectangle", "Polygon", "Triangle",
        "Right triangle", "Diamond", "Pentagon", "Hexagon", "Right arrow", "Left arrow",
        "Up arrow", "Down arrow", "Four-point star", "Five-point star", "Six-point star",
        "Rounded rectangular callout", "Oval callout", "Cloud callout", "Heart", "Lightning"
    };
    return n[k];
}

const char *brush_name(BrushKind k)
{
    static const char *n[BRUSH_COUNT] = {
        "Brush", "Calligraphy brush 1", "Calligraphy brush 2", "Airbrush", "Oil brush",
        "Crayon", "Marker", "Natural pencil", "Watercolor brush"
    };
    return n[k];
}

const char *media_name(MediaKind m)
{
    static const char *n[MEDIA_COUNT] = {
        "No outline", "Solid color", "Crayon", "Marker", "Oil", "Natural pencil", "Watercolor"
    };
    return n[m];
}
