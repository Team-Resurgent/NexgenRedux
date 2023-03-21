#include "ECSManager.h"
#include "EntityEngine/SceneManager.h"
#include "EntityEngine/NodeManager.h"
#include "EntityEngine/NodeSprite.h"

#include <Gensys/DebugUtility.h>

using namespace Gensys;
using namespace NexgenRedux;

void ECSManager::ECSManagerExample(void) 
{
    SceneManager* sceneManager = new SceneManager();
    uint32_t sceneID = sceneManager->CreateScene(true);
    sceneManager->SetCurrentScene(sceneID);

    NodeManager* nodeManager = new NodeManager(sceneManager);

    NodeSprite* nodeSprite1 = new NodeSprite();
    uint32_t nodeID1 = nodeManager->AddSceneNode(nodeSprite1, sceneID);
    NodeSprite* nodeSprite2 = new NodeSprite();
    uint32_t nodeID2 = nodeManager->AddNode(nodeSprite2, sceneID);
    NodeSprite* nodeSprite3 = new NodeSprite();
    uint32_t nodeID3 = nodeManager->AddNode(nodeSprite3, nodeID2);
    NodeSprite* nodeSprite4 = new NodeSprite();
    uint32_t nodeID4 = nodeManager->AddNode(nodeSprite4, nodeID2);
    NodeSprite* nodeSprite5 = new NodeSprite();
    uint32_t nodeID5 = nodeManager->AddNodeAt(nodeSprite5, nodeID2, nodeID4);

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