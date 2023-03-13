#pragma once

#include "ECSComponent.h"

#include <Gensys/Int.h>
#include <typeinfo>
#include <vector>

class ECSEntity 
{
public:
    ECSEntity(uint32_t id);
    uint32_t GetID() const;
    void AddComponent(ECSComponent* component);
    ECSComponent* GetComponent(const std::type_info& type) const;
    void AddChild(uint32_t child_id);
    const std::vector<uint32_t>& GetChildren() const;
    void SetParentID(uint32_t parent_id);
    uint32_t GetParentID() const;

private:

};