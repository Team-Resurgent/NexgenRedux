#include "ECSEntity.h"

#include <Gensys/Int.h>

uint32_t m_id;
std::vector<ECSComponent*> m_components;
std::vector<uint32_t> m_children;
uint32_t m_parentID = 0xffffffff;

ECSEntity::ECSEntity(uint32_t id) 
{
    m_id = id;
}

uint32_t ECSEntity::GetID() const 
{
    return m_id;
}

void ECSEntity::AddComponent(ECSComponent* component) 
{
    m_components.push_back(component);
}

ECSComponent* ECSEntity::GetComponent(const std::type_info& type) const 
{
	for (uint32_t i = 0; i < m_components.size(); i++)
    {
		ECSComponent* component = m_components.at(i);
        if (typeid(*component) == type) 
        {
            return component;
        }
    }
    return NULL;
}

void ECSEntity::AddChild(uint32_t childID) 
{
    m_children.push_back(childID);
}

const std::vector<uint32_t>& ECSEntity::GetChildren() const
{
    return m_children;
}

void ECSEntity::SetParentID(uint32_t parentID)
{
    m_parentID = parentID;
}

uint32_t ECSEntity::GetParentID() const
{
    return m_parentID;
}