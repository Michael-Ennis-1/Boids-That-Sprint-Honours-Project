#pragma once
#include <vector>
#include <random>
#include <DirectXMath.h>
#include "CommandList.h"

class BoidObject;

struct BoundingBox
{
	DirectX::XMFLOAT3 BoundingBoxHalfSize = DirectX::XMFLOAT3{15, 15, 15};
};

struct BoidProperties
{
	DirectX::XMFLOAT4 BoidPosition;
	DirectX::XMFLOAT4 BoidDirection;
};

// Model properties, structured in easy to access method for compute shader
struct ModelProperties
{
	float BoidCount;
	float BoidSpeed = 5.0f;
	DirectX::XMFLOAT2 padding1;

	float MinimumSeparationDistance = 1;
	float MaximumSeparationDistance = 5;
	float SeparationDistanceWeight = 12;
	float padding2;
	
	float MinimumAlignmentDistnace = 1;
	float MaximumAlignmentDistance = 12;
	float AlignmentDistanceWeight = 4;
	float padding3;

	float MinimumCohesionDistance = 1;
	float MaximumCohesionDistance = 12;
	float CohesionDistanceWeight = 4;
	float padding4;
};

// Provides CPU implementation of boids algorithm
// initialized boids still need to be registered even if not in CPU mode due to random rotation logic implemented here
class BoidPhysicsSystem
{
public:
	// Initialize and register single or multiple boids and randomize their directions
	BoidPhysicsSystem();
	BoidPhysicsSystem(BoidObject* BoidToRegister, bool RandomlyInitializeDirection = true);
	BoidPhysicsSystem(std::vector<BoidObject*> BoidsToRegister, bool RandomlyInitializeDirection = true);

	~BoidPhysicsSystem();

	// Register single or multiple boids with physics system and randomize their directions
	void RegisterBoids(BoidObject* BoidToRegister, bool RandomlyInitializeDirection = true);
	void RegisterBoids(std::vector<BoidObject*> BoidsToRegister, bool RandomlyInitializeDirection = true);

	// Remove all boids from physics system, freeing memory
	void DeleteAllBoids();

	// Update function for CPU boids. See Fig 3.4 for breakdown - comments similar to those in activity diagram
	void UpdateBoidPhysics(float DeltaTime);

	// Get boid, bounding box and overall model properties data
	std::vector<BoidProperties> GetBoidProperties();
	DirectX::XMFLOAT4 GetBoundingBoxProperties();
	ModelProperties GetModelProperties();

	void SetBoundingBoxHalfSize(DirectX::XMFLOAT3 BoxHalfSize);
	void SetModelProperties(ModelProperties NewProperties);
	void SetBoidCount(int BoidAmount);

protected:
	// Force boid within alignment of bounds of bounding box using AABB collision detection
	void ForceAlignWithinBounds(DirectX::XMFLOAT3& BoidDir, DirectX::XMFLOAT3& BoidPos);

	// Calculate distance between two boids
	float CalculateDistance(DirectX::XMFLOAT3 ThisBoidPos, DirectX::XMFLOAT3 OtherBoidPos);
	bool CheckSamePosition(DirectX::XMFLOAT3 ThisBoidPos, DirectX::XMFLOAT3 OtherBoidPos);

	// Calculate rules for all boids - see Fig 3.3 for simplified breakdown
	DirectX::XMVECTOR CalculateSeparationRule(BoidObject* ThisBoid, BoidObject* OtherBoid, float Distance);
	DirectX::XMVECTOR CalculateCohesionRule(BoidObject* ThisBoid, BoidObject* OtherBoid, float Distance);
	DirectX::XMVECTOR CalculateAlignmentRule(BoidObject* ThisBoid, BoidObject* OtherBoid, float Distance);

	// Initialize random direction for boid
	DirectX::XMFLOAT3 CalculateRandomDirection(std::mt19937 MTEngine, std::uniform_real_distribution<> RandomDistribution);

	// Calculate next translation based on boid direction
	DirectX::XMFLOAT3 CalculateNextPosition(BoidObject* Boid, float DeltaTime);

	// Properties of Boids Model
	ModelProperties m_ModelProperties;

	std::vector<BoidObject*> m_RegisteredBoids;
	BoundingBox m_Bounds;
};
