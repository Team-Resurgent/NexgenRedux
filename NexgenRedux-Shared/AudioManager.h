#pragma once

#include <Gensys/Int.h>

#include <string>

namespace NexgenRedux
{
	class AudioManager
	{		
	public:

		static AudioManager* GetInstance();
		
		void Init();
		void Close();
		void Update();
		uint32_t PlayAudio(const std::string path, bool loop);

	};
}
