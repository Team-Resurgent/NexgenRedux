#include "ECSManager.h"
#include "EntityEngine/SceneManager.h"
#include "EntityEngine/NodeManager.h"

#include <Gensys/DebugUtility.h>

using namespace Gensys;
using namespace NexgenRedux;

void ECSManager::ECSManagerExample(void) 
{
    SceneManager* sceneManager = new SceneManager();
    uint32_t sceneID = sceneManager->CreateScene(true);
    sceneManager->SetCurrentScene(sceneID);

    NodeManager* nodeManager = new NodeManager(sceneManager);

    uint32_t nodeID1 = nodeManager->CreateSceneNode(NodeTypeCamera, sceneID);
    uint32_t nodeID2 = nodeManager->CreateNode(NodeTypeSprite, sceneID);
    uint32_t nodeID3 = nodeManager->CreateNode(NodeTypeSprite, nodeID2);
    uint32_t nodeID4 = nodeManager->CreateNode(NodeTypeSprite, nodeID2);
    uint32_t nodeID5 = nodeManager->CreateNodeAt(NodeTypeSprite, nodeID2, nodeID4);

    sceneManager->Update(nodeManager, 0.1f);
    sceneManager->Render(nodeManager);

    // nodeManager->DeleteNode(nodeID3);
    // sceneManager->Update(nodeManager, 0.1f);
    // sceneManager->Render(nodeManager);

    nodeManager->PurgeNodes();
    nodeManager->CheckForOrphans();

    delete sceneManager;
    delete nodeManager;

    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "Done entity work.");
}