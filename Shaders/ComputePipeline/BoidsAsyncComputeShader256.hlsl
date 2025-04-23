struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID; // 3D index of the thread group in the dispatch.
    uint3 GroupThreadID : SV_GroupThreadID; // 3D index of local thread ID in a thread group.
    uint3 DispatchThreadID : SV_DispatchThreadID; // 3D index of global thread ID in the dispatch.
    uint GroupIndex : SV_GroupIndex; // Flattened local index of the thread within a thread group.
};

// Calculate rule specific target vectors - see Fig 3.3 for more details

float3 CalculateSeparationVector(float3 CurrentBoidPos, float3 OtherBoidPos, float TotalDistance,
                                 float MinimumSeparationDistance, float MaximumSeparationDistance)
{
    float3 SeparationVector = float3(0, 0, 0);

    // Calculate target vector - Separation target vector is opposite direction to Other boid from This boid
    float3 DirectionVector = CurrentBoidPos - OtherBoidPos;
    DirectionVector = normalize(DirectionVector);

    float DistanceWeight = 1;

    TotalDistance -= MinimumSeparationDistance;
    if (TotalDistance > 0)
    {
        float DistanceWeightDivisibleFactor = MaximumSeparationDistance - MinimumSeparationDistance;
        DistanceWeight -= (TotalDistance / DistanceWeightDivisibleFactor);
    }

    // Modify final vector by rule-specific distance value
    if (DistanceWeight > 0.01f)
    {
        SeparationVector = DirectionVector * DistanceWeight;
    }
    
    // Return target vector
    return SeparationVector;
}

float3 CalculateAlignmentVector(float3 OtherBoidDir, float TotalDistance,
                                float MinimumAlignmentDistance, float MaximumAlignmentDistance)
{
    float3 AlignmentVector = float3(0, 0, 0);

    // Calculate target vector - Alignment target vector is Other boid's direction vector
    float3 DirectionVector = OtherBoidDir;
    DirectionVector = normalize(DirectionVector);

    float DistanceWeight = 1;

    TotalDistance -= MinimumAlignmentDistance;
    if (TotalDistance > 0)
    {
        float DistanceWeightDivisibleFactor = MaximumAlignmentDistance - MinimumAlignmentDistance;
        DistanceWeight -= (TotalDistance / DistanceWeightDivisibleFactor);
    }

    // Modify final vector by rule-specific distance value
    if (DistanceWeight > 0.01f)
    {
        AlignmentVector = DirectionVector * DistanceWeight;
    }

    // Return target vector
    return AlignmentVector;
}

float3 CalculateCohesionVector(float3 CurrentBoidPos, float3 OtherBoidPos, float TotalDistance,
                               float MinimumCohesionDistance, float MaximumCohesionDistance)
{
    float3 CohesionVector = float3(0, 0, 0);

    // Calculate target vector - Cohesion target vector is direction to Other boid from This boid
    float3 DirectionVector = OtherBoidPos - CurrentBoidPos;
    DirectionVector = normalize(DirectionVector);

    float DistanceWeight = 1;

    TotalDistance -= MinimumCohesionDistance;
    if (TotalDistance > 0)
    {
        float DistanceWeightDivisibleFactor = MaximumCohesionDistance - MinimumCohesionDistance;
        DistanceWeight -= (TotalDistance / DistanceWeightDivisibleFactor);
    }

    // Modify final vector by rule-specific distance value
    if (DistanceWeight > 0.01f)
    {
        CohesionVector = DirectionVector * DistanceWeight;
    }

    // Return target vector
    return CohesionVector;
}

float3 CalculateNextPosition(float3 BoidPos, float3 BoidDir, float BoidSpeed, float DeltaTime)
{
    float3 NewBoidPos = float3(BoidPos.x + (BoidDir.x * DeltaTime * BoidSpeed),
                               BoidPos.y + (BoidDir.y * DeltaTime * BoidSpeed),
                               BoidPos.z + (BoidDir.z * DeltaTime * BoidSpeed));
    
    return NewBoidPos;
}


bool IsAtSamePosition(float4 Current, float4 Other)
{
    if (Current.x == Other.x && Current.y == Other.y && Current.z == Other.z)
    {
        return true;
    }
    
    return false;
}

// Force boid within alignment of bounds of bounding box using AABB collision detection
float2x3 ForceAlignWithinBounds(float3 BoidPos, float3 BoidDir, float3 BoundingBoxHalfSize)
{
    if (BoidPos.x > BoundingBoxHalfSize.x)
    {
        BoidDir.x *= -1;
        BoidPos.x = BoundingBoxHalfSize.x - 0.1f;
    }
    else if (BoidPos.x < -BoundingBoxHalfSize.x)
    {
        BoidDir.x *= -1;
        BoidPos.x = -BoundingBoxHalfSize.x + 0.1f;
    }

    if (BoidPos.y > BoundingBoxHalfSize.y)
    {
        BoidDir.y *= -1;
        BoidPos.y = BoundingBoxHalfSize.y - 0.1f;
    }
    else if (BoidPos.y < -BoundingBoxHalfSize.y)
    {
        BoidDir.y *= -1;
        BoidPos.y = -BoundingBoxHalfSize.y + 0.1f;
    }

    if (BoidPos.z > BoundingBoxHalfSize.z)
    {
        BoidDir.z *= -1;
        BoidPos.z = BoundingBoxHalfSize.z - 0.1f;
    }
    else if (BoidPos.z < -BoundingBoxHalfSize.z)
    {
        BoidDir.z *= -1;
        BoidPos.z = -BoundingBoxHalfSize.z + 0.1f;
    }
    
    return float2x3(float3(BoidPos), float3(BoidDir));
}

// Custom Buffers
struct BoidProperties
{
    float4 BoidPos;
    float4 BoidDir;
};

struct ModelSettings
{
    float BoidCount;
    float BoidSpeed;
    float2 padding1;

    float MinimumSeparationDistance;
    float MaximumSeparationDistance;
    float SeparationDistanceWeight;
    float padding2;
	
    float MinimumAlignmentDistnace;
    float MaximumAlignmentDistance;
    float AlignmentDistanceWeight;
    float padding3;

    float MinimumCohesionDistance;
    float MaximumCohesionDistance;
    float CohesionDistanceWeight;
    float padding4;
};

struct DeltaTime
{
    float DeltaTime;
    float3 padding;
};

struct BoundingBoxSettings
{
    float3 HalfSize;
    float padding;
};


RWStructuredBuffer<BoidProperties> Properties : register(u0);
RWStructuredBuffer<BoidProperties> OutProperties : register(u1);
ConstantBuffer<ModelSettings> ModelProperties : register(b0);
ConstantBuffer<DeltaTime> DeltaTimeBuffer : register(b1);
ConstantBuffer<BoundingBoxSettings> BoundingBox : register(b2);

#define ThreadCount 256

[numthreads(ThreadCount, 1, 1)]
void main(ComputeShaderInput IN)
{
    // Cache current boid depending on dispatch thread ID - Replaces "First loop through" stage in Fig 3.4
    BoidProperties CurrentBoid = Properties[IN.DispatchThreadID.x];
    
    float3 FinalSeperationVector = float3(0, 0, 0);
    float3 FinalAlignmentVector = float3(0, 0, 0);
    float3 FinalCohesionVector = float3(0, 0, 0);
    
    float NumberOfSeperationVectors;
    float NumberOfAlignmentVectors;
    float NumberOfCohesionVectors;
    
    for (int i = 0; i < ModelProperties.BoidCount; i++)
    {
        // Cache other boid - Replaces "Second loop through" stage in Fig 3.4
        BoidProperties OtherBoid = Properties[i];
        
        // Is new boid entity same as current one?
        if (i == IN.DispatchThreadID.x || IsAtSamePosition(CurrentBoid.BoidPos, OtherBoid.BoidPos))
        {
            continue;
        }
        
        // Calculate Distance between current boid and other boid
        float Distance = distance(CurrentBoid.BoidPos, OtherBoid.BoidPos);
        if (Distance < ModelProperties.MaximumSeparationDistance)
        {
            // Calculate rule specific target vector 
            FinalSeperationVector += CalculateSeparationVector(CurrentBoid.BoidPos, OtherBoid.BoidPos, Distance,
                                                               ModelProperties.MinimumSeparationDistance,
                                                               ModelProperties.MaximumSeparationDistance);
            NumberOfSeperationVectors++;
        }
        if (Distance < ModelProperties.MaximumCohesionDistance)
        {
            // Calculate rule specific target vector 
            FinalCohesionVector += CalculateCohesionVector(CurrentBoid.BoidPos, OtherBoid.BoidPos, Distance,
                                                           ModelProperties.MinimumCohesionDistance,
                                                           ModelProperties.MaximumCohesionDistance);
            
            NumberOfCohesionVectors++;
        }
        if (Distance < ModelProperties.MaximumAlignmentDistance)
        {
            // Calculate rule specific target vector 
            FinalAlignmentVector += CalculateAlignmentVector(OtherBoid.BoidDir, Distance,
                                                             ModelProperties.MinimumAlignmentDistnace,
                                                             ModelProperties.MaximumAlignmentDistance);
            
            NumberOfAlignmentVectors++;
        }
    }
    
    // Divide final rule vectors by number of vectors added per rule
    if (NumberOfSeperationVectors > 0)
    {
        FinalSeperationVector /= NumberOfSeperationVectors;
    }
    if (NumberOfAlignmentVectors > 0)
    {
        FinalAlignmentVector /= NumberOfAlignmentVectors;
    }
    if (NumberOfCohesionVectors > 0)
    {
        FinalCohesionVector /= NumberOfCohesionVectors;
    }
    
   // Modify final vectors by delta time and rule-specific weight value 
    float3 NextDirection = float3(CurrentBoid.BoidDir.x, CurrentBoid.BoidDir.y, CurrentBoid.BoidDir.z);
    NextDirection += FinalSeperationVector * ModelProperties.SeparationDistanceWeight * DeltaTimeBuffer.DeltaTime;
    NextDirection += FinalCohesionVector * ModelProperties.CohesionDistanceWeight * DeltaTimeBuffer.DeltaTime;
    NextDirection += FinalAlignmentVector * ModelProperties.AlignmentDistanceWeight * DeltaTimeBuffer.DeltaTime;
    NextDirection = normalize(NextDirection);
    
    // Calculate next position based on next direction, current position, speed and delta time
    float3 NextPosition = CalculateNextPosition(CurrentBoid.BoidPos, NextDirection, ModelProperties.BoidSpeed, DeltaTimeBuffer.DeltaTime);
    
    // Force align pos/dir within bounding box
    float2x3 AlignedPosDir = ForceAlignWithinBounds(NextPosition, NextDirection, BoundingBox.HalfSize);
    NextPosition = float3(AlignedPosDir[0][0], AlignedPosDir[0][1], AlignedPosDir[0][2]);
    NextDirection = float3(AlignedPosDir[1][0], AlignedPosDir[1][1], AlignedPosDir[1][2]);
    
    // No need to worry about syncing threads here -
    // all threads can write at their own discretion as no current data in use is being modified
    OutProperties[IN.DispatchThreadID.x].BoidPos = float4(NextPosition.x, NextPosition.y, NextPosition.z, 0);
    OutProperties[IN.DispatchThreadID.x].BoidDir = float4(NextDirection.x, NextDirection.y, NextDirection.z, 0);
}