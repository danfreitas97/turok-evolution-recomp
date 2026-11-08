/* Shared DXGI enumeration for the launcher and D3D11 renderer. */
#ifndef TUROK_GPU_SELECT_WIN32_H
#define TUROK_GPU_SELECT_WIN32_H
#include <dxgi.h>
#include <string.h>
static int turok_gpu_name(IDXGIAdapter *adapter, char *name, size_t size, SIZE_T *memory)
{
    DXGI_ADAPTER_DESC desc;
    if (FAILED(IDXGIAdapter_GetDesc(adapter, &desc)) || desc.VendorId == 0x1414) return 0;
    if (!WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, (int)size, NULL, NULL)) return 0;
    if (memory) *memory = desc.DedicatedVideoMemory;
    return 1;
}
/* A description is persisted rather than a volatile enumeration index. */
static IDXGIAdapter *turok_gpu_select(const char *requested, int *matched)
{
    IDXGIFactory *factory = NULL;
    IDXGIAdapter *adapter = NULL, *best = NULL;
    SIZE_T best_memory = 0;
    UINT i;
    *matched = 0;
    if (FAILED(CreateDXGIFactory(&IID_IDXGIFactory, (void **)&factory))) return NULL;
    for (i = 0; IDXGIFactory_EnumAdapters(factory, i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
        char name[256]; SIZE_T memory;
        if (turok_gpu_name(adapter, name, sizeof name, &memory)) {
            if (requested && *requested && strcmp(requested, name) == 0) {
                if (best) IDXGIAdapter_Release(best);
                best = adapter; *matched = 1; break;
            }
            if (!best || memory > best_memory) {
                if (best) IDXGIAdapter_Release(best);
                best = adapter; best_memory = memory; adapter = NULL;
            }
        }
        if (adapter) IDXGIAdapter_Release(adapter);
    }
    IDXGIFactory_Release(factory);
    return best;
}
#endif
