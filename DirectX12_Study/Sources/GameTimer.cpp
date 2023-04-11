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
	// Ÿ�̸Ӱ� ���� �����̸� ������ �������� �帥 �ð��� ������� �ʴ´�.
	// mStopTime���� �Ͻ� ���� �����ð��� ����.
	if (mStopped)
	{
		return (float)(((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
	}
	// mCurrTime���� �Ͻ� ���� �����ð��� ����.
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

	//���� ���¿��� Ÿ�̸Ӹ� �簳�ϴ� ���
	if (mStopped)
	{
		//�Ͻ� ������ �ð��� �����Ѵ�.
		mPausedTime += (startTime - mStopTime);

		//������ mPrevTime�� ��ȿ���� �ʴ�. ����ð����� �ٽ� �����Ѵ�.
		mPrevTime = startTime;

		//���� ���� ���°� �ƴϹǷ� ���� ������� �����Ѵ�.
		mStopTime = 0;
		mStopped = false;
	}
}

void GameTimer::Stop()
{
	//�̹� ���� �����̸� �ƹ� �ϵ� ���� �ʴ´�.
	if (!mStopped)
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		//�׷��� �ʴٸ� ���� �ð��� Ÿ�̸� ���� ���� �ð����� ���ϰ�, �÷��׸� �����Ѵ�.
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
