/** @file Week7-2-TreeBillboardsApp.cpp
 * 
 *   Controls:
 *   Hold the left mouse button down and move the mouse to rotate.
 *   Hold the right mouse button down and move the mouse to zoom in and out.
 *
 *  @authors Devin & Migel 
 */

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"
#include "Waves.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	AlphaTestedTreeSprites,
	Count
};

class TreeBillboardsApp : public D3DApp
{
public:
    TreeBillboardsApp(HINSTANCE hInstance);
    TreeBillboardsApp(const TreeBillboardsApp& rhs) = delete;
    TreeBillboardsApp& operator=(const TreeBillboardsApp& rhs) = delete;
    ~TreeBillboardsApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt); 

	void LoadTextures();
    void BuildRootSignature();
	void BuildDescriptorHeaps();
    void BuildShadersAndInputLayouts();
    void BuildLandGeometry();
    void BuildWavesGeometry();
	void BuildBoxGeometry();
	void BuildConeGeometry();
	void BuildCylinderGeometry();
	void BuildTreeSpritesGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

    float GetHillsHeight(float x, float z)const;
    XMFLOAT3 GetHillsNormal(float x, float z)const;

private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mStdInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTreeSpriteInputLayout;

    RenderItem* mWavesRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	std::unique_ptr<Waves> mWaves;

    PassConstants mMainPassCB;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f*XM_PI;
    float mPhi = XM_PIDIV2 - 0.1f;
    float mRadius = 50.0f;

    POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        TreeBillboardsApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

TreeBillboardsApp::TreeBillboardsApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

TreeBillboardsApp::~TreeBillboardsApp()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool TreeBillboardsApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);
 
	LoadTextures();
    BuildRootSignature();
	BuildDescriptorHeaps();
    BuildShadersAndInputLayouts();
    BuildLandGeometry();
    BuildWavesGeometry();
	BuildBoxGeometry();
	BuildConeGeometry();
	BuildCylinderGeometry();
	BuildTreeSpritesGeometry();
	BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}
 
void TreeBillboardsApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void TreeBillboardsApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
	UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if(mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
    UpdateWaves(gt);
}

void TreeBillboardsApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mMainPassCB.FogColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);

	mCommandList->SetPipelineState(mPSOs["treeSprites"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites]);

	mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;

    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void TreeBillboardsApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void TreeBillboardsApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void TreeBillboardsApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.2 unit in the scene.
        float dx = 0.2f*static_cast<float>(x - mLastMousePos.x);
        float dy = 0.2f*static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}
 
void TreeBillboardsApp::OnKeyboardInput(const GameTimer& gt)
{
}
 
void TreeBillboardsApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius*sinf(mPhi)*cosf(mTheta);
	mEyePos.z = mRadius*sinf(mPhi)*sinf(mTheta);
	mEyePos.y = mRadius*cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void TreeBillboardsApp::AnimateMaterials(const GameTimer& gt)
{
	// Scroll the water material texture coordinates.
	auto waterMat = mMaterials["water"].get();

	float& tu = waterMat->MatTransform(3, 0);
	float& tv = waterMat->MatTransform(3, 1);

	tu += 0.1f * gt.DeltaTime();
	tv += 0.02f * gt.DeltaTime();

	if(tu >= 1.0f)
		tu -= 1.0f;

	if(tv >= 1.0f)
		tv -= 1.0f;

	waterMat->MatTransform(3, 0) = tu;
	waterMat->MatTransform(3, 1) = tv;

	// Material has changed, so need to update cbuffer.
	waterMat->NumFramesDirty = gNumFrameResources;
}

void TreeBillboardsApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for(auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void TreeBillboardsApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for(auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if(mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void TreeBillboardsApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	mMainPassCB.Lights[3].Position = { 0.0f, 8.0f, 2.0f };
	mMainPassCB.Lights[3].Strength = { 1.0f, 0.0f, 0.0f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void TreeBillboardsApp::UpdateWaves(const GameTimer& gt)
{
	// Every quarter second, generate a random wave.
	static float t_base = 0.0f;
	if((mTimer.TotalTime() - t_base) >= 0.25f)
	{
		t_base += 0.25f;

		int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
		int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);

		float r = MathHelper::RandF(0.2f, 0.5f);

		mWaves->Disturb(i, j, r);
	}

	// Update the wave simulation.
	mWaves->Update(gt.DeltaTime());

	// Update the wave vertex buffer with the new solution.
	auto currWavesVB = mCurrFrameResource->WavesVB.get();
	for(int i = 0; i < mWaves->VertexCount(); ++i)
	{
		Vertex v;

		v.Pos = mWaves->Position(i);
		v.Normal = mWaves->Normal(i);
		
		// Derive tex-coords from position by 
		// mapping [-w/2,w/2] --> [0,1]
		v.TexC.x = 0.5f + v.Pos.x / mWaves->Width();
		v.TexC.y = 0.5f - v.Pos.z / mWaves->Depth();

		currWavesVB->CopyData(i, v);
	}

	// Set the dynamic VB of the wave renderitem to the current frame VB.
	mWavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
}

void TreeBillboardsApp::LoadTextures()
{
	auto grassTex = std::make_unique<Texture>();
	grassTex->Name = "grassTex";
	grassTex->Filename = L"../../Textures/grass.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), grassTex->Filename.c_str(),
		grassTex->Resource, grassTex->UploadHeap));

	auto waterTex = std::make_unique<Texture>();
	waterTex->Name = "waterTex";
	waterTex->Filename = L"../../Textures/water1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), waterTex->Filename.c_str(),
		waterTex->Resource, waterTex->UploadHeap));

	auto fenceTex = std::make_unique<Texture>();
	fenceTex->Name = "fenceTex";
	fenceTex->Filename = L"../../Textures/WireFence.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), fenceTex->Filename.c_str(),
		fenceTex->Resource, fenceTex->UploadHeap));

	auto brickTex = std::make_unique<Texture>();
	brickTex->Name = "brickTex";
	brickTex->Filename = L"../../Textures/bricks2.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), brickTex->Filename.c_str(),
		brickTex->Resource, brickTex->UploadHeap));

	auto tileTex = std::make_unique<Texture>();
	tileTex->Name = "tileTex";
	tileTex->Filename = L"../../Textures/bricks3.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), tileTex->Filename.c_str(),
		tileTex->Resource, tileTex->UploadHeap));

	auto woodTex = std::make_unique<Texture>();
	woodTex->Name = "woodTex";
	woodTex->Filename = L"../../Textures/WoodCrate02.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), woodTex->Filename.c_str(),
		woodTex->Resource, woodTex->UploadHeap));

	auto treeArrayTex = std::make_unique<Texture>();
	treeArrayTex->Name = "treeArrayTex";
	treeArrayTex->Filename = L"../../Textures/treeArray.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), treeArrayTex->Filename.c_str(),
		treeArrayTex->Resource, treeArrayTex->UploadHeap));

	mTextures[grassTex->Name] = std::move(grassTex);
	mTextures[waterTex->Name] = std::move(waterTex);
	mTextures[fenceTex->Name] = std::move(fenceTex);
	mTextures[brickTex->Name] = std::move(brickTex);
	mTextures[tileTex->Name] = std::move(tileTex);
	mTextures[woodTex->Name] = std::move(woodTex);
	mTextures[treeArrayTex->Name] = std::move(treeArrayTex);
}

void TreeBillboardsApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if(errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void TreeBillboardsApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 7;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto grassTex = mTextures["grassTex"]->Resource;
	auto waterTex = mTextures["waterTex"]->Resource;
	auto fenceTex = mTextures["fenceTex"]->Resource;
	auto brickTex = mTextures["brickTex"]->Resource;
	auto tileTex = mTextures["tileTex"]->Resource;
	auto woodTex = mTextures["woodTex"]->Resource;
	auto treeArrayTex = mTextures["treeArrayTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = grassTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = waterTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = fenceTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(fenceTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = brickTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(brickTex.Get(), &srvDesc, hDescriptor);

	//next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = tileTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(tileTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = woodTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(woodTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	auto desc = treeArrayTex->GetDesc();
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Format = treeArrayTex->GetDesc().Format;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = -1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = treeArrayTex->GetDesc().DepthOrArraySize;
	md3dDevice->CreateShaderResourceView(treeArrayTex.Get(), &srvDesc, hDescriptor);

}

void TreeBillboardsApp::BuildShadersAndInputLayouts()
{
	const D3D_SHADER_MACRO defines[] =
	{
		"FOG", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_5_1");
	
	mShaders["treeSpriteVS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["treeSpriteGS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "GS", "gs_5_1");
	mShaders["treeSpritePS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_1");

    mStdInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

	mTreeSpriteInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void TreeBillboardsApp::BuildLandGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(70.0f, 50.0f, 50, 50);

    //
    // Extract the vertex elements we are interested and apply the height function to
    // each vertex.  In addition, color the vertices based on their height so we have
    // sandy looking beaches, grassy low hills, and snow mountain peaks.
    //

    std::vector<Vertex> vertices(grid.Vertices.size());
    for(size_t i = 0; i < grid.Vertices.size(); ++i)
    {
        auto& p = grid.Vertices[i].Position;
        vertices[i].Pos = p;
		vertices[i].Pos.y = 3;//GetHillsHeight(p.x, p.z);
		vertices[i].Normal = GetHillsNormal(p.x, p.z);
		vertices[i].TexC = grid.Vertices[i].TexC;
    }

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

    std::vector<std::uint16_t> indices = grid.GetIndices16();
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "landGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["landGeo"] = std::move(geo);
}

void TreeBillboardsApp::BuildWavesGeometry()
{
    std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face
	assert(mWaves->VertexCount() < 0x0000ffff);

    // Iterate over each quad.
    int m = mWaves->RowCount();
    int n = mWaves->ColumnCount();
    int k = 0;
    for(int i = 0; i < m - 1; ++i)
    {
        for(int j = 0; j < n - 1; ++j)
        {
            indices[k] = i*n + j;
            indices[k + 1] = i*n + j + 1;
            indices[k + 2] = (i + 1)*n + j;

            indices[k + 3] = (i + 1)*n + j;
            indices[k + 4] = i*n + j + 1;
            indices[k + 5] = (i + 1)*n + j + 1;

            k += 6; // next quad
        }
    }

	UINT vbByteSize = mWaves->VertexCount()*sizeof(Vertex);
	UINT ibByteSize = (UINT)indices.size()*sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

	// Set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["waterGeo"] = std::move(geo);
}

void TreeBillboardsApp::BuildBoxGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(10.0f, 10.0f, 10.0f, 3);

	std::vector<Vertex> vertices(box.Vertices.size());
	for (size_t i = 0; i < box.Vertices.size(); ++i)
	{
		auto& p = box.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = box.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["box"] = submesh;

	mGeometries["boxGeo"] = std::move(geo);
}

void TreeBillboardsApp::BuildConeGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData cone = geoGen.CreateCylinder(8.0f, 0.0f, 40.0f, 20, 20);

	std::vector<Vertex> vertices(cone.Vertices.size());
	for (size_t i = 0; i < cone.Vertices.size(); ++i)
	{
		auto& p = cone.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = cone.Vertices[i].Normal;
		vertices[i].TexC = cone.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = cone.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "coneGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["cone"] = submesh;

	mGeometries["coneGeo"] = std::move(geo);
}

void TreeBillboardsApp::BuildCylinderGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(8.0f, 8.0f, 40.0f, 20, 20);

	std::vector<Vertex> vertices(cylinder.Vertices.size());
	for (size_t i = 0; i < cylinder.Vertices.size(); ++i)
	{
		auto& p = cylinder.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = cylinder.Vertices[i].Normal;
		vertices[i].TexC = cylinder.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = cylinder.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "cylinderGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["cylinder"] = submesh;

	mGeometries["cylinderGeo"] = std::move(geo);
}

void TreeBillboardsApp::BuildTreeSpritesGeometry()
{
	//step5
	struct TreeSpriteVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT2 Size;
	};

	static const int treeCount = 5;
	std::array<TreeSpriteVertex, 16> vertices;
	for(UINT i = 0; i < treeCount; ++i)
	{
		float x = MathHelper::RandF(-25.0f, 25.0f);
		float z = MathHelper::RandF(-25.0f, 25.0f);
		float y = 4;

		// Move tree slightly above land height.
		y += 8.0f;

		vertices[i].Pos = XMFLOAT3(x, y, z);
		vertices[i].Size = XMFLOAT2(20.0f, 20.0f);
	}

	std::array<std::uint16_t, 16> indices =
	{
		0, 1, 2, 3, 4, 5, 6, 7,
		8, 9, 10, 11, 12, 13, 14, 15
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(TreeSpriteVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "treeSpritesGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(TreeSpriteVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["points"] = submesh;

	mGeometries["treeSpritesGeo"] = std::move(geo);
}

void TreeBillboardsApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mStdInputLayout.data(), (UINT)mStdInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()), 
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;

	//there is abug with F2 key that is supposed to turn on the multisampling!
//Set4xMsaaState(true);
	//m4xMsaaState = true;

	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	//
	// PSO for transparent objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//transparentPsoDesc.BlendState.AlphaToCoverageEnable = true;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

	//
	// PSO for alpha tested objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()),
		mShaders["alphaTestedPS"]->GetBufferSize()
	};
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));

	//
	// PSO for tree sprites
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC treeSpritePsoDesc = opaquePsoDesc;
	treeSpritePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteVS"]->GetBufferPointer()),
		mShaders["treeSpriteVS"]->GetBufferSize()
	};
	treeSpritePsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteGS"]->GetBufferPointer()),
		mShaders["treeSpriteGS"]->GetBufferSize()
	};
	treeSpritePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpritePS"]->GetBufferPointer()),
		mShaders["treeSpritePS"]->GetBufferSize()
	};
	//step1
	treeSpritePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	treeSpritePsoDesc.InputLayout = { mTreeSpriteInputLayout.data(), (UINT)mTreeSpriteInputLayout.size() };
	treeSpritePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&treeSpritePsoDesc, IID_PPV_ARGS(&mPSOs["treeSprites"])));
}

void TreeBillboardsApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size(), mWaves->VertexCount()));
    }
}

void TreeBillboardsApp::BuildMaterials()
{
	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseSrvHeapIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;

	// This is not a good water material definition, but we do not have all the rendering
	// tools we need (transparency, environment reflection), so we fake it for now.
	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseSrvHeapIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;

	auto wirefence = std::make_unique<Material>();
	wirefence->Name = "wirefence";
	wirefence->MatCBIndex = 2;
	wirefence->DiffuseSrvHeapIndex = 2;
	wirefence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wirefence->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	wirefence->Roughness = 0.25f;

	auto treeSprites = std::make_unique<Material>();
	treeSprites->Name = "treeSprites";
	treeSprites->MatCBIndex = 6;
	treeSprites->DiffuseSrvHeapIndex = 6;
	treeSprites->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprites->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeSprites->Roughness = 0.125f;

	auto bricks = std::make_unique<Material>();
	bricks->Name = "bricks";
	bricks->MatCBIndex = 3;
	bricks->DiffuseSrvHeapIndex = 3;
	bricks->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks->Roughness = 0.25f;

	auto tiles = std::make_unique<Material>();
	tiles->Name = "tiles";
	tiles->MatCBIndex = 4;
	tiles->DiffuseSrvHeapIndex = 4;
	tiles->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	tiles->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	tiles->Roughness = 0.25f;

	auto wood = std::make_unique<Material>();
	wood->Name = "wood";
	wood->MatCBIndex = 5;
	wood->DiffuseSrvHeapIndex = 5;
	wood->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wood->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	wood->Roughness = 0.125f;

	mMaterials["grass"] = std::move(grass);
	mMaterials["water"] = std::move(water);
	mMaterials["wirefence"] = std::move(wirefence);
	mMaterials["treeSprites"] = std::move(treeSprites);
	mMaterials["bricks"] = std::move(bricks);
	mMaterials["tiles"] = std::move(tiles);
	mMaterials["wood"] = std::move(wood);
}

void TreeBillboardsApp::BuildRenderItems()
{
	// WATER
    auto wavesRitem = std::make_unique<RenderItem>();
    wavesRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&wavesRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	wavesRitem->ObjCBIndex = 0;
	wavesRitem->Mat = mMaterials["water"].get();
	wavesRitem->Geo = mGeometries["waterGeo"].get();
	wavesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["grid"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
    mWavesRitem = wavesRitem.get();
	mRitemLayer[(int)RenderLayer::Transparent].push_back(wavesRitem.get());

	// GROUND
    auto gridRitem = std::make_unique<RenderItem>();
    gridRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	gridRitem->ObjCBIndex = 1;
	gridRitem->Mat = mMaterials["grass"].get();
	gridRitem->Geo = mGeometries["landGeo"].get();
	gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());

	// FENCED GATE
	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(1.0f, 0.6f, 0.0f) * XMMatrixTranslation(0.0f, 6.0f, -15.0f));
	boxRitem->ObjCBIndex = 2;
	boxRitem->Mat = mMaterials["wirefence"].get();
	boxRitem->Geo = mGeometries["boxGeo"].get();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(boxRitem.get());

	//////////////////////////////////////////////////////////////
	
	// CASTLE BOX
	auto boxRitem1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem1->World, XMMatrixScaling(1.0f, 1.8f, 1.0f) * XMMatrixTranslation(0.0f, 11.5f, 10.0f));
	boxRitem1->ObjCBIndex = 3;
	boxRitem1->Geo = mGeometries["boxGeo"].get();
	boxRitem1->Mat = mMaterials["bricks"].get();
	boxRitem1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem1->IndexCount = boxRitem1->Geo->DrawArgs["box"].IndexCount;  // 36
	boxRitem1->StartIndexLocation = boxRitem1->Geo->DrawArgs["box"].StartIndexLocation; // 0
	boxRitem1->BaseVertexLocation = boxRitem1->Geo->DrawArgs["box"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem1.get());
	mAllRitems.push_back(std::move(boxRitem1));

	// TOWER CYLINDERS
	auto cylinderRitem1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&cylinderRitem1->World, XMMatrixScaling(0.2f, 0.3f, 0.2f) * XMMatrixTranslation(32.0f, 9.0f, 22.0f));
	cylinderRitem1->ObjCBIndex = 4;
	cylinderRitem1->Geo = mGeometries["cylinderGeo"].get();
	cylinderRitem1->Mat = mMaterials["bricks"].get();
	cylinderRitem1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	cylinderRitem1->IndexCount = cylinderRitem1->Geo->DrawArgs["cylinder"].IndexCount;  // 36
	cylinderRitem1->StartIndexLocation = cylinderRitem1->Geo->DrawArgs["cylinder"].StartIndexLocation; // 0
	cylinderRitem1->BaseVertexLocation = cylinderRitem1->Geo->DrawArgs["cylinder"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(cylinderRitem1.get());
	mAllRitems.push_back(std::move(cylinderRitem1));

	auto cylinderRitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&cylinderRitem2->World, XMMatrixScaling(0.2f, 0.3f, 0.2f) * XMMatrixTranslation(-32.0f, 9.0f, -15.0f));
	cylinderRitem2->ObjCBIndex = 5;
	cylinderRitem2->Geo = mGeometries["cylinderGeo"].get();
	cylinderRitem2->Mat = mMaterials["bricks"].get();
	cylinderRitem2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	cylinderRitem2->IndexCount = cylinderRitem2->Geo->DrawArgs["cylinder"].IndexCount;  // 36
	cylinderRitem2->StartIndexLocation = cylinderRitem2->Geo->DrawArgs["cylinder"].StartIndexLocation; // 0
	cylinderRitem2->BaseVertexLocation = cylinderRitem2->Geo->DrawArgs["cylinder"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(cylinderRitem2.get());
	mAllRitems.push_back(std::move(cylinderRitem2));

	auto cylinderRitem3 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&cylinderRitem3->World, XMMatrixScaling(0.2f, 0.3f, 0.2f) * XMMatrixTranslation(32.0f, 9.0f, -15.0f));
	cylinderRitem3->ObjCBIndex = 6;
	cylinderRitem3->Geo = mGeometries["cylinderGeo"].get();
	cylinderRitem3->Mat = mMaterials["bricks"].get();
	cylinderRitem3->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	cylinderRitem3->IndexCount = cylinderRitem3->Geo->DrawArgs["cylinder"].IndexCount;  // 36
	cylinderRitem3->StartIndexLocation = cylinderRitem3->Geo->DrawArgs["cylinder"].StartIndexLocation; // 0
	cylinderRitem3->BaseVertexLocation = cylinderRitem3->Geo->DrawArgs["cylinder"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(cylinderRitem3.get());
	mAllRitems.push_back(std::move(cylinderRitem3));

	auto cylinderRitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&cylinderRitem4->World, XMMatrixScaling(0.2f, 0.3f, 0.2f) * XMMatrixTranslation(-32.0f, 9.0f, 22.0f));
	cylinderRitem4->ObjCBIndex = 7;
	cylinderRitem4->Geo = mGeometries["cylinderGeo"].get();
	cylinderRitem4->Mat = mMaterials["bricks"].get();
	cylinderRitem4->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	cylinderRitem4->IndexCount = cylinderRitem4->Geo->DrawArgs["cylinder"].IndexCount;  // 36
	cylinderRitem4->StartIndexLocation = cylinderRitem4->Geo->DrawArgs["cylinder"].StartIndexLocation; // 0
	cylinderRitem4->BaseVertexLocation = cylinderRitem4->Geo->DrawArgs["cylinder"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(cylinderRitem4.get());
	mAllRitems.push_back(std::move(cylinderRitem4));

	// ROOFTOP CONES
	auto coneRitem1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem1->World, XMMatrixScaling(0.3f, 0.1f, 0.3f)* XMMatrixTranslation(-32.0f, 16.0f, 22.0f));
	coneRitem1->ObjCBIndex = 8;
	coneRitem1->Geo = mGeometries["coneGeo"].get();
	coneRitem1->Mat = mMaterials["tiles"].get();
	coneRitem1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem1->IndexCount = coneRitem1->Geo->DrawArgs["cone"].IndexCount;  // 36
	coneRitem1->StartIndexLocation = coneRitem1->Geo->DrawArgs["cone"].StartIndexLocation; // 0
	coneRitem1->BaseVertexLocation = coneRitem1->Geo->DrawArgs["cone"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(coneRitem1.get());
	mAllRitems.push_back(std::move(coneRitem1));

	auto coneRitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem2->World, XMMatrixScaling(0.3f, 0.1f, 0.3f)* XMMatrixTranslation(-32.0f, 16.0f, -15.0f));
	coneRitem2->ObjCBIndex = 9;
	coneRitem2->Geo = mGeometries["coneGeo"].get();
	coneRitem2->Mat = mMaterials["tiles"].get();
	coneRitem2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem2->IndexCount = coneRitem2->Geo->DrawArgs["cone"].IndexCount;  // 36
	coneRitem2->StartIndexLocation = coneRitem2->Geo->DrawArgs["cone"].StartIndexLocation; // 0
	coneRitem2->BaseVertexLocation = coneRitem2->Geo->DrawArgs["cone"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(coneRitem2.get());
	mAllRitems.push_back(std::move(coneRitem2));

	auto coneRitem3 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem3->World, XMMatrixScaling(0.3f, 0.1f, 0.3f)* XMMatrixTranslation(32.0f, 16.0f, 22.0f));
	coneRitem3->ObjCBIndex = 10;
	coneRitem3->Geo = mGeometries["coneGeo"].get();
	coneRitem3->Mat = mMaterials["tiles"].get();
	coneRitem3->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem3->IndexCount = coneRitem3->Geo->DrawArgs["cone"].IndexCount;  // 36
	coneRitem3->StartIndexLocation = coneRitem3->Geo->DrawArgs["cone"].StartIndexLocation; // 0
	coneRitem3->BaseVertexLocation = coneRitem3->Geo->DrawArgs["cone"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(coneRitem3.get());
	mAllRitems.push_back(std::move(coneRitem3));

	auto coneRitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem4->World, XMMatrixScaling(0.3f, 0.1f, 0.3f)* XMMatrixTranslation(32.0f, 16.0f, -15.0f));
	coneRitem4->ObjCBIndex = 11;
	coneRitem4->Geo = mGeometries["coneGeo"].get();
	coneRitem4->Mat = mMaterials["tiles"].get();
	coneRitem4->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem4->IndexCount = coneRitem4->Geo->DrawArgs["cone"].IndexCount;  // 36
	coneRitem4->StartIndexLocation = coneRitem4->Geo->DrawArgs["cone"].StartIndexLocation; // 0
	coneRitem4->BaseVertexLocation = coneRitem4->Geo->DrawArgs["cone"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(coneRitem4.get());
	mAllRitems.push_back(std::move(coneRitem4));

	auto coneRitem5 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem5->World, XMMatrixScaling(0.3f, 0.1f, 0.3f)* XMMatrixTranslation(5.5f, 16.0f, -15.0f));
	coneRitem5->ObjCBIndex = 12;
	coneRitem5->Geo = mGeometries["coneGeo"].get();
	coneRitem5->Mat = mMaterials["tiles"].get();
	coneRitem5->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem5->IndexCount = coneRitem5->Geo->DrawArgs["cone"].IndexCount;  // 36
	coneRitem5->StartIndexLocation = coneRitem5->Geo->DrawArgs["cone"].StartIndexLocation; // 0
	coneRitem5->BaseVertexLocation = coneRitem5->Geo->DrawArgs["cone"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(coneRitem5.get());
	mAllRitems.push_back(std::move(coneRitem5));

	auto coneRitem6 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem6->World, XMMatrixScaling(0.3f, 0.1f, 0.3f)* XMMatrixTranslation(-5.5f, 16.0f, -15.0f));
	coneRitem6->ObjCBIndex = 13;
	coneRitem6->Geo = mGeometries["coneGeo"].get();
	coneRitem6->Mat = mMaterials["tiles"].get();
	coneRitem6->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem6->IndexCount = coneRitem6->Geo->DrawArgs["cone"].IndexCount;  // 36
	coneRitem6->StartIndexLocation = coneRitem6->Geo->DrawArgs["cone"].StartIndexLocation; // 0
	coneRitem6->BaseVertexLocation = coneRitem6->Geo->DrawArgs["cone"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(coneRitem6.get());
	mAllRitems.push_back(std::move(coneRitem6));

	// WALL BOXES
	auto boxRitem3 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem3->World, XMMatrixScaling(2.8f, 0.8f, 0.1f)* XMMatrixTranslation(-18.0f, 6.5f, -15.0f));
	boxRitem3->ObjCBIndex = 14;
	boxRitem3->Geo = mGeometries["boxGeo"].get();
	boxRitem3->Mat = mMaterials["bricks"].get();
	boxRitem3->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem3->IndexCount = boxRitem3->Geo->DrawArgs["box"].IndexCount;  // 36
	boxRitem3->StartIndexLocation = boxRitem3->Geo->DrawArgs["box"].StartIndexLocation; // 0
	boxRitem3->BaseVertexLocation = boxRitem3->Geo->DrawArgs["box"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem3.get());
	mAllRitems.push_back(std::move(boxRitem3));

	auto boxRitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem4->World, XMMatrixScaling(2.8f, 0.8f, 0.1f)* XMMatrixTranslation(18.0f, 6.5f, -15.0f));
	boxRitem4->ObjCBIndex = 15;
	boxRitem4->Geo = mGeometries["boxGeo"].get();
	boxRitem4->Mat = mMaterials["bricks"].get();
	boxRitem4->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem4->IndexCount = boxRitem4->Geo->DrawArgs["box"].IndexCount;  // 36
	boxRitem4->StartIndexLocation = boxRitem4->Geo->DrawArgs["box"].StartIndexLocation; // 0
	boxRitem4->BaseVertexLocation = boxRitem4->Geo->DrawArgs["box"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem4.get());
	mAllRitems.push_back(std::move(boxRitem4));

	auto boxRitem5 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem5->World, XMMatrixScaling(6.2f, 0.8f, 0.1f)* XMMatrixTranslation(0.0f, 6.5f, 22.0f));
	boxRitem5->ObjCBIndex = 16;
	boxRitem5->Geo = mGeometries["boxGeo"].get();
	boxRitem5->Mat = mMaterials["bricks"].get();
	boxRitem5->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem5->IndexCount = boxRitem5->Geo->DrawArgs["box"].IndexCount;  // 36
	boxRitem5->StartIndexLocation = boxRitem5->Geo->DrawArgs["box"].StartIndexLocation; // 0
	boxRitem5->BaseVertexLocation = boxRitem5->Geo->DrawArgs["box"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem5.get());
	mAllRitems.push_back(std::move(boxRitem5));

	auto boxRitem6 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem6->World, XMMatrixScaling(0.1f, 0.8f, 3.6f)* XMMatrixTranslation(32.0f, 6.5f, 3.0f));
	boxRitem6->ObjCBIndex = 17;
	boxRitem6->Geo = mGeometries["boxGeo"].get();
	boxRitem6->Mat = mMaterials["bricks"].get();
	boxRitem6->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem6->IndexCount = boxRitem6->Geo->DrawArgs["box"].IndexCount;  // 36
	boxRitem6->StartIndexLocation = boxRitem6->Geo->DrawArgs["box"].StartIndexLocation; // 0
	boxRitem6->BaseVertexLocation = boxRitem6->Geo->DrawArgs["box"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem6.get());
	mAllRitems.push_back(std::move(boxRitem6));

	auto boxRitem7 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem7->World, XMMatrixScaling(0.1f, 0.8f, 3.6f)* XMMatrixTranslation(-32.0f, 6.5f, 3.0f));
	boxRitem7->ObjCBIndex = 18;
	boxRitem7->Geo = mGeometries["boxGeo"].get();
	boxRitem7->Mat = mMaterials["bricks"].get();
	boxRitem7->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem7->IndexCount = boxRitem7->Geo->DrawArgs["box"].IndexCount;  // 36
	boxRitem7->StartIndexLocation = boxRitem7->Geo->DrawArgs["box"].StartIndexLocation; // 0
	boxRitem7->BaseVertexLocation = boxRitem7->Geo->DrawArgs["box"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem7.get());
	mAllRitems.push_back(std::move(boxRitem7));

	// GATE TOWER CYLINDERS
	auto cylinderRitem5 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&cylinderRitem5->World, XMMatrixScaling(0.2f, 0.3f, 0.2f)* XMMatrixTranslation(5.5f, 9.0f, -15.0f));
	cylinderRitem5->ObjCBIndex = 19;
	cylinderRitem5->Geo = mGeometries["cylinderGeo"].get();
	cylinderRitem5->Mat = mMaterials["bricks"].get();
	cylinderRitem5->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	cylinderRitem5->IndexCount = cylinderRitem5->Geo->DrawArgs["cylinder"].IndexCount;  // 36
	cylinderRitem5->StartIndexLocation = cylinderRitem5->Geo->DrawArgs["cylinder"].StartIndexLocation; // 0
	cylinderRitem5->BaseVertexLocation = cylinderRitem5->Geo->DrawArgs["cylinder"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(cylinderRitem5.get());
	mAllRitems.push_back(std::move(cylinderRitem5));

	auto cylinderRitem6 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&cylinderRitem6->World, XMMatrixScaling(0.2f, 0.3f, 0.2f)* XMMatrixTranslation(-5.5f, 9.0f, -15.0f));
	cylinderRitem6->ObjCBIndex = 20;
	cylinderRitem6->Geo = mGeometries["cylinderGeo"].get();
	cylinderRitem6->Mat = mMaterials["bricks"].get();
	cylinderRitem6->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	cylinderRitem6->IndexCount = cylinderRitem6->Geo->DrawArgs["cylinder"].IndexCount;  // 36
	cylinderRitem6->StartIndexLocation = cylinderRitem6->Geo->DrawArgs["cylinder"].StartIndexLocation; // 0
	cylinderRitem6->BaseVertexLocation = cylinderRitem6->Geo->DrawArgs["cylinder"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(cylinderRitem6.get());
	mAllRitems.push_back(std::move(cylinderRitem6));

	// WOODEN BOXES
	auto boxRitem8 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem8->World, XMMatrixScaling(0.3f, 0.3f, 0.3f)* XMMatrixTranslation(15.0f, 4.5f, 0.0f));
	boxRitem8->ObjCBIndex = 21;
	boxRitem8->Geo = mGeometries["boxGeo"].get();
	boxRitem8->Mat = mMaterials["wood"].get();
	boxRitem8->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem8->IndexCount = boxRitem8->Geo->DrawArgs["box"].IndexCount;  // 36
	boxRitem8->StartIndexLocation = boxRitem8->Geo->DrawArgs["box"].StartIndexLocation; // 0
	boxRitem8->BaseVertexLocation = boxRitem8->Geo->DrawArgs["box"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem8.get());
	mAllRitems.push_back(std::move(boxRitem8));

	auto boxRitem9 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem9->World, XMMatrixScaling(0.3f, 0.3f, 0.3f)* XMMatrixTranslation(-15.0f, 4.5f, 0.0f));
	boxRitem9->ObjCBIndex = 22;
	boxRitem9->Geo = mGeometries["boxGeo"].get();
	boxRitem9->Mat = mMaterials["wood"].get();
	boxRitem9->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem9->IndexCount = boxRitem9->Geo->DrawArgs["box"].IndexCount;  // 36
	boxRitem9->StartIndexLocation = boxRitem9->Geo->DrawArgs["box"].StartIndexLocation; // 0
	boxRitem9->BaseVertexLocation = boxRitem9->Geo->DrawArgs["box"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem9.get());
	mAllRitems.push_back(std::move(boxRitem9));

	// CASTLE TOWER CYLINDERS
	auto cylinderRitem7 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&cylinderRitem7->World, XMMatrixScaling(0.2f, 0.5f, 0.2f)* XMMatrixTranslation(-5.0f, 12.0f, 5.0f));
	cylinderRitem7->ObjCBIndex = 23;
	cylinderRitem7->Geo = mGeometries["cylinderGeo"].get();
	cylinderRitem7->Mat = mMaterials["bricks"].get();
	cylinderRitem7->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	cylinderRitem7->IndexCount = cylinderRitem7->Geo->DrawArgs["cylinder"].IndexCount;  // 36
	cylinderRitem7->StartIndexLocation = cylinderRitem7->Geo->DrawArgs["cylinder"].StartIndexLocation; // 0
	cylinderRitem7->BaseVertexLocation = cylinderRitem7->Geo->DrawArgs["cylinder"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(cylinderRitem7.get());
	mAllRitems.push_back(std::move(cylinderRitem7));

	auto cylinderRitem8 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&cylinderRitem8->World, XMMatrixScaling(0.2f, 0.5f, 0.2f)* XMMatrixTranslation(5.0f, 12.0f, 5.0f));
	cylinderRitem8->ObjCBIndex = 24;
	cylinderRitem8->Geo = mGeometries["cylinderGeo"].get();
	cylinderRitem8->Mat = mMaterials["bricks"].get();
	cylinderRitem8->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	cylinderRitem8->IndexCount = cylinderRitem8->Geo->DrawArgs["cylinder"].IndexCount;  // 36
	cylinderRitem8->StartIndexLocation = cylinderRitem8->Geo->DrawArgs["cylinder"].StartIndexLocation; // 0
	cylinderRitem8->BaseVertexLocation = cylinderRitem8->Geo->DrawArgs["cylinder"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(cylinderRitem8.get());
	mAllRitems.push_back(std::move(cylinderRitem8));

	auto cylinderRitem9 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&cylinderRitem9->World, XMMatrixScaling(0.2f, 0.5f, 0.2f)* XMMatrixTranslation(-5.0f, 12.0f, 15.0f));
	cylinderRitem9->ObjCBIndex = 25;
	cylinderRitem9->Geo = mGeometries["cylinderGeo"].get();
	cylinderRitem9->Mat = mMaterials["bricks"].get();
	cylinderRitem9->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	cylinderRitem9->IndexCount = cylinderRitem9->Geo->DrawArgs["cylinder"].IndexCount;  // 36
	cylinderRitem9->StartIndexLocation = cylinderRitem9->Geo->DrawArgs["cylinder"].StartIndexLocation; // 0
	cylinderRitem9->BaseVertexLocation = cylinderRitem9->Geo->DrawArgs["cylinder"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(cylinderRitem9.get());
	mAllRitems.push_back(std::move(cylinderRitem9));

	auto cylinderRitem10 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&cylinderRitem10->World, XMMatrixScaling(0.2f, 0.5f, 0.2f)* XMMatrixTranslation(5.0f, 12.0f, 15.0f));
	cylinderRitem10->ObjCBIndex = 26;
	cylinderRitem10->Geo = mGeometries["cylinderGeo"].get();
	cylinderRitem10->Mat = mMaterials["bricks"].get();
	cylinderRitem10->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	cylinderRitem10->IndexCount = cylinderRitem10->Geo->DrawArgs["cylinder"].IndexCount;  // 36
	cylinderRitem10->StartIndexLocation = cylinderRitem10->Geo->DrawArgs["cylinder"].StartIndexLocation; // 0
	cylinderRitem10->BaseVertexLocation = cylinderRitem10->Geo->DrawArgs["cylinder"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(cylinderRitem10.get());
	mAllRitems.push_back(std::move(cylinderRitem10));

	// CASTLE ROOFTOP CONES
	auto coneRitem7 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem7->World, XMMatrixScaling(0.3f, 0.1f, 0.3f)* XMMatrixTranslation(5.0f, 24.0f, 5.0f));
	coneRitem7->ObjCBIndex = 27;
	coneRitem7->Geo = mGeometries["coneGeo"].get();
	coneRitem7->Mat = mMaterials["tiles"].get();
	coneRitem7->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem7->IndexCount = coneRitem7->Geo->DrawArgs["cone"].IndexCount;  // 36
	coneRitem7->StartIndexLocation = coneRitem7->Geo->DrawArgs["cone"].StartIndexLocation; // 0
	coneRitem7->BaseVertexLocation = coneRitem7->Geo->DrawArgs["cone"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(coneRitem7.get());
	mAllRitems.push_back(std::move(coneRitem7));

	auto coneRitem8 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem8->World, XMMatrixScaling(0.3f, 0.1f, 0.3f)* XMMatrixTranslation(-5.0f, 24.0f, 5.0f));
	coneRitem8->ObjCBIndex = 28;
	coneRitem8->Geo = mGeometries["coneGeo"].get();
	coneRitem8->Mat = mMaterials["tiles"].get();
	coneRitem8->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem8->IndexCount = coneRitem8->Geo->DrawArgs["cone"].IndexCount;  // 36
	coneRitem8->StartIndexLocation = coneRitem8->Geo->DrawArgs["cone"].StartIndexLocation; // 0
	coneRitem8->BaseVertexLocation = coneRitem8->Geo->DrawArgs["cone"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(coneRitem8.get());
	mAllRitems.push_back(std::move(coneRitem8));

	auto coneRitem9 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem9->World, XMMatrixScaling(0.3f, 0.1f, 0.3f)* XMMatrixTranslation(5.0f, 24.0f, 15.0f));
	coneRitem9->ObjCBIndex = 29;
	coneRitem9->Geo = mGeometries["coneGeo"].get();
	coneRitem9->Mat = mMaterials["tiles"].get();
	coneRitem9->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem9->IndexCount = coneRitem9->Geo->DrawArgs["cone"].IndexCount;  // 36
	coneRitem9->StartIndexLocation = coneRitem9->Geo->DrawArgs["cone"].StartIndexLocation; // 0
	coneRitem9->BaseVertexLocation = coneRitem9->Geo->DrawArgs["cone"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(coneRitem9.get());
	mAllRitems.push_back(std::move(coneRitem9));

	auto coneRitem10 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem10->World, XMMatrixScaling(0.3f, 0.1f, 0.3f)* XMMatrixTranslation(-5.0f, 24.0f, 15.0f));
	coneRitem10->ObjCBIndex = 30;
	coneRitem10->Geo = mGeometries["coneGeo"].get();
	coneRitem10->Mat = mMaterials["tiles"].get();
	coneRitem10->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem10->IndexCount = coneRitem10->Geo->DrawArgs["cone"].IndexCount;  // 36
	coneRitem10->StartIndexLocation = coneRitem10->Geo->DrawArgs["cone"].StartIndexLocation; // 0
	coneRitem10->BaseVertexLocation = coneRitem10->Geo->DrawArgs["cone"].BaseVertexLocation; // 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(coneRitem10.get());
	mAllRitems.push_back(std::move(coneRitem10));

	//////////////////////////////////////////////////////////
	
	auto treeSpritesRitem = std::make_unique<RenderItem>();
	treeSpritesRitem->World = MathHelper::Identity4x4();
	treeSpritesRitem->ObjCBIndex = 31;
	treeSpritesRitem->Mat = mMaterials["treeSprites"].get();
	treeSpritesRitem->Geo = mGeometries["treeSpritesGeo"].get();
	//step2
	treeSpritesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	treeSpritesRitem->IndexCount = treeSpritesRitem->Geo->DrawArgs["points"].IndexCount;
	treeSpritesRitem->StartIndexLocation = treeSpritesRitem->Geo->DrawArgs["points"].StartIndexLocation;
	treeSpritesRitem->BaseVertexLocation = treeSpritesRitem->Geo->DrawArgs["points"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeSpritesRitem.get());

    mAllRitems.push_back(std::move(wavesRitem));
    mAllRitems.push_back(std::move(gridRitem));
	mAllRitems.push_back(std::move(boxRitem));
	mAllRitems.push_back(std::move(treeSpritesRitem));
}

void TreeBillboardsApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

    // For each render item...
    for(size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		//step3
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> TreeBillboardsApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return { 
		pointWrap, pointClamp,
		linearWrap, linearClamp, 
		anisotropicWrap, anisotropicClamp };
}

float TreeBillboardsApp::GetHillsHeight(float x, float z)const
{
    return 0.3f*(z*sinf(0.1f*x) + x*cosf(0.1f*z));
}

XMFLOAT3 TreeBillboardsApp::GetHillsNormal(float x, float z)const
{
    // n = (-df/dx, 1, -df/dz)
    XMFLOAT3 n(
        -0.03f*z*cosf(0.1f*x) - 0.3f*cosf(0.1f*z),
        1.0f,
        -0.3f*sinf(0.1f*x) + 0.03f*x*sinf(0.1f*z));

    XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
    XMStoreFloat3(&n, unitNormal);

    return n;
}
