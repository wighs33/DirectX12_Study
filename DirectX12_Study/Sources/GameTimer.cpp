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

float GameTimer::TotalTime()const
{
	// 타이머가 정지 상태이면 정지된 시점부터 흐른 시간은 계산하지 않는다.
	// mStopTime에서 일시 정지 누적시간을 뺀다.
	if (mStopped)
	{
		return (float)(((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
	}
	// mCurrTime에서 일시 정지 누적시간을 뺀다.
	else
	{
		return (float)(((mCurrTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
	}
}

float GameTimer::DeltaTime()const
{
	return (float)mDeltaTime;
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

void GameTimer::Start()
{
	__int64 startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

	//정지 상태에서 타이머를 재개하는 경우
	if (mStopped)
	{
		//일시 정지된 시간을 누적한다.
		mPausedTime += (startTime - mStopTime);

		//현재의 mPrevTime은 유효하지 않다. 현재시간으로 다시 설정한다.
		mPrevTime = startTime;

		//이젠 정지 상태가 아니므로 관련 멤버들을 갱신한다.
		mStopTime = 0;
		mStopped = false;
	}
}

void GameTimer::Stop()
{
	//이미 정지 상태이면 아무 일도 하지 않는다.
	if (!mStopped)
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		//그렇지 않다면 현재 시간을 타이머 정지 시점 시간으로 정하고, 플래그를 설정한다.
		mStopTime = currTime;
		mStopped = true;
	}
}

void GameTimer::Tick()
{
	if (mStopped)
	{
		mDeltaTime = 0.0;
		return;
	}

	//이번 프레임의 시간을 얻는다.
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;

	// 이번 프레임의 시간과 이전 프레임의 시간의 차이를 구한다.
	mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;

	// 다음 프레임을 준비한다.
	mPrevTime = mCurrTime;

	// 음수가 되지 않게 한다.
	if (mDeltaTime < 0.0)
	{
		mDeltaTime = 0.0;
	}
}
