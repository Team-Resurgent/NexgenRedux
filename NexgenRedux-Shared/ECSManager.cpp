#include "ECSManager.h"

void ECSManager::ECSManagerExample(void) 
{
    ECSEntityManager entityManager;
    ECSRenderSystem* renderSystem = new ECSRenderSystem();

    ECSEntity* parentEntity = entityManager.CreateEntity();
    parentEntity->AddComponent(new ECSTransformComponent(0, 0, 0));

    ECSEntity* childEntity = entityManager.CreateEntity();
    childEntity->AddComponent(new ECSTransformComponent(1, 1, 1));

    parentEntity->AddChild(childEntity->GetID());
    childEntity->SetParentID(parentEntity->GetID());

    while (true) {
        // Update systems
        renderSystem->Update(&entityManager, parentEntity, 0.016f);



        // Exit loop
        break;
    }

	// Destroy entities
	entityManager.DestroyEntity(childEntity->GetID());
	entityManager.DestroyEntity(parentEntity->GetID());

	// Clean up systems
	delete renderSystem;

}