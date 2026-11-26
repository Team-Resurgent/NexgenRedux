#include "gpu/gpu.h"
#include "base/log.h"

// Direct3D 9 on the Xbox 360. Until it is written, the title runs without drawing.
namespace nx::gpu
{
    Device* createDevice(void*)
    {
        NX_WARN("d3d9: nothing is drawn yet");
        return createNullDevice(1280, 720);
    }
}
