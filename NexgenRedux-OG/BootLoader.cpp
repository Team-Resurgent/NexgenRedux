#include "BootLoader.h"
#include <Gensys/DriveManager.h>
#include "ConfigLoader.h"

using namespace Gensys;
using namespace NexgenRedux;

void BootLoader::Run()
{
	ConfigData configData;
	memset(&configData, 0, sizeof(configData));

	ConfigLoader *configLoader = new ConfigLoader();
	if (configLoader->LoadConfig(&configData) == true) 
	{
		// do something
	}
	delete configLoader;
}