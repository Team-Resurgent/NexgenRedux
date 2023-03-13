#include "MeshUtility.h"

#include <map>

using namespace NexgenRedux;

namespace 
{
    uint32_t m_maxMeshID = 0;
    std::map<uint32_t, MeshUtility::Mesh> m_meshMap;
}

MeshUtility::Mesh* MeshUtility::GetMesh(uint32_t meshID)
{
    std::map<uint32_t, MeshUtility::Mesh>::iterator it = m_meshMap.find(meshID);
	if (it == m_meshMap.end()) 
	{
		return NULL;
	}
	return (MeshUtility::Mesh*)&it->second;
}

uint32_t MeshUtility::CreateMeshFromVertices(const std::vector<Vertex>& vertices, const uint32_t& textureID)
{
    uint32_t meshID = ++m_maxMeshID;

    Mesh mesh;
    mesh.meshID = meshID;
    mesh.textureID = textureID;
    mesh.vertices = vertices;
    mesh.meshInstance = NULL;
    mesh.isDirty = true;

    m_meshMap.insert(std::pair<uint32_t, Mesh>(meshID, mesh));
    return meshID;
}

uint32_t MeshUtility::CreateQuadXY(const MathUtility::Vec3F& position, const MathUtility::SizeF& size, const MathUtility::RectF& uvRect, const uint32_t& textureID)
{
    std::vector<Vertex> vertices;

    Vertex v1 = Vertex(position.x, position.y, position.z, 0.0f, 0.0f, 1.0f, uvRect.x, 1.0f - uvRect.y);
    Vertex v2 = Vertex(position.x + size.width, position.y, position.z, 0.0f, 0.0f, 1.0f, uvRect.x + uvRect.width, 1.0f - uvRect.y);
    Vertex v3 = Vertex(position.x + size.width, position.y + size.height, position.z, 0.0f, 0.0f, 1.0f, uvRect.x + uvRect.width, 1.0f - (uvRect.y + uvRect.height));
    Vertex v4 = Vertex(position.x, position.y + size.height, position.z, 0.0f, 0.0f, 1.0f, uvRect.x, 1.0f - (uvRect.y + uvRect.height));

    float nx = (v2.position.y - v1.position.y) * (v3.position.z - v1.position.z) - (v2.position.z - v1.position.z) * (v3.position.y - v1.position.y);
    float ny = (v2.position.z - v1.position.z) * (v3.position.x - v1.position.x) - (v2.position.x - v1.position.x) * (v3.position.z - v1.position.z);
    float nz = (v2.position.x - v1.position.x) * (v3.position.y - v1.position.y) - (v2.position.y - v1.position.y) * (v3.position.x - v1.position.x);

    MathUtility::Vec3F normal = MathUtility::Vec3F(nx, ny, nz).Normal();

    v1.normal = normal;
    v2.normal = normal;
    v3.normal = normal;
    v4.normal = normal;

    vertices.push_back(v1);
    vertices.push_back(v2);
    vertices.push_back(v3);
    vertices.push_back(v4);
    vertices.push_back(v1);
    vertices.push_back(v3);

    return CreateMeshFromVertices(vertices, textureID);
}