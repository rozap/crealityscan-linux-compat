/* Local Rusticl fix. The scanner SDK only considers GPUs on platforms whose
 * vendor it recognises, so it finds no device on Mesa's Rusticl platform and
 * scanning fails with OB_SCAN_ERROR_GRAPHICS_PROBLEM. Report AMD's platform
 * name and vendor for the Rusticl platform only. Devices are unchanged. */
#define _GNU_SOURCE
#define CL_TARGET_OPENCL_VERSION 220
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef int32_t cl_int;
typedef uint32_t cl_uint;
typedef struct _cl_platform_id *cl_platform_id;
#define CL_INVALID_VALUE -30
#define CL_INVALID_OPERATION -59
#define CL_PLATFORM_NAME 0x0902
#define CL_PLATFORM_VENDOR 0x0903

typedef cl_int (*info_fn)(cl_platform_id, cl_uint, size_t, void *, size_t *);
static pthread_once_t once = PTHREAD_ONCE_INIT;
static info_fn platform_info;
static int enabled;
static void initialize(void) {
    void *lib = dlopen("libOpenCL.so.1", RTLD_NOW | RTLD_LOCAL);
    if (lib) platform_info = (info_fn)dlsym(lib, "clGetPlatformInfo");
    const char *opt = getenv("CREALITY_RUSTICL_VENDOR_FIX");
    const char *prefix = getenv("WINEPREFIX");
    const char *guard = getenv("CREALITY_PREFIX_GUARD");
    enabled = opt && !strcmp(opt, "1") && prefix && guard && !strcmp(prefix, guard);
}
static int rusticl(cl_platform_id platform) {
    char vendor[64] = {0};
    return !platform_info(platform, CL_PLATFORM_VENDOR, sizeof(vendor), vendor, NULL) &&
        !strcmp(vendor, "Mesa/X.org");
}
cl_int clGetPlatformInfo(cl_platform_id platform, cl_uint param, size_t size,
                         void *value, size_t *size_ret) {
    pthread_once(&once, initialize);
    if (!platform_info) return CL_INVALID_OPERATION;
    if (!enabled || (param != CL_PLATFORM_NAME && param != CL_PLATFORM_VENDOR) || !rusticl(platform))
        return platform_info(platform, param, size, value, size_ret);
    const char *text = param == CL_PLATFORM_VENDOR ? "Advanced Micro Devices, Inc."
                                                   : "AMD Accelerated Parallel Processing";
    size_t needed = strlen(text) + 1;
    if (value) {
        if (size < needed) return CL_INVALID_VALUE;
        memcpy(value, text, needed);
    }
    if (size_ret) *size_ret = needed;
    return 0;
}
