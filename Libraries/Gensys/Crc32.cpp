#include "Crc32.h"

using namespace Gensys;

Crc32::Crc32()
{
    const unsigned long ulPolynomial = 0x04C11DB7;
    for (int i = 0; i <= 0xff; i++)
    {
        Table[i] = Reflect(i, 8) << 24;
        for (int j = 0; j < 8; j++) {
            Table[i] = (Table[i] << 1) ^ (Table[i] & (1 << 31) ? ulPolynomial : 0);
		}
        Table[i] = Reflect(Table[i], 32);
    }
}

unsigned long Crc32::CalcCrc(char *buffer, long size)
{
	unsigned long crc = 0xffffffff;
	Calculate(buffer, size, crc);
	return crc;
}

unsigned long Crc32::CalcCrc(uint32_t fileHandle)
{   
	unsigned long crc = 0xffffffff;
	char *buffer = new char[65535];
	uint32_t bytesRead = 0;
	while (true)
	{
		if (FileSystem::FileRead(fileHandle, buffer, 65535, bytesRead)) {
			return 0;
		}
		if (bytesRead == 0) {
			break;
		}
		Calculate(buffer, bytesRead, crc);
	}
    delete[]buffer;
    return crc ^ 0xffffffff;
}
    
unsigned long Crc32::Reflect(long ref, char ch)
{
    unsigned long value = 0;
    for (int i = 1; i < (ch + 1); i++) {
        if (ref & 1) {
            value |= 1 << (ch - i);
		}
        ref >>= 1;
    }
    return value;
}

void Crc32::Calculate(char *buffer, long size, unsigned long &crc)
{
    char *pbuffer = buffer;
    while (size--) {
        crc = (crc >> 8) ^ Table[(crc & 0xff) ^ *pbuffer++];
	}
}