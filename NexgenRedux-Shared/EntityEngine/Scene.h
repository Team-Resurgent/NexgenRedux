#pragma once

#include "Node.h"

#include <Gensys/Int.h>

#include <vector>
#include <map>

class Scene
{
public:
    Scene(void);
    ~Scene(void);

    uint32_t AddNode(Node* node);
    void RemoveNode(uint32_t nodeID);
    void Update(float dt);
    void Render();
private:
    uint32_t m_maxNodeID;
    std::vector<uint32_t> m_nodeOrder;
    std::map<uint32_t, Node*> m_nodeMap;
};

