#include "ECSManager.h"
#include "EntityEngine/SceneManager.h"
#include "EntityEngine/NodeManager.h"
#include "EntityEngine/Sprite.h"

#include <Gensys/DebugUtility.h>

using namespace Gensys;
using namespace NexgenRedux;

void ECSManager::ECSManagerExample(void) 
{
    uint32_t sceneID = SceneManager::CreateScene(true);
    SceneManager::SetCurrentScene(sceneID);


    //NodeSprite* nodeSprite1 = new NodeSprite();
    // uint32_t nodeID1 = NodeManager::AddSceneNode(nodeSprite1, sceneID);
    // //NodeSprite* nodeSprite2 = new NodeSprite();
    // uint32_t nodeID2 = NodeManager::AddNode(nodeSprite2, sceneID);
    // //NodeSprite* nodeSprite3 = new NodeSprite();
    // uint32_t nodeID3 = NodeManager::AddNode(nodeSprite3, nodeID2);
    // //NodeSprite* nodeSprite4 = new NodeSprite();
    // uint32_t nodeID4 = NodeManager::AddNode(nodeSprite4, nodeID2);
    // //NodeSprite* nodeSprite5 = new NodeSprite();
    // uint32_t nodeID5 = NodeManager::AddNodeAt(nodeSprite5, nodeID2, nodeID4);

    SceneManager::Update(0.1f);
    SceneManager::Render();

    // nodeManager->DeleteNode(nodeID3);
    // sceneManager->Update(nodeManager, 0.1f);
    // sceneManager->Render(nodeManager);

    NodeManager::PurgeNodes();
    NodeManager::CheckForOrphans();

    NodeManager::Close();

    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "Done entity work.");
}