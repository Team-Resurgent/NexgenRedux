/* Only where FBX is read. */
#if defined(NX_FBX)
#include "third_party/ufbx/ufbx.c"
#else
typedef int nx_no_fbx;
#endif
