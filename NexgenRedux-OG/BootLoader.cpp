#include "BootLoader.h"
#include "DriveManager.h"
#include "ConfigLoader.h"

using namespace Gensys;
using namespace NexgenRedux;

void BootLoader::Run()
{
	DriveManager::Init();

	ConfigData configData;
	memset(&configData, 0, sizeof(configData));

	ConfigLoader *configLoader = new ConfigLoader();
	if (configLoader->LoadConfig(&configData) == true) 
	{
		// do something
	}
	delete configLoader;
}