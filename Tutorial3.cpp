#include <Tutorial3.h>

#include <Application.h>
#include <CommandQueue.h>
#include <CommandList.h>
#include <Helpers.h>
#include <Light.h>
#include <Material.h>
#include <Window.h>
#include <filesystem>

#include <wrl.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <DirectXColors.h>

using namespace Microsoft::WRL;
using namespace DirectX;

#include <algorithm> // For std::min and std::max.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#include "BoidRenderSystem.h"
#include "BoidObject.h"

// Clamp a value between a min and max range.
template<typename T>
constexpr const T& clamp( const T& val, const T& min, const T& max )
{
    return val < min ? min : val > max ? max : val;
}

// Builds a look-at (world) matrix from a point, up and direction vectors.
XMMATRIX XM_CALLCONV LookAtMatrix( FXMVECTOR Position, FXMVECTOR Direction, FXMVECTOR Up )
{
    assert( !XMVector3Equal( Direction, XMVectorZero() ) );
    assert( !XMVector3IsInfinite( Direction ) );
    assert( !XMVector3Equal( Up, XMVectorZero() ) );
    assert( !XMVector3IsInfinite( Up ) );

    XMVECTOR R2 = XMVector3Normalize( Direction );

    XMVECTOR R0 = XMVector3Cross( Up, R2 );
    R0 = XMVector3Normalize( R0 );

    XMVECTOR R1 = XMVector3Cross( R2, R0 );

    XMMATRIX M( R0, R1, R2, Position );

    return M;
}

Tutorial3::Tutorial3( const std::wstring& name, int width, int height, bool vSync )
    : super( name, width, height, vSync )
    , m_ScissorRect( CD3DX12_RECT( 0, 0, LONG_MAX, LONG_MAX ) )
    , m_Viewport( CD3DX12_VIEWPORT( 0.0f, 0.0f, static_cast<float>( width ), static_cast<float>( height ) ) )
    , m_Forward( 0 )
    , m_Backward( 0 )
    , m_Left( 0 )
    , m_Right( 0 )
    , m_Up( 0 )
    , m_Down( 0 )
    , m_Pitch( 0 )
    , m_Yaw( 0 )
    , m_Shift( false )
    , m_Width( 0 )
    , m_Height( 0 )
{

    XMVECTOR cameraPos = XMVectorSet( 0, 5, -20, 1 );
    XMVECTOR cameraTarget = XMVectorSet( 0, 5, 0, 1 );
    XMVECTOR cameraUp = XMVectorSet( 0, 1, 0, 0 );

    m_Camera.set_LookAt( cameraPos, cameraTarget, cameraUp );

    m_pAlignedCameraData = (CameraData*)_aligned_malloc( sizeof( CameraData ), 16 );
    
    m_pAlignedCameraData->m_InitialCamPos = m_Camera.get_Translation();
    m_pAlignedCameraData->m_InitialCamRot = m_Camera.get_Rotation();
}

Tutorial3::~Tutorial3()
{
    _aligned_free( m_pAlignedCameraData );

    delete m_BoidRenderSystem;
    m_BoidRenderSystem = nullptr;

    delete m_BoidPhysicsSystem;
    m_BoidPhysicsSystem = nullptr;

    delete m_BoidMatricesDoubleBuffer[0];
    m_BoidMatricesDoubleBuffer[0] = nullptr;

    delete m_BoidMatricesDoubleBuffer[1];
    m_BoidMatricesDoubleBuffer[1] = nullptr;

    m_BoidObjects.clear();
    m_BoidObjects.shrink_to_fit();
}

bool Tutorial3::LoadContent()
{
    auto device = Application::Get().GetDevice();
    auto commandQueue = Application::Get().GetCommandQueue( D3D12_COMMAND_LIST_TYPE_COPY );
    auto commandList = commandQueue->GetCommandList();

    // Create Boids Systems
    m_BoidRenderSystem = new BoidRenderSystem(*commandList);
    m_BoidPhysicsSystem = new BoidPhysicsSystem();

    // Create Boids Double Buffers
    m_BoidMatricesDoubleBuffer[0] = new UnorderedAccessViewBuffer();
    m_BoidMatricesDoubleBuffer[1] = new UnorderedAccessViewBuffer();

    // Initialize Camera Matrices
    CamViewProj.CameraView = m_Camera.get_ViewMatrix();
    CamViewProj.CameraProjection = m_Camera.get_ProjectionMatrix();

    // Initialize query heap for detailed GPU execution timings. Create multiple queries for both render and update functions
    D3D12_QUERY_HEAP_DESC heapDesc = { };
    heapDesc.Count = 2;
    heapDesc.NodeMask = 0;
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_QueryHeap));
    device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_RenderQueryHeap));

    // Setup readback buffer to grab info from query
    D3D12_HEAP_PROPERTIES readbackHeapProperties{};
    readbackHeapProperties.Type = D3D12_HEAP_TYPE_READBACK;
    readbackHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    readbackHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    readbackHeapProperties.CreationNodeMask = 1;
    readbackHeapProperties.VisibleNodeMask = 1;

    // Setup readback buffer description
    D3D12_RESOURCE_DESC readbackBufferDesc{};
    readbackBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    readbackBufferDesc.Width = sizeof(UINT64) * 2; // Size for four timestamps
    readbackBufferDesc.Height = 1;
    readbackBufferDesc.DepthOrArraySize = 1;
    readbackBufferDesc.MipLevels = 1;
    readbackBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    readbackBufferDesc.SampleDesc.Count = 1;
    readbackBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    readbackBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // Initialize readback buffer resource. Create multiple resources for both render and update functions
    ThrowIfFailed(device->CreateCommittedResource(
        &readbackHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &readbackBufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_ReadbackBuffer)));

    ThrowIfFailed(device->CreateCommittedResource(
        &readbackHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &readbackBufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_RenderReadbackBuffer)));

    // Undo disable optimization on BoidsVertexShader when finished debugging, for improved performance.
    // Same for enabling debug info.
    //DWORD compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_ENABLE_STRICTNESS;

    //ComPtr<ID3DBlob> errorBlob;

    // Load Boids vertex and pixel shader cso data
    ComPtr<ID3DBlob> BoidsVertexShaderBlob;
    //ThrowIfFailed(D3DCompileFromFile(L"Tutorial3/shaders/BoidsVertexShader.hlsl", NULL, NULL, "main", "vs_5_1", compileFlags, 0, &BoidsVertexShaderBlob, &errorBlob));
    ThrowIfFailed(D3DReadFileToBlob(L"data/shaders/Tutorial3/BoidsVertexShader.cso", &BoidsVertexShaderBlob));

    ComPtr<ID3DBlob> BoidsPixelShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"data/shaders/Tutorial3/BoidsPixelShader.cso", &BoidsPixelShaderBlob));

    // Load Compute Shaders with varying thread types
    ComPtr<ID3DBlob> BoidsComputeShaderBlob128;
    ThrowIfFailed(D3DReadFileToBlob(L"data/shaders/Tutorial3/BoidsComputeShader128.cso", &BoidsComputeShaderBlob128));
    ComPtr<ID3DBlob> BoidsComputeShaderBlob256;
    ThrowIfFailed(D3DReadFileToBlob(L"data/shaders/Tutorial3/BoidsComputeShader256.cso", &BoidsComputeShaderBlob256));
    ComPtr<ID3DBlob> BoidsComputeShaderBlob512;
    ThrowIfFailed(D3DReadFileToBlob(L"data/shaders/Tutorial3/BoidsComputeShader512.cso", &BoidsComputeShaderBlob512));
    ComPtr<ID3DBlob> BoidsComputeShaderBlob1024;
    ThrowIfFailed(D3DReadFileToBlob(L"data/shaders/Tutorial3/BoidsComputeShader1024.cso", &BoidsComputeShaderBlob1024));

    // Load Async Compute Shaders with varying thread types
    ComPtr<ID3DBlob> BoidsAsyncComputeShaderBlob128;
    ThrowIfFailed(D3DReadFileToBlob(L"data/shaders/Tutorial3/BoidsAsyncComputeShader128.cso", &BoidsAsyncComputeShaderBlob128));
    ComPtr<ID3DBlob> BoidsAsyncComputeShaderBlob256;
    ThrowIfFailed(D3DReadFileToBlob(L"data/shaders/Tutorial3/BoidsAsyncComputeShader256.cso", &BoidsAsyncComputeShaderBlob256));
    ComPtr<ID3DBlob> BoidsAsyncComputeShaderBlob512;
    ThrowIfFailed(D3DReadFileToBlob(L"data/shaders/Tutorial3/BoidsAsyncComputeShader512.cso", &BoidsAsyncComputeShaderBlob512));
    ComPtr<ID3DBlob> BoidsAsyncComputeShaderBlob1024;
    ThrowIfFailed(D3DReadFileToBlob(L"data/shaders/Tutorial3/BoidsAsyncComputeShader1024.cso", &BoidsAsyncComputeShaderBlob1024));

    // Create root signatures.
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if ( FAILED( device->CheckFeatureSupport( D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof( featureData ) ) ) )
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    // Create a Boids Standard GPU Compute root signature
    CD3DX12_DESCRIPTOR_RANGE1 BoidsMatrices(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
    D3D12_ROOT_SIGNATURE_FLAGS ComputeBoidsRootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    CD3DX12_ROOT_PARAMETER1 ComputeRootParameters[4];
    ComputeRootParameters[0].InitAsDescriptorTable(1, &BoidsMatrices, D3D12_SHADER_VISIBILITY_ALL);
    ComputeRootParameters[1].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
    ComputeRootParameters[2].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
    ComputeRootParameters[3].InitAsConstantBufferView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC ComputeRootSignatureDesc;
    ComputeRootSignatureDesc.Init_1_1(_countof(ComputeRootParameters), ComputeRootParameters, 0, nullptr, ComputeBoidsRootSignatureFlags);

    m_ComputeRootSignature.SetRootSignatureDesc(ComputeRootSignatureDesc.Desc_1_1, featureData.HighestVersion);

    // Create a Boids Async Compute root signature
    CD3DX12_DESCRIPTOR_RANGE1 BoidsFutureMatrices(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

    CD3DX12_ROOT_PARAMETER1 AsyncComputeRootParameters[5];
    AsyncComputeRootParameters[0].InitAsDescriptorTable(1, &BoidsMatrices, D3D12_SHADER_VISIBILITY_ALL);
    AsyncComputeRootParameters[1].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
    AsyncComputeRootParameters[2].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
    AsyncComputeRootParameters[3].InitAsConstantBufferView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
    AsyncComputeRootParameters[4].InitAsDescriptorTable(1, &BoidsFutureMatrices, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC AsyncComputeRootSignatureDesc;
    AsyncComputeRootSignatureDesc.Init_1_1(_countof(AsyncComputeRootParameters), AsyncComputeRootParameters, 0, nullptr, ComputeBoidsRootSignatureFlags);

    m_AsyncComputeRootSignature.SetRootSignatureDesc(AsyncComputeRootSignatureDesc.Desc_1_1, featureData.HighestVersion);

    // Create a Boids root signature
    D3D12_ROOT_SIGNATURE_FLAGS BoidsRootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    CD3DX12_ROOT_PARAMETER1 BoidsRootParameters[2];
    BoidsRootParameters[0].InitAsDescriptorTable(1, &BoidsMatrices, D3D12_SHADER_VISIBILITY_ALL);
    BoidsRootParameters[1].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC BoidsRootSignautreDesc;
    BoidsRootSignautreDesc.Init_1_1(_countof(BoidsRootParameters), BoidsRootParameters, 0, nullptr, BoidsRootSignatureFlags);

    m_BoidsRootSignature.SetRootSignatureDesc(BoidsRootSignautreDesc.Desc_1_1, featureData.HighestVersion);

    // Setup the pipeline state.
    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS VS;
        CD3DX12_PIPELINE_STATE_STREAM_PS PS;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
    } pipelineStateStream;

    // sRGB formats provide free gamma correction!
    DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;

    // Check the best multisample quality level that can be used for the given back buffer format.
    DXGI_SAMPLE_DESC sampleDesc = Application::Get().GetMultisampleQualityLevels( backBufferFormat, D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT );

    D3D12_RT_FORMAT_ARRAY rtvFormats = {};
    rtvFormats.NumRenderTargets = 1;
    rtvFormats.RTFormats[0] = backBufferFormat;

    // Create a Boids graphics pipeline state for both GPU + Async and CPU versions 
    const D3D12_INPUT_ELEMENT_DESC BoidsInputLayout[] =
    {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "ID", 0, DXGI_FORMAT_R32_UINT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 0},
    };

    pipelineStateStream.pRootSignature = m_BoidsRootSignature.GetRootSignature().Get();
    pipelineStateStream.InputLayout = { BoidsInputLayout, _countof(BoidsInputLayout) };
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(BoidsVertexShaderBlob.Get());
    pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(BoidsPixelShaderBlob.Get());
    pipelineStateStream.DSVFormat = depthBufferFormat;
    pipelineStateStream.RTVFormats = rtvFormats;
    pipelineStateStream.SampleDesc = sampleDesc;

    D3D12_PIPELINE_STATE_STREAM_DESC BoidsPipelineStateStreamDesc = {
        sizeof(PipelineStateStream), &pipelineStateStream
    };
    ThrowIfFailed(device->CreatePipelineState(&BoidsPipelineStateStreamDesc, IID_PPV_ARGS(&m_BoidsPipelineState)));

    // Create a Compute Boids pipeline state
    struct ComputePipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS CS;
    } ComputePipelineStateStream;

    // Initialize Compute Pipelines based on Async/Standard GPU compute shaders and differing numbers of threads
    // Start of standard GPU compute pipelines
    ComputePipelineStateStream.pRootSignature = m_ComputeRootSignature.GetRootSignature().Get();
    ComputePipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(BoidsComputeShaderBlob128.Get());

    D3D12_PIPELINE_STATE_STREAM_DESC ComputePipelineStateStreamDesc = {
        sizeof(ComputePipelineStateStream), &ComputePipelineStateStream
    };
    ThrowIfFailed(device->CreatePipelineState(&ComputePipelineStateStreamDesc, IID_PPV_ARGS(&m_ComputePipelineState128)));

    ComputePipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(BoidsComputeShaderBlob256.Get());
    ComputePipelineStateStreamDesc = { sizeof(ComputePipelineStateStream), &ComputePipelineStateStream };
    ThrowIfFailed(device->CreatePipelineState(&ComputePipelineStateStreamDesc, IID_PPV_ARGS(&m_ComputePipelineState256)));

    ComputePipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(BoidsComputeShaderBlob512.Get());
    ComputePipelineStateStreamDesc = { sizeof(ComputePipelineStateStream), &ComputePipelineStateStream };
    ThrowIfFailed(device->CreatePipelineState(&ComputePipelineStateStreamDesc, IID_PPV_ARGS(&m_ComputePipelineState512)));

    ComputePipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(BoidsComputeShaderBlob1024.Get());
    ComputePipelineStateStreamDesc = { sizeof(ComputePipelineStateStream), &ComputePipelineStateStream };
    ThrowIfFailed(device->CreatePipelineState(&ComputePipelineStateStreamDesc, IID_PPV_ARGS(&m_ComputePipelineState1024)));

    // Start of Async Compute pipelines
    ComputePipelineStateStream.pRootSignature = m_AsyncComputeRootSignature.GetRootSignature().Get();
    ComputePipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(BoidsAsyncComputeShaderBlob128.Get());
    ComputePipelineStateStreamDesc = { sizeof(ComputePipelineStateStream), &ComputePipelineStateStream };
    ThrowIfFailed(device->CreatePipelineState(&ComputePipelineStateStreamDesc, IID_PPV_ARGS(&m_AsyncPipelineState128)));

    ComputePipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(BoidsAsyncComputeShaderBlob256.Get());
    ComputePipelineStateStreamDesc = { sizeof(ComputePipelineStateStream), &ComputePipelineStateStream };
    ThrowIfFailed(device->CreatePipelineState(&ComputePipelineStateStreamDesc, IID_PPV_ARGS(&m_AsyncPipelineState256)));

    ComputePipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(BoidsAsyncComputeShaderBlob512.Get());
    ComputePipelineStateStreamDesc = { sizeof(ComputePipelineStateStream), &ComputePipelineStateStream };
    ThrowIfFailed(device->CreatePipelineState(&ComputePipelineStateStreamDesc, IID_PPV_ARGS(&m_AsyncPipelineState512)));

    ComputePipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(BoidsAsyncComputeShaderBlob1024.Get());
    ComputePipelineStateStreamDesc = { sizeof(ComputePipelineStateStream), &ComputePipelineStateStream };
    ThrowIfFailed(device->CreatePipelineState(&ComputePipelineStateStreamDesc, IID_PPV_ARGS(&m_AsyncPipelineState1024)));

    // Create an off-screen render target with a single color buffer and a depth buffer.
    auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D( backBufferFormat,
                                                   m_Width, m_Height,
                                                   1, 1,
                                                   sampleDesc.Count, sampleDesc.Quality,
                                                   D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET );
    D3D12_CLEAR_VALUE colorClearValue;
    colorClearValue.Format = colorDesc.Format;
    colorClearValue.Color[0] = 0.4f;
    colorClearValue.Color[1] = 0.6f;
    colorClearValue.Color[2] = 0.9f;
    colorClearValue.Color[3] = 1.0f;

    Texture colorTexture = Texture( colorDesc, &colorClearValue, 
                                    TextureUsage::RenderTarget, 
                                    L"Color Render Target" );

    // Create a depth buffer.
    auto depthDesc = CD3DX12_RESOURCE_DESC::Tex2D( depthBufferFormat, 
                                                   m_Width, m_Height,
                                                   1, 1, 
                                                   sampleDesc.Count, sampleDesc.Quality,
                                                   D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL );
    D3D12_CLEAR_VALUE depthClearValue;
    depthClearValue.Format = depthDesc.Format;
    depthClearValue.DepthStencil = { 1.0f, 0 };

    Texture depthTexture = Texture( depthDesc, &depthClearValue, 
                                    TextureUsage::Depth, 
                                    L"Depth Render Target" );

    // Attach the textures to the render target.
    m_RenderTarget.AttachTexture( AttachmentPoint::Color0, colorTexture );
    m_RenderTarget.AttachTexture( AttachmentPoint::DepthStencil, depthTexture );

    auto fenceValue = commandQueue->ExecuteCommandList( commandList );
    commandQueue->WaitForFenceValue( fenceValue );

    return true;
}

void Tutorial3::OnResize( ResizeEventArgs& e )
{
    super::OnResize( e );

    if ( m_Width != e.Width || m_Height != e.Height )
    {
        m_Width = std::max( 1, e.Width );
        m_Height = std::max( 1, e.Height );

        float aspectRatio = m_Width / (float)m_Height;
        m_Camera.set_Projection( 45.0f, aspectRatio, 0.1f, 100.0f );

        m_Viewport = CD3DX12_VIEWPORT( 0.0f, 0.0f,
            static_cast<float>(m_Width), static_cast<float>(m_Height));

        m_RenderTarget.Resize( m_Width, m_Height );
    }
}

void Tutorial3::BeginSimulation()
{
    // Remove all previous boids from model
    m_BoidObjects.clear();
    m_BoidObjects.shrink_to_fit();

    // Physics system deletes data and sets pointers to null
    m_BoidPhysicsSystem->DeleteAllBoids();

    // Exit early if incorrect number of boids entered
    if (m_BoidCount <= 0)
    {
        m_EnableCPUVersion = false;
        m_EnableGPUVersion = false;
        m_EnableAsyncCompute = false;

        return;
    }

    // Initialize all boids within vector
    for (int i = 0; i < m_BoidCount; i++)
    {
        m_BoidObjects.push_back(new BoidObject());
    }

    // Register boids with physics system to randomize directions, regardless of being in GPU/Async mode
    m_BoidPhysicsSystem->RegisterBoids(m_BoidObjects);

    if (m_EnableGPUVersion)
    {
        auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
        auto commandList = commandQueue->GetCommandList();

        size_t numElements = m_BoidObjects.size();
        size_t elementSize = sizeof(BoidProperties);
        size_t bufferSize = numElements * elementSize;

        // Initialize single buffer in standard GPU approach
        CD3DX12_RESOURCE_DESC bufferType = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        m_BoidMatricesUAVBuffer.SetResource(bufferType, nullptr, L"BoidBuffer");
        m_BoidMatricesUAVBuffer.CreateViews(m_BoidObjects.size(), sizeof(BoidProperties));

        // Initialize buffer with basic boids data
        commandList->CopyBuffer(m_BoidMatricesUAVBuffer, m_BoidObjects.size(), sizeof(BoidProperties), m_BoidPhysicsSystem->GetBoidProperties().data(), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        // This ensures that the command lists are finished before moving on to render first frame
        auto fenceValue = commandQueue->ExecuteCommandList(commandList);
        commandQueue->WaitForFenceValue(fenceValue);
    }
    if (m_EnableAsyncCompute)
    {
        auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
        auto commandList = commandQueue->GetCommandList();

        size_t numElements = m_BoidObjects.size();
        size_t elementSize = sizeof(BoidProperties);
        size_t bufferSize = numElements * elementSize;

        // Initialize both current and next frame buffers
        CD3DX12_RESOURCE_DESC bufferType = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        m_BoidMatricesDoubleBuffer[1]->SetResource(bufferType, nullptr, L"BoidFrameBuffer1");
        m_BoidMatricesDoubleBuffer[1]->CreateViews(m_BoidObjects.size(), sizeof(BoidProperties));
        
        m_BoidMatricesDoubleBuffer[0]->SetResource(bufferType, nullptr, L"BoidFrameBuffer0");
        m_BoidMatricesDoubleBuffer[0]->CreateViews(m_BoidObjects.size(), sizeof(BoidProperties));

        // Initialize buffers with basic boids data
        commandList->CopyBuffer(*m_BoidMatricesDoubleBuffer[0], m_BoidObjects.size(), sizeof(BoidProperties), m_BoidPhysicsSystem->GetBoidProperties().data(), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        commandList->CopyBuffer(*m_BoidMatricesDoubleBuffer[1], m_BoidObjects.size(), sizeof(BoidProperties), m_BoidPhysicsSystem->GetBoidProperties().data(), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        // This ensures that the command lists are finished before moving on to render first frame
        uint64_t fenceValue = commandQueue->ExecuteCommandList(commandList);
        commandQueue->WaitForFenceValue(fenceValue);
    }
}

void Tutorial3::PrintResultsToTextFile(const char filename[], std::queue<double>& ValueQueue)
{
    // Create and open a text file
    std::ofstream CurrentFile(filename);

    // Apply results in row of 10
    int ResultsPerLine = 10;
    int CurrentResultsThisLine = 0;
    int FullQueueSize = ValueQueue.size();
    for (int i = 0; i < FullQueueSize; i++)
    {
        CurrentFile << ValueQueue.front() << ", ";
        CurrentResultsThisLine++;

        if (CurrentResultsThisLine >= ResultsPerLine)
        {
            CurrentResultsThisLine = 0;
            CurrentFile << std::endl;
        }

        ValueQueue.pop();
    }

    if (!CurrentFile.is_open())
    {
        bool test = false;
    }

    auto TestOutput = std::filesystem::current_path();

    // Close the file
    CurrentFile.close();
}

void Tutorial3::SwapBoidsDoubleBuffers()
{
    // Swap pointers of buffers around, making next buffer the current buffer in preparation for next frame
    UnorderedAccessViewBuffer* Temp = m_BoidMatricesDoubleBuffer[0];
    
    m_BoidMatricesDoubleBuffer[0] = m_BoidMatricesDoubleBuffer[1];
    m_BoidMatricesDoubleBuffer[1] = Temp;
}

void Tutorial3::CalculateGPUQueryTime(CommandQueue& commandQueue, std::vector<double>& TimePerFrameVector, Microsoft::WRL::ComPtr<ID3D12Resource> ReadbackBuffer)
{
    UINT64 gpuFrequency = 0;
    commandQueue.GetD3D12CommandQueue()->GetTimestampFrequency(&gpuFrequency);

    // Grab data from readback buffer
    UINT64* timestampData;
    ReadbackBuffer->Map(0, nullptr, reinterpret_cast<void**>(&timestampData));

    UINT64 startTimestamp = timestampData[0];
    UINT64 endTimestamp = timestampData[1];

    ReadbackBuffer->Unmap(0, nullptr);

    // Convert timestamps to milliseconds
    UINT64 delta = endTimestamp - startTimestamp;
    double timeDifference;
    if (endTimestamp > startTimestamp)
    {
        double frequency = static_cast<double>(gpuFrequency);
        timeDifference = (delta / frequency) * 1000;

        // Add value to per-frame data, if in appropriate mode
        TimePerFrameVector.push_back(timeDifference);
    }
}

double Tutorial3::CalculateAverageTimePerSecond(std::vector<double> TimeVector)
{
    // Early out if we have not captured any frame results, failed to capture them appropriately
    if (TimeVector.size() == 0)
    {
        return 0;
    }

    // Calculate average of all elements in vector
    double Total = 0;
    for (int i = 0; i < TimeVector.size(); i++)
    {
        Total += TimeVector[i];
    }

    if (TimeVector.size() != 1)
    {
        Total /= TimeVector.size();
    }

    return Total;
}

void Tutorial3::UpdateResults(std::vector<double>& TimePerFrameVector, std::queue<double>& TimePerSecondQueue, double& CurrentTime)
{
    // Calculate average ms over entire second - only save result if capturing is enabled
    CurrentTime = CalculateAverageTimePerSecond(TimePerFrameVector);
    if (m_EnableCapturingResults)
    {
        if (TimePerSecondQueue.size() < m_AmountOfCaptures)
        {
            TimePerSecondQueue.push(CurrentTime);
        }
    }

    TimePerFrameVector.clear();
    TimePerFrameVector.shrink_to_fit();
}

void Tutorial3::UnloadContent()
{
}

void Tutorial3::OnUpdate( UpdateEventArgs& e )
{
    static double totalTime = 0.0;

    super::OnUpdate( e );

    totalTime += e.ElapsedTime;

    // Generate results every second
    if ( totalTime >= 1.0 )
    {

        if (m_EnableCPUVersion)
        {
            UpdateResults(m_CPUCalculationTimePerFrame, m_CPUCalculationTimePerSecond, m_CurrentCPUTime);
            UpdateResults(m_RenderCalculationTimePerFrame, m_RenderCalculationTimePerSecond, m_CurrentRenderTime);
            UpdateResults(m_FullCalculationTimePerFrame, m_FullCalculationTimePerSecond, m_CurrentOverallTime);
        }
        else if (m_EnableGPUVersion || m_EnableAsyncCompute)
        {
            UpdateResults(m_GPUCalculationTimePerFrame, m_GPUCalculationTimePerSecond, m_CurrentGPUTime);
            UpdateResults(m_RenderCalculationTimePerFrame, m_RenderCalculationTimePerSecond, m_CurrentRenderTime);
            UpdateResults(m_FullCalculationTimePerFrame, m_FullCalculationTimePerSecond, m_CurrentOverallTime);
        }

        totalTime = 0.0;
    }

    if (m_ShouldResetSimulation)
    {
        BeginSimulation();

        m_ShouldResetSimulation = false;
    }

    // Update boids physics system
    if (m_RunningSimulation && m_EnableCPUVersion)
    {
        auto StartPhysics = std::chrono::high_resolution_clock::now();

        m_BoidPhysicsSystem->UpdateBoidPhysics(static_cast<float>(e.ElapsedTime));

        auto StopPhysics = std::chrono::high_resolution_clock::now();
        auto DurationPhysics = std::chrono::duration_cast<std::chrono::microseconds>(StopPhysics - StartPhysics);
        double TotalPhysicsTime = static_cast<double>(DurationPhysics.count());
        TotalPhysicsTime /= 1000;

        m_CPUCalculationTimePerFrame.push_back(TotalPhysicsTime);
    }

    CamViewProj.CameraView = m_Camera.get_ViewMatrix();
    CamViewProj.CameraProjection = m_Camera.get_ProjectionMatrix();

    // Update the camera.
    float speedMultipler = ( m_Shift ? 16.0f : 4.0f );

    XMVECTOR cameraTranslate = XMVectorSet( m_Right - m_Left, 0.0f, m_Forward - m_Backward, 1.0f ) * speedMultipler * static_cast<float>( e.ElapsedTime );
    XMVECTOR cameraPan = XMVectorSet( 0.0f, m_Up - m_Down, 0.0f, 1.0f ) * speedMultipler * static_cast<float>( e.ElapsedTime );
    m_Camera.Translate( cameraTranslate, Space::Local );
    m_Camera.Translate( cameraPan, Space::Local );

    XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw( XMConvertToRadians( m_Pitch ), XMConvertToRadians( m_Yaw ), 0.0f );
    m_Camera.set_Rotation( cameraRotation );

    m_TotalUpdateFunctionTime = e.ElapsedTime * 1000;
}

void Tutorial3::OnRender(RenderEventArgs& e)
{
    // Calculate FPS given OnRender and OnUpdate function timings
    static uint64_t frameCount = 0;
    static double totalTime = 0.0;
    frameCount++;
    totalTime += e.ElapsedTime;

    if (totalTime > 1)
    {
        double fps = frameCount / totalTime;
        m_FPS = fps;

        if (m_EnableCapturingResults)
        {
            m_PreviousFPSValues.push(m_FPS);
            if (m_PreviousFPSValues.size() > m_AmountOfCaptures)
            {
                m_PreviousFPSValues.pop();
            }
        }

        frameCount = 0;
        totalTime = 0.0;
    }

    // Start timing Render function
    auto Start = std::chrono::high_resolution_clock::now();

    super::OnRender(e);

    auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    auto commandList = commandQueue->GetCommandList();

    // Clear the render targets.
    {
        FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };

        commandList->ClearTexture(m_RenderTarget.GetTexture(AttachmentPoint::Color0), clearColor);
        commandList->ClearDepthStencilTexture(m_RenderTarget.GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH);
    }

    if (m_RunningSimulation)
    {
        // Initialize UAV description for applying Unordered Access Views to graphics/compute pipeline
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.NumElements = static_cast<UINT>(m_BoidObjects.size());
        uavDesc.Buffer.StructureByteStride = static_cast<UINT>(sizeof(BoidProperties));
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        uint64_t ComputeFenceValue = 0;

        if (m_EnableGPUVersion || m_EnableAsyncCompute)
        {
            auto computeCommandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
            auto computeCommandList = computeCommandQueue->GetCommandList();

            // Apply timestamp after previous command
            computeCommandList->GetGraphicsCommandList()->EndQuery(m_QueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);

            // Apply Boids Compute Pipeline
            computeCommandList->SetPipelineState(m_CurrentComputePipelineState);

            // Setup compute root signature and apply only one buffer for single buffer approach
            if (m_EnableGPUVersion)
            {
                computeCommandList->SetComputeRootSignature(m_ComputeRootSignature);
                computeCommandList->SetUnorderedAccessView(0, 0, m_BoidMatricesUAVBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, 0, &uavDesc);
            }

            // Setup async root signature and apply both buffers of the double buffer to compute shader
            if (m_EnableAsyncCompute)
            {
                computeCommandList->SetComputeRootSignature(m_AsyncComputeRootSignature);
                computeCommandList->SetUnorderedAccessView(4, 0, *m_BoidMatricesDoubleBuffer[1], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, 0, &uavDesc);
                computeCommandList->SetUnorderedAccessView(0, 0, *m_BoidMatricesDoubleBuffer[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, 0, &uavDesc);
            }

            // Ensure model properties, delta time and bounding box limits are sent to compute shader
            computeCommandList->SetComputeDynamicConstantBuffer(1, m_BoidPhysicsSystem->GetModelProperties());
            computeCommandList->SetComputeDynamicConstantBuffer(2, XMFLOAT4(static_cast<float>(e.ElapsedTime), 0, 0, 0));
            computeCommandList->SetComputeDynamicConstantBuffer(3, m_BoidPhysicsSystem->GetBoundingBoxProperties());

            // Execute compute shader with given number of thread groups
            computeCommandList->Dispatch(m_NumOfThreadGroups, 1, 1);

            // Apply timestap after dispatch, to measure execution length of compute shader
            computeCommandList->GetGraphicsCommandList()->EndQuery(m_QueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

            // Copy query data to readback buffer
            computeCommandList->GetGraphicsCommandList()->ResolveQueryData(
                m_QueryHeap.Get(),
                D3D12_QUERY_TYPE_TIMESTAMP,
                0,
                2,
                m_ReadbackBuffer.Get(),
                0);

            computeCommandQueue->ExecuteCommandList(computeCommandList);

            // Wait until compute shader has finished executing in synchronous compute version
            if (m_EnableGPUVersion)
            {
                commandQueue->Wait(*computeCommandQueue.get());
            }
        }

        // Apply timestamp after previous command
        commandList->GetGraphicsCommandList()->EndQuery(m_RenderQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);

        // Apply Boids Graphics Pipeline
        commandList->SetPipelineState(m_BoidsPipelineState);
        commandList->SetGraphicsRootSignature(m_BoidsRootSignature);

        commandList->SetViewport(m_Viewport);
        commandList->SetScissorRect(m_ScissorRect);
        commandList->SetRenderTarget(m_RenderTarget);

        commandList->SetGraphicsDynamicConstantBuffer(1, CamViewProj);

        // Determine the number of elements and size of each one to appropriately copy boids info to UAV
        size_t numElements = m_BoidObjects.size();
        size_t elementSize = sizeof(BoidProperties);
        size_t bufferSize = numElements * elementSize;

        // Update buffer based on CPU calculation of Boids algorithm
        if (m_EnableCPUVersion)
        {
            commandList->CopyBuffer(m_BoidMatricesUAVBuffer, numElements, elementSize, m_BoidPhysicsSystem->GetBoidProperties().data(), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        }

        // Set UAV to single buffer or first frame of double-buffer for rendering
        if (m_EnableCPUVersion || m_EnableGPUVersion)
        {
            commandList->SetUnorderedAccessView(0, 0, m_BoidMatricesUAVBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, 0, &uavDesc);
        }
        else if (m_EnableAsyncCompute)
        {
            commandList->SetUnorderedAccessView(0, 0, *m_BoidMatricesDoubleBuffer[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, 0, &uavDesc);
        }

        // Render all boid instances
        m_BoidRenderSystem->RenderBoids(*commandList, m_BoidObjects.size());

        // Apply timestap after render, to measure execution length of compute shader
        commandList->GetGraphicsCommandList()->EndQuery(m_RenderQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

        // Grab query data and apply to readback buffer
        commandList->GetGraphicsCommandList()->ResolveQueryData(
            m_RenderQueryHeap.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            0,
            2,
            m_RenderReadbackBuffer.Get(),
            0);

        commandQueue->ExecuteCommandList(commandList);

        // Wait for both Compute shader and Rendering to finish before swapping buffers
        if (m_EnableAsyncCompute)
        {
            // Wait until all previous commands have executed, then readback from buffer so it is not updated mid-access
            commandQueue->Wait(*commandQueue.get());
            auto computeCommandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
            commandQueue->Wait(*computeCommandQueue.get());

            CalculateGPUQueryTime(*computeCommandQueue.get(), m_GPUCalculationTimePerFrame, m_ReadbackBuffer);
            CalculateGPUQueryTime(*commandQueue.get(), m_RenderCalculationTimePerFrame, m_RenderReadbackBuffer);

            SwapBoidsDoubleBuffers();
        }
    }

    if (m_EnableGPUVersion)
    {
        // Wait until all previous commands have executed, then readback from buffer so it is not updated mid-access
        commandQueue->Wait(*commandQueue.get());

        auto computeCommandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
        CalculateGPUQueryTime(*computeCommandQueue.get(), m_GPUCalculationTimePerFrame, m_ReadbackBuffer);
        CalculateGPUQueryTime(*commandQueue.get(), m_RenderCalculationTimePerFrame, m_RenderReadbackBuffer);
    }

    if (m_EnableCPUVersion)
    {        
        // Wait until all previous commands have executed, then readback from buffer so it is not updated mid-access
        commandQueue->Wait(*commandQueue.get());
        CalculateGPUQueryTime(*commandQueue.get(), m_RenderCalculationTimePerFrame, m_RenderReadbackBuffer);

    }

    // Update ImGui menu
    OnBoidsMenuGui();

    // Present, this has it's own internal fence synchronization, so count this as end to render func
    m_pWindow->Present( m_RenderTarget.GetTexture(AttachmentPoint::Color0) );

    // Stop timing render func and add render + update together
    auto Stop = std::chrono::high_resolution_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(Stop - Start);

    double TotalRenderTime = static_cast<double>(Duration.count());
    TotalRenderTime /= 1000;

    //double TotalUpdateAndRenderTime = TotalRenderTime + m_TotalUpdateFunctionTime;
    double TotalUpdateAndRenderTime = m_TotalUpdateFunctionTime + (e.ElapsedTime * 1000);
    m_FullCalculationTimePerFrame.push_back(TotalUpdateAndRenderTime);
}

static bool g_AllowFullscreenToggle = true;

void Tutorial3::OnKeyPressed( KeyEventArgs& e )
{
    super::OnKeyPressed( e );

    if ( !ImGui::GetIO().WantCaptureKeyboard )
    {
        switch ( e.Key )
        {
            case KeyCode::Escape:
                Application::Get().Quit( 0 );
                break;
            case KeyCode::Enter:
                if ( e.Alt )
                {
            case KeyCode::F11:
                if ( g_AllowFullscreenToggle )
                {
                    m_pWindow->ToggleFullscreen();
                    g_AllowFullscreenToggle = false;
                }
                break;
                }
            case KeyCode::V:
                m_pWindow->ToggleVSync();
                break;
            case KeyCode::R:
                // Reset camera transform
                m_Camera.set_Translation( m_pAlignedCameraData->m_InitialCamPos );
                m_Camera.set_Rotation( m_pAlignedCameraData->m_InitialCamRot );
                m_Pitch = 0.0f;
                m_Yaw = 0.0f;
                break;
            case KeyCode::Up:
            case KeyCode::W:
                m_Forward = 1.0f;
                break;
            case KeyCode::Left:
            case KeyCode::A:
                m_Left = 1.0f;
                break;
            case KeyCode::Down:
            case KeyCode::S:
                m_Backward = 1.0f;
                break;
            case KeyCode::Right:
            case KeyCode::D:
                m_Right = 1.0f;
                break;
            case KeyCode::Q:
                m_Down = 1.0f;
                break;
            case KeyCode::E:
                m_Up = 1.0f;
                break;
            case KeyCode::ShiftKey:
                m_Shift = true;
                break;
        }
    }
}

void Tutorial3::OnKeyReleased( KeyEventArgs& e )
{
    super::OnKeyReleased( e );

    switch ( e.Key )
    {
        case KeyCode::Enter:
            if ( e.Alt )
            {
        case KeyCode::F11:
                g_AllowFullscreenToggle = true;
            }
            break;
        case KeyCode::Up:
        case KeyCode::W:
            m_Forward = 0.0f;
            break;
        case KeyCode::Left:
        case KeyCode::A:
            m_Left = 0.0f;
            break;
        case KeyCode::Down:
        case KeyCode::S:
            m_Backward = 0.0f;
            break;
        case KeyCode::Right:
        case KeyCode::D:
            m_Right = 0.0f;
            break;
        case KeyCode::Q:
            m_Down = 0.0f;
            break;
        case KeyCode::E:
            m_Up = 0.0f;
            break;
        case KeyCode::ShiftKey:
            m_Shift = false;
            break;
    }
}

void Tutorial3::OnMouseMoved( MouseMotionEventArgs& e )
{
    super::OnMouseMoved( e );

    const float mouseSpeed = 0.1f;

    if ( !ImGui::GetIO().WantCaptureMouse )
    {
        if ( e.LeftButton )
        {
            m_Pitch -= e.RelY * mouseSpeed;

            m_Pitch = clamp( m_Pitch, -90.0f, 90.0f );

            m_Yaw -= e.RelX * mouseSpeed;
        }
    }
}

void Tutorial3::OnMouseWheel( MouseWheelEventArgs& e )
{
    if ( !ImGui::GetIO().WantCaptureMouse )
    {
        auto fov = m_Camera.get_FoV();

        fov -= e.WheelDelta;
        fov = clamp( fov, 12.0f, 90.0f );

        m_Camera.set_FoV( fov );

        char buffer[256];
        sprintf_s( buffer, "FoV: %f\n", fov );
        OutputDebugStringA( buffer );
    }
}

void Tutorial3::OnBoidsMenuGui()
{
    // Render option screens providing necessary information for testing purposes and changing model properties

    static bool showBoidOptions = true;
    static bool showModelOptions = true;

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Boid Options", nullptr, &showBoidOptions);
            ImGui::MenuItem("Model Options", nullptr, &showModelOptions);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (showModelOptions)
    {
        if (ImGui::Begin("Boid Model Settings"))
        {
            ImGui::Text("Alignment Rule Properties");
            ImGui::Text("Alignment Rule Weight: %.2f", m_BoidPhysicsSystem->GetModelProperties().AlignmentDistanceWeight);
            ImGui::SliderFloat("0", &m_NewModelProperties.AlignmentDistanceWeight, 0, 25);
            ImGui::Text("Alignment Rule Max Distance: %.2f", m_BoidPhysicsSystem->GetModelProperties().MaximumAlignmentDistance);
            ImGui::SliderFloat("1", &m_NewModelProperties.MaximumAlignmentDistance, 0, 25);
            ImGui::Text("Alignment Rule Min Distance: %.2f", m_BoidPhysicsSystem->GetModelProperties().MinimumAlignmentDistnace);
            ImGui::SliderFloat("2", &m_NewModelProperties.MinimumAlignmentDistnace, 0, 25);
            ImGui::Separator();

            ImGui::Text("Separation Rule Properties");
            ImGui::Text("Separation Rule Weight: %.2f", m_BoidPhysicsSystem->GetModelProperties().SeparationDistanceWeight);
            ImGui::SliderFloat("3", &m_NewModelProperties.SeparationDistanceWeight, 0, 25);
            ImGui::Text("Separation Rule Max Distance: %.2f", m_BoidPhysicsSystem->GetModelProperties().MaximumSeparationDistance);
            ImGui::SliderFloat("4", &m_NewModelProperties.MaximumSeparationDistance, 0, 25);
            ImGui::Text("Separation Rule Min Distance: %.2f", m_BoidPhysicsSystem->GetModelProperties().MinimumSeparationDistance);
            ImGui::SliderFloat("5", &m_NewModelProperties.MinimumSeparationDistance, 0, 25);
            ImGui::Separator();

            ImGui::Text("Cohesion Rule Properties");
            ImGui::Text("Cohesion Rule Weight: %.2f", m_BoidPhysicsSystem->GetModelProperties().CohesionDistanceWeight);
            ImGui::SliderFloat("6", &m_NewModelProperties.CohesionDistanceWeight, 0, 25);
            ImGui::Text("Cohesion Rule Max Distance: %.2f", m_BoidPhysicsSystem->GetModelProperties().MaximumCohesionDistance);
            ImGui::SliderFloat("7", &m_NewModelProperties.MaximumCohesionDistance, 0, 25);
            ImGui::Text("Cohesion Rule Min Distance: %.2f", m_BoidPhysicsSystem->GetModelProperties().MinimumCohesionDistance);
            ImGui::SliderFloat("8", &m_NewModelProperties.MinimumCohesionDistance, 0, 25);
            ImGui::Separator();

            static float TempNewBoundingBoxHalfSize[3] = { 15, 15, 15 };
            ImGui::Text("Misc Properties");
            ImGui::Text("Boid Speed: %.2f", m_BoidPhysicsSystem->GetModelProperties().BoidSpeed);
            ImGui::SliderFloat("9", &m_NewModelProperties.BoidSpeed, 0, 25);

            ImGui::Text("Bounding Box Half Size x: %.2f", m_BoidPhysicsSystem->GetBoundingBoxProperties().x);
            ImGui::SameLine();
            ImGui::Text(", y: %.2f", m_BoidPhysicsSystem->GetBoundingBoxProperties().y);
            ImGui::SameLine();
            ImGui::Text(", z: %.2f", m_BoidPhysicsSystem->GetBoundingBoxProperties().z);

            ImGui::SliderFloat3("10", TempNewBoundingBoxHalfSize, 5, 75);

            ImGui::Separator();

            if (ImGui::Button("Apply Changes"))
            {
                m_BoidPhysicsSystem->SetModelProperties(m_NewModelProperties);
                
                XMFLOAT3 NewBoundingBoxHalfSize = { TempNewBoundingBoxHalfSize[0],
                                                   TempNewBoundingBoxHalfSize[1],
                                                   TempNewBoundingBoxHalfSize[2] };

                m_BoidPhysicsSystem->SetBoundingBoxHalfSize(NewBoundingBoxHalfSize);
            }
            if (ImGui::Button("Reset"))
            {
                ModelProperties modelProperties;
                m_BoidPhysicsSystem->SetModelProperties(modelProperties);

                m_BoidPhysicsSystem->SetBoundingBoxHalfSize(DirectX::XMFLOAT3(15, 15, 15));
                TempNewBoundingBoxHalfSize[0] = 15;
                TempNewBoundingBoxHalfSize[1] = 15;
                TempNewBoundingBoxHalfSize[2] = 15;

                m_NewModelProperties = modelProperties;
            }

            ImGui::End();
        }
    }

    if (showBoidOptions)
    {
        if (ImGui::Begin("Boid Test Settings"), &showBoidOptions, ImGuiWindowFlags_AlwaysAutoResize)
        {
            ImGui::Text("Version");
            ImGui::RadioButton("Enable CPU", &m_SelectedBoidModel, 0);
            ImGui::RadioButton("Enable GPU", &m_SelectedBoidModel, 1);
            ImGui::RadioButton("Enable Async Compute", &m_SelectedBoidModel, 2);
            ImGui::Separator();

            ImGui::Text("Thread Group Size");
            ImGui::RadioButton("128", &m_SelectedThreadGroupSize, 0);
            ImGui::RadioButton("256", &m_SelectedThreadGroupSize, 1);
            ImGui::RadioButton("512", &m_SelectedThreadGroupSize, 2);
            ImGui::RadioButton("1024", &m_SelectedThreadGroupSize, 3);
            ImGui::Separator();

            ImGui::Text("Thread Block Amount");
            ImGui::RadioButton("Manual Input", &m_SelectedThreadGroupInputType, 0);
            ImGui::RadioButton("Automatic Input", &m_SelectedThreadGroupInputType, 1);
            ImGui::InputText("Numb of Blocks", m_NumOfThreadGroupsBuffer, IM_ARRAYSIZE(m_NumOfThreadGroupsBuffer));
            ImGui::Text("Current Thread Group Count: %i", m_NumOfThreadGroups);
            ImGui::Separator();

            ImGui::Text("Model Settings");
            ImGui::InputText("Numb of Boids", m_BoidNumberBuffer, IM_ARRAYSIZE(m_BoidNumberBuffer));
            ImGui::Text("Current Boid Count: %i", m_BoidObjects.size());
            ImGui::Separator();

            ImGui::Text("Bounding Box Settings");
            ImGui::Separator();

            ImGui::Text("Debug Settings");
            ImGui::Text("FPS: %f", m_FPS);
            ImGui::Text("CPU ms: %.5f", static_cast<float>(m_CurrentCPUTime));
            ImGui::Text("Compute ms: %.5f", static_cast<float>(m_CurrentGPUTime));
            ImGui::Text("Render ms: %.5f", static_cast<float>(m_CurrentRenderTime));
            ImGui::Text("Overall Frame ms: %.5f", static_cast<float>(m_CurrentOverallTime));
            ImGui::Separator();

            ImGui::Text("Capturing Results Settings");
            ImGui::RadioButton("Manual: ", &m_SelectedCaptureType, 0);
            ImGui::RadioButton("Automatic: ", &m_SelectedCaptureType, 1);
            ImGui::InputText("Num of Captures: ", m_NumOfCaptures, IM_ARRAYSIZE(m_NumOfCaptures));
            ImGui::Text("Current number of captures: %i", m_AmountOfCaptures);

            if (ImGui::Button("Start Capturing"))
            {
                m_EnableCapturingResults = true;
            }
            if (ImGui::Button("Stop Capturing"))
            {
                if (m_EnableCapturingResults)
                {
                    PrintResultsToTextFile("FPS_Results.txt", m_PreviousFPSValues);

                    if (m_EnableAsyncCompute || m_EnableGPUVersion)
                    {
                        PrintResultsToTextFile("GPU_Results.txt", m_GPUCalculationTimePerSecond);
                    }
                    else if (m_EnableCPUVersion)
                    {
                        PrintResultsToTextFile("CPU_Results.txt", m_CPUCalculationTimePerSecond);
                    }

                    PrintResultsToTextFile("Render_Results.txt", m_RenderCalculationTimePerSecond);
                    PrintResultsToTextFile("Frame_Results.txt", m_FullCalculationTimePerSecond);
                }

                m_EnableCapturingResults = false;
            }
            ImGui::Separator();

            if (ImGui::Button("Start Simulation"))
            {
                m_ImguiResetSimulation = true;
            }

            // ImGui Calculate input text and selected thread group calculation type
            
            // Calculate next boid count based on input text
            int ElementCount = sizeof(m_BoidNumberBuffer) / sizeof(char);
            int NewBoidAmount = CalculateNumberFromCharArray(ElementCount, m_BoidNumberBuffer);
            if (m_BoidCount != NewBoidAmount)
            {
                m_BoidCount = NewBoidAmount;
            }

            ElementCount = sizeof(m_NumOfCaptures) / sizeof(char);
            int NewCaptureCount = CalculateNumberFromCharArray(ElementCount, m_NumOfCaptures);
            if (m_AmountOfCaptures != NewCaptureCount)
            {
                m_AmountOfCaptures = NewCaptureCount;
            }

            // Determine whether the group count should be input automatically or manually
            switch (m_SelectedThreadGroupInputType)
            {
            case 0:
                m_AutomaticThreadGroupAmount = false;
                break;
            case 1:
                m_AutomaticThreadGroupAmount = true;
                break;
            }

            // Calculate thread group count based on input text
            if (!m_AutomaticThreadGroupAmount)
            {
                ElementCount = sizeof(m_NumOfThreadGroupsBuffer) / sizeof(char);
                int NewThreadGroupAmount = CalculateNumberFromCharArray(ElementCount, m_NumOfThreadGroupsBuffer);

                if (m_NextThreadBlockCount != NewThreadGroupAmount)
                {
                    m_NextThreadBlockCount = NewThreadGroupAmount;
                }
            }

            ImGui::End();
        }

        if (m_ImguiResetSimulation)
        {
            if (m_SelectedCaptureType == 1)
            {
                m_EnableCapturingResults = true;
            }
            else if (m_SelectedCaptureType == 0)
            {
                m_EnableCapturingResults = false;
            }

            // Reset buffers, as issues arise if dragging over text and inputting new number
            int ElementCount = sizeof(m_BoidNumberBuffer) / sizeof(char);
            for (int i = 0; i < ElementCount; i++)
            {
                m_BoidNumberBuffer[i] = 0;
            }

            ElementCount = sizeof(m_NumOfThreadGroupsBuffer) / sizeof(char);
            for (int i = 0; i < ElementCount; i++)
            {
                m_NumOfThreadGroupsBuffer[i] = 0;
            }

            // If no thread groups allocated, will throw so ensure at least 1 is allocated regardless of inputted number
            if (!m_AutomaticThreadGroupAmount)
            {
                if (m_NextThreadBlockCount > 0)
                {
                    m_NumOfThreadGroups = m_NextThreadBlockCount;
                }
                else
                {
                    m_NumOfThreadGroups = 1;
                }
            }

            // Enable appropriate version based on radio buttons
            if (m_SelectedBoidModel == 0)
            {
                m_EnableCPUVersion = true;
                m_EnableGPUVersion = false;
                m_EnableAsyncCompute = false;
            }
            else if (m_SelectedBoidModel == 1)
            {
                m_EnableCPUVersion = false;
                m_EnableGPUVersion = true;
                m_EnableAsyncCompute = false;
            }
            else if (m_SelectedBoidModel == 2)
            {
                m_EnableCPUVersion = false;
                m_EnableGPUVersion = false;
                m_EnableAsyncCompute = true;
            }

            // Determine thread count per group and apply appropriate compute shader based on this
            int ThreadCount = 0;
            if (m_EnableGPUVersion)
            {
                switch (m_SelectedThreadGroupSize)
                {
                case 0:
                    m_CurrentComputePipelineState = m_ComputePipelineState128;
                    ThreadCount = 128;
                    break;
                case 1:
                    m_CurrentComputePipelineState = m_ComputePipelineState256;
                    ThreadCount = 256;
                    break;
                case 2:
                    m_CurrentComputePipelineState = m_ComputePipelineState512;
                    ThreadCount = 512;
                    break;
                case 3:
                    m_CurrentComputePipelineState = m_ComputePipelineState1024;
                    ThreadCount = 1024;
                    break;
                }
            }
            else if (m_EnableAsyncCompute)
            {
                switch (m_SelectedThreadGroupSize)
                {
                case 0:
                    m_CurrentComputePipelineState = m_AsyncPipelineState128;
                    ThreadCount = 128;
                    break;
                case 1:
                    m_CurrentComputePipelineState = m_AsyncPipelineState256;
                    ThreadCount = 256;
                    break;
                case 2:
                    m_CurrentComputePipelineState = m_AsyncPipelineState512;
                    ThreadCount = 512;
                    break;
                case 3:
                    m_CurrentComputePipelineState = m_AsyncPipelineState1024;
                    ThreadCount = 1024;
                    break;
                }
            }

            // Automatically calculate amount of thread groups needed based on amount of threads per group
            if (m_AutomaticThreadGroupAmount && (m_EnableGPUVersion || m_EnableAsyncCompute))
            {
                m_NumOfThreadGroups = CalculateThreadBlockAmountNeeded(ThreadCount);
            }

            // Reset simulation, allowing settings to apply appropriately
            m_ImguiResetSimulation = false;
            m_ShouldResetSimulation = true;
        }
    }
}

int Tutorial3::CalculateNumberFromCharArray(int ElementCount, char Buffer[])
{
    // Iterate through char array backwards, increasing multiplier every iteration
    int Multiplier = 1;
    int NewBoidAmount = 0;
    for (int i = ElementCount - 1; i > -1; i--)
    {
        int CharacterValue = Buffer[i];
        if (CharacterValue < '0' || CharacterValue > '9')
        {
            continue;
        }

        CharacterValue -= '0';
        CharacterValue *= Multiplier;

        NewBoidAmount += CharacterValue;

        // Modify multiplier for next column
        Multiplier *= 10;
    }

    return NewBoidAmount;
}

int Tutorial3::CalculateThreadBlockAmountNeeded(int SelectedThreadAmountPerBlock)
{
    // Calculate amount of thread groups needed, rounding up
    double Division = static_cast<double>(m_BoidCount) / static_cast<double>(SelectedThreadAmountPerBlock);
    return static_cast<int>(std::ceil(Division));
}
