#include "Scene.h"
#include "Node.h"

#include <Gensys/Int.h>

#include <vector>
#include <map>

Scene::Scene(void) : m_maxNodeID(0)
{
}

Scene::~Scene(void)
{
	for (std::map<uint32_t, Node*>::iterator it = m_nodeMap.begin(); it != m_nodeMap.end(); ++it)
	{
        delete it->second;
	}
}

uint32_t Scene::AddNode(Node* node)
{
    m_maxNodeID++;
    m_nodeOrder.push_back(m_maxNodeID);
    m_nodeMap.insert(std::pair<uint32_t, Node*>(m_maxNodeID, node));
    return m_maxNodeID;
}

void Scene::RemoveNode(uint32_t nodeID)
{
    std::vector<uint32_t>::iterator itOrder = std::find(m_nodeOrder.begin(), m_nodeOrder.end(), nodeID);
    if (itOrder != m_nodeOrder.end())
    {
        m_nodeOrder.erase(itOrder);
    }
    std::map<uint32_t, Node*>::iterator itMap = m_nodeMap.find(nodeID);
	if (itMap == m_nodeMap.end()) 
    {
        m_nodeMap.erase(itMap);
        delete itMap->second;
    }
}

void Scene::Update(float dt)
{

}

void Scene::Render()
{

}