#include "ECSTransformComponent.h"

ECSTransformComponent::Position m_position;

ECSTransformComponent::ECSTransformComponent(float x, float y, float z)
{
    m_position.x = x;
    m_position.y = y;
    m_position.z = z;
}