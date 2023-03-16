#pragma once

#include <vector>
#include <string>

class Node 
{
public:
    Node(std::string tag);
    Node(uint32_t parentID, std::string tag);
    ~Node(void);
    
    const std::string& GetTag();
    void MarkForDelete();
    bool MarkedForDelete();
    uint32_t GetParentID();

    void AddChildNode(uint32_t nodeID);
    const std::vector<uint32_t>& GetChildNodes();
    void DeleteChild(uint32_t nodeID);
//     virtual void Update(float dt);
//     virtual void Draw();

//     void AddChild(int childId);
//     void RemoveChild(int childId);

//     void SetPosition(float x, float y);
//     void SetRotation(float rotation);
//     void SetScale(float scaleX, float scaleY);

//     float GgetPositionX() const;
//     float GetPositionY() const;
//     float GetRotation() const;
//     float GetScaleX() const;
//     float GetScaleY() const;
//     int GetId() const;
//     int GetParentId() const;

// protected:
//     int m_id;
//     int m_parentId;

// private:
//     std::vector<int> m_childrenIds;
//     float m_positionX;
//     float m_positionY;
//     float m_rotation;
//     float m_scaleX;
//     float m_scaleY;
    private:
        std::string m_nodeTag;
        uint32_t m_parentID;
        std::vector<uint32_t> m_childNodes;
        bool m_deleteFlag;
};