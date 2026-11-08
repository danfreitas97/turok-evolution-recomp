/*
 * Draw the title's frames on the host GPU.
 *
 * Turok links XDK Direct3D statically: it writes NV2A methods straight into
 * its pushbuffer and never calls the runtime's Direct3D. The pushbuffer
 * executor (tools/xboxrecomp/src/kernel/nv2a_pb_exec.c) decodes that stream;
 * with RECOMP_PB_D3D11 set it forwards each draw, texture and state change to
 * the PGRAPH translator (src/nv2a/nv2a_pgraph_d3d11.c), which turns them into
 * draws on a Direct3D 11 device instead of rasterising them on the CPU.
 *
 * The translator draws through xbox_GetD3DDevice(), which exists only once
 * something has gone through the runtime's CreateDevice. The title never
 * does, so this file does: our window, our swap chain, our device. The title
 * never learns it exists. At the end of each frame the translator calls
 * gpu_translator_frame_end(), which presents.
 *
 * GPU when there is one, CPU when not: if the window or the device cannot be
 * created, RECOMP_PB_D3D11 is cleared again before the executor reads it, so
 * the executor keeps its CPU rasteriser and the framebuffer window.
 * TUROK_RENDERER=cpu asks for the CPU path outright.
 *
 * Adapted from FFNY Online's src/hooks/d3d11_translator.c (MIT, Vlad
 * Yanhursky), without its settings, overlay, recorder and texture packs.
 *
 * Windows only for now. Off Windows the toolkit's device is Vulkan and needs
 * a host window of its own (FFNY's src/host_posix.c); until that is ported
 * the CPU path is used there.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <dxgi.h>

#include "d3d8_xbox.h"
#include "nv2a_pgraph_d3d11.h"
#include "keyboard.h"

/* The title sets a 640x480 mode. The translator works in those units; the
 * swap chain scales the picture into whatever size the window is. */
#define GT_WIDTH   640
#define GT_HEIGHT  480

#define WM_APP_FULLSCREEN (WM_APP + 1)

static HWND   s_window;
static HANDLE s_window_ready;
static int    s_ready;
static int    s_fullscreen;
static WINDOWPLACEMENT s_windowed = { sizeof(WINDOWPLACEMENT) };

/* Borderless full screen on the window's monitor: no mode change, so Alt-Tab
 * is instant and the swap chain just follows the window's size. Runs on the
 * window thread. */
static void set_fullscreen(HWND h, int on)
{
    if (!!on == s_fullscreen)
        return;
    if (on) {
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        if (!GetWindowPlacement(h, &s_windowed) ||
            !GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi))
            return;
        s_fullscreen = 1;
        SetWindowLongPtrW(h, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(h, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        s_fullscreen = 0;
        SetWindowLongPtrW(h, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPlacement(h, &s_windowed);
        SetWindowPos(h, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    ShowCursor(on ? FALSE : TRUE);
}

static LRESULT CALLBACK gt_wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
    case WM_KEYUP:   case WM_SYSKEYUP: {
        int down = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
        /* Alt+Enter and F11: full screen. Bit 30 set = auto-repeat. */
        if (down && !(lp & (1 << 30)) &&
            (wp == VK_F11 || (wp == VK_RETURN && (lp & (1 << 29))))) {
            set_fullscreen(h, !s_fullscreen);
            return 0;
        }
        /* The keyboard as the port-0 pad (RECOMP_KEYBOARD=1): the same table
         * the toolkit's own windows write. The side-specific code too, so a
         * binding to "lshift" can be held. */
        xbox_InputKeySet((int)wp, down);
        {
            int side = xbox_InputWin32SideVk((int)wp, (uint32_t)lp);
            if (side)
                xbox_InputKeySet(side, down);
        }
        if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP)
            break;          /* Alt+F4 still closes */
        return 0;
    }
    case WM_SYSCHAR:
        if (wp == VK_RETURN)
            return 0;       /* no beep for Alt+Enter */
        break;
    case WM_KILLFOCUS:
        xbox_InputKeysClear();      /* no key stays held while away */
        break;
    case WM_APP_FULLSCREEN:
        set_fullscreen(h, (int)wp);
        return 0;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lp)->ptMinTrackSize.x = 320;
        ((MINMAXINFO *)lp)->ptMinTrackSize.y = 240;
        return 0;
    case WM_ERASEBKGND:
        return 1;           /* the renderer paints every pixel */
    case WM_CLOSE:
        /* Closing the window ends the run; left to DefWindowProc the title
         * would carry on with nothing to draw into. */
        fprintf(stderr, "[GPU] window closed by the user; exiting\n");
        fflush(stderr);
        fflush(stdout);
        TerminateProcess(GetCurrentProcess(), 0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static HWND make_window(void)
{
    WNDCLASSEXW wc;
    RECT r, work;
    int x = CW_USEDEFAULT, y = CW_USEDEFAULT, w, hh;
    const char *scale_env = getenv("RECOMP_WINDOW_SCALE");
    int scale = scale_env && atoi(scale_env) > 0 ? atoi(scale_env) : 2;

    memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = gt_wndproc;
    wc.hInstance     = GetModuleHandleW(NULL);
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"TurokRecompGpu";
    RegisterClassExW(&wc);

    r.left = r.top = 0;
    r.right = GT_WIDTH * scale;
    r.bottom = GT_HEIGHT * scale;
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    w = r.right - r.left;
    hh = r.bottom - r.top;
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
        if (w > work.right - work.left)  w = work.right - work.left;
        if (hh > work.bottom - work.top) hh = work.bottom - work.top;
        x = work.left + (work.right - work.left - w) / 2;
        y = work.top + (work.bottom - work.top - hh) / 2;
    }
    return CreateWindowExW(0, L"TurokRecompGpu", L"Turok: Evolution",
                           WS_OVERLAPPEDWINDOW, x, y, w, hh,
                           NULL, NULL, wc.hInstance, NULL);
}

/* The window lives on a thread of its own that only pumps its messages. The
 * title's thread never looks at a message queue, so a window created there
 * would never answer and Windows would mark it Not Responding. Direct3D
 * presents from the executor's thread; a swap chain only needs its window's
 * thread to keep answering. */
static DWORD WINAPI window_thread(LPVOID unused)
{
    MSG msg;
    (void)unused;
    {
        /* Real pixels on a scaled desktop (Windows 10 1703+): otherwise the
         * window is rendered small and stretched. */
        typedef BOOL (WINAPI *SetDpiContext)(HANDLE);
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        SetDpiContext set = user32
            ? (SetDpiContext)(void *)GetProcAddress(user32, "SetProcessDpiAwarenessContext") : NULL;
        if (set)
            set((HANDLE)(INT_PTR)-4);   /* PER_MONITOR_AWARE_V2 */
    }
    s_window = make_window();
    if (s_window) {
        const char *fs = getenv("RECOMP_FULLSCREEN");
        ShowWindow(s_window, SW_SHOW);
        /* Started from a launcher or a shell in the background, Windows
         * would leave the window behind the one that started it. */
        SetForegroundWindow(s_window);
        if (fs && *fs && *fs != '0')
            set_fullscreen(s_window, 1);
    }
    SetEvent(s_window_ready);
    if (!s_window)
        return 1;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

static int want_gpu(void)
{
    const char *r = getenv("TUROK_RENDERER");
    return !(r && _stricmp(r, "cpu") == 0);
}

int gpu_translator_init(void)
{
    IDirect3D8 *d3d;
    IDirect3DDevice8 *dev = NULL;
    D3DPRESENT_PARAMETERS pp;
    HRESULT hr;
    HANDLE th;

    if (s_ready)
        return 1;
    if (!want_gpu()) {
        fprintf(stderr, "[GPU] TUROK_RENDERER=cpu: drawing on the CPU\n");
        return 0;
    }

    s_window_ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    th = s_window_ready ? CreateThread(NULL, 0, window_thread, NULL, 0, NULL) : NULL;
    if (!th) {
        fprintf(stderr, "[GPU] no window thread (error %lu); drawing on the CPU\n", GetLastError());
        return 0;
    }
    CloseHandle(th);
    WaitForSingleObject(s_window_ready, INFINITE);
    if (!s_window) {
        fprintf(stderr, "[GPU] no window; drawing on the CPU\n");
        return 0;
    }

    d3d = xbox_Direct3DCreate8(0);
    if (!d3d) {
        fprintf(stderr, "[GPU] Direct3DCreate8 failed; drawing on the CPU\n");
        PostMessageW(s_window, WM_CLOSE, 0, 0);
        return 0;
    }
    memset(&pp, 0, sizeof(pp));
    pp.BackBufferWidth        = GT_WIDTH;
    pp.BackBufferHeight       = GT_HEIGHT;
    pp.BackBufferFormat       = D3DFMT_A8R8G8B8;
    pp.BackBufferCount        = 1;
    pp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow          = s_window;
    pp.Windowed               = TRUE;
    pp.EnableAutoDepthStencil = FALSE;
    hr = d3d->lpVtbl->CreateDevice(d3d, 0, 1 /* HAL */, s_window,
                                   0x00000040 /* SOFTWARE_VERTEXPROCESSING */, &pp, &dev);
    if (FAILED(hr) || !dev) {
        /* No usable GPU (or no D3D11 driver). The window would only stay
         * black, so hide it: the CPU path opens its own. */
        fprintf(stderr, "[GPU] CreateDevice failed (0x%08lX); drawing on the CPU\n",
                (unsigned long)hr);
        ShowWindow(s_window, SW_HIDE);
        return 0;
    }

    {
        /* At most one frame queued ahead of the display: DXGI's default of
         * three adds ~50 ms between a press and its picture. */
        extern void *d3d8_GetD3D11Device(void);
        IUnknown *device = (IUnknown *)d3d8_GetD3D11Device();
        IDXGIDevice1 *dxgi = NULL;
        if (device && SUCCEEDED(device->lpVtbl->QueryInterface(device, &IID_IDXGIDevice1,
                                                               (void **)&dxgi))) {
            dxgi->lpVtbl->SetMaximumFrameLatency(dxgi, 1);
            dxgi->lpVtbl->Release(dxgi);
        }
    }

    pgraph_d3d11_init();
    s_ready = 1;
    fprintf(stderr, "[GPU] drawing on the GPU (Direct3D 11), %dx%d picture\n", GT_WIDTH, GT_HEIGHT);
    fflush(stderr);
    return 1;
}

/* Write the back buffer as a 24-bit BMP (no library needed, anything opens
 * it). Bring-up only: RECOMP_TRANS_SHOT=<path>. */
static void save_backbuffer(IDirect3DDevice8 *dev, const char *path)
{
    IDirect3DSurface8 *surf = NULL;
    D3DLOCKED_RECT lr;
    FILE *f;
    uint32_t w = GT_WIDTH, h = GT_HEIGHT, row = ((GT_WIDTH * 3) + 3) & ~3u, x, y;
    uint32_t size = 54 + row * h;
    uint8_t hdr[54];
    uint8_t *line;

    if (FAILED(dev->lpVtbl->GetBackBuffer(dev, 0, 0 /* D3DBACKBUFFER_TYPE_MONO */, &surf)) || !surf)
        return;
    if (FAILED(surf->lpVtbl->LockRect(surf, &lr, NULL, D3DLOCK_READONLY))) {
        surf->lpVtbl->Release(surf);
        return;
    }
    f = fopen(path, "wb");
    line = (uint8_t *)malloc(row);
    if (f && line) {
        memset(hdr, 0, sizeof hdr);
        hdr[0] = 'B'; hdr[1] = 'M';
        memcpy(hdr + 2, &size, 4);
        hdr[10] = 54; hdr[14] = 40;
        memcpy(hdr + 18, &w, 4);
        memcpy(hdr + 22, &h, 4);
        hdr[26] = 1; hdr[28] = 24;
        fwrite(hdr, 1, sizeof hdr, f);
        for (y = 0; y < h; y++) {        /* BMP rows run bottom-up */
            const uint8_t *src = (const uint8_t *)lr.pBits + (size_t)(h - 1 - y) * lr.Pitch;
            memset(line, 0, row);
            for (x = 0; x < w; x++) {    /* A8R8G8B8 in memory: B, G, R, A */
                line[x * 3 + 0] = src[x * 4 + 0];
                line[x * 3 + 1] = src[x * 4 + 1];
                line[x * 3 + 2] = src[x * 4 + 2];
            }
            fwrite(line, 1, row, f);
        }
    }
    if (f) fclose(f);
    free(line);
    surf->lpVtbl->UnlockRect(surf);
    surf->lpVtbl->Release(surf);
}

/* The translator draws but does not own the swap chain, so it calls this at
 * the end of every frame and the host presents. Without it every frame would
 * be rendered and thrown away. */
void gpu_translator_frame_end(void)
{
    IDirect3DDevice8 *dev;
    if (!s_ready)
        return;
    dev = xbox_GetD3DDevice();
    if (!dev)
        return;
    {
        /* RECOMP_TRANS_SHOT=<path.bmp>: the presented picture, for bring-up. */
        static int shot_done;
        const char *path = getenv("RECOMP_TRANS_SHOT");
        const char *every = getenv("RECOMP_TRANS_SHOT_EVERY");
        static unsigned frames;
        unsigned n = every ? (unsigned)atoi(every) : 0;
        frames++;
        if (path && ((n && frames % n == 0) || (!n && !shot_done && frames == 300))) {
            shot_done = 1;
            save_backbuffer(dev, path);
        }
    }
    dev->lpVtbl->Present(dev, NULL, NULL, NULL, NULL);
}

#else  /* !_WIN32 */

int gpu_translator_init(void) { return 0; }
void gpu_translator_frame_end(void) { }

#endif
