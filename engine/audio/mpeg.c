/* pl_mpeg, compiled as C here so none of its macros or warnings reach engine code. Its memory comes from the
   engine's, as the sound libraries' does. */

#include <stddef.h>

extern void* nxAudioRealloc(void* old, size_t size);

#define PLM_MALLOC(sz) nxAudioRealloc(NULL, (sz))
#define PLM_REALLOC(p, sz) nxAudioRealloc((p), (sz))
#define PLM_FREE(p) ((p) ? (void)nxAudioRealloc((p), 0) : (void)0)
#define PLM_NO_STDIO
#define PL_MPEG_IMPLEMENTATION
#include "third_party/pl_mpeg/pl_mpeg.h"
