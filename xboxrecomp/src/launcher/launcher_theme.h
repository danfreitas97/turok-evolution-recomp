/*
 * launcher_theme.h -- how the launcher looks, kept behind an interface.
 *
 * Everything drawn goes through here, so the platform's drawing is kept
 * out of the launcher. The shapes are arithmetic over a 32-bit buffer
 * (launcher_theme.c), the same everywhere; the buffer and the text are the
 * host's: GDI and a DIB on Windows (launcher_theme_gdi.c), CoreText on
 * macOS (launcher_theme_coretext.c), FreeType on Linux
 * (launcher_theme_freetype.c).
 *
 * The look is the original Xbox dashboard's: black, a green glow that
 * comes from behind, panels with soft edges, and the selected thing
 * lit rather than outlined. None of the console's own artwork is used
 * or reproduced -- no logos, no wordmarks, no fonts, no ripped
 * textures. What is here is drawn from arithmetic at run time.
 */
#ifndef XBOXRECOMP_LAUNCHER_THEME_H
#define XBOXRECOMP_LAUNCHER_THEME_H

#ifdef _WIN32
#include <windows.h>
#else
#include <stdint.h>
/* GDI's colour, 0x00BBGGRR, so the colours below read the same everywhere. */
typedef uint32_t COLORREF;
#define RGB(r, g, b) ((COLORREF)(((r) & 0xFF) | (((g) & 0xFF) << 8) | \
                                 (((b) & 0xFF) << 16)))
#endif

typedef struct ThemeRect { int x, y, w, h; } ThemeRect;

/* The accent and the text colours. The Xbox green and the shades that go
 * with it, unless the title brands its launcher (theme_brand_load), which
 * gives its own accent and neutral text. Variables, not constants, so the
 * names below read the same in both cases. */
extern COLORREF theme_accent, theme_accent_dim, theme_text_colour, theme_text_dim_colour;
#define THEME_GREEN      theme_accent
#define THEME_GREEN_DIM  theme_accent_dim
#define THEME_TEXT       theme_text_colour
#define THEME_TEXT_DIM   theme_text_dim_colour

/* A back buffer the size of the window. Everything below draws into it
 * and theme_present puts it on screen in one go, so nothing flickers. */
int  theme_begin(int width, int height);
void theme_shutdown(void);
#ifdef _WIN32
void theme_present(HDC dc);
#else
/* Pixels per window unit, for a display with more than one (Retina): the
 * buffer is drawn at the display's own resolution rather than stretched up
 * to it. Takes effect at the next theme_begin; 1 until then. */
void theme_set_scale(double scale);
/* The buffer as it is now, for the front end to put on screen, or NULL.
 * macOS: a CGImageRef the caller releases, a copy, so drawing the next
 * frame does not reach into the one being shown. Linux: the buffer itself
 * (theme_px), which the front end copies to the screen before drawing
 * again. */
void *theme_image(void);
#endif

/* The background: black, with the glow behind where the content sits.
 * `pulse` runs 0..1 and drifts, so the glow breathes as the dashboard's
 * did rather than sitting still. */
void theme_background(double pulse);

/* A panel. `lit` from 0 (resting) to 1 (selected) takes it from a dark
 * translucent slab to one edged and washed with green. */
void theme_panel(ThemeRect r, double lit);

/* Text, left/centre/right aligned within `r`. `size` is a point size;
 * `weight` is 400 for normal and 700 for bold. */
typedef enum { THEME_LEFT, THEME_CENTRE, THEME_RIGHT } ThemeAlign;
void theme_text(ThemeRect r, const char *text, int size, int weight,
                COLORREF colour, ThemeAlign align);

/* The same, wrapped to the rectangle and top aligned, for the lines
 * of explanation. theme_text is single line: a newline in its text
 * draws as nothing and the line runs off the edge. */
void theme_text_wrapped(ThemeRect r, const char *text, int size,
                        COLORREF colour);

/* The tab strip's underline, which slides rather than jumping: `from`
 * and `to` are tab rectangles and `t` runs 0..1 between them. */
void theme_tab_underline(ThemeRect from, ThemeRect to, double t);

/* A left/right chevron pair beside a value, dimmed when the value is at
 * the end of its range and there is nowhere further to go. */
void theme_arrows(ThemeRect r, int can_left, int can_right, double lit);

/* A progress bar: a dark rounded track, green across the first `fraction`
 * (0..1) of it. The self-building release's build screen. `phase` (0..1,
 * advanced by the caller each frame) moves a soft highlight along it, so a
 * stage that sits on one percentage still shows the build is alive; a
 * negative phase draws it still. */
void theme_bar(ThemeRect r, double fraction, double phase);

/* ------------------------------------------------------------ branding
 *
 * A title may give its launcher its own artwork: a background picture, a
 * logo and an accent colour (recomp_add_launcher(... BRAND_BACKGROUND ...),
 * src/launcher/CMakeLists.txt). The pictures are the title project's own,
 * compiled into the launcher, PNG or JPEG. Without them nothing changes.
 *
 * theme_brand_load decodes them; 1 if the background at least was usable.
 * After it, theme_background draws the picture, darkened towards the left
 * where the logo and the buttons sit, with motes of light in `motes`
 * rising through it. */
int  theme_brand_load(const unsigned char *background, unsigned background_size,
                      const unsigned char *logo, unsigned logo_size,
                      COLORREF accent, COLORREF motes);
int  theme_branded(void);
/* The logo, as large as fits `r` without cropping, centred. */
void theme_logo(ThemeRect r);
/* A card: a dark glass panel with a heading in small capitals. Returns the
 * area under the heading, for the card's own lines. */
ThemeRect theme_card(ThemeRect r, const char *heading);
/* A button: `primary` fills it with the accent (Play); otherwise a dark
 * one. `lit` 0..1 is the keyboard or pad focus, or the mouse over it. */
void theme_button(ThemeRect r, const char *label, int primary, double lit);
/* An on/off switch: a pill track, filled with the accent when on, and its
 * knob at that end. */
void theme_switch(ThemeRect r, int on, double lit);
/* A filled dot, for a status line. */
void theme_flag(ThemeRect r, int brazil);
void theme_dot(int cx, int cy, int radius, COLORREF colour);

#endif /* XBOXRECOMP_LAUNCHER_THEME_H */
