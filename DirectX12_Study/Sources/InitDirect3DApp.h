#pragma once
#include "d3dApp.h"

class BoxApp : public D3DApp
{
public:
	BoxApp(HINSTANCE hInstance);
	~BoxApp();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

};