#pragma once

#include <Camera.h>
#include <Game.h>
#include <IndexBuffer.h>
#include <Light.h>
#include <Window.h>
#include <Mesh.h>
#include <RenderTarget.h>
#include <RootSignature.h>
#include <Texture.h>
#include <VertexBuffer.h>

#include <DirectXMath.h>
#include "UnorderedAccessViewBuffer.h"
#include <Buffer.h>
#include "BoidPhysicsSystem.h"
#include <queue>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <ResourceStateTracker.h>
#include <d3d12.h>
#include <CommandQueue.h>
#include <CommandList.h>
#include <chrono>

class BoidRenderSystem;
class BoidObject;

struct ID3D12QueryHeap;

struct CameraViewProjectionMatrices
{
    DirectX::XMMATRIX CameraView;
    DirectX::XMMATRIX CameraProjection;
};

class Tutorial3 : public Game
{
public:
    using super = Game;

    Tutorial3(const std::wstring& name, int width, int height, bool vSync = false);
    virtual ~Tutorial3();

    /**
     *  Load content required for the demo.
     */
    virtual bool LoadContent() override;

    /**
     *  Unload demo specific content that was loaded in LoadContent.
     */
    virtual void UnloadContent() override;
protected:

    /**
     * Invoked by the registered window when a key is pressed
     * while the window has focus.
     */
    virtual void OnKeyPressed(KeyEventArgs& e) override;

    /**
     * Invoked when a key on the keyboard is released.
     */
    virtual void OnKeyReleased(KeyEventArgs& e);

    /**
     * Invoked when the mouse is moved over the registered window.
     */
    virtual void OnMouseMoved(MouseMotionEventArgs& e);

    /**
     * Invoked when the mouse wheel is scrolled while the registered window has focus.
     */
    virtual void OnMouseWheel(MouseWheelEventArgs& e) override;

    virtual void OnResize(ResizeEventArgs& e) override; 

    // Main methods
    virtual void OnUpdate(UpdateEventArgs& e) override;
    virtual void OnRender(RenderEventArgs& e) override;

    // Boids Methods
    void BeginSimulation();
    void SwapBoidsDoubleBuffers();

    // ImGui Methods
    void OnBoidsMenuGui();

    int CalculateThreadBlockAmountNeeded(int SelectedThreadAmountPerBlock);
    int CalculateNumberFromCharArray(int ElementCount, char Buffer[]);

    // Results Gathering Methods
    void UpdateResults(std::vector<double>& TimePerFrameVector, std::queue<double>& TimePerSecondQueue, double& CurrentTime);
    void PrintResultsToTextFile(const char filename[], std::queue<double>& ValueQueue);

    void CalculateGPUQueryTime(CommandQueue& commandQueue, std::vector<double>& TimePerFrameVector, Microsoft::WRL::ComPtr<ID3D12Resource> ReadbackBuffer);

    double CalculateAverageTimePerSecond(std::vector<double> TimePerFrameVector);

private:
    // Boids Systems and Variables
    RootSignature m_BoidsRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_BoidsPipelineState;

    std::vector<BoidObject*> m_BoidObjects;
    BoidRenderSystem* m_BoidRenderSystem;
    BoidPhysicsSystem* m_BoidPhysicsSystem;

    CameraViewProjectionMatrices CamViewProj;

    // Boids Compute Shader
    RootSignature m_ComputeRootSignature;
    RootSignature m_AsyncComputeRootSignature;

    // Pipeline State based on one of three thread group sizes
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_ComputePipelineState128;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_ComputePipelineState256;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_ComputePipelineState512;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_ComputePipelineState1024;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_AsyncPipelineState128;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_AsyncPipelineState256;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_AsyncPipelineState512;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_AsyncPipelineState1024;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_CurrentComputePipelineState;

    // Unordered Access Views
    UnorderedAccessViewBuffer m_BoidMatricesUAVBuffer;
    UnorderedAccessViewBuffer* m_BoidMatricesDoubleBuffer[2];

    // Results Gathering
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_QueryHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_ReadbackBuffer;

    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_RenderQueryHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_RenderReadbackBuffer;

    std::vector<double> m_CPUCalculationTimePerFrame;
    std::vector<double> m_GPUCalculationTimePerFrame;
    std::vector<double> m_RenderCalculationTimePerFrame;
    std::vector<double> m_FullCalculationTimePerFrame;

    std::queue<double> m_CPUCalculationTimePerSecond;
    std::queue<double> m_GPUCalculationTimePerSecond;
    std::queue<double> m_RenderCalculationTimePerSecond;
    std::queue<double> m_FullCalculationTimePerSecond;

    double m_CurrentCPUTime = 0, m_CurrentGPUTime = 0, m_CurrentOverallTime = 0, m_CurrentRenderTime = 0;

    int m_AmountOfCaptures = 100;

    double m_TotalUpdateFunctionTime = 0;

    // FPS Settings
    bool m_EnableFPS = true;
    bool m_EnableCapturingResults = false;
    float m_FPS = 0;
    std::queue<double> m_PreviousFPSValues;

    // Boids ImGui
    bool m_RunningSimulation = true;
    bool m_ShouldResetSimulation = false;
    bool m_ImguiResetSimulation = false;
    ModelProperties m_NewModelProperties;

    // ImGui Specific
    int m_NextThreadBlockCount = 0;
    int m_BoidCount = 0; // (Use BoidObjects.size() instead)

    // Versions ImGui
    bool m_EnableCPUVersion = false;
    bool m_EnableGPUVersion = false;
    bool m_EnableAsyncCompute = false;

    // Thread Settings ImGui
    bool m_AutomaticThreadGroupAmount = true;
    int m_NumOfThreadGroups = 0;

    // Radio Button ImGui Variables
    int m_SelectedThreadGroupInputType = 1;
    int m_SelectedBoidModel = 0;
    int m_SelectedThreadGroupSize = 0;
    int m_SelectedCaptureType = 0;

    // Input Text Buffers
    char m_NumOfThreadGroupsBuffer[5] = "0";
    char m_BoidNumberBuffer[7] = "0";
    char m_NumOfCaptures[10] = "100";

    // Render target
    RenderTarget m_RenderTarget;

    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT m_ScissorRect;

    // Camera variables
    Camera m_Camera;
    struct alignas( 16 ) CameraData
    {
        DirectX::XMVECTOR m_InitialCamPos;
        DirectX::XMVECTOR m_InitialCamRot;
    };
    CameraData* m_pAlignedCameraData;

    // Camera controller
    float m_Forward;
    float m_Backward;
    float m_Left;
    float m_Right;
    float m_Up;
    float m_Down;

    float m_Pitch;
    float m_Yaw;

    // Set to true if the Shift key is pressed.
    bool m_Shift;

    int m_Width;
    int m_Height;
};