#include "ECSRenderSystem.h"

void ECSRenderSystem::Update(ECSEntityManager* entityManager, ECSEntity* parentEntity, float dt)
{
    // Loop through all entities with a transform component, need to recurse
    std::vector<uint32_t> children = parentEntity->GetChildren();
    for (uint32_t i = 0; i < children.size(); i++)
    {
        uint32_t childId = children.at(i);
        ECSEntity* entity = entityManager->GetEntity(childId);
        if (entity != NULL) 
        {
            RenderEntity(entity);
        }
    }
}

// Privates

void ECSRenderSystem::RenderEntity(ECSEntity* entity) 
{
    ECSComponent* component = entity->GetComponent(typeid(ECSTransformComponent));
    if (component != NULL) 
    {
        ECSTransformComponent* transform = static_cast<ECSTransformComponent*>(component);
        // Render the entity using its transform
        
    }
}