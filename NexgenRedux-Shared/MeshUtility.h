#pragma once

#include "MathUtility.h"

#include <Gensys/Int.h>

#include <vector>

namespace NexgenRedux
{
	class MeshUtility
	{		
	public:

		typedef struct Vertex
		{
			MathUtility::Vec3F position;
			MathUtility::Vec3F normal;
			MathUtility::Vec2F texcoord;

			Vertex(const float& positionX, const float& positionY, const float& positionZ, 
			      const float& normalX, const float& normalY, const float& normalZ,
			      const float& texcoordX, const float& texcoordY) 
			{
				position = MathUtility::Vec3F(positionX, positionY, positionZ);
				normal = MathUtility::Vec3F(normalX, normalY, normalZ);
				texcoord = MathUtility::Vec2F(texcoordX, texcoordY);
			}

			Vertex(const MathUtility::Vec3F& position, const MathUtility::Vec3F& normal, const MathUtility::Vec2F& texcoord) : position(position), normal(normal), texcoord(texcoord) {}

		} Vertex;	

		typedef struct Mesh
		{
			uint32_t meshID;
			uint32_t textureID;
			std::vector<Vertex> vertices;
			void* meshInstance;
			bool isDirty;
		} Mesh;	

		static MeshUtility::Mesh* GetMesh(uint32_t meshID);
		static uint32_t CreateMeshFromVertices(const std::vector<Vertex>& vertices, const uint32_t& textureID);
		static uint32_t CreateQuadXY(const MathUtility::Vec3F& position, const MathUtility::SizeF& size, const MathUtility::RectF& uvRect, const uint32_t& textureID);

	};
}
