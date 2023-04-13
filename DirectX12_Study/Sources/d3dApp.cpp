#include "d3dApp.h"
#include <WindowsX.h>	//GET_X_LPARAM, GET_Y_LPARAM ��ũ�� ���

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
	return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

D3DApp* D3DApp::mApp = nullptr;
D3DApp* D3DApp::GetApp()
{
	return mApp;
}

D3DApp::D3DApp(HINSTANCE hInstance)
	: mhAppInst(hInstance)
{
	// Only one D3DApp can be constructed.
	assert(mApp == nullptr);
	mApp = this;
}

D3DApp::~D3DApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

HWND D3DApp::MainWnd()const
{
	return mhMainWnd;
}

float D3DApp::AspectRatio()const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

void D3DApp::Set4xMsaaState(bool value)
{
	if (m4xMsaaState != value)
	{
		m4xMsaaState = value;

		// Recreate the swapchain and buffers with new multisample settings.
		CreateSwapChain();
		OnResize();
	}
}

int D3DApp::Run()
{
	MSG msg = { 0 };

	mTimer.Reset();

	while (msg.message != WM_QUIT)
	{
		// Windows �޽����� ������ ó���Ѵ�.
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// ������ �ִϸ��̼�/���� �۾��� �����Ѵ�.
		else
		{
			mTimer.Tick();

			if (!mAppPaused)
			{
				CalculateFrameStats();
				Update(mTimer);
				Draw(mTimer);
			}
			else
			{
				Sleep(100);
			}
		}
	}

	return (int)msg.wParam;
}

bool D3DApp::Initialize()
{
	if (!InitMainWindow())
		return false;

	if (!InitDirect3D())
		return false;

	// Do the initial resize code.
	OnResize();

	return true;
}

void D3DApp::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));


	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void D3DApp::OnResize()
{
	assert(md3dDevice);
	assert(mSwapChain);
	assert(mDirectCmdListAlloc);

	// Flush before changing any resources.
	FlushCommandQueue();

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Release the previous resources we will be recreating.
	for (int i = 0; i < SwapChainBufferCount; ++i)
		mSwapChainBuffer[i].Reset();
	mDepthStencilBuffer.Reset();

	// Resize the swap chain.
	ThrowIfFailed(mSwapChain->ResizeBuffers(
		SwapChainBufferCount,
		mClientWidth, mClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	mCurrBackBuffer = 0;
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		//��ȯ �罽�� i��° ���۸� ��´�.
		// �Ű����� 1 : ����� �ϴ� Ư�� �ĸ� ���� ����
		// �Ű����� 2 : �� �ĸ� ���۸� ��Ÿ���� ID3D12Resource �������̽��� COM ID
		// �Ű����� 3 : �� �������̽��� ����Ű�� ������
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
		// �� ���ۿ� ���� RTV�� �����Ѵ�.
		// �Ű����� 1 : ���� ������� ����� �ڿ��� ����Ű�� ������
		// �Ű����� 2 : ���� ��� ������ ����ü�� ����Ű�� ������, nullptr�� �ĸ� ������ �Ӹʼ��ؿ� ���� �並 ����
		// �Ű����� 3 : ������ ���� ��� �䰡 ����� �������� �ڵ�
		md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		//���� ���� �׸����� �Ѿ��.
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}

	// ���� ���ٽ� ���ۿ� �並 �����Ѵ�.
	// 1. �ڿ��� ����
	// 2. �ʺ�
	// 3. ����
	// 4. 3���� �ؽ�ó�� ����
	// 5. �Ӹ� ������ ����
	// 6. �ؼ��� �ڷ� ����
	// 7. ���� ǥ��ȭ�� ǥ�� ������ ǰ�� ����
	// 8. �ؽ�ó�� ��ġ
	// 9. ��Ÿ �ڿ� �÷��׵�
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = mDepthStencilFormat;
	depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	// �ڿ��� �����ϰ� ������ �Ӽ��鿡 �����ϴ� ���� �� �ڿ��� �ñ��.
	// �Ű����� 1 : �ڿ��� �ñ� ���� �Ӽ����� ���� ����ü
	// �Ű����� 2 : �ڿ��� �ñ� ���� �������� �ϴ� �Ӽ����� ��Ÿ���� �߰����� �÷��׵�
	// �Ű����� 3 : �����ϰ��� �ϴ� �ڿ� ������ ����ü �ν��Ͻ��� ������
	// �Ű����� 4 : �ڿ��� �ʱ��ä�� �����Ѵ�.
	// �Ű����� 5 : �ڿ� ����⿡ ����ȭ�� ���� ��Ÿ���� D3D12_CLEAR_VALUE�� ����Ű�� ������
	// �Ű����� 6 : �����Ϸ��� �ڿ� �������̽� COM ID
	// �Ű����� 7 : �� �������̽��� ������
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

	// ��ü �ڿ��� �Ӹ� ���� 0�� ���� �����ڸ� �ش� �ڿ��� �ȼ� ������ �����ؼ� �����Ѵ�.
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr, DepthStencilView());

	// �ڿ��� �ʱ� ���¿��� ���� ���۷� ����� �� �ִ� ���·� �����Ѵ�.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// ��� ����� �ݴ´�.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	// resise ��ɵ��� ��⿭�� �߰��Ѵ�.
	// �Ű�����1 : �迭�� �ִ� ��� ��� ����
	// �Ű�����2 : ��� ��ϵ��� �迭�� ù ���Ҹ� ����Ű�� ������
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// resise ����� �Ϸ�� ������ ����Ѵ�.
	FlushCommandQueue();

	// Update the viewport transform to cover the client area.
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, mClientWidth, mClientHeight };
}

LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;

	// WM_SIZE�� ���� ���α׷� â�� ũ�Ⱑ �ٲ� �� �߻��Ѵ�.
	case WM_SIZE:
		// Save the new client area dimensions.
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if (md3dDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{
				// Restoring from minimized state?
				if (mMinimized)
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}

				// Restoring from maximized state?
				else if (mMaximized)
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if (mResizing)
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					OnResize();
				}
			}
		}
		return 0;

	// WM_EXITSIZEMOVE�� ����ڰ� ũ�� ���� �׵θ��� ������ ���޵ȴ�.
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;

	// WM_EXITSIZEMOVE�� ����ڰ� ũ�� ���� �׵θ��� ������ ���޵ȴ�.
	// â�� �� ũ�⿡ �°� ��� ���� �缳���Ѵ�.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		OnResize();
		return 0;

	// WM_DESTROY�� â�� �ı��Ƿ� �� �� ���޵ȴ�.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	// The WM_MENUCHAR�� �޴��� Ȱ��ȭ�Ǿ ����ڰ� Ű�� �������� � Ű�� ����Ű����
	// �ش���� ���� �� ���޵ȴ�.
	case WM_MENUCHAR:
		// alt-enter�� ������ �� �߼Ҹ� ���� �ʰ� �Ѵ�.
		return MAKELRESULT(0, MNC_CLOSE);

	// â�� �ʹ� �۾����� �ʰ� �Ѵ�.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		else if ((int)wParam == VK_F2)
			Set4xMsaaState(!m4xMsaaState);

		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool D3DApp::InitMainWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mhAppInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0);
	if (!mhMainWnd)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	return true;
}

bool D3DApp::InitDirect3D()
{
#if defined(DEBUG) || defined(_DEBUG) 
	// D3D12 ����� ���� Ȱ��ȭ�Ѵ�.
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

	// 1. �ϵ���� ����͸� ��Ÿ���� ��ġ ����
	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,             // nullptr�� �� �⺻ �����
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&md3dDevice));

	// ���� ��WARP ��ġ ����
	if (FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&md3dDevice)));
	}

	// 2. ��Ÿ�� ������ ������ ũ�� ���
	// ��Ÿ�� �������̽��� �����Ѵ�.
	// 0���� �ʱ�ȭ
	// �ٸ� �ڵ忡�� ���ο� ��Ÿ�� ���� ������ 1�� ������Ų��.
	ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&mFence)));

	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// 3. 4X MSAA ǰ�� ���� ���� ����
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mBackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;

	//msQualityLevels�� �ؽ�ó ���İ� ǥ�� ������ �а� �׿� ������ ǰ�� ������ �Է��Ѵ�.
	//�Ű�����1 : ������ ��� ����
	//�Ű�����2 : ��� ���� ������ ������ ����ü�� ����Ű�� ������
	//�Ű�����3 : ����ü�� ũ��
	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

	m4xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

//#ifdef _DEBUG
//	LogAdapters();
//#endif
//
	// 4. ��� ��⿭�� ��� ��� ����
	CreateCommandObjects();
	// 5. ��ȯ �罽�� ������ ����
	CreateSwapChain();
	// 6. ������ �� ����
	CreateRtvAndDsvDescriptorHeaps();

	return true;
}

void D3DApp::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	//��⿭ ����ü�� �������� ��ɴ�⿭ �������̽� ����
	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

	// ��� �޸� �Ҵ��� �������̽��� �����Ѵ�.
	// �Ű����� 1 : ��� ����� ����
	// �Ű����� 2 : �����ϰ��� �ϴ� �������̽��� ID
	// �Ű����� 3 : ������ ��� �Ҵ��ڸ� ����Ű�� ������
	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));

	// ��� ��� �������̽��� �����Ѵ�.
	// �Ű����� 1 : GPU�� �ϳ��� �ý����̱⿡ 0�� ����
	// �Ű����� 2 : ��� ����� ����
	// �Ű����� 3 : ������ ��� ��Ͽ� ������ų �Ҵ���
	// �Ű����� 4 : ��� ����� �ʱ� ���������� ���¸� ����, �ʱ�ȭ ������ ���� nullptr�� �ִ´�.
	// �Ű����� 5 : �����ϰ��� �ϴ� �������̽��� ID
	// �Ű����� 6 : ������ ��� ����� ����Ű�� ������
	ThrowIfFailed(md3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mDirectCmdListAlloc.Get(),
		nullptr,
		IID_PPV_ARGS(mCommandList.GetAddressOf())));

	// Reset�� ȣ���Ϸ��� ��� ����� ���� �־�� �Ѵ�.
	mCommandList->Close();
}

void D3DApp::CreateSwapChain()
{
	// �� ��ȯ �罽�� �����ϱ� ���� ���� ���� ��ȯ �罽�� �����Ѵ�.
	mSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = mClientWidth;
	sd.BufferDesc.Height = mClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	//���� ���÷��� ����
	sd.BufferDesc.Format = mBackBufferFormat;
	//���� �ֻ� �� ��� �ֻ�
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	//�̹����� ����Ϳ� �°� Ȯ�� ����ϴ� ���
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	//�ĸ� ���ۿ� �������Ѵ�.
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	//��ȯ �罽�� ����� ���� ����. ���� ���۸��� 2
	sd.BufferCount = SwapChainBufferCount;
	// ������ ����� ǥ�õ� â�� �ڵ�
	sd.OutputWindow = mhMainWnd;
	// â ���� true, ��üȭ�� ���� false
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	//��üȭ�鿡 �� �´� ���÷��� ��� ����
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// ��ȯ �罽 �������̽� ����
	// ��ȯ �罽�� ��� ��⿭�� �̿��ؼ� �����Ѵ�.
	// �Ű����� 1 : ��⿭ ������
	// �Ű����� 2 : ��ȯ �罽 ���� ����ü ������
	// �Ű����� 3 : ������ ��ȯ�罽�� ����Ű�� ������
	ThrowIfFailed(mdxgiFactory->CreateSwapChain(
		mCommandQueue.Get(),
		&sd,
		mSwapChain.GetAddressOf()));
}

void D3DApp::FlushCommandQueue()
{
	// ���� ��Ÿ�� ���������� ��ɵ��� ǥ���ϵ��� ��Ÿ�� ���� ������Ų��.
	mCurrentFence++;

	// �� ��Ÿ�� ������ �����ϴ� ����� ��⿭�� �߰��Ѵ�.
	// GPU�� Signal��ɱ����� ��� ����� ó���ϱ� �������� �������� �ʴ´�.
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	// GPU�� �� ��Ÿ�� ���������� ��ɵ��� �Ϸ��� ������ ��ٸ���.
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		// GPU�� ���� ��Ÿ�� ������ ���������� �̺�Ʈ�� �ߵ��Ѵ�.
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

		// GPU�� ���� ��Ÿ�� ������ ���������� ���ϴ� �̺�Ʈ�� ��ٸ���.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

ID3D12Resource* D3DApp::CurrentBackBuffer()const
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView()const
{
	// ���Ǹ� ���� CD3DX12_CPU_DESCRIPTOR_HANDLE ������ ���
	// �� �����ڴ� �־��� �����¿� �ش��ϴ� �ĸ� ���� RTV�� �ڵ��� �����ش�.
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),//ù �ڵ�
		mCurrBackBuffer,// ������ ����
		mRtvDescriptorSize);//�������� ����Ʈ ũ��
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView()const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3DApp::CalculateFrameStats()
{
	// �� �޼���� ��� FPS�� ����ϸ�, �ϳ��� �������� �������ϴ� �� �ɸ��� ��� �ð��� ����Ѵ�.

	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// 1�� ������ ��� ������ ���� ����Ѵ�.
	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		wstring fpsStr = to_wstring(fps);
		wstring mspfStr = to_wstring(mspf);

		wstring windowText = mMainWndCaption +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(mhMainWnd, windowText.c_str());

		// ���� �� ����� ���� ��ġ���� �ʱ�ȭ�Ѵ�.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}