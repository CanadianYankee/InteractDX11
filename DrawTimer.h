#pragma once

class CDrawTimer
{
public:
	CDrawTimer(void);
	virtual ~CDrawTimer(void);

	void Reset(); // Call before message loop.
	void Start(); // Call when unpaused.
	void Stop();  // Call when paused.
	bool Tick(float fFrameRefreshInterval = 0.0f);  // Call to request frame.  Returns true if it's time to do new frame.

	float TotalTime() const;  // in seconds
	float DeltaTime() const; // in seconds

private:
	double m_SecondsPerCount;
	double m_DeltaTime;

	__int64 m_BaseTime;
	__int64 m_PausedTime;
	__int64 m_StopTime;
	__int64 m_PrevTime;
	__int64 m_CurrTime;

	bool m_bStopped;
};

