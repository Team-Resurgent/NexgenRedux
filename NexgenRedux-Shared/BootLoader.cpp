#include "BootLoader.h"
#include <Gensys/DriveManager.h>
#include "ConfigLoader.h"
#include <cstring>

using namespace Gensys;
using namespace NexgenRedux;

void BootLoader::Run()
{
	ConfigData configData;
	memset(&configData, 0, sizeof(configData));

	ConfigLoader configLoader = ConfigLoader();
	if (configLoader.LoadConfig(&configData) == true) 
	{
		// do something
	}
}