#pragma once

#include "FileSystem.h"

namespace Gensys
{
	class Crc32
	{
	public:
		Crc32();
		unsigned long CalcCrc(char *buffer, long size);
		unsigned long CalcCrc(FileSystem::FileInfo fileInfo);  			
	protected:
		unsigned long Reflect(long ref, char ch);
		unsigned long Table[256];
		void Calculate(char *buffer, long size, unsigned long &crc);
	};
}
