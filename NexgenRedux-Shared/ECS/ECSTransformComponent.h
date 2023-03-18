#pragma once

#include "ECSComponent.h"

class ECSTransformComponent : public ECSComponent 
{
public:

    typedef struct Position 
    {
        float x;
        float y;
        float z;
    } Position;

    ECSTransformComponent(float x, float y, float z);
};
