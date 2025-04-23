#pragma once

#include <DirectXMath.h>

// Provides storage for both position and direction of boid object
class BoidObject
{
public:
	BoidObject(	DirectX::XMFLOAT3 BoidPosition = DirectX::XMFLOAT3(0,0,0), 
				DirectX::XMFLOAT3 BoidDirection = DirectX::XMFLOAT3(0,1,0));

	DirectX::XMFLOAT3 m_Position;
	DirectX::XMFLOAT3 m_Direction;
};
