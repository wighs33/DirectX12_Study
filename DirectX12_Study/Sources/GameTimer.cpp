#include <windows.h>
#include "GameTimer.h"

GameTimer::GameTimer()
	: mSecondsPerCount(0.0), mDeltaTime(-1.0), mBaseTime(0),
	mPausedTime(0), mPrevTime(0), mCurrTime(0), mStopped(false)
{
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	mSecondsPerCount = 1.0 / (double)countsPerSec;
}

void GameTimer::Reset()
{
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped = false;
}

void GameTimer::Tick()
{
	if (mStopped)
	{
		mDeltaTime = 0.0;
		return;
	}

	//�̹� �������� �ð��� ��´�.
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;

	// �̹� �������� �ð��� ���� �������� �ð��� ���̸� ���Ѵ�.
	mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;

	// ���� �������� �غ��Ѵ�.
	mPrevTime = mCurrTime;

	// ������ ���� �ʰ� �Ѵ�.
	if (mDeltaTime < 0.0)
	{
		mDeltaTime = 0.0;
	}
}
