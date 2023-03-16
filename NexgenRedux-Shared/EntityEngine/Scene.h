#pragma once

#include "Node.h"
#include "NodeManager.h"

#include <Gensys/Int.h>

#include <vector>
#include <map>

class Scene
{
public:
    Scene(std::string sceneTag);
    ~Scene(void);

    void MarkForDelete();
    bool MarkedForDelete();

    void AddNode(uint32_t nodeID);
    void DeleteNode(uint32_t nodeID);
    const std::vector<uint32_t>& GetNodes();
    void Update(NodeManager *nodeManager, float dt);
    void Render(NodeManager *nodeManager);
private:
    std::vector<uint32_t> m_nodes;
    std::string m_sceneTag;
    bool m_deleteFlag;
};

