#pragma once

#include <string>

namespace NexgenRedux
{
	class AudioManager
	{		
	public:

                static void Init();
                static void Close();
                static void Update();
                static uint32_t PlayAudio(const std::string path, bool loop);
                static void PlayAudiox();
	};
}
