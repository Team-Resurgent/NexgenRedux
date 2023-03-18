#include "PropertyTransform.h"
#include "MathUtility.h"


using namespace NexgenRedux;

PropertyTransform::PropertyTransform(Node* node) 
{
    m_node = node;

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
    bool isDirty = m_position != value;
    if (isDirty) 
    {
        m_position = value;
    }
    m_matrixDirty = isDirty;
}

const MathUtility::Vec3F PropertyTransform::GetAnchor() 
{
    return m_anchor;
}

void PropertyTransform::SetAnchor(MathUtility::Vec3F value) 
{
    bool isDirty = m_anchor != value;
    if (isDirty) 
    {
        m_anchor = value;
    }
    m_matrixDirty = isDirty;
}

const MathUtility::Vec3F PropertyTransform::GetRotation()
{
    return m_rotation;
}

void PropertyTransform::SetRotation(MathUtility::Vec3F value)
{
    bool isDirty = m_rotation != value;
    if (isDirty) 
    {
        m_rotation = value;
    }
    m_matrixDirty = isDirty;
}

const MathUtility::Vec3F PropertyTransform::GetSkew()
{
    return m_skew;
}

void PropertyTransform::SetSkew(MathUtility::Vec3F value)
{
    bool isDirty = m_skew != value;
    if (isDirty) 
    {
        m_skew = value;
    }
    m_matrixDirty = isDirty;
}

const MathUtility::Matrix4x4 PropertyTransform::GetTransform() 
{
    if (m_matrixDirty == false)
    {
        return m_matrix;
    }

    MathUtility::Matrix4x4 parentMatrix = MathUtility::Matrix4x4();
    Node* parentNode = m_node->GetParentNode();
    if (parentNode != NULL && parentNode->HasProperty(PropertyTypeTransform))
    {
        PropertyTransform* propertyTransform = (PropertyTransform*)parentNode->GetProperty(PropertyTypeTransform);
        parentMatrix = propertyTransform->GetTransform();
    }
    
    MathUtility::Matrix4x4 inverseAnchorMatrix = MathUtility::Matrix4x4::Translate(MathUtility::Vec3F() - m_anchor);
    MathUtility::Matrix4x4 rotationMatrix = MathUtility::Matrix4x4::Rotation(m_rotation);
    MathUtility::Matrix4x4 skewMatrix = MathUtility::Matrix4x4::Skew(m_skew);
    MathUtility::Matrix4x4 anchorPositionMatrix = MathUtility::Matrix4x4::Translate(m_anchor + m_position);
    m_matrix = parentMatrix * inverseAnchorMatrix * rotationMatrix * skewMatrix * anchorPositionMatrix;
    m_matrixDirty = false;
    return m_matrix;
}