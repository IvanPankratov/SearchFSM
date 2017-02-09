#line 2 "main.cpp" // Make __FILE__ omit the path

#include <QCoreApplication>
#include <QDateTime>

#include "SearchFsm.h"
#include "FsmCreator.h"
#include "FsmTest.h"

const int g_nTraceBits = 70;
const int g_nTestCorrectnessBytes = 1024 * 1024 * 1024; // 1024 MiB
const int g_nFastTestCorrectnessBytes = 1024 * 1024; // 1 MiB
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

struct STestResult {
	int nFsmStates;
	unsigned int dwTableSize;
	unsigned int dwMinTableSize;
	double dFsmRate;
	double dRegisterRate;
};

STestResult TestSpeed(const TPatterns patterns) {
	const unsigned int g_dwMebi = 1024 * 1024;

	CFsmTest tester;
	printf("\nSearch FSM for patterns:\n");
	printf("Creating FSM...\r");
	PrintPatterns(patterns);
	tester.CreateFsm(patterns);
	PrintFsmStats(tester);
	unsigned int dwCollisions = tester.GetCollisionsCount();
	if (dwCollisions > 0) {
		printf("FSM state hashes have %i collisions\n", dwCollisions);
	}

	printf("\nTest correctness...");
	unsigned int dwHits;
	bool fOk = tester.TestCorrectness(g_nFastTestCorrectnessBytes, 0, &dwHits);
	puts(fOk? "OK" : "FAIL");
	Print(QString("Tested on %1 data, found %2 entries\n").arg(DataSizeToString(g_nFastTestCorrectnessBytes)).arg(dwHits));

	Print(QString("\nSpeed tests (on %1 data):\n").arg(DataSizeToString(g_nTestSpeedBytes)));
	long double dFsmRate = tester.TestFsmRate(g_nTestSpeedBytes, &dwHits);
	printf("FSM speed: %Lg MiB/s (found %i entries)\n", dFsmRate / g_dwMebi, dwHits);
	long double dRegisterRate = tester.TestRegisterRate(g_nTestSpeedBytes, &dwHits);
	printf("Register speed: %Lg MiB/s (found %i entries)\n", dRegisterRate / g_dwMebi, dwHits);

	STestResult result;
	result.nFsmStates = tester.GetStatesCount();
	CFsmTest::STableSize size = tester.GetTableSize();
	result.dwTableSize = size.dwTotalSize;
	size = tester.GetMinimalTableSize();
	result.dwMinTableSize = size.dwTotalSize;
	result.dFsmRate = dFsmRate;
	result.dRegisterRate = dRegisterRate;

	return result;
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

struct STest {
	int nPatternsLength;
	int nPatternsCount;
	int nErrorsCount;
	bool fMasked;
	STestResult result;
};
typedef QList<STest> TTestList;

void PrintTestLabel(const STest& test) {
	static const QString g_sTestLabelPattern = "%1, begin test: %3 bits, %4 errors, %5 mask, %2 patterns\n";

	QString sTimestamp = QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss.zzz");
	QString sTestLabel = g_sTestLabelPattern.arg(sTimestamp).arg(test.nPatternsCount).arg(test.nPatternsLength)
			.arg(test.nErrorsCount).arg(test.fMasked? "with" : "no");
	fputs(sTestLabel.toLocal8Bit().constData(), stderr);
}

TTestList TestOnPatterns(int nMaxCount, int nLength, int nErrors = 0, bool fMasked = false) {
	TTestList testList;
	TPatterns patterns;
	int idx;
	for (idx = 0; idx < nMaxCount; idx++) {
		patterns << GeneratePattern(nLength, nErrors, fMasked);
		STest test;
		test.nPatternsCount = idx + 1;
		test.nPatternsLength = nLength;
		test.nErrorsCount = nErrors;
		test.fMasked = fMasked;
		puts("\n--------------------");
		PrintTestLabel(test);
		test.result = TestSpeed(patterns);

		testList << test;
	}

	return testList;
}

void DumpTestList(const TTestList &list) {
	const unsigned int g_dwMebi = 1024 * 1024;

	printf("\nlength");
	int idx;
	for (idx = 0; idx < list.count(); idx++) {
		printf("\t%i", list[idx].nPatternsLength);
	}

	printf("\ncount");
	for (idx = 0; idx < list.count(); idx++) {
		printf("\t%i", list[idx].nPatternsCount);
	}

	printf("\nerrors");
	for (idx = 0; idx < list.count(); idx++) {
		printf("\t%i", list[idx].nErrorsCount);
	}

	printf("\nmasked");
	for (idx = 0; idx < list.count(); idx++) {
		printf("\t%s", list[idx].fMasked? "true" : "false");
	}

	printf("\nstates");
	for (idx = 0; idx < list.count(); idx++) {
		printf("\t%i", list[idx].result.nFsmStates);
	}

	printf("\ntable");
	for (idx = 0; idx < list.count(); idx++) {
		Print(QString("\t%1").arg(DataSizeToString(list[idx].result.dwTableSize)));
	}

	printf("\nmin-table");
	for (idx = 0; idx < list.count(); idx++) {
		Print(QString("\t%1").arg(DataSizeToString(list[idx].result.dwMinTableSize)));
	}

	printf("\nFSM rate");
	for (idx = 0; idx < list.count(); idx++) {
		printf("\t%g", list[idx].result.dFsmRate / g_dwMebi);
	}


	printf("\nReg rate");
	for (idx = 0; idx < list.count(); idx++) {
		printf("\t%g", list[idx].result.dRegisterRate / g_dwMebi);
	}

	printf("\n\n");
}

void BunchTest(int nErrors, bool fMasked, int nReduction = 0, int nSubReduction = 0) {
	QList<int> LengthsList;
	LengthsList << 8 << 28 << 32 << 48 << 65 << 165;
	int idx;
	for (idx = 0; idx < LengthsList.count() - nReduction; idx++) {
		int nLength = LengthsList[idx];
		int nPatternsCount = 6;
		if (idx == LengthsList.count() - nReduction - 1) {
			nPatternsCount -= nSubReduction;
		}
		TTestList testList = TestOnPatterns(nPatternsCount, nLength, nErrors, fMasked);
		DumpTestList(testList);
	}
}

int main(int argc, char *argv[]) {
	QCoreApplication a(argc, argv);

	BunchTest(0, false);
	BunchTest(1, false);
	BunchTest(2, false);
	BunchTest(3, false);
	BunchTest(4, false, 0, 2);
	BunchTest(5, false, 2, 2);
	BunchTest(0, true);
	BunchTest(1, true, 2, 1);
	BunchTest(2, true, 4, 3);
	BunchTest(3, true, 4, 5);
	BunchTest(4, true, 5);
	BunchTest(5, true, 5);

	return 0;
}
