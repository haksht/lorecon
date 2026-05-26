/* Force-included via -include flag (build_src_flags) before each tbeam_supreme TU.
 *
 * qio_qspi/include/sdkconfig.h defines CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=1,
 * which causes lwip headers to pull in PSRAM-backed memory-pool code. This
 * roughly doubles the lwip header tree, and 32-bit Windows GCC 8.4 cc1plus.exe
 * runs out of contiguous virtual address space even at -O0.
 *
 * Strategy: include sdkconfig.h here first (sets the header guard so it won't
 * be re-processed later), then undef the bloat macros. Subsequent #include
 * <sdkconfig.h> calls in lwip/WiFi headers are no-ops. Net effect: lwip compiles
 * with the smaller code path while PSRAM hardware support stays enabled in the
 * IDF .a files (which were pre-compiled with CONFIG_SPIRAM=1).
 */
#include <sdkconfig.h>

/* Strip SPIRAM macros that expand lwip into massive TUs.
 * CONFIG_SPIRAM itself is kept -- psramInit() in the Arduino core (a pre-compiled
 * .c file) checks it, and we keep it so ESP.getPsramSize() and friends are
 * conditionally compiled correctly in any core header that checks it. */
#ifdef CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP
#undef CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP
#endif
#ifdef CONFIG_SPIRAM_USE_MALLOC
#undef CONFIG_SPIRAM_USE_MALLOC
#endif
