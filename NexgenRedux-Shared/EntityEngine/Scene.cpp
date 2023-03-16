#include "Scene.h"
#include "Node.h"

#include <Gensys/Int.h>
#include <Gensys/DebugUtility.h>

#include <vector>
#include <map>

using namespace Gensys;

Scene::Scene(std::string sceneTag) 
{
    m_sceneTag = sceneTag;
    m_deleteFlag = false;
}

Scene::~Scene(void)
{
    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "Deleting scene '%s'", m_sceneTag.c_str());
}

void Scene::MarkForDelete()
{
    m_deleteFlag = true;
}

bool Scene::MarkedForDelete()
{
    return m_deleteFlag;
}

void Scene::AddNode(uint32_t nodeID)
{
    m_nodes.push_back(nodeID);
}

const std::vector<uint32_t>& Scene::GetNodes()
{
    return m_nodes;
}

void Scene::Update(NodeManager *nodeManager, float dt)
{
    for (uint32_t i = 0; i < m_nodes.size(); i++)
    {
        int nodeID = m_nodes.at(i);
        const std::string *nodeTag = nodeManager->GetNodeTag(nodeID);
        if (nodeTag != NULL) 
        {
            DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "Updating nodeid = %i, tag = %s", nodeID, nodeTag->c_str());
        }
    }
}

void Scene::Render(NodeManager *nodeManager)
{
    for (uint32_t i = 0; i < m_nodes.size(); i++)
    {
        int nodeID = m_nodes.at(i);
        const std::string *nodeTag = nodeManager->GetNodeTag(nodeID);
        if (nodeTag != NULL) 
        {
            DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "Rendering nodeid = %i, tag = %s", nodeID, nodeTag->c_str());
        }
    }
}

// Privates

void Scene::DeleteNode(uint32_t nodeID)
{
    std::vector<uint32_t>::iterator it = std::find(m_nodes.begin(), m_nodes.end(), nodeID);
    if (it != m_nodes.end())
    {
        m_nodes.erase(it);
    }
}
