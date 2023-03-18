#include "PropertyTransform.h"
#include "MathUtility.h"


using namespace NexgenRedux;

PropertyTransform::PropertyTransform() : m_matrixDirty(true)
{

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
    return m_position;
}

void PropertyTransform::SetAnchor(MathUtility::Vec3F value) 
{
    m_position = value;
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
    MathUtility::Matrix4x4 matrix = MathUtility::Matrix4x4();

    // TODO calculate new matrix
    m_matrixDirty = false;
    return m_matrix;
}

