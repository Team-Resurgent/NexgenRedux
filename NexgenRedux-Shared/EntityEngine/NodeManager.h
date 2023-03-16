#pragma once

#include "Node.h"

#include <Gensys/Int.h>

#include <string>
#include <map>

class NodeManager 
{
public:

    NodeManager(void);
    ~NodeManager(void);

    const std::string* GetNodeTag(uint32_t nodeID);
    void MarkForDelete(uint32_t nodeID);
    uint32_t CreateNode(std::string nodeTag);
    uint32_t CreateChildNode(uint32_t parentNodeID, std::string nodeTag);

    void CheckForOrphans();

private:

    friend class SceneManager;

    Node* GetNode(uint32_t nodeID);
    void CleanNodes();

private:

    uint32_t m_maxNodeID;
    std::map<uint32_t, Node*> m_nodeMap;
};