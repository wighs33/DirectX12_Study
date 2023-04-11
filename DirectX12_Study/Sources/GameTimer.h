#pragma once

class GameTimer
{
public:
	GameTimer();

	//float TotalTime()const; // �� ����
	//float DeltaTime()const; // �� ����

	void Reset(); // �޽��� ���� ������ ȣ���ؾ� ��
	//void Start(); // Ÿ�̸Ӹ� ���� �Ǵ� �簳�� �� ȣ���ؾ� ��
	//void Stop();  // Ÿ�̸Ӹ� ������ �� ȣ���ؾ� ��
	void Tick();  // �� ������ ȣ���ؾ� ��

private:
	double mSecondsPerCount;
	double mDeltaTime;

	__int64 mBaseTime;
	__int64 mPausedTime;
	__int64 mStopTime;
	__int64 mPrevTime;
	__int64 mCurrTime;

	bool mStopped;
};

