/**
 * Bug workaround for ZebraNativeUsbAdapter_64.dll
 * This DLL is intended to act as a DLL proxy for MSVCR90.DLL imported by ZebraNativeUsbAdapter_64.dll
 * It will patch the memory allocator with a workaround so everything works under 64 bits.
 *
 * MQALLOC: Absolutely terrible but sufficiently working memory allocator(TM).
 * Ensures that the virtual addresses of the memory allocated with `operator new` or `operator delete`
 * are below 2^32, so there wouldn't be a crash when the pointer is accidentally truncated from uint64_t
 * to uint32_t anywhere in the process (lol).
 */

#include <windows.h>
#include <stdio.h>
#include <stdint.h>

#pragma comment(linker, "/export:_lock=MSVCR90._lock")
#pragma comment(linker, "/export:__dllonexit=MSVCR90.__dllonexit")
#pragma comment(linker, "/export:_unlock=MSVCR90._unlock")
#pragma comment(linker, "/export:__clean_type_info_names_internal=MSVCR90.__clean_type_info_names_internal")
#pragma comment(linker, "/export:strncpy_s=MSVCR90.strncpy_s")
#pragma comment(linker, "/export:_strnicmp=MSVCR90._strnicmp")
#pragma comment(linker, "/export:?terminate@@YAXXZ=MSVCR90.?terminate@@YAXXZ")
#pragma comment(linker, "/export:__crt_debugger_hook=MSVCR90.__crt_debugger_hook")
#pragma comment(linker, "/export:__CppXcptFilter=MSVCR90.__CppXcptFilter")
#pragma comment(linker, "/export:__C_specific_handler=MSVCR90.__C_specific_handler")
#pragma comment(linker, "/export:_amsg_exit=MSVCR90._amsg_exit")
#pragma comment(linker, "/export:_decode_pointer=MSVCR90._decode_pointer")
#pragma comment(linker, "/export:_encoded_null=MSVCR90._encoded_null")
#pragma comment(linker, "/export:_initterm_e=MSVCR90._initterm_e")
#pragma comment(linker, "/export:_initterm=MSVCR90._initterm")
#pragma comment(linker, "/export:_malloc_crt=MSVCR90._malloc_crt")
#pragma comment(linker, "/export:_encode_pointer=MSVCR90._encode_pointer")
#pragma comment(linker, "/export:_onexit=MSVCR90._onexit")
#pragma comment(linker, "/export:__CxxFrameHandler3=MSVCR90.__CxxFrameHandler3")
#pragma comment(linker, "/export:?what@exception@std@@UEBAPEBDXZ=MSVCR90.?what@exception@std@@UEBAPEBDXZ")
#pragma comment(linker, "/export:tolower=MSVCR90.tolower")
#pragma comment(linker, "/export:strcpy_s=MSVCR90.strcpy_s")
#pragma comment(linker, "/export:_invalid_parameter_noinfo=MSVCR90._invalid_parameter_noinfo")
#pragma comment(linker, "/export:??1exception@std@@UEAA@XZ=MSVCR90.??1exception@std@@UEAA@XZ")
#pragma comment(linker, "/export:malloc=MSVCR90.malloc")
#pragma comment(linker, "/export:free=MSVCR90.free")
#pragma comment(linker, "/export:?_type_info_dtor_internal_method@type_info@@QEAAXXZ=MSVCR90.?_type_info_dtor_internal_method@type_info@@QEAAXXZ")
#pragma comment(linker, "/export:_CxxThrowException=MSVCR90._CxxThrowException")

#define STATUS_NO_MEM_REGION         0xE000AC01
#define STATUS_TOO_BIG_ALLOC         0xE000AC02
#define STATUS_OUT_OF_STATIC_POOL    0xE000AC03
#define STATUS_DOUBLE_FREE           0xE000AC04
#define STATUS_BAD_FREE              0xE000AC05
#define STATUS_INIT_FAILED           0xE000AC06

#define ALLOC_CHUNKS_NUM  (468)
#define ALLOC_CHUNK_SIZE  (280)

#define MEM_REGION_SIZE               (ALLOC_CHUNKS_NUM * ALLOC_CHUNK_SIZE)
#define MEM_REGION_SEARCH_START_ADDR  (0x05000000)
#define MEM_REGION_SEARCH_END_ADDR    (0x08000000)
#define MEM_REGION_SEARCH_STEP        (  0x100000)

uint8_t enable_debug = FALSE;
void *mem_region = NULL;
uint8_t alloc_table[ALLOC_CHUNKS_NUM];

void fail(DWORD excCode) {
    RaiseException(excCode, EXCEPTION_NONCONTINUABLE_EXCEPTION, 0, NULL);
}

#pragma comment(linker, "/export:??2@YAPEAX_K@Z=patch_operator_new")
void *__fastcall patch_operator_new(unsigned __int64 size) {
    if (mem_region == NULL) {
        fprintf(stderr, "[MQALLOC] BUG! Allocation failed, mem_region is not initialized.\n");
        fflush(stderr);
        fail(STATUS_NO_MEM_REGION);
        return NULL;
    }

    if (size > ALLOC_CHUNK_SIZE) {
        fprintf(stderr, "[MQALLOC] BUG! Allocation of %lld bytes failed, we only support "
               "allocating up to %lld bytes at once.\n", size, (unsigned long long) ALLOC_CHUNK_SIZE);
        fflush(stderr);
        fail(STATUS_TOO_BIG_ALLOC);
        return NULL;
    }

    for (int i = 0; i < ALLOC_CHUNKS_NUM; i++) {
        if (alloc_table[i] == 0) {
            alloc_table[i] = TRUE;
            uint64_t off = i * ALLOC_CHUNK_SIZE;

            void* out = (void *) ((uint64_t) mem_region + off);

            if (enable_debug) {
                fprintf(stderr, "[MQALLOC] patch_operator_new(%lld) "
                       "=> allocated 0x%llx\n", size, (unsigned long long) out);
                fflush(stderr);
            }

            return out;
        }
    }

    fprintf(stderr, "[MQALLOC] BUG! Unable to allocate memory out of static pool. "
           "More than %d open printers?\n", ALLOC_CHUNKS_NUM);
    fflush(stderr);
    fail(STATUS_OUT_OF_STATIC_POOL);
    return NULL;
}

#pragma comment(linker, "/export:??3@YAXPEAX@Z=patch_operator_delete")
void __fastcall patch_operator_delete(void *ptr) {
    if (mem_region == NULL) {
        fprintf(stderr, "[MQALLOC] BUG! Deallocation failed, mem_region is not initialized.\n");
        fflush(stderr);
        fail(STATUS_NO_MEM_REGION);
        return;
    }

    if (enable_debug) {
        fprintf(stderr, "[MQALLOC] patch_operator_delete(0x%llx)\n", (uint64_t) ptr);
        fflush(stderr);
    }

    for (int i = 0; i < ALLOC_CHUNKS_NUM; i++) {
        uint64_t off = i * ALLOC_CHUNK_SIZE;

        if (ptr == (void *)((uint64_t) mem_region + off)) {
            if (!alloc_table[i]) {
                fprintf(stderr, "[MQALLOC] BUG! Freeing region that was not allocated: 0x%llx.\n",
                       (unsigned long long) ptr);
                fflush(stderr);
                fail(STATUS_DOUBLE_FREE);
                return;
            }

            alloc_table[i] = FALSE;
            return;
        }
    }

    fprintf(stderr, "[MQALLOC] BUG! Requested to delete unrecognized pointer: 0x%llx\n",
            (unsigned long long) ptr);
    fflush(stderr);
    fail(STATUS_BAD_FREE);
}

BOOL WINAPI DllMain(
        HINSTANCE hinstDLL,
        DWORD fdwReason,
        LPVOID lpReserved )
{
    switch( fdwReason )
    {
        case DLL_PROCESS_ATTACH:
            char inBuff[256];
            enable_debug = 0;
            DWORD getEnvRes = GetEnvironmentVariable("DEBUG_MQALLOC", inBuff, 256);

            if (getEnvRes > 0 && getEnvRes < 256 && strcmp(inBuff, "1") == 0) {
                enable_debug = TRUE;
            }

            if (enable_debug) {
                fprintf(stderr,
                        "[MQALLOC] Loading ZebraNativeUsbAdapter_64.dll allocator compatibility hack by MQ\n");
                fprintf(stderr, ""
                       "                       __________.__        __   __                __ ________  \n"
                       "   /\\_/\\               \\____    /|__|__ ___/  |_|  | _______      / / \\_____  \\ \n"
                       "   >^.^<.---.            /     / |  |  |  \\   __\\  |/ /\\__  \\    / /    _(__  < \n"
                       "  _'-`-'     )\\         /     /_ |  |  |  /|  | |    <  / __ \\_  \\ \\   /       \\\n"
                       " (6--\\ |--\\ (`.`-.     /_______ \\|__|____/ |__| |__|_ \\(____  /   \\_\\ /______  /\n"
                       "     --'  --'  ``-'BP          \\/                    \\/     \\/               \\/ \n");
            }

            for (int i = 0; i < ALLOC_CHUNKS_NUM; i++) {
                alloc_table[i] = FALSE;
            }

            for (uint64_t cptr = MEM_REGION_SEARCH_START_ADDR;
                    cptr < MEM_REGION_SEARCH_END_ADDR;
                    cptr += MEM_REGION_SEARCH_STEP) {
                mem_region = VirtualAlloc((void *) cptr, MEM_REGION_SIZE,
                                          MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

                if (mem_region != NULL) {
                    break;
                }
            }

            if (mem_region == NULL) {
                fprintf(stderr, "[MQALLOC] BUG! Failed to allocate low-address region.\n");
                fflush(stderr);
                fail(STATUS_INIT_FAILED);
                return FALSE;
            }

            if (enable_debug) {
                fprintf(stderr, "[MQALLOC] Allocated low-address region: 0x%llx\n",
                        (unsigned long long) mem_region);
                fflush(stderr);
            }
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;

        case DLL_PROCESS_DETACH:
            if (enable_debug) {
                fprintf(stderr, "[MQALLOC] Detaching from the process\n");
                fflush(stderr);
            }

            VirtualFree(mem_region, MEM_REGION_SIZE, MEM_RELEASE);
            break;
    }

    return TRUE;
}
