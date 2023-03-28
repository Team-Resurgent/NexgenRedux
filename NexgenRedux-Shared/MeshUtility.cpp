#include "MeshUtility.h"

#include <vector>

using namespace NexgenRedux;

const std::vector<MeshUtility::Vertex> MeshUtility::CreateQuadXY(const MathUtility::Vec3F& position, const MathUtility::SizeF& size, const MathUtility::RectF& uvRect)
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

    return vertices;
}
