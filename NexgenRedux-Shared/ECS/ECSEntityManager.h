#pragma once

#include "ECSEntity.h"

#include <Gensys/Int.h>

#include <map>

class ECSEntityManager 
{
public:
    ECSEntity* CreateEntity();
    ECSEntity* GetEntity(uint32_t id);
    void DestroyEntity(uint32_t id);
};

