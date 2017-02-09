#line 2 "main.cpp" // Make __FILE__ omit the path

#include <QCoreApplication>

#include "SearchFsm.h"
#include "FsmCreator.h"
#include "FsmTest.h"

const int g_nTraceBits = 70;
const int g_nTestCorrectnessBytes = 1024 * 1024 * 1024; // 1024 MiB
const int g_nTestSpeedBytes = 100 * 1024 * 1024; // 100 MiB

void Print(const QString &s) {
	printf("%s", s.toLocal8Bit().constData());
}

void Print(const SPattern &pattern) {
	QString sPattern = PatternToString(pattern);
	sPattern = QString("%1, %2 errors\n").arg(sPattern).arg(pattern.nMaxErrors);
	Print(sPattern);
}

void PrintPatterns(const TPatterns &patterns) {
	int idx;
	for (idx = 0; idx < patterns.count(); idx++) {
		printf("#%i ", idx);
		Print(patterns[idx]);
	}
}

QString DoubleToString(double dValue, int nPrecision) {
	double dPart = dValue;
	while (nPrecision > 0 && dPart >= 1) { // part has at least one nonzero digit
		nPrecision--;
		dPart /= 10; // crop digit
	}
	return QString::number(dValue, 'f', nPrecision);
}

QString DataSizeToString(unsigned int dwSize) {
	const unsigned int g_dwKibi = 1024;
	const unsigned int g_dwMebi = g_dwKibi * g_dwKibi;
	const int g_nPrecision = 3;
	if (dwSize < g_dwKibi) {
		return QString("%1 B").arg(dwSize);

	} else if (dwSize < g_dwMebi) { // kibibytes
		double dKibibytes = (double)dwSize / g_dwKibi;
		return QString("%1 KiB").arg(DoubleToString(dKibibytes, g_nPrecision));

	} else { // mebibytes
		double dMebibytes = (double)dwSize / g_dwMebi;
		return QString("%1 MiB").arg(DoubleToString(dMebibytes, g_nPrecision));
	}
}

void PrintTableSize(const CFsmTest::STableSize &size) {
	Print(QString("FSM table: %1\n").arg(DataSizeToString(size.dwFsmTableSize)));
	Print(QString("Output table: %1\n").arg(DataSizeToString(size.dwOutputTableSize)));
	Print(QString("Total size: %1\n").arg(DataSizeToString(size.dwTotalSize)));
}

void PrintFsmStats(const CFsmTest &fsm) {
	unsigned int dwStatesCount = fsm.GetStatesCount();
	unsigned int dwOutputCellsCount = fsm.GetOutputElementsCount();
	Print(QString("\nFSM with %1 states, %2 output elements.\n").arg(dwStatesCount).arg(dwOutputCellsCount));

	CFsmTest::STableSize size = fsm.GetTableSize();
	printf("\nDefault FSM memory requirements:\n");
	PrintTableSize(size);

	printf("\nFSM memory requirements with minimal data types and no memory alignment:\n");
	size = fsm.GetMinimalTableSize();
	PrintTableSize(size);

}

void TestSpeed(const TPatterns patterns, bool fRecursive = false) {
	const unsigned int g_dwMebi = 1024 * 1024;

	puts("--------------------");

	CFsmTest tester;
	printf("\nSearch FSM for patterns:\n");
	PrintPatterns(patterns);
	tester.CreateFsm(patterns);
	PrintFsmStats(tester);

	unsigned int dwHits;
	bool fOk = tester.TestCorrectness(g_nTestCorrectnessBytes, 0, &dwHits);
	puts(fOk? "OK" : "FAIL");
	Print(QString("Tested on %1 data, found %2 entries\n").arg(DataSizeToString(g_nTestCorrectnessBytes)).arg(dwHits));

	puts("\nSpeed tests:");
	long double dRate = tester.TestFsmRate(g_nTestSpeedBytes, &dwHits);
	printf("FSM speed: %g MiB/s (found %i entries)\n", dRate / g_dwMebi, dwHits);
	dRate = tester.TestRegisterRate(g_nTestSpeedBytes, &dwHits);
	printf("Register speed: %g MiB/s (found %i entries)\n", dRate / g_dwMebi, dwHits);

	if (fRecursive) {
		TPatterns patsReduced = patterns;
		if (patsReduced.count() > 1) {
			patsReduced.removeLast();
			TestSpeed(patsReduced, fRecursive);
		}
	}
}

SPattern::TData GenarateData(int nBytes) {
	SPattern::TData data;
	int idx;
	for (idx = 0; idx < nBytes; idx++) {
		data << rand();
	}

	return data;
}

SPattern GeneratePattern(int nLength, int nErrors, bool fMasked = false) {
	int nBytes = nLength / BITS_IN_BYTE;
	if (nLength % BITS_IN_BYTE > 0) {
		nBytes++;
	}
	SPattern pat;
	pat.nLength = nLength;
	pat.nMaxErrors = nErrors;
	pat.data = GenarateData(nBytes);
	if (fMasked) {
		pat.mask = GenarateData(nBytes);
	}

	return pat;
}

void TestOnPatterns(int nCount, int nLength, int nErrors = 0, bool fMasked = false) {
	TPatterns patterns;
	int idx;
	for (idx = 0; idx < nCount; idx++) {
		patterns << GeneratePattern(nLength, nErrors, fMasked);
	}

	TestSpeed(patterns, true);
}

int main(int argc, char *argv[]) {
	QCoreApplication a(argc, argv);

	TestOnPatterns(6, 8);
	TestOnPatterns(6, 28);
	TestOnPatterns(6, 32);
	TestOnPatterns(6, 48);
	TestOnPatterns(6, 65);

	return 0;

	SPattern pat1;
	pat1.data << 0x1e;
	pat1.nLength = 5;
	pat1.nMaxErrors = 2;

	SPattern pat2;
	pat2.data << 0xaa;
	pat2.nLength = 8;
	pat2.nMaxErrors = 2;

	SPattern patTwoParts; // 1011----1101
	patTwoParts.data << 0xb0 << 0x0d;
	patTwoParts.mask << 0xf0 << 0x0f;
	patTwoParts.nLength = 12;
	patTwoParts.nMaxErrors = 1;

	TPatterns patterns;
	patterns << pat1 << pat2;// << patTwoParts;

	TestSpeed(patterns, true);

	CFsmTest tester;
	printf("\nSearch FSM for patterns:\n");
	PrintPatterns(patterns);
	tester.CreateFsm(patterns);
	PrintFsmStats(tester);

	printf("\nTracing the FSM:\n");
	tester.TraceFsm(g_nTraceBits);
	printf("\nTesting FSM correctness... ");
	unsigned int dwHits;
	bool fOk = tester.TestCorrectness(g_nTestCorrectnessBytes, 25, &dwHits);
	printf("%s\n", fOk? "OK" : "FAIL");
	Print(QString("Tested on %1 data, found %2 entries\n").arg(DataSizeToString(g_nTestCorrectnessBytes)).arg(dwHits));

	puts("\nSpeed tests:");
	long double dRate = tester.TestFsmRate(g_nTestSpeedBytes, &dwHits);
	printf("FSM speed: %g B/s (found %i entries)\n", dRate, dwHits);
	dRate = tester.TestRegisterRate(g_nTestSpeedBytes, &dwHits);
	printf("Register speed: %g B/s (found %i entries)\n", dRate, dwHits);

	return 0;
}
