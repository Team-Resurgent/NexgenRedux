#include "util.as"

WindowDetails OnWindowCreate()
{
    DebugPrint(LogLevelInfo, "OnWindowCreate");
    
	WindowDetails windowDetails = WindowDetails();
	windowDetails.monitorIndex = 0;
	windowDetails.width = 640;
	windowDetails.height = 480;
	return windowDetails;
}

void Init()
{
    
}

void Update(double dt)
{
    
}
