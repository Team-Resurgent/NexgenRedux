#include "PropertyTransform.h"
#include "MathUtility.h"


using namespace NexgenRedux;

PropertyTransform::PropertyTransform() 
{
    m_matrixDirty = true;
    m_position = MathUtility::Vec3F(0, 0, 0);
    m_anchor = MathUtility::Vec3F(0, 0, 0);
    m_rotation = MathUtility::Vec3F(0, 0, 0);
    m_skew = MathUtility::Vec3F(0, 0, 0);
}

const MathUtility::Vec3F PropertyTransform::GetPosition()
{
    return m_position;
}

void PropertyTransform::SetPosition(MathUtility::Vec3F value) 
{
    m_position = value;
    m_matrixDirty = true;
}

const MathUtility::Vec3F PropertyTransform::GetAnchor() 
{
    return m_anchor;
}

void PropertyTransform::SetAnchor(MathUtility::Vec3F value) 
{
    m_anchor = value;
    m_matrixDirty = true;
}

const MathUtility::Vec3F PropertyTransform::GetRotation()
{
    return m_rotation;
}

void PropertyTransform::SetRotation(MathUtility::Vec3F value)
{
    m_rotation = value;
    m_matrixDirty = true;
}

const MathUtility::Vec3F PropertyTransform::GetSkew()
{
    return m_skew;
}

void PropertyTransform::SetSkew(MathUtility::Vec3F value)
{
    m_skew = value;
    m_matrixDirty = true;
}

const MathUtility::Matrix4x4 PropertyTransform::GetTransform() 
{
    if (m_matrixDirty == false)
    {
        return m_matrix;
    }
    MathUtility::Matrix4x4 inverseAnchorMatrix = MathUtility::Matrix4x4::Translate(MathUtility::Vec3F() - m_anchor);
    MathUtility::Matrix4x4 rotationMatrix = MathUtility::Matrix4x4::Rotation(m_rotation);
    MathUtility::Matrix4x4 skewMatrix = MathUtility::Matrix4x4::Skew(m_skew);
    MathUtility::Matrix4x4 anchorPositionMatrix = MathUtility::Matrix4x4::Translate(m_anchor + m_position);
    m_matrix = inverseAnchorMatrix * rotationMatrix * skewMatrix * anchorPositionMatrix;
    m_matrixDirty = false;
    return m_matrix;
}

