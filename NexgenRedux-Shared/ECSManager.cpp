#include "ECSManager.h"
#include "EntityEngine/SceneManager.h"
#include "EntityEngine/NodeManager.h"
#include "EntityEngine/Scene.h"

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

    NodeManager* nodeManager = new NodeManager();
    SceneManager* sceneManager = new SceneManager(nodeManager);
    uint32_t sceneID = sceneManager->CreateScene("myscene", true);
    uint32_t nodeID1 = sceneManager->CreateSceneNode(sceneID, "mynode1");
    uint32_t nodeID2 = sceneManager->CreateSceneNode(sceneID, "mynode2");
    //uint32_t nodeID = nodeManager->CreateNode();

    uint32_t nodeID3 = nodeManager->CreateNode("mynode3");
    uint32_t nodeID4 = nodeManager->CreateChildNode(nodeID1, "mynode4");
    uint32_t nodeID4a = nodeManager->CreateChildNode(nodeID4, "mynode4a");
    uint32_t nodeID5 = nodeManager->CreateChildNode(nodeID4, "mynode5");
   // nodeManager->MarkForDelete(nodeID3);

    nodeManager->MarkForDelete(nodeID1);

    sceneManager->Update(0.1f);
    sceneManager->Render();

    sceneManager->CleanUp();

    nodeManager->CheckForOrphans();

    delete sceneManager;
    delete nodeManager;

    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "Done entity work.");
}