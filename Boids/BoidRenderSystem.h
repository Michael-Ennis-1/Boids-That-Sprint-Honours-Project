#pragma once

#include <VertexBuffer.h>
#include <IndexBuffer.h>
#include <DirectXMath.h>
#include <CommandList.h>

struct VertexPosColour
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Colour;
};

class BoidRenderSystem
{
public:
	BoidRenderSystem(CommandList& commandList);

	// Render all boids using mesh instancing
	void RenderBoids(CommandList& commandList, int amount = 1);

protected:
	void InitializePyramidVerticesAndIndices();

	// Initialize vertex and index buffers
	void InitializeBuffers(CommandList& commandList);

	std::vector<VertexPosColour> m_Vertices;
	std::vector<uint16_t> m_Indicies;

	VertexBuffer m_BoidVertexBuffer;
	IndexBuffer m_BoidIndexBuffer;

	UINT m_NumOfIndices;
};
