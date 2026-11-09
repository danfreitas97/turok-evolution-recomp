/*
 * launcher_theme_brand.inc -- a title's own launcher artwork (see
 * theme_brand_load in launcher_theme.h). Included at the end of
 * launcher_theme.c, whose blend() and rrect_distance() it draws with.
 */

/* PNG and JPEG only, from memory: the pictures are compiled in. */
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "stb_image.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

typedef struct { int w, h; unsigned char *rgba; } BrandImage;

static BrandImage g_bg_img, g_logo_img;
static COLORREF   g_motes = RGB(0x5C, 0xFF, 0x8A);

/* What is drawn, at the size it is drawn: the background already fitted and
 * darkened, and the logo fitted, premultiplied. Remade when the size
 * changes, so a frame is a copy and a blend rather than a resample. */
static uint32_t      *g_bg_frame;
static int            g_bg_fw, g_bg_fh;
static unsigned char *g_logo_px;
static int            g_logo_w, g_logo_h;

static double smooth(double e0, double e1, double v)
{
    double t = clamp01((v - e0) / (e1 - e0));
    return t * t * (3.0 - 2.0 * t);
}

/* `im` at (u, v) in its pixels, bilinear, premultiplied by alpha, so a
 * transparent edge does not bleed its colour. out[3] is alpha, 0..1. */
static void sample(const BrandImage *im, double u, double v, double out[4])
{
    int x0, y0, x1, y1, k;
    double fx, fy;
    int xs[4], ys[4];
    double ws[4];

    if (u < 0) u = 0;
    if (v < 0) v = 0;
    if (u > im->w - 1) u = im->w - 1;
    if (v > im->h - 1) v = im->h - 1;
    x0 = (int)u; y0 = (int)v;
    x1 = x0 + 1 < im->w ? x0 + 1 : x0;
    y1 = y0 + 1 < im->h ? y0 + 1 : y0;
    fx = u - x0; fy = v - y0;
    xs[0] = x0; xs[1] = x1; xs[2] = x0; xs[3] = x1;
    ys[0] = y0; ys[1] = y0; ys[2] = y1; ys[3] = y1;
    ws[0] = (1 - fx) * (1 - fy); ws[1] = fx * (1 - fy);
    ws[2] = (1 - fx) * fy;       ws[3] = fx * fy;
    out[0] = out[1] = out[2] = out[3] = 0;
    for (k = 0; k < 4; k++) {
        const unsigned char *p = im->rgba + ((size_t)ys[k] * im->w + xs[k]) * 4;
        double a = p[3] / 255.0 * ws[k];

        out[0] += p[0] * a;
        out[1] += p[1] * a;
        out[2] += p[2] * a;
        out[3] += a;
    }
}

/* `im` scaled by `scale` (output pixels per input pixel) at output pixel
 * (x, y): four samples inside the pixel, so a picture drawn at half its
 * size is averaged rather than skipped. */
static void sample_area(const BrandImage *im, double scale, double x, double y, double out[4])
{
    double one[4];
    int i, c;

    out[0] = out[1] = out[2] = out[3] = 0;
    for (i = 0; i < 4; i++) {
        double ox = (i & 1) ? 0.75 : 0.25, oy = (i & 2) ? 0.75 : 0.25;

        sample(im, (x + ox) / scale - 0.5, (y + oy) / scale - 0.5, one);
        for (c = 0; c < 4; c++)
            out[c] += one[c] * 0.25;
    }
}

static int bg_frame_ready(void)
{
    double scale, ox, oy;
    int x, y;

    if (g_bg_frame && g_bg_fw == theme_w && g_bg_fh == theme_h)
        return 1;
    free(g_bg_frame);
    g_bg_frame = malloc((size_t)theme_w * theme_h * sizeof *g_bg_frame);
    if (!g_bg_frame)
        return 0;
    g_bg_fw = theme_w;
    g_bg_fh = theme_h;

    /* Cover: the picture fills the window and the overflow is cut evenly. */
    scale = (double)theme_w / g_bg_img.w;
    if ((double)theme_h / g_bg_img.h > scale)
        scale = (double)theme_h / g_bg_img.h;
    ox = (g_bg_img.w * scale - theme_w) * 0.5;
    oy = (g_bg_img.h * scale - theme_h) * 0.5;

    for (y = 0; y < theme_h; y++) {
        double yn = (double)y / theme_h;

        for (x = 0; x < theme_w; x++) {
            double xn = (double)x / theme_w, px[4], k;
            int r, g, b;

            sample_area(&g_bg_img, scale, x + ox, y + oy, px);
            /* Dark on the left, where the logo and Play sit, opening up to
             * the right; heavier at the foot and a little at the top. */
            k = 0.26 + 0.44 * smooth(0.12, 0.85, xn);
            k *= 1.0 - 0.45 * smooth(0.55, 1.0, yn);
            k *= 1.0 - 0.25 * smooth(0.18, 0.0, yn);
            r = (int)(px[0] * k);
            g = (int)(px[1] * k);
            b = (int)(px[2] * k);
            g_bg_frame[(size_t)y * theme_w + x] =
                ((uint32_t)(r > 255 ? 255 : r) << 16) |
                ((uint32_t)(g > 255 ? 255 : g) << 8) | (uint32_t)(b > 255 ? 255 : b);
        }
    }
    return 1;
}

/* A soft round light: a bright core `radius` pixels across. */
static void glow_dot(double cx, double cy, double radius, int r, int g, int b, double alpha)
{
    int x0 = (int)floor(cx - radius * 3), x1 = (int)ceil(cx + radius * 3);
    int y0 = (int)floor(cy - radius * 3), y1 = (int)ceil(cy + radius * 3);
    int x, y;

    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            double dx = (x + 0.5 - cx) / radius, dy = (y + 0.5 - cy) / radius;

            blend(x, y, r, g, b, alpha * exp(-(dx * dx + dy * dy)));
        }
}

static uint32_t hash32(uint32_t v)
{
    v ^= v >> 16; v *= 0x7FEB352Du;
    v ^= v >> 15; v *= 0x846CA68Bu;
    v ^= v >> 16;
    return v;
}

/* Motes of light rising through the picture, the colour the title chose:
 * each drifts up on a slow spiral, breathes, and now and then flares, with
 * a wide faint halo round a small bright core, so they read as something
 * conjured rather than sparks off a fire. Each one's hue wanders a little
 * around the chosen colour, towards white and towards blue. */
static void brand_motes(unsigned frame)
{
    const double s = theme_scale;
    const int mr = (int)(g_motes & 0xFF), mg = (int)((g_motes >> 8) & 0xFF);
    const int mb = (int)((g_motes >> 16) & 0xFF);
    int i;

    for (i = 0; i < 90; i++) {
        uint32_t h = hash32((uint32_t)i * 2654435761u + 17u);
        double speed = (0.12 + (h & 0xFF) / 255.0 * 0.38) * s;
        double travel = theme_h + 60 * s;
        double x = (double)((h >> 8) % (uint32_t)theme_w);
        double y = theme_h + 30 * s - fmod((double)((h >> 4) % 4096) + frame * speed, travel);
        double size = (1.0 + ((h >> 20) & 0xF) / 15.0 * 1.8) * s;
        double breathe = 0.5 + 0.5 * sin(frame * (0.03 + ((h >> 24) & 7) * 0.006) + i * 2.1);
        double flare = pow(0.5 + 0.5 * sin(frame * 0.011 + i * 0.77), 24.0);
        double fade = smooth(0.0, 0.4, y / theme_h) * smooth(1.05, 0.8, y / theme_h);
        double swirl = frame * (0.008 + ((h >> 12) & 7) * 0.002) + i * 1.3;
        double tint = ((h >> 16) & 0xFF) / 255.0;          /* 0 bluer .. 1 paler */
        int r = mr, g = mg, b = mb;

        if (tint < 0.5) {
            b = b + (int)((255 - b) * (0.5 - tint) * 0.9);
        } else {
            r = r + (int)((255 - r) * (tint - 0.5) * 0.7);
            b = b + (int)((255 - b) * (tint - 0.5) * 0.5);
        }
        x += sin(swirl) * (10 + (h & 15)) * s;
        y += cos(swirl * 0.7) * 4 * s;

        glow_dot(x, y, size * 4.5, r, g, b, 0.18 * (0.4 + 0.6 * breathe) * fade);
        glow_dot(x, y, size, (r + 255) / 2, (g + 255) / 2, (b + 255) / 2,
                 (0.55 + 0.45 * breathe + 0.6 * flare) * fade);
        if (flare > 0.05)                       /* a brief star when it flares */
            glow_dot(x, y, size * 9.0, r, g, b, 0.12 * flare * fade);
    }
}

static void brand_background(void)
{
    static unsigned frame;

    if (!bg_frame_ready()) {
        memset(theme_px, 0, (size_t)theme_w * theme_h * sizeof *theme_px);
        return;
    }
    memcpy(theme_px, g_bg_frame, (size_t)theme_w * theme_h * sizeof *theme_px);
    brand_motes(++frame);
}

static int logo_ready(int w, int h)
{
    double scale;
    int x, y;

    if (g_logo_px && g_logo_w == w && g_logo_h == h)
        return 1;
    free(g_logo_px);
    g_logo_px = malloc((size_t)w * h * 4);
    if (!g_logo_px)
        return 0;
    g_logo_w = w;
    g_logo_h = h;
    scale = (double)w / g_logo_img.w;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            double px[4];
            unsigned char *o = g_logo_px + ((size_t)y * w + x) * 4;

            sample_area(&g_logo_img, scale, x, y, px);
            o[0] = (unsigned char)(px[0] > 255 ? 255 : px[0]);
            o[1] = (unsigned char)(px[1] > 255 ? 255 : px[1]);
            o[2] = (unsigned char)(px[2] > 255 ? 255 : px[2]);
            o[3] = (unsigned char)(px[3] * 255 > 255 ? 255 : px[3] * 255);
        }
    return 1;
}

void theme_logo(ThemeRect r)
{
    const double s = theme_scale;
    double fit;
    int w, h, x0, y0, x, y;

    if (!theme_px || !g_logo_img.rgba)
        return;
    fit = r.w * s / g_logo_img.w;
    if (r.h * s / g_logo_img.h < fit)
        fit = r.h * s / g_logo_img.h;
    w = (int)(g_logo_img.w * fit);
    h = (int)(g_logo_img.h * fit);
    if (w < 1 || h < 1 || !logo_ready(w, h))
        return;
    x0 = (int)(r.x * s + (r.w * s - w) / 2);
    y0 = (int)(r.y * s + (r.h * s - h) / 2);
    for (y = 0; y < h; y++) {
        int ty = y0 + y;

        if (ty < 0 || ty >= theme_h)
            continue;
        for (x = 0; x < w; x++) {
            int tx = x0 + x;
            const unsigned char *p = g_logo_px + ((size_t)y * w + x) * 4;
            uint32_t *d;
            double ia;
            int dr, dg, db;

            if (tx < 0 || tx >= theme_w || !p[3])
                continue;
            d = &theme_px[(size_t)ty * theme_w + tx];
            ia = 1.0 - p[3] / 255.0;
            dr = (int)(p[0] + ((*d >> 16) & 0xFF) * ia);
            dg = (int)(p[1] + ((*d >> 8) & 0xFF) * ia);
            db = (int)(p[2] + (*d & 0xFF) * ia);
            *d = ((uint32_t)(dr > 255 ? 255 : dr) << 16) |
                 ((uint32_t)(dg > 255 ? 255 : dg) << 8) | (uint32_t)(db > 255 ? 255 : db);
        }
    }
}

/* A rounded rectangle in one colour, lighter towards the top by `sheen`,
 * with an accent glow outside it as strong as `glow`. */
static void fill_rrect(ThemeRect r, double radius, int cr, int cg, int cb,
                       double alpha, double sheen, double glow)
{
    const double s = theme_scale;
    int pad = glow > 0 ? 22 : 1;
    int x0 = (int)floor((r.x - pad) * s), y0 = (int)floor((r.y - pad) * s);
    int x1 = (int)ceil((r.x + r.w + pad) * s), y1 = (int)ceil((r.y + r.h + pad) * s);
    int x, y;

    if (!theme_px)
        return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > theme_w) x1 = theme_w;
    if (y1 > theme_h) y1 = theme_h;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            double px = (x + 0.5) / s, py = (y + 0.5) / s;
            double d = rrect_distance(px, py, r, radius);

            if (d < 0) {
                double t = clamp01((py - r.y) / r.h);
                double l = sheen * (1.0 - t);

                blend(x, y, cr + (int)((255 - cr) * l), cg + (int)((255 - cg) * l),
                      cb + (int)((255 - cb) * l), alpha * clamp01(-d * s));
            } else if (glow > 0) {
                blend(x, y, AR, AG, AB, exp(-d / 7.0) * 0.45 * glow);
            }
        }
}

ThemeRect theme_card(ThemeRect r, const char *heading)
{
    ThemeRect h = r;

    theme_panel(r, 0.0);
    h.x += 20; h.y += 14; h.w -= 40; h.h = 22;
    if (heading)
        theme_text(h, heading, 11, 700, THEME_TEXT, THEME_LEFT);
    h.y += 32;
    h.h = r.y + r.h - h.y - 14;
    return h;
}

void theme_button(ThemeRect r, const char *label, int primary, double lit)
{
    lit = clamp01(lit);
    if (primary) {
        fill_rrect(r, 8.0, AR, AG, AB, 1.0, 0.10 + 0.10 * lit, 0.35 + 0.65 * lit);
        theme_text(r, label, r.h >= 56 ? 20 : 13, 700, RGB(0xFF, 0xF6, 0xF0), THEME_CENTRE);
    } else {
        fill_rrect(r, 8.0, 0x16 + (int)(0x14 * lit), 0x14 + (int)(0x12 * lit),
                   0x14 + (int)(0x12 * lit), 0.88, 0.04, lit * 0.6);
        theme_text(r, label, 12, lit > 0.5 ? 700 : 400,
                   lit > 0.5 ? THEME_TEXT : RGB(0xD8, 0xD2, 0xCE), THEME_CENTRE);
    }
}

void theme_dot(int cx, int cy, int radius, COLORREF colour)
{
    const double s = theme_scale;
    int x, y, rr = (int)ceil((radius + 1) * s);
    int pcx = (int)(cx * s), pcy = (int)(cy * s);

    for (y = -rr; y <= rr; y++)
        for (x = -rr; x <= rr; x++) {
            double d = sqrt((double)x * x + (double)y * y) - radius * s;

            blend(pcx + x, pcy + y, (int)(colour & 0xFF), (int)((colour >> 8) & 0xFF),
                  (int)((colour >> 16) & 0xFF), clamp01(0.5 - d));
        }
}

static int decode(const unsigned char *data, unsigned size, BrandImage *out)
{
    int n;

    if (!data || !size)
        return 0;
    out->rgba = stbi_load_from_memory(data, (int)size, &out->w, &out->h, &n, 4);
    return out->rgba != NULL;
}

int theme_brand_load(const unsigned char *background, unsigned background_size,
                     const unsigned char *logo, unsigned logo_size,
                     COLORREF accent, COLORREF motes)
{
    if (!decode(background, background_size, &g_bg_img))
        return 0;
    decode(logo, logo_size, &g_logo_img);
    g_branded = 1;
    g_motes = motes;
    theme_accent = accent;
    theme_accent_dim = RGB((accent & 0xFF) / 2, ((accent >> 8) & 0xFF) / 2,
                           ((accent >> 16) & 0xFF) / 2);
    theme_text_colour = RGB(0xF4, 0xF0, 0xEC);
    theme_text_dim_colour = RGB(0xA9, 0xA3, 0x9F);
    return 1;
}

int theme_branded(void)
{
    return g_branded;
}

void theme_switch(ThemeRect r, int on, double lit)
{
    ThemeRect track = r;
    int knob_r = r.h / 2 - 3;
    int cy = r.y + r.h / 2;
    int cx = on ? r.x + r.w - r.h / 2 : r.x + r.h / 2;

    lit = clamp01(lit);
    if (on)
        fill_rrect(track, r.h / 2.0, AR, AG, AB, 1.0, 0.12, 0.25 + 0.5 * lit);
    else
        fill_rrect(track, r.h / 2.0, 0x2C + (int)(0x10 * lit), 0x2A + (int)(0x10 * lit),
                   0x29 + (int)(0x10 * lit), 0.92, 0.05, 0.0);
    theme_dot(cx, cy, knob_r, on ? RGB(0xFF, 0xFF, 0xFF) : RGB(0xB4, 0xAE, 0xAA));
}
