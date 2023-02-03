#pragma once

namespace Gensys
{
	class Memory
	{
	public:
		static unsigned long long GetTotalVirtualMemory();
		static unsigned long long GetFreeVirtualMemory();
		static unsigned long long GetTotalPhysicalMemory();
		static unsigned long long GetFreePhysicalMemory();
	};
}