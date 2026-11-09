/**
 * Manual function overrides and ICALL diagnostics
 *
 * This file provides:
 *   - recomp_lookup_manual()  : intercept specific Xbox VAs with hand-written code
 *   - recomp_icall_fail_log() : log when an indirect call target can't be resolved
 *   - ICALL trace ring buffer  : globals used by the RECOMP_ICALL macro
 *
 * The recomp pipeline generates an auto-dispatch table (recomp_lookup) that
 * resolves most function addresses. recomp_lookup_manual() is called FIRST,
 * giving you a chance to override any function with a custom implementation.
 *
 * Common reasons to add manual overrides:
 *   - Trace a function to understand call flow (wrap the generated version)
 *   - Fix a function the lifter translated incorrectly
 *   - Stub out a function that crashes (return early, set eax to a safe value)
 *   - Redirect a function to a native implementation (e.g., skip CRT init)
 *   - Intercept D3D/audio calls for custom rendering or sound
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>   /* ptrdiff_t: not in stdlib.h off MSVC */
#include <stdlib.h>   /* getenv, exit: the spin verdict below */
#include <string.h>   /* strcmp: RECOMP_ICALL_FATAL */
#ifdef _WIN32
#include <windows.h>  /* CreateThread, Sleep: the Back-to-quit watch */
#else
#include <pthread.h>
#include <unistd.h>
#endif
#ifndef _MSC_VER
#  include <strings.h>                  /* strncasecmp */
#  define _strnicmp strncasecmp
#endif

/* One diagnostic report, one piece.
 *
 * Each report below is a dozen fprintf calls and the CRT takes its lock per
 * call, so with more than one guest thread running the lines interleave in
 * the middle of a report. Dino Crisis 3 runs six workers once it is past its
 * menus, and its log came out like this:
 *
 *   callers: 0x004DB424 <- 0x002705B4 <- 0x002705B4[ICALL] unresolved jump
 *   target 0x41B0B870 -- 1 time(s) (total calls: 6930990)
 *
 * -- two threads' reports spliced together, with a caller list that stops
 * mid-sentence and an address that belongs to neither line as read. These
 * diagnostics exist to be read during exactly the multi-threaded bring-up
 * that breaks them.
 *
 * The CRT exposes the lock these calls were already taking one at a time, so
 * hold it across the whole report instead. */
#if defined(_MSC_VER)
#  define RECOMP_DIAG_LOCK()   _lock_file(stderr)
#  define RECOMP_DIAG_UNLOCK() _unlock_file(stderr)
#elif defined(__unix__) || defined(__APPLE__)
#  define RECOMP_DIAG_LOCK()   flockfile(stderr)
#  define RECOMP_DIAG_UNLOCK() funlockfile(stderr)
#else
#  define RECOMP_DIAG_LOCK()   ((void)0)
#  define RECOMP_DIAG_UNLOCK() ((void)0)
#endif

/* ── ICALL trace ring buffer ───────────────────────────────── */

/*
 * These globals are written by the RECOMP_ICALL macro (defined in
 * recomp_types.h) every time an indirect call is dispatched. When a
 * crash occurs, the VEH handler or recomp_icall_fail_log() can dump
 * the last 16 call targets to help you trace what happened.
 *
 * The runtime owns them: xbox_kernel defines all three in
 * src/kernel/xbox_memory_layout.c, and recomp_types.h declares them extern.
 * Declare, do not define -- a definition here as well is a duplicate symbol,
 * and a project copied from this template failed to link on all three:
 *
 *   xbox_memory_layout.obj : error LNK2005: g_icall_count already defined
 *                            in recomp_manual.obj
 */
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;

typedef void (*recomp_func_t)(void);

/* ── Register state (defined in xbox_memory_layout.c) ──────── */

/* The guest registers (g_eax, g_esp, g_icall_saved_esp, ...) are fields of
 * one thread-local struct, declared once in recomp_cpu.h, which the lifter
 * copies into gen/ beside the code it describes. Take them from there; never
 * redeclare one here. They used to be redeclared by hand, and a declaration
 * without the thread-local storage class linked and silently read other
 * storage, so every read got 0 -- which is why the log below once reported
 * no call site: not because the guest esp was stale, but because this file
 * was not reading the guest esp at all. The names are macros now, so a
 * redeclaration no longer compiles. */
#include "recomp/gen/recomp_cpu.h"

/* g_eax: recomp_cpu.h. */
/* The guest stack pointer. At the moment an indirect call is refused, the
 * caller has already pushed its guest return address, so the top of the
 * guest stack is the call site -- the one thing the old log did not say. */
/* g_esp: recomp_cpu.h. */
extern ptrdiff_t g_xbox_mem_offset;
/* The lifted code sections, so a value on the guest stack can be told
 * apart from data when naming the callers of a refused call. */
extern uint32_t g_xbox_code_lo;
extern uint32_t g_xbox_code_hi;
/* The esp the dispatch macro captured. Not g_esp, which is stale by the time
 * a refused call is reported. Zero when the title's generated header predates
 * this, and the log then says it has no callers rather than inventing them. */
/* g_icall_saved_esp: recomp_cpu.h. */
/* Which dispatch form was refused: 0 unknown, 1 call, 2 jump. Unknown
 * means this title was lifted before the macros published it. */
/* g_icall_dispatch_form: recomp_cpu.h. */

/* ── Manual function overrides ─────────────────────────────── */

/*
 * Return a function pointer to override the given Xbox VA, or NULL
 * to fall through to the auto-generated dispatch table.
 *
 * This is called on every indirect call (RECOMP_ICALL) and every
 * direct call through the dispatch table, so keep it fast. A chain
 * of if-statements on uint32_t compiles to a simple comparison
 * sequence; for large override tables, consider a sorted array
 * with binary search.
 *
 * Examples of common override patterns:
 *
 *   // Trace wrapper: log entry/exit around the generated function
 *   extern void sub_00012345(void);
 *   static void traced_sub_00012345(void) {
 *       fprintf(stderr, "[TRACE] sub_00012345 entered, eax=0x%08X\n", g_eax);
 *       sub_00012345();
 *       fprintf(stderr, "[TRACE] sub_00012345 returned, eax=0x%08X\n", g_eax);
 *   }
 *
 *   // Stub: skip a function entirely (return 0 in eax)
 *   static void stub_00067890(void) {
 *       g_eax = 0;
 *   }
 *
 *   // Fix: replace a broken lifted function with correct C
 *   static void fixed_sub_000ABCDE(void) {
 *       // Read arguments from stack/registers per calling convention
 *       uint32_t arg1 = g_ecx;
 *       uint32_t arg2 = MEM32(g_esp + 4);
 *       // ... correct implementation ...
 *       g_eax = result;
 *   }
 */
recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    /*
     * TODO: Add your overrides here. Examples:
     *
     * if (xbox_va == 0x00012345) return traced_sub_00012345;
     * if (xbox_va == 0x00067890) return stub_00067890;
     * if (xbox_va == 0x000ABCDE) return fixed_sub_000ABCDE;
     */

    (void)xbox_va;
    return (recomp_func_t)0;
}

/* ── ICALL failure logging ─────────────────────────────────── */

/*
 * Called when RECOMP_ICALL cannot resolve a target address.
 * This usually means one of:
 *   - A vtable dispatch to an address not in the dispatch table
 *   - A function pointer loaded from uninitialized or corrupt memory
 *   - A kernel thunk address that the bridge doesn't handle
 *
 * During early bring-up you will see many of these. Most are harmless
 * (the ICALL macro pops the dummy return address and continues).
 * Focus on the ones that cause crashes or incorrect behavior.
 */
void recomp_icall_fail_log(uint32_t va)
{
    /*
     * Rate-limited per target, reporting at 1, 10, 100 ...
     *
     * An unresolved target inside a loop is the normal case, not the rare
     * one: Mortal Kombat: Deadly Alliance produced 46843 of these for a
     * single address in one 30-second run, seventeen lines each. That buries
     * every other diagnostic in the log and says nothing the first report did
     * not. The progression is the useful part -- one line says a target was
     * never identified, a run of them says the title is stuck on it.
     */
    enum { SLOTS = 16 };
    static uint32_t seen[SLOTS];
    static uint64_t hits[SLOTS];
    static int count;
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va)
            break;
    if (i == count) {
        if (count == SLOTS)
            return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    hits[i]++;
    {
        uint64_t n = hits[i];
        while (n >= 10 && n % 10 == 0)
            n /= 10;
        if (n != 1)
            return;
    }

    RECOMP_DIAG_LOCK();
    fprintf(stderr, "[ICALL] unresolved %starget 0x%08X -- %llu time(s) "
                    "(total calls: %llu)\n",
            g_icall_dispatch_form == 1 ? "call " :
            g_icall_dispatch_form == 2 ? "jump " : "",
            va, (unsigned long long)hits[i],
            (unsigned long long)g_icall_count);
    if (g_icall_dispatch_form == 2 && hits[i] == 1)
        fprintf(stderr, "  a jump, not a call: if this address is inside a "
                        "function rather than at its start, the guest is doing "
                        "its own control flow (a coroutine, a longjmp, or a "
                        "switch arm) and seeding it as a function will not "
                        "help\n");

    /*
     * Who called it. The lifted caller pushed its return address on the guest
     * stack, so code-range values above esp name the chain -- the same scan
     * the memory watchpoints use. It is a heuristic: stale return addresses
     * from earlier frames show up too, and the first entry is the reliable
     * one. It is still the difference between an address with no context and
     * a function to open, which is what an unresolved target needs.
     */
    {
        const uint8_t *stack = (const uint8_t *)g_xbox_mem_offset
                             + g_icall_saved_esp;
        int shown = 0;
        fprintf(stderr, "  callers:");
        for (i = 0; g_xbox_mem_offset && g_icall_saved_esp
                    && i < 160 && shown < 6; i++) {
            uint32_t v;
            memcpy(&v, stack + (size_t)i * 4, sizeof v);
            if (v >= g_xbox_code_lo && v < g_xbox_code_hi) {
                fprintf(stderr, "%s 0x%08X", shown ? " <-" : "", v);
                shown++;
            }
        }
        if (!shown)
            fprintf(stderr, " (none on the stack)");
        fprintf(stderr, "\n");
    }

    /* The recent-target ring buffer, once per address. What ran just before
     * an unresolved call is context for the first report and noise after. */
    if (hits[i] == 1) {
        fprintf(stderr, "  Recent ICALL targets:\n");
        for (i = 0; i < 16; i++) {
            int idx = (g_icall_trace_idx - 16 + i) & 15;
            if (g_icall_trace[idx])
                fprintf(stderr, "    [%2d] 0x%08X\n", i, g_icall_trace[idx]);
        }
    }
    RECOMP_DIAG_UNLOCK();
    fflush(stderr);
}
/* An indirect call whose target is not code: a null or wild function pointer.
 *
 * Skipping these is right -- calling a data address is worse -- but skipping
 * them *silently* is not. They almost always arrive inside a loop, so the
 * symptom is a hang with no output rather than a diagnosable null vtable call.
 *
 * Rate-limited per address: a spin can produce millions of these, and the
 * useful information is which addresses occur, not how often.
 */
void recomp_icall_not_code_log(uint32_t va, uint32_t saved_esp)
{
    /* A power of ten, because the rate limiter above only reaches this
     * function body at 1, 10, 100 ... and the verdict has to land on one of
     * those. A hundred thousand skips of one target is unambiguous: no title
     * makes progress through that. */
    enum { SLOTS = 16, SPIN_VERDICT = 100000 };
    static uint32_t seen[SLOTS];
    static uint64_t hits[SLOTS];
    static int count;
    static int said_it;   /* the verdict below is said once, not once per target */
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va)
            break;
    if (i == count) {
        if (count == SLOTS)
            return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    hits[i]++;
    /* Report at 1, 10, 100, 1000 ... rather than once. A single line says a
     * wild pointer was skipped; the progression says it is being skipped in a
     * loop, which is the difference between a curiosity and the reason the
     * title is hung. */
    {
        uint64_t n = hits[i];
        while (n >= 10 && n % 10 == 0)
            n /= 10;
        if (n != 1)
            return;
    }
    {
        /* The call site, read off the guest stack. Without it the log
         * says a wild pointer was skipped but not by whom, and the
         * caller is the only thing that leads anywhere.
         *
         * It comes from the esp the dispatch macro captured, not from
         * g_esp. The lifted call site pushes its return address onto the
         * *local* esp and only syncs g_esp at certain points, so g_esp is
         * stale here -- it read 0 exactly when a null target most needed
         * explaining, which sent three rounds of Jet Set Radio Future
         * chasing inferences instead of a call site. saved_esp is the value
         * before that push, so the return address is the dword below it.
         *
         * Only for the call form. A tail jump pushes no return address, so
         * that dword is whatever the frame happened to leave there: Tony
         * Hawk's Pro Skater 2X reported "from 0x00000008" and then "from
         * 0x00000004" for the same million skipped calls, and both are
         * plainly not code. Printing a number that cannot be an address as
         * though it were the caller is worse than printing nothing, because
         * it is the one field a reader goes to next.
         *
         * So: name the form, and where the top of the stack cannot answer,
         * scan for code-range values the way the unresolved-target logger
         * above does. Stale return addresses from earlier frames come back
         * too and the first is the reliable one, but a list of real code
         * addresses is a place to start and 0x00000008 is not. */
        uint32_t caller = 0;
        if (g_icall_dispatch_form != 2 && saved_esp >= 4 && g_xbox_mem_offset)
            caller = *(const uint32_t *)((const uint8_t *)g_xbox_mem_offset
                                         + (saved_esp - 4));
        if (caller < g_xbox_code_lo || caller >= g_xbox_code_hi)
            caller = 0;
        RECOMP_DIAG_LOCK();
    fprintf(stderr, "[ICALL] target 0x%08X is not code -- skipped "
                        "%llu time(s) via a %s",
                va, (unsigned long long)hits[i],
                g_icall_dispatch_form == 2 ? "jump" : "call");
        if (caller)
            fprintf(stderr, " from 0x%08X", caller);
        fprintf(stderr, " (null or wild function pointer, at call #%llu)\n",
                (unsigned long long)g_icall_count);

        /* The stack scan, once per address: the same heuristic the
         * unresolved-target logger uses, and the only thing that names a
         * caller when the return address is not where it should be. */
        if (hits[i] == 1) {
            const uint8_t *stack = (const uint8_t *)g_xbox_mem_offset
                                 + saved_esp;
            int shown = 0, k;
            fprintf(stderr, "  callers:");
            for (k = 0; g_xbox_mem_offset && saved_esp
                        && k < 160 && shown < 6; k++) {
                uint32_t v;
                memcpy(&v, stack + (size_t)k * 4, sizeof v);
                if (v >= g_xbox_code_lo && v < g_xbox_code_hi) {
                    fprintf(stderr, "%s 0x%08X", shown ? " <-" : "", v);
                    shown++;
                }
            }
            if (!shown)
                fprintf(stderr, " (none on the stack)");
            fprintf(stderr, "\n");
        }

        /* RECOMP_ICALL_FATAL=1: fault here instead of skipping, so the crash
         * handler prints the guest call stack that reached this call.
         *
         * The caller printed above is read from the top of the guest stack,
         * which is right only when the lifted call site pushed a return
         * address there and nothing has since moved esp. When it reads 0 --
         * exactly the case where a null target most needs explaining -- the
         * line says a wild pointer was skipped and nothing about by whom. A
         * deliberate fault costs the run and buys the backtrace. */
        {
            static int fatal = -1;
            if (fatal < 0) {
                const char *v = getenv("RECOMP_ICALL_FATAL");
                fatal = v && *v && strcmp(v, "0") != 0;
            }
            if (fatal) {
                fprintf(stderr, "[ICALL] RECOMP_ICALL_FATAL: faulting here for "
                                "a backtrace\n");
                fflush(stderr);
                *(volatile int *)0 = 1;
            }
        }
    }

    /* Past a certain count this stops being a warning and becomes a verdict.
     *
     * A skipped call sets eax to 0 and returns. Zero is a fine answer for a
     * null function pointer in most code -- it reads as NULL, false, or
     * nothing -- but it is also S_OK, so in COM-shaped code a loop that
     * repeats while the result is non-negative can never leave. Panzer
     * Dragoon Orta hung exactly there, three billion skips in forty seconds,
     * with no frame ever presented.
     *
     * The value is deliberately left alone: there is no return value that is
     * right for both conventions, and guessing failure would break the
     * callers for which zero is correct. What can be fixed is the silence.
     * Say plainly that the title is hung, once, and say what to do about it.
     */
    if (hits[i] == SPIN_VERDICT && !said_it) {
        said_it = 1;
        fprintf(stderr,
            "[ICALL] ^^ this is a hang, not slow progress. One target has been\n"
            "        skipped %d times. The skip returns eax = 0, which is also\n"
            "        S_OK, so a loop testing for a non-negative result will\n"
            "        never exit. Try `py -3 -m tools.seed_from_log <log>` to\n"
            "        recover the target as a function, and read\n"
            "        docs/technical/memory-watchpoints.md for who wrote the\n"
            "        pointer. Set RECOMP_ICALL_SPIN_FATAL=1 to stop here\n"
            "        instead of spinning.\n", SPIN_VERDICT);
        fflush(stderr);
        if (getenv("RECOMP_ICALL_SPIN_FATAL")) {
            fprintf(stderr, "[ICALL] RECOMP_ICALL_SPIN_FATAL is set; exiting.\n");
            fflush(stderr);
            exit(3);
        }
    }
    RECOMP_DIAG_UNLOCK();
    fflush(stderr);
}


/* ── Turok: Evolution ──────────────────────────────────────── */

#include "recomp/gen/recomp_funcs.h"

/* ── Widescreen: which screen the game is on ──────────────────
 *
 * Turok has no 16:9 mode of its own: CreateDevice gets no widescreen flag
 * and the game never reads the console's video flags (XC_VIDEO). Its world
 * is drawn by vertex programs from a model-view-projection the game builds
 * on the CPU for every object and loads at c96..c99, oPos.x = dp4(v0, c96),
 * so the toolkit widens the view by scaling c96 on screen draws
 * (CMakeLists.txt: recomp_title_widescreen(HOR_PLUS REGISTER 96)).
 *
 * The front end is a 3D stage built for 4:3 -- the title screen's jungle
 * set ends just past the 4:3 edges -- so it is shown 4:3 between bars, as
 * are frames with nothing in 3D (the XMV movies). The game says which screen
 * it is entering by the precache list it loads for it:
 * sub_00054A60 (this-call) builds y:\data\precachelists\autolists\<name>.pcl
 * from the name at this+0x20 -- bootupsequence, splashsequence, intro,
 * frontendbackground, firstinteractivescreen, initialspacecheck, mainmenu,
 * newgame, gamestart.
 * Everything from gamestart on is the game; a front-end list after it is
 * the way back out. */
extern void xbox_D3D8SetWideFrames(int wide);
extern void xbox_D3D8SetWideFramesAuto(int on);
static void tk_cheats_apply(void);
static void tk_quit_screen(const char *name);
extern void sub_00054A60_gen(void);

static int tk_is_front_end(const char *name)
{
    static const char *const front[] = {
        "begin", "bootupsequence", "splashsequence", "intro", "frontendbackground",
        "firstinteractivescreen", "initialspacecheck", "mainmenu", "newgame",
    };
    const char *base = name, *p;
    size_t i, n;

    for (p = name; *p; p++)
        if (*p == '\\' || *p == '/')
            base = p + 1;
    n = strcspn(base, ".");
    for (i = 0; i < sizeof front / sizeof front[0]; i++)
        if (strlen(front[i]) == n && _strnicmp(base, front[i], n) == 0)
            return 1;
    return 0;
}

void sub_00054A60(void)
{
    static int started;
    uint32_t self = g_ecx;
    uint32_t name_va = self ? *(volatile uint32_t *)((uintptr_t)self + 0x20u + g_xbox_mem_offset) : 0;

    if (!started) {
        started = 1;
        xbox_D3D8SetWideFramesAuto(1);
    }
    if (name_va) {
        const char *name = (const char *)((uintptr_t)name_va + g_xbox_mem_offset);
        char shown[64];
        int front = tk_is_front_end(name);

        snprintf(shown, sizeof shown, "%s", name);
        fprintf(stderr, "[TUROK] screen %s: %s\n", shown, front ? "front end, 4:3" : "game");
        tk_quit_screen(name);
        xbox_D3D8SetWideFrames(!front);
    }
    tk_cheats_apply();          /* every screen: the cheats menu shows them */
    sub_00054A60_gen();
}

/* ── Widescreen: edge effects span the screen ──────────────────
 *
 * Every 2D image the game draws goes through sub_0009C9A0, one
 * pre-transformed quad each: the health box, the front end's panels, and
 * the damage indicator, which lights the side a hit came from with
 * quarter-screen quads that overhang the screen's edge (x 329.6 to 649.6).
 * Half the width, those read as HUD pieces to the renderer and were
 * squeezed into the middle 4:3, so the red stopped short of the screen's
 * edges. EDGE spans a draw that reaches the left or right edge and leaves
 * the rest to the renderer's judgement, so the health box stays at 4:3. */
extern void xbox_D3D8SetTwoDPlacement(int placement, uint32_t tag);
extern void sub_0009C9A0_gen(void);

#define TK_2D_AUTO 0            /* XBOX_D3D8_2D_AUTO */
#define TK_2D_EDGE 6            /* XBOX_D3D8_2D_EDGE */

void sub_0009C9A0(void)
{
    xbox_D3D8SetTwoDPlacement(TK_2D_EDGE, 0x0009C9A0u);
    sub_0009C9A0_gen();
    xbox_D3D8SetTwoDPlacement(TK_2D_AUTO, 0);
}

/* ── Cheats, switched on from the launcher ─────────────────────
 *
 * The game has its own cheats, entered as words in its Cheats menu
 * (Data\Text\Cheats.txm keys 20..28: texas, madman, emerpus, sllewgh,
 * sellout, heid, hunter, zoo, fmnfb). A word that matches sets item 15 + its
 * position in $\Data\History\Cheats.hst to 1, which shows that cheat in the
 * menu (the menu's "allow focus?" links read items 15..20); the menu's
 * switches then set items 1..6 (1 all weapons, 2 unlimited ammo,
 * 3 invincible, 4 invisible, 6 big heads; their widgets' W_THI).
 *
 * Nothing reads items 1..6 while playing: sub_00153A00, which sets up the
 * player, copies them once into the player (+0x3BC all weapons, +0x3B8
 * ammo) and into globals (0x2580DC invincible, 0x257FD0 invisible,
 * 0x257FE0 big heads). So a cheat chosen in the launcher is written into
 * Cheats.hst as the menu would have written it, on every screen change and
 * just before the player is set up, and the game does the rest. A cheat
 * left off in the launcher is left alone: words typed in the game still
 * work.
 *
 * A history is a vector of 8-byte entries, { value, maximum }, items
 * counted from 1, under the history manager at [[0x257E14] + 0x80], whose
 * first two words are the begin and end of its list of histories; each
 * history names its file at +0x14 and holds its entries' begin and end at
 * +4 and +8 (sub_00060680 finds one by name, sub_00060700 sets an item and
 * clamps it to the maximum, sub_000608E0 reads one).
 *
 * Settings file keys (or the environment variables beside them):
 *   cheat_invincible  cheat_unlimited_ammo  cheat_all_weapons
 *   cheat_invisible   cheat_level_select */
extern int recomp_config_bool(const char *env_name, const char *key, int fallback);
extern void sub_00153A00_gen(void);

#define TK_GAME_OBJECT      0x00257E14u
#define TK_HISTORY_MANAGER  0x80u

typedef struct {
    const char *env, *key;
    int item;                   /* the switch, 0 for none */
    int unlock;                 /* the "shown in the menu" item */
} TkCheat;

static const TkCheat k_tk_cheats[] = {
    { "TUROK_CHEAT_ALL_WEAPONS",    "cheat_all_weapons",    1, 15 },   /* texas */
    { "TUROK_CHEAT_UNLIMITED_AMMO", "cheat_unlimited_ammo", 2, 16 },   /* madman */
    { "TUROK_CHEAT_INVINCIBLE",     "cheat_invincible",     3, 17 },   /* emerpus */
    { "TUROK_CHEAT_INVISIBLE",      "cheat_invisible",      4, 18 },   /* sllewgh */
    { "TUROK_CHEAT_LEVEL_SELECT",   "cheat_level_select",   0, 19 },   /* sellout */
};
#define TK_CHEATS ((int)(sizeof k_tk_cheats / sizeof k_tk_cheats[0]))

static uint32_t tk_rd32(uint32_t va)
{
    return *(volatile uint32_t *)((uintptr_t)va + g_xbox_mem_offset);
}

static void tk_wr32(uint32_t va, uint32_t v)
{
    *(volatile uint32_t *)((uintptr_t)va + g_xbox_mem_offset) = v;
}

static int tk_name_is(uint32_t name_va, const char *want)
{
    const char *name;
    size_t n, w = strlen(want);

    if (!name_va)
        return 0;
    name = (const char *)((uintptr_t)name_va + g_xbox_mem_offset);
    n = strnlen(name, 128);
    return n >= w && _stricmp(name + n - w, want) == 0;
}

/* Cheats.hst in the history manager, or 0 before the game has loaded it. */
static uint32_t tk_cheats_history(void)
{
    uint32_t game = tk_rd32(TK_GAME_OBJECT), mgr, p, end;

    if (!game || !(mgr = tk_rd32(game + TK_HISTORY_MANAGER)))
        return 0;
    p = tk_rd32(mgr);
    end = tk_rd32(mgr + 4);
    if (!p || end < p || end - p > 4 * 256)
        return 0;
    for (; p < end; p += 4) {
        uint32_t h = tk_rd32(p);

        if (h && tk_name_is(tk_rd32(h + 0x14), "\\History\\Cheats.hst"))
            return h;
    }
    return 0;
}

/* Item `item` (from 1) to `value`, held to its maximum as sub_00060700 does. */
static void tk_history_set(uint32_t h, int item, int value)
{
    uint32_t begin = tk_rd32(h + 4), end = tk_rd32(h + 8), e;
    int max;

    if (item < 1 || end < begin || (end - begin) / 8 < (uint32_t)item)
        return;
    e = begin + (uint32_t)(item - 1) * 8;
    max = (int)tk_rd32(e + 4);
    tk_wr32(e, (uint32_t)(value > max ? max : value));
}

static void tk_cheats_apply(void)
{
    static int read, on[TK_CHEATS], any;
    static int reported;
    uint32_t h;
    int i;

    if (!read) {
        read = 1;
        for (i = 0; i < TK_CHEATS; i++) {
            on[i] = recomp_config_bool(k_tk_cheats[i].env, k_tk_cheats[i].key, 0);
            any |= on[i];
        }
    }
    if (!any || !(h = tk_cheats_history()))
        return;
    for (i = 0; i < TK_CHEATS; i++) {
        if (!on[i])
            continue;
        tk_history_set(h, k_tk_cheats[i].unlock, 1);
        if (k_tk_cheats[i].item)
            tk_history_set(h, k_tk_cheats[i].item, 1);
    }
    if (!reported) {
        reported = 1;
        fprintf(stderr, "[TUROK] cheats on:%s%s%s%s%s\n",
                on[0] ? " all-weapons" : "", on[1] ? " unlimited-ammo" : "",
                on[2] ? " invincible" : "", on[3] ? " invisible" : "",
                on[4] ? " level-select" : "");
    }
}

/* ── Quit: Back on the main menu ───────────────────────────────
 *
 * A console game has no Quit. On the main menu Back (the controller's
 * SELECT, Backspace on the keyboard by default) closes the game, as the
 * window's close button does. Back has no other use on that screen.
 *
 * A small thread reads the four ports as the game sees them (bindings
 * applied, keyboard included) while the main menu is up, and acts on a press
 * and release that began after the menu had been up for half a second, so a
 * Back held while arriving there does nothing. */
typedef struct {                /* RecompInputGamepad (src/hle/input_model.h) */
    uint16_t buttons;
    uint8_t  analog_buttons[8];
    int16_t  thumb_lx, thumb_ly, thumb_rx, thumb_ry;
} TkPad;

extern _Bool recomp_input_host_sample_port(unsigned port, TkPad *gamepad);
extern void xbox_HostExit(const char *why);

#define TK_PAD_BACK 0x0020u

static volatile int      g_tk_on_main_menu;
static volatile unsigned g_tk_menu_epoch;

static void tk_sleep_ms(unsigned ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000u);
#endif
}

static void tk_quit_watch(void)
{
    unsigned epoch = 0, waited = 0;
    int armed = 0, down = 0;


    for (;;) {
        TkPad pad;
        unsigned port;
        int back = 0;

        tk_sleep_ms(30);
        if (!g_tk_on_main_menu) {
            armed = down = 0;
            continue;
        }
        if (epoch != g_tk_menu_epoch) {         /* arrived again: wait again */
            epoch = g_tk_menu_epoch;
            armed = down = 0;
            waited = 0;
        }
        for (port = 0; port < 4; port++) {
            memset(&pad, 0, sizeof pad);
            if (recomp_input_host_sample_port(port, &pad) && (pad.buttons & TK_PAD_BACK))
                back = 1;
        }
        if (!armed) {
            waited += 30;
            if (waited >= 500 && !back)
                armed = 1;
            continue;
        }
        if (back)
            down = 1;
        else if (down) {
            fprintf(stderr, "[TUROK] Back on the main menu: quitting\n");
            fflush(stderr);
            xbox_HostExit("Back on the main menu");
        }
    }
}

#ifdef _WIN32
static DWORD WINAPI tk_quit_thread(LPVOID unused)
{
    (void)unused;
    tk_quit_watch();
    return 0;
}
#else
static void *tk_quit_thread(void *unused)
{
    (void)unused;
    tk_quit_watch();
    return NULL;
}
#endif

static void tk_quit_screen(const char *name)
{
    static int started;
    const char *base = name, *p;

    for (p = name; *p; p++)
        if (*p == '\\' || *p == '/')
            base = p + 1;
    g_tk_on_main_menu = _strnicmp(base, "mainmenu", 8) == 0;
    if (g_tk_on_main_menu)
        g_tk_menu_epoch++;
    if (!started) {
        started = 1;
#ifdef _WIN32
        {
            HANDLE t = CreateThread(NULL, 0, tk_quit_thread, NULL, 0, NULL);

            if (t)
                CloseHandle(t);
        }
#else
        {
            pthread_t t;

            if (pthread_create(&t, NULL, tk_quit_thread, NULL) == 0)
                pthread_detach(t);
        }
#endif
    }
}

/* The player is set up: Cheats.hst first, so it copies the cheats in. */
void sub_00153A00(void)
{
    tk_cheats_apply();
    sub_00153A00_gen();
}
