#include "audio/audio.h"
#include "base/log.h"

// The Xbox 360's sound. Until XAudio2 is joined up, a clock that plays nothing, so that everything else runs as it
// will.
namespace nx
{
    AudioOutput* createAudioOutput()
    {
        NX_WARN("audio: nothing is heard yet");
        return createNullAudioOutput(true);
    }
}
