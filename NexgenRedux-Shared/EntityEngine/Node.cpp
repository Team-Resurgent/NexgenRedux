// Node::Node(int id) {
//     m_id = id;
//     m_parentId = -1;
//     m_positionX = 0;
//     m_positionY = 0;
//     m_rotation = 0;
//     m_scaleX = 1;
//     m_scaleY = 1;
// }

// void Node::Update(float dt) {}

// void Node::Draw() {}

// void Node::AddChild(int childId) 
// {
//     m_childrenIds.push_back(childId);
// }

// void Node::RemoveChild(int childId) 
// {
//     auto it = std::find(m_childrenIds.begin(), m_childrenIds.end(), childId);
//     if (it != m_childrenIds.end()) {
//         m_childrenIds.erase(it);
//     }
// }

// void Node::SetPosition(float x, float y) 
// {
//     m_positionX = x;
//     m_positionY = y;
// }

// void Node::SetRotation(float rotation) 
// {
//     m_rotation = rotation;
// }

// void Node::SetScale(float scaleX, float scaleY) 
// {
//     m_scaleX = scaleX;
//     m_scaleY = scaleY;
// }

// float Node::GetPositionX() const 
// {
//     return m_positionX;
// }

// float Node::GetPositionY() const 
// {
//     return m_positionY;
// }

// float Node::GetRotation() const 
// {
//     return m_rotation;
// }

// float Node::GetScaleX() const 
// {
//     return m_scaleX;
// }

// float Node::GetScaleY() const 
// {
//     return m_scaleY;
// }

// int Node::GetID() const 
// {
//     return m_id;
// }

// int Node::GetParentID() const 
// {
//     return m_parentId;
// }