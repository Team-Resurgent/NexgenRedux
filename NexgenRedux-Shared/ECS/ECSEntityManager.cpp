#include "ECSEntityManager.h"

#include <Gensys/Int.h>

uint32_t m_maxEntityId = 0;
std::map<uint32_t, ECSEntity*> m_entities;

ECSEntity* ECSEntityManager::CreateEntity() 
{
    ECSEntity* entity = new ECSEntity(m_maxEntityId++);
    uint32_t id = entity->GetID();
    m_entities.insert(std::pair<uint32_t, ECSEntity*>(id, entity));
    return entity;
}

ECSEntity* ECSEntityManager::GetEntity(uint32_t id) 
{
    std::map<uint32_t, ECSEntity*>::iterator it = m_entities.find(id);
    return it != m_entities.end() ? it->second : NULL;
}

void ECSEntityManager::DestroyEntity(uint32_t id) 
{
    ECSEntity* entity = GetEntity(id);
    if (entity) 
    {
        std::vector<uint32_t> children = entity->GetChildren();
        for (uint32_t i = 0; i < children.size(); i++)
        {
            uint32_t childId = children.at(i);
            DestroyEntity(childId);
        }
        m_entities.erase(id);
        delete entity;
    }
}
