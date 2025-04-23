#include "BoidRenderSystem.h"

using namespace DirectX;

BoidRenderSystem::BoidRenderSystem(CommandList& commandList)
{
	InitializePyramidVerticesAndIndices();
	InitializeBuffers(commandList);
}

void BoidRenderSystem::RenderBoids(CommandList& commandList, int amount)
{
	commandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList.SetVertexBuffer(0, m_BoidVertexBuffer);
	commandList.SetIndexBuffer(m_BoidIndexBuffer);

	// Custom command list calls DrawIndexedInstanced allowing for mesh instancing
	commandList.DrawIndexed(m_NumOfIndices, amount);
}

void BoidRenderSystem::InitializePyramidVerticesAndIndices()
{
	// Construct array of VertexPosColour structs holding appropriate vertex position data
	VertexPosColour PyramidVertices[11] =
	{
		{XMFLOAT3(-1,0,1),  XMFLOAT3(0,1,0)}, // Back Left     0
		{XMFLOAT3(1,0,1),	XMFLOAT3(0,1,0)}, // Back Right    1
		{XMFLOAT3(1,0, -1), XMFLOAT3(0,1,0)}, // Front Right   2
		{XMFLOAT3(-1,0, -1),XMFLOAT3(0,1,0)}, // Front Left	3

		{XMFLOAT3(-1,0,1),	XMFLOAT3(0,1,0)}, // Back Left     4  (0)
		{XMFLOAT3(1,0,1),	XMFLOAT3(0,1,0)}, // Back Right    5  (1)
		{XMFLOAT3(1,0, -1), XMFLOAT3(0,1,0)}, // Front Right   6  (2)
		{XMFLOAT3(-1,0, -1),XMFLOAT3(0,1,0)}, // Front Left	7  (3)

		{XMFLOAT3(0,2,0),	XMFLOAT3(0,1,0)}, // Top           8

		{XMFLOAT3(1,0, -1),	XMFLOAT3(0,1,0)}, // Front Right   9  (2) 
		{XMFLOAT3(-1,0, -1),XMFLOAT3(0,1,0)}, // Front Left	10 (3)
	};

	// Construct array of index data 
	uint16_t PyramidIndices[18] =
	{
		8,5,0,
		8,6,1,
		8,7,2,
		8,4,3,
		5,9,10,
		0,5,10
	};

	int VerticesArraySize = sizeof(PyramidVertices) / sizeof(PyramidVertices[0]);
	for (int i = 0; i < VerticesArraySize; i++)
	{
		m_Vertices.push_back(PyramidVertices[i]);
	}

	int IndiciesArraySize = sizeof(PyramidIndices) / sizeof(PyramidIndices[0]);
	for (int i = 0; i < IndiciesArraySize; i++)
	{
		m_Indicies.push_back(PyramidIndices[i]);
	}
}

void BoidRenderSystem::InitializeBuffers(CommandList& commandList)
{
	if (m_Vertices.size() >= USHRT_MAX)
	{
		throw std::exception("Too many vertices for 16-bit index buffer");
	}

	commandList.CopyVertexBuffer(m_BoidVertexBuffer, m_Vertices);
	commandList.CopyIndexBuffer(m_BoidIndexBuffer, m_Indicies);

	m_NumOfIndices = m_Indicies.size();
}