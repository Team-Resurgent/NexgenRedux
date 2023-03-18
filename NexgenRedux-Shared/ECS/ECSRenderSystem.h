#pragma once

#include "ECSRenderSystem.h"
#include "ECSComponent.h"
#include "ECSEntity.h"
#include "ECSTransformComponent.h"
#include "ECSEntityManager.h"

class ECSRenderSystem 
{
public:
    void Update(ECSEntityManager* entityManager, ECSEntity* parentEntity, float dt);
private:
    void RenderEntity(ECSEntity* entity);
};