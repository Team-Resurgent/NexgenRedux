#include "BootLoader.h"
#include <Gensys/DriveManager.h>
#include "ConfigLoader.h"
#include <cstring>

using namespace Gensys;
using namespace NexgenRedux;

void BootLoader::Run()
{
	ConfigLoader::ConfigData configData;
	memset(&configData, 0, sizeof(configData));

	if (ConfigLoader::LoadConfig(&configData) == true) 
	{
		// do something
	}
}