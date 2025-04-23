// Input of Vertex Shader
struct VertexPosColorIndex
{
    float3 Position : POSITION;
    float3 Color : COLOR;
    uint Index : SV_InstanceID;
};

// Ouput of Vertex Shader
struct VertexShaderOutput
{
    float4 Color : COLOR;
    float4 Position : SV_Position;
};

// Calculate rotation matrix from Quaternion (w = angle, x, y, z = axis)
float4x4 RotationMatrixFromQuaternion(float4 Quaternion)
{
    float4x4 RotationMatrix;
    
    Quaternion = float4(normalize(Quaternion));
    
    float x = Quaternion.x;
    float y = Quaternion.y;
    float z = Quaternion.z;
    float w = Quaternion.w;
    
    RotationMatrix = float4x4(1 - (2 * ((y * y) + (z * z))), 2 * ((x * y) - (z * w)), 2 * ((x * z) + (y * w)), 0,
                              2 * ((x * y) + (z * w)), 1 - (2 * ((x * x) + (z * z))), 2 * ((y * z) - (x * w)), 0,
                              2 * ((x * z) - (y * w)), 2 * ((y * z) + (x * w)), 1 - (2 * ((x * x) + (y * y))), 0,
                              0, 0, 0, 1);
    
    return RotationMatrix;
}

// Calculate quaternion given axis of rotation and angle
float4 QuaternionFromAxisAngle(float3 Axis, float Angle)
{
    float4 Quaternion = float4(0, 0, 0, 0);
    
    float HalfAngle = float(Angle / 2);
    Quaternion.x = float(mul(Axis.x, sin(HalfAngle)));
    Quaternion.y = float(mul(Axis.y, sin(HalfAngle)));
    Quaternion.z = float(mul(Axis.z, sin(HalfAngle)));
    Quaternion.w = float(cos(HalfAngle));
    
    return Quaternion;
}

float4x4 CalculateBoidRotationMatrix(float3 BoidDir)
{
    float4x4 RotationMatrix = float4x4(1, 0, 0, 0,
                                       0, 1, 0, 0,
                                       0, 0, 1, 0,
                                       0, 0, 0, 1);
    
    // Boid rotates in relation to world forward, or "Up" direction for pyramid-shaped boid
    float3 TargetDir = BoidDir;
    float3 CurrentDir = float3(0, 1, 0);
    
    // Return rotation to point the boid towards its direction vector
    float Dot = dot(CurrentDir, TargetDir);
    if (Dot < 1.0f)
    {
        float Angle = acos(Dot);
        
        // Axis of rotation becomes the perpendicular vector between world forward and boid targeted direction
        float3 Axis = cross(CurrentDir, TargetDir);
        Axis = normalize(Axis);
        
        if (Axis.x == 0 && Axis.y == 0 && Axis.z == 0)
        {
            // Rotate around Z axis if too close
            Axis = float3(0, 1, 0);
        }
        
        // Calculate quaternion given both axis of rotation and angle, use that to generate rotation matrix
        float4 Quaternion = QuaternionFromAxisAngle(Axis, Angle);
        RotationMatrix = RotationMatrixFromQuaternion(Quaternion);
    }
    
    return RotationMatrix;
}

float4x4 CalculateBoidWorldViewProjectionMatrix(float3 BoidPos, float3 BoidDir, float4x4 CameraViewMatrix, float4x4 CameraProjMatrix)
{
    float4x4 translationMatrix = float4x4(1, 0, 0, 0,
                                          0, 1, 0, 0,
                                          0, 0, 1, 0,
                                          BoidPos.x, BoidPos.y, BoidPos.z, 1);
    
    // Calculate boid's rotation based on its direction
    float4x4 rotationMatrix = CalculateBoidRotationMatrix(BoidDir);
    rotationMatrix = transpose(rotationMatrix);
    
    float4x4 scaleMatrix = float4x4(1, 0, 0, 0,
                                    0, 1, 0, 0,
                                    0, 0, 1, 0,
                                    0, 0, 0, 1);
    
    float4x4 worldMatrix = mul(scaleMatrix, mul(rotationMatrix, translationMatrix));
    float4x4 cameraViewProjectionMatrix = mul(CameraViewMatrix, CameraProjMatrix);
    float4x4 worldViewProjectionMatrix = mul(worldMatrix, cameraViewProjectionMatrix);
    
    return transpose(worldViewProjectionMatrix);
}

struct CameraViewProjectionMatrices
{
    matrix CameraViewMatrix;
    matrix CameraProjectionMatrix;
};

struct BoidProperties
{
    float4 BoidPos;
    float4 BoidDir;
};

RWStructuredBuffer<BoidProperties> Properties : register(u0);
ConstantBuffer<CameraViewProjectionMatrices> CameraViewProjection : register(b0);

VertexShaderOutput main(VertexPosColorIndex IN)
{
    VertexShaderOutput OUT;
    
    float3 Pos = float3(Properties[IN.Index].BoidPos.x, Properties[IN.Index].BoidPos.y, Properties[IN.Index].BoidPos.z);
    float3 Dir = float3(Properties[IN.Index].BoidDir.x, Properties[IN.Index].BoidDir.y, Properties[IN.Index].BoidDir.z);
   
    // Final Camera View + Proj Matrices
    float4x4 CameraViewMatrix = transpose(CameraViewProjection.CameraViewMatrix);
    float4x4 CameraProjMatrix = transpose(CameraViewProjection.CameraProjectionMatrix);
    
    OUT.Position = mul(CalculateBoidWorldViewProjectionMatrix(Pos, Dir, CameraViewMatrix, CameraProjMatrix), float4(IN.Position, 1));
    OUT.Color = float4(IN.Color, 1.0f);

    return OUT;
}