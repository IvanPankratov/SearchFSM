#ifndef WINTIMER_H
#define WINTIMER_H

#include <Windows.h>

class CWinTimer { // timer
public:
	/// Common type for time and duration
	typedef long double TTime; // t
	/// Platform-specific time format
	typedef FILETIME TWinTime; // wt

public:
	CWinTimer() {Start();} ///< Create timer. Timer is started immediately.

public:
	/// Start/restart timer from zero. \sa Stop, GetDuration
	void Start();

	/// Stop (pause) timer. Time is frozen. \sa Start, GetDuration
	void Stop();

	/// Get duration - real number of seconds.
	/** \sa Start, Stop, GetPlatformDuration */
	TTime GetDuration() const;

	/// Get duration in the platform-dependant format.
	/** \sa Start, Stop, GetDuration */
	TWinTime GetPlatformDuration() const {
		return m_wtDuration;
	}

private:
	static TWinTime GetThreadFullTime();
	static TWinTime Sum(const TWinTime wt1, const TWinTime wt2); // wt1 + wt2
	static TWinTime Diff(const TWinTime wt1, const TWinTime wt2); // wt1 - wt2

private:
	TWinTime m_wtStartTime;
	TWinTime m_wtDuration;
	bool m_fRunning;
};


// implementation
void inline CWinTimer::Start() {
	m_wtStartTime = GetThreadFullTime();
	m_wtDuration.dwLowDateTime = 0;
	m_wtDuration.dwHighDateTime = 0;
	m_fRunning = true;
}

void inline CWinTimer::Stop() {
	if (!m_fRunning) return;
	TWinTime wtStopTime = GetThreadFullTime();
	m_wtDuration = Diff(wtStopTime, m_wtStartTime);
	m_fRunning = false;
}

CWinTimer::TTime inline CWinTimer::GetDuration() const {
	// in Windows API format one unit is 100 ns, i.e. 10^(-7) seconds
	const TTime g_tLowCoefficient = 1e-7;
	// in high part the unit is 4294967296 times bigger
	const TTime g_tHighCoefficient = 4294967296e-7;

	TWinTime wtDuration = GetPlatformDuration();
	return wtDuration.dwHighDateTime * g_tHighCoefficient + wtDuration.dwLowDateTime * g_tLowCoefficient;
}

// private members
CWinTimer::TWinTime inline CWinTimer::GetThreadFullTime() {
	TWinTime wtDummy, wtKernelTime, wtUserTime;
	GetThreadTimes(GetCurrentThread(), &wtDummy, &wtDummy, &wtKernelTime, &wtUserTime);
	return Sum(wtKernelTime, wtUserTime);
}

CWinTimer::TWinTime inline CWinTimer::Sum(const TWinTime wt1, const TWinTime wt2) {
	// dwLow ~ dwLowDateTime in Sum
	DWORD dwLow = wt1.dwLowDateTime + wt2.dwLowDateTime;
	// carry <=> dwLow < t1.dwLowDateTime
	DWORD dwCarry = (dwLow < wt1.dwLowDateTime)? 1 : 0;
	// dwHigh ~ dwHighDateTime in Sum
	DWORD dwHigh = wt1.dwHighDateTime + wt2.dwHighDateTime + dwCarry;

	TWinTime wtSum;
	wtSum.dwLowDateTime = dwLow;
	wtSum.dwHighDateTime = dwHigh;
	return wtSum;
}

CWinTimer::TWinTime inline CWinTimer::Diff(const TWinTime wt1, const TWinTime wt2) {
	// dwLow ~ dwLowDateTime in Difference
	DWORD dwLow = wt1.dwLowDateTime - wt2.dwLowDateTime;
	// borrow <=> t1.dwLowDateTime < t2.dwLowDateTime
	DWORD dwBorrow = (wt1.dwLowDateTime < wt2.dwLowDateTime)? 1 : 0;
	// dwHigh ~ dwHighDateTime in Difference
	DWORD dwHigh = wt1.dwHighDateTime - wt2.dwHighDateTime - dwBorrow;

	TWinTime wtDiff;
	wtDiff.dwLowDateTime = dwLow;
	wtDiff.dwHighDateTime = dwHigh;
	return wtDiff;
}

#endif // WINTIMER_H
