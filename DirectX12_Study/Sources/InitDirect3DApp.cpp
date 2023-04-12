//***************************************************************************************
// Init Direct3D.cpp by Frank Luna (C) 2015 All Rights Reserved.
//
// Demonstrates the sample framework by initializing Direct3D, clearing 
// the screen, and displaying frame stats.
//
//***************************************************************************************
#include "InitDirect3DApp.h"
#include <DirectXColors.h>

using namespace DirectX;

BoxApp::BoxApp(HINSTANCE hInstance)
: D3DApp(hInstance) 
{
}

BoxApp::~BoxApp()
{
}

bool BoxApp::Initialize()
{
    if(!D3DApp::Initialize())
		return false;
		
	return true;
}

void BoxApp::OnResize()
{
	D3DApp::OnResize();
}

void BoxApp::Update(const GameTimer& gt)
{

}


void BoxApp::Draw(const GameTimer& gt)
{
	//명령 기록에 관련된 메모리의 재활용을 위해 명령 할당자를 재설정한다.
	//재설정은 GPU가 관련 명령 목록들을 모두 처리한 후에 일어난다.
	ThrowIfFailed(mDirectCmdListAlloc->Reset());

	// 명령 목록을 ExecuteCommandList를 통해서 명령 대기열에 추가했다면 명령 목록을 재설정할 수 있다.
    // 명령 목록을 재설정하면 메모리가 재활용된다.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// 자원 용도에 관련된 상태 전이를 다렉에 통지한다.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // 뷰포트와 가위 직사각형을 설정한다.
	// 명령 목록을 재설정할 때마다 이들도 재설정해야 한다.
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // 후면 버퍼와 깊이 버퍼를 지운다.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	
    // 렌더링 결과가 기록될 렌더 대상 버퍼들을 지정한다.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
	
    // 자원 용도에 관련된 상태 전이를 다렉에 통지한다.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // 명령들을 기록을 마친다.
	ThrowIfFailed(mCommandList->Close());
 
    // 명령 실행을 위해 명령 목록을 대기열에 추가한다.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	
	// 후면 버퍼와 전면 버퍼를 교환한다.
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// 이 프레임의 명령들이 모두 처리되길 기다린다. (비효율적)
	FlushCommandQueue();
}
