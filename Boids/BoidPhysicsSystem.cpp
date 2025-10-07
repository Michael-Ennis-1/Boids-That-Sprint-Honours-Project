#include "BoidPhysicsSystem.h"

#include "CommandList.h"
#include <math.h>
#include "BoidObject.h"

using namespace DirectX;

BoidPhysicsSystem::BoidPhysicsSystem()
{
}

BoidPhysicsSystem::BoidPhysicsSystem(BoidObject* BoidToRegister, bool RandomlyInitializeDirection)
{
	RegisterBoids(BoidToRegister, RandomlyInitializeDirection);
}

BoidPhysicsSystem::BoidPhysicsSystem(std::vector<BoidObject*> BoidsToRegister, bool RandomlyInitializeDirection)
{
	RegisterBoids(BoidsToRegister, RandomlyInitializeDirection);
}

BoidPhysicsSystem::~BoidPhysicsSystem()
{
	DeleteAllBoids();
}

void BoidPhysicsSystem::RegisterBoids(BoidObject* BoidToRegister, bool RandomlyInitializeDirection)
{
	if (BoidToRegister)
	{
		if (RandomlyInitializeDirection)
		{
			std::random_device RandomDevice;
			std::mt19937 MTEngine(RandomDevice());
			std::uniform_real_distribution<> RandomDistribution(-1.0, 1.0);

			BoidToRegister->m_Direction = CalculateRandomDirection(MTEngine, RandomDistribution);
		}

		m_RegisteredBoids.push_back(BoidToRegister);
	}
}

void BoidPhysicsSystem::RegisterBoids(std::vector<BoidObject*> BoidsToRegister, bool RandomlyInitializeDirection)
{
	if (BoidsToRegister.size() > 0)
	{
		int NumberOfBoidsToRegister = BoidsToRegister.size();
		for (int i = 0; i < NumberOfBoidsToRegister; i++)
		{
			if (RandomlyInitializeDirection)
			{
				std::random_device RandomDevice;
				std::mt19937 MTEngine(RandomDevice());
				std::uniform_real_distribution<> RandomDistribution(-1.0, 1.0);

				BoidsToRegister[i]->m_Direction = CalculateRandomDirection(MTEngine, RandomDistribution);
			}

			m_RegisteredBoids.push_back(BoidsToRegister[i]);
		}
	}
}

void BoidPhysicsSystem::DeleteAllBoids()
{
	for (int i = 0; i < m_RegisteredBoids.size(); i++)
	{
		delete m_RegisteredBoids[i];
		m_RegisteredBoids[i] = nullptr;
	}

	m_RegisteredBoids.clear();
	m_RegisteredBoids.shrink_to_fit();
}

void BoidPhysicsSystem::UpdateBoidPhysics(float DeltaTime)
{
	std::vector<XMFLOAT3> NewBoidPos;
	std::vector<XMFLOAT3> NewBoidDir;

	if (m_RegisteredBoids.size() > 0)
	{
		int NumberOfRegisteredBoids = m_RegisteredBoids.size();
		for (int i = 0; i < NumberOfRegisteredBoids; i++)
		{
			// Cache current boid in first loop
			BoidObject* CurrentBoid = m_RegisteredBoids[i];
			if (CurrentBoid)
			{
				int SeparationVectors = 0;
				int AlignmentVectors = 0;
				int CohesionVectors = 0;

				XMVECTOR SeparationVectorResult{ 0, 0, 0 };
				XMVECTOR AlignmentVectorResult{ 0, 0, 0 };
				XMVECTOR CohesionVectorResult{ 0, 0, 0 };

				for (int j = 0; j < NumberOfRegisteredBoids; j++)
				{
					// Cache other boid in second loop
					BoidObject* OtherBoid = m_RegisteredBoids[j];

					// Is new boid entity same as current one?
					if (i == j)
					{
						continue;
					}

					// Also ignore if in same position, intial position will be same for all boids
					if (CheckSamePosition(CurrentBoid->m_Position, OtherBoid->m_Position))
					{
						continue;
					}

					if (OtherBoid)
					{
						// Calculate Distance between current boid and other boid
						float DistanceBetweenTwoBoids = CalculateDistance(CurrentBoid->m_Position, OtherBoid->m_Position);
						if (DistanceBetweenTwoBoids < m_ModelProperties.MaximumSeparationDistance)
						{
							// Calculate rule specific target vector 
							SeparationVectorResult += CalculateSeparationRule(CurrentBoid, OtherBoid, DistanceBetweenTwoBoids);
							SeparationVectors++;
						}
						if (DistanceBetweenTwoBoids < m_ModelProperties.MaximumAlignmentDistance)
						{
							// Calculate rule specific target vector
							AlignmentVectorResult += CalculateAlignmentRule(CurrentBoid, OtherBoid, DistanceBetweenTwoBoids);
							AlignmentVectors++;
						}
						if (DistanceBetweenTwoBoids < m_ModelProperties.MaximumCohesionDistance)
						{
							// Calculate rule specific target vector
							CohesionVectorResult += CalculateCohesionRule(CurrentBoid, OtherBoid, DistanceBetweenTwoBoids);
							CohesionVectors++;
						}
					}
				}

				// Divide final rule vectors by number of vectors added per rule
				if (SeparationVectors > 0)
				{
					SeparationVectorResult /= SeparationVectors;
				}
				if (AlignmentVectors > 0)
				{
					AlignmentVectorResult /= AlignmentVectors;
				}
				if (CohesionVectors > 0)
				{
					CohesionVectorResult /= CohesionVectors;
				}

				// Modify final vectors by delta time and rule-specific weight value 
				XMVECTOR NewDirectionVector = { CurrentBoid->m_Direction.x, CurrentBoid->m_Direction.y, CurrentBoid->m_Direction.z };
				NewDirectionVector += SeparationVectorResult * m_ModelProperties.SeparationDistanceWeight * DeltaTime;
				NewDirectionVector += AlignmentVectorResult * m_ModelProperties.AlignmentDistanceWeight * DeltaTime;
				NewDirectionVector += CohesionVectorResult * m_ModelProperties.CohesionDistanceWeight * DeltaTime;
				NewDirectionVector = XMVector3Normalize(NewDirectionVector);

				XMFLOAT3 NewDirection= { XMVectorGetX(NewDirectionVector), XMVectorGetY(NewDirectionVector), XMVectorGetZ(NewDirectionVector) };

				XMFLOAT3 NewPosition = CalculateNextPosition(CurrentBoid, DeltaTime);
				ForceAlignWithinBounds(NewDirection, NewPosition);

				NewBoidDir.push_back(NewDirection);
				NewBoidPos.push_back(NewPosition);
			}
		}
	}

	// Apply Final Vectors to Current boid entity 
	for (int i = 0; i < m_RegisteredBoids.size(); i++)
	{
		m_RegisteredBoids[i]->m_Direction = NewBoidDir[i];
		m_RegisteredBoids[i]->m_Position = NewBoidPos[i];
	}
}

std::vector<BoidProperties> BoidPhysicsSystem::GetBoidProperties()
{
	// Convert all boids data into BoidProperties struct, from BoidObjects and return the conversion
	std::vector<BoidProperties> PropertiesVector;

	int NumberOfRegisteredBoids = m_RegisteredBoids.size();
	for (int i = 0; i < NumberOfRegisteredBoids; i++)
	{
		BoidProperties Properties{XMFLOAT4(m_RegisteredBoids[i]->m_Position.x, m_RegisteredBoids[i]->m_Position.y, m_RegisteredBoids[i]->m_Position.z, 0.0f),
								  XMFLOAT4(m_RegisteredBoids[i]->m_Direction.x, m_RegisteredBoids[i]->m_Direction.y, m_RegisteredBoids[i]->m_Direction.z, 0.0f) };

		PropertiesVector.push_back(Properties);
	}

	return PropertiesVector;
}

DirectX::XMFLOAT4 BoidPhysicsSystem::GetBoundingBoxProperties()
{
	return DirectX::XMFLOAT4(m_Bounds.BoundingBoxHalfSize.x, m_Bounds.BoundingBoxHalfSize.y, m_Bounds.BoundingBoxHalfSize.z, 0);
}

ModelProperties BoidPhysicsSystem::GetModelProperties()
{
	// Lazy initialization for Boid Count 
	// Don't know if more entities might be registered in future, so only set when updating Compute Shader model properties
	SetBoidCount(m_RegisteredBoids.size());

	return m_ModelProperties;
}

void BoidPhysicsSystem::SetBoundingBoxHalfSize(DirectX::XMFLOAT3 BoxHalfSize)
{
	m_Bounds.BoundingBoxHalfSize = BoxHalfSize;
}

void BoidPhysicsSystem::SetModelProperties(ModelProperties NewProperties)
{
	int BoidCount = m_ModelProperties.BoidCount;

	m_ModelProperties = NewProperties;

	// Keep original boid count: Don't need to change this
	m_ModelProperties.BoidCount = BoidCount;
}

void BoidPhysicsSystem::SetBoidCount(int BoidAmount)
{
	m_ModelProperties.BoidCount = BoidAmount;
}

void BoidPhysicsSystem::ForceAlignWithinBounds(DirectX::XMFLOAT3& BoidDir, DirectX::XMFLOAT3& BoidPos)
{
	if (BoidPos.x > m_Bounds.BoundingBoxHalfSize.x)
	{
		BoidDir.x *= -1;
		BoidPos.x = m_Bounds.BoundingBoxHalfSize.x - 0.1f;
	}
	else if (BoidPos.x < -m_Bounds.BoundingBoxHalfSize.x)
	{
		BoidDir.x *= -1;
		BoidPos.x = -m_Bounds.BoundingBoxHalfSize.x + 0.1f;
	}

	if (BoidPos.y > m_Bounds.BoundingBoxHalfSize.y)
	{
		BoidDir.y *= -1;
		BoidPos.y = m_Bounds.BoundingBoxHalfSize.y - 0.1f;
	}
	else if (BoidPos.y < -m_Bounds.BoundingBoxHalfSize.y)
	{
		BoidDir.y *= -1;
		BoidPos.y = -m_Bounds.BoundingBoxHalfSize.y + 0.1f;
	}

	if (BoidPos.z > m_Bounds.BoundingBoxHalfSize.z)
	{
		BoidDir.z *= -1;
		BoidPos.z = m_Bounds.BoundingBoxHalfSize.z - 0.1f;
	}
	else if (BoidPos.z < -m_Bounds.BoundingBoxHalfSize.z)
	{
		BoidDir.z *= -1;
		BoidPos.z = -m_Bounds.BoundingBoxHalfSize.z + 0.1f;
	}
}

float BoidPhysicsSystem::CalculateDistance(XMFLOAT3 ThisBoidPos, XMFLOAT3 OtherBoidPos)
{
	// sqrt[(x2 - x1)^2 + (y2 - y1)^2 + (z2 - z1)^2] for distance

	float XCoordsSquared = (OtherBoidPos.x - ThisBoidPos.x) * (OtherBoidPos.x - ThisBoidPos.x);
	float YCoordsSquared = (OtherBoidPos.y - ThisBoidPos.y) * (OtherBoidPos.y - ThisBoidPos.y);
	float ZCoordsSquared = (OtherBoidPos.z - ThisBoidPos.z) * (OtherBoidPos.z - ThisBoidPos.z);
	float SumOfCoordsSquared = XCoordsSquared + YCoordsSquared + ZCoordsSquared;

	return sqrt(SumOfCoordsSquared);
}

bool BoidPhysicsSystem::CheckSamePosition(XMFLOAT3 ThisBoidPos, XMFLOAT3 OtherBoidPos)
{
	if (ThisBoidPos.x == OtherBoidPos.x && ThisBoidPos.y == OtherBoidPos.y && ThisBoidPos.z == OtherBoidPos.z)
	{
		return true;
	}
	return false;
}

XMVECTOR BoidPhysicsSystem::CalculateSeparationRule(BoidObject* ThisBoid, BoidObject* OtherBoid, float Distance)
{
	XMVECTOR FinalSeparationVector{ 0, 0, 0 };

	XMVECTOR ThisBoidPos = { ThisBoid->m_Position.x, ThisBoid->m_Position.y, ThisBoid->m_Position.z };
	XMVECTOR OtherBoidPos = { OtherBoid->m_Position.x, OtherBoid->m_Position.y, OtherBoid->m_Position.z };

	// Calculate target vector - Separation target vector is opposite direction to Other boid from This boid
	XMVECTOR DirectionVector = ThisBoidPos - OtherBoidPos;
	DirectionVector = XMVector3Normalize(DirectionVector);

	float DistanceWeight = 1;

	Distance -= m_ModelProperties.MinimumSeparationDistance;
	if (Distance > 0)
	{
		float DistanceWeightDivisibleFactor = m_ModelProperties.MaximumSeparationDistance - m_ModelProperties.MinimumSeparationDistance;
		DistanceWeight -= (Distance / DistanceWeightDivisibleFactor);
	}

	// Modify final vector by rule-specific distance value
	if (DistanceWeight > 0.01f)
	{
		FinalSeparationVector = DirectionVector * DistanceWeight;
	}

	// Return target vector
	return FinalSeparationVector;
}

XMVECTOR BoidPhysicsSystem::CalculateCohesionRule(BoidObject* ThisBoid, BoidObject* OtherBoid, float Distance)
{
	XMVECTOR FinalCohesionVector{ 0, 0, 0 };

	XMVECTOR ThisBoidPos = { ThisBoid->m_Position.x, ThisBoid->m_Position.y, ThisBoid->m_Position.z };
	XMVECTOR OtherBoidPos = { OtherBoid->m_Position.x, OtherBoid->m_Position.y, OtherBoid->m_Position.z };

	// Calculate target vector - Cohesion target vector is direction to Other boid from This boid
	XMVECTOR DirectionVector = OtherBoidPos - ThisBoidPos;
	DirectionVector = XMVector3Normalize(DirectionVector);

	float DistanceWeight = 1;

	Distance -= m_ModelProperties.MinimumCohesionDistance;
	if (Distance > 0)
	{
		float DistanceWeightDivisibleFactor = m_ModelProperties.MaximumCohesionDistance - m_ModelProperties.MinimumCohesionDistance;
		DistanceWeight -= (Distance / DistanceWeightDivisibleFactor);
	}

	// Modify final vector by rule-specific distance value
	if (DistanceWeight > 0.01f)
	{
		FinalCohesionVector = DirectionVector * DistanceWeight;
	}

	// Return target vector
	return FinalCohesionVector;
}

XMVECTOR BoidPhysicsSystem::CalculateAlignmentRule(BoidObject* ThisBoid, BoidObject* OtherBoid, float Distance)
{
	XMVECTOR FinalAlignmentVector{ 0, 0, 0 };

	XMVECTOR ThisBoidPos = { ThisBoid->m_Position.x, ThisBoid->m_Position.y, ThisBoid->m_Position.z };
	XMVECTOR OtherBoidPos = { OtherBoid->m_Position.x, OtherBoid->m_Position.y, OtherBoid->m_Position.z };

	// Calculate target vector - Alignment target vector is Other boid's direction vector
	XMVECTOR DirectionVector = { OtherBoid->m_Direction.x, OtherBoid->m_Direction.y, OtherBoid->m_Direction.z };
	DirectionVector = XMVector3Normalize(DirectionVector);

	float DistanceWeight = 1;

	Distance -= m_ModelProperties.MinimumAlignmentDistnace;
	if (Distance > 0)
	{
		float DistanceWeightDivisibleFactor = m_ModelProperties.MaximumAlignmentDistance - m_ModelProperties.MinimumAlignmentDistnace;
		DistanceWeight -= (Distance / DistanceWeightDivisibleFactor);
	}

	// Modify final vector by rule-specific distance value
	if (DistanceWeight > 0.01f)
	{
		FinalAlignmentVector = DirectionVector * DistanceWeight;
	}

	// Return target vector
	return FinalAlignmentVector;
}

XMFLOAT3 BoidPhysicsSystem::CalculateRandomDirection(std::mt19937 MTEngine, std::uniform_real_distribution<> RandomDistribution)
{
	XMVECTOR RandomBoidDirection = XMVECTOR{ static_cast<float>(RandomDistribution(MTEngine)),
							 static_cast<float>(RandomDistribution(MTEngine)),
							 static_cast<float>(RandomDistribution(MTEngine)) };

	RandomBoidDirection = XMVector3Normalize(RandomBoidDirection);

	return XMFLOAT3{ XMVectorGetX(RandomBoidDirection), XMVectorGetY(RandomBoidDirection), XMVectorGetZ(RandomBoidDirection) };
}

XMFLOAT3 BoidPhysicsSystem::CalculateNextPosition(BoidObject* Boid, float DeltaTime)
{
	XMFLOAT3 BoidNewPos = XMFLOAT3{ Boid->m_Position.x + (Boid->m_Direction.x * DeltaTime * m_ModelProperties.BoidSpeed),
									Boid->m_Position.y + (Boid->m_Direction.y * DeltaTime * m_ModelProperties.BoidSpeed),
									Boid->m_Position.z + (Boid->m_Direction.z * DeltaTime * m_ModelProperties.BoidSpeed) };
	return BoidNewPos;
}