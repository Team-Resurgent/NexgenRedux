#include "ECSManager.h"
#include "EntityEngine/SceneManager.h"
#include "EntityEngine/NodeManager.h"

#include <Gensys/DebugUtility.h>

using namespace Gensys;

void ECSManager::ECSManagerExample(void) 
{
    // ECSEntityManager entityManager;
    // ECSRenderSystem* renderSystem = new ECSRenderSystem();

    // ECSEntity* parentEntity = entityManager.CreateEntity();
    // parentEntity->AddComponent(new ECSTransformComponent(0, 0, 0));

    // ECSEntity* childEntity = entityManager.CreateEntity();
    // childEntity->AddComponent(new ECSTransformComponent(1, 1, 1));

    // parentEntity->AddChild(childEntity->GetID());
    // childEntity->SetParentID(parentEntity->GetID());

    // while (true) {
    //     // Update systems
    //     renderSystem->Update(&entityManager, parentEntity, 0.016f);



    //     // Exit loop
    //     break;
    // }

	// // Destroy entities
	// entityManager.DestroyEntity(childEntity->GetID());
	// entityManager.DestroyEntity(parentEntity->GetID());

	// // Clean up systems
	// delete renderSystem;





    SceneManager* sceneManager = new SceneManager();
    uint32_t sceneID = sceneManager->CreateScene(true);
    sceneManager->SetCurrentScene(sceneID);

    NodeManager* nodeManager = new NodeManager(sceneManager);

    uint32_t nodeID1 = nodeManager->CreateSceneNode(sceneID);
    uint32_t nodeID2 = nodeManager->CreateNode(sceneID);
    uint32_t nodeID3 = nodeManager->CreateNode(nodeID2);
    uint32_t nodeID4 = nodeManager->CreateNode(nodeID2);
     uint32_t nodeID5 = nodeManager->CreateNodeAt(nodeID2, nodeID4);

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