// multivoid bootstrap entry.
//
// The mod DLL has ONE way into the process (UE4SS_ARC WP-2 commit 3 retired
// the standalone xinput-proxy lane whole, its filename lane-discriminator
// included): UE4SS LoadLibrary's it as Mods/Multivoid/dlls/main.dll at
// mod-SCAN time (for every mod found, enabled or not) and starts ENABLED mods
// later via the exported start_mod() (src/loader/cppmod_entry.cpp). Nothing
// boots from ATTACH -- a disabled mod folder is LOADED but never STARTED, so
// DllMain must not boot. DETACH performs only the lock-free GC-pin retirement guard.

#include "ue_wrap/core/gc_pin.h"

#include <windows.h>

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        ::DisableThreadLibraryCalls(module);
    } else if (reason == DLL_PROCESS_DETACH) {
        // FIRST, and it is one relaxed atomic store -- nothing else. From here a
        // GcPin releases WITHOUT touching the engine or the registry lock.
        //
        // It has to be HERE and not only in DoShutdown, because DoShutdown runs
        // only from CoopWndProc's close branch: the game's own quit (RequestExit ->
        // FEngineLoop::Exit) never reaches it. Pins live inside statics -- the proxy
        // map holds up to ~871 -- and the CRT runs those destructors after this
        // callback, where un-rooting would deref a freed UObject and walk a
        // GUObjectArray UE has already torn down. Two independent post-ship audits
        // found this on the same day the pins shipped.
        ue_wrap::GcPin::StopReleases();
        // Nothing else is loader-lock-safe here: no persistence, filesystem/CRT
        // I/O, logging, synchronization, network teardown, or worker coordination.
        // Normal DoShutdown owns the coherent final profile + logger barriers;
        // periodic and ownership checkpoints cover exits that bypass it.
    }
    return TRUE;
}
