#line 2 "WinTimer.h" // Make __FILE__ omit the path

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

	/// Get real number of seconds for current thread.
	/** \sa Start, Stop, GetUserDuration, GetKernelDuration, GetTotalDuration */
	TTime GetThreadDuration() const;

	/// Get real number of seconds for current thread in user mode.
	/** \sa Start, Stop, GetThreadDuration, GetKernelDuration, GetTotalDuration */
	TTime GetUserDuration() const;

	/// Get real number of seconds for current thread in kernel mode.
	/** \sa Start, Stop, GetThreadDuration, GetUserDuration, GetTotalDuration */
	TTime GetKernelDuration() const;

	/// Get real number of seconds elapsed since start moment.
	/** \sa Start, Stop, GetThreadDuration, GetUserDuration, GetKernelDuration */
	TTime GetTotalDuration() const;

private:
	static TWinTime Sum(const TWinTime wt1, const TWinTime wt2); // wt1 + wt2
	static TWinTime Diff(const TWinTime wt1, const TWinTime wt2); // wt1 - wt2
	static TTime WinTimeToTime(const TWinTime &wt);

private:
	// start times
	TWinTime m_wtStartTimeUser; // thread in kernel mode
	TWinTime m_wtStartTimeKernel; // thread in core mode
	TWinTime m_wtStartTimeSystem; // total real time

	// start-to-stop durations
	TWinTime m_wtUserDuration;
	TWinTime m_wtKernelDuration;
	TWinTime m_wtTotalDuration;
	bool m_fRunning;
};


// implementation
void inline CWinTimer::Start() {
	TWinTime wtDummy;
	GetThreadTimes(GetCurrentThread(), &wtDummy, &wtDummy, &m_wtStartTimeKernel, &m_wtStartTimeUser);
	GetSystemTimeAsFileTime(&m_wtStartTimeSystem);

	TWinTime wtZero = {0, 0};
	m_wtUserDuration = wtZero;
	m_wtKernelDuration = wtZero;
	m_wtTotalDuration = wtZero;

	m_fRunning = true;
}

void inline CWinTimer::Stop() {
	if (!m_fRunning) return;
	TWinTime wtDummy, wtStopTimeKernel, wtStopTimeUser, wtStopTimeSystem;
	GetThreadTimes(GetCurrentThread(), &wtDummy, &wtDummy, &wtStopTimeKernel, &wtStopTimeUser);
	GetSystemTimeAsFileTime(&wtStopTimeSystem);

	m_wtUserDuration = Diff(wtStopTimeUser, m_wtStartTimeUser);
	m_wtKernelDuration = Diff(wtStopTimeKernel, m_wtStartTimeKernel);
	m_wtTotalDuration = Diff(wtStopTimeSystem, m_wtStartTimeSystem);

	m_fRunning = false;
}

CWinTimer::TTime inline CWinTimer::GetThreadDuration() const {
	return WinTimeToTime(Sum(m_wtUserDuration, m_wtKernelDuration));
}

CWinTimer::TTime inline CWinTimer::GetUserDuration() const {
	return WinTimeToTime(m_wtUserDuration);
}

CWinTimer::TTime inline CWinTimer::GetKernelDuration() const {
	return WinTimeToTime(m_wtKernelDuration);
}

CWinTimer::TTime inline CWinTimer::GetTotalDuration() const {
	return WinTimeToTime(m_wtTotalDuration);
}

// private members
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

CWinTimer::TTime inline CWinTimer::WinTimeToTime(const CWinTimer::TWinTime &wt) {
	// in Windows API format one unit is 100 ns, i.e. 10^(-7) seconds
	const TTime g_tLowCoefficient = 1e-7;
	// in high part the unit is 4294967296 times bigger
	const TTime g_tHighCoefficient = 4294967296e-7;

	return wt.dwHighDateTime * g_tHighCoefficient + wt.dwLowDateTime * g_tLowCoefficient;
}

#endif // WINTIMER_H
