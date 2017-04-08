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

void PrintTableSize(const CFsmTest::SFsmTableSize &size) {
	Print(QString("FSM table: %1\n").arg(DataSizeToString(size.dwMainTableSize)));
	Print(QString("Output table: %1\n").arg(DataSizeToString(size.dwOutputTableSize)));
	Print(QString("Total size: %1\n").arg(DataSizeToString(size.dwTotalSize)));
}

void PrintTimings(const char *szPrefix, const CFsmTest::STimeings &timings) {
	long double dCpuUsage = 1., dCpuKernelUsage = 0;
	if (timings.dTotalTime > 0) {
		dCpuUsage = timings.dCpuTime / timings.dTotalTime;
		if (dCpuUsage > 1) {
			dCpuUsage = 1;
		}

		dCpuKernelUsage = timings.dKernelTime / timings.dTotalTime;
		if (dCpuKernelUsage > 1) {
			dCpuKernelUsage = 1;
		}
	}
	printf("%s: %Lg s (%Lg CPU, %Lg Core)\n", szPrefix, timings.dTotalTime, dCpuUsage, dCpuKernelUsage);
}

void PrintFsmStatistics(const CFsmTest::SFsmStatistics &stats) {
	printf("FSM statistics: %i states, %i output elements.\n", stats.dwStatesCount, stats.dwOutputCellsCount);
	if (stats.dwCollisionsCount > 0) {
		printf("FSM state hashes have %i collisions\n", stats.dwCollisionsCount);
	}

	printf("Default FSM memory requirements:\n");
	PrintTableSize(stats.tableSize);

	printf("FSM memory requirements with minimal data types and no memory alignment:\n");
	PrintTableSize(stats.tableMinSize);
}

void PrintEnginePerformance(const char *szEngineName, const CFsmTest::SEnginePerformance &performance) {
	printf("= Results for %s =\n", szEngineName);
	PrintTimings("Initialization", performance.timInitialization);
	PrintTimings("Operating", performance.timOperating);

	long double dRate = performance.dwBytesCount / performance.timOperating.dTotalTime;
	Print(QString("Total rate: %1/s, found %2 entries\n").arg(DataSizeToString(dRate)).arg(performance.dwHits));

	Print(QString("Total memory requirements: %1\n").arg(DataSizeToString(performance.dwMemoryRequirements)));
	if (performance.fIsFsm) { // results for some SearchFSM engine
		PrintFsmStatistics(performance.fsmStatistics);
	}
	puts("");
}

void PrintEnginePerformance(const char *szEngineName, bool fSuccess, const CFsmTest::SEnginePerformance &performance) {
	if (fSuccess) {
		PrintEnginePerformance(szEngineName, performance);
	} else {
		printf("Failed to test %s!\n", szEngineName);
	}
}

struct STestResult {
	CFsmTest::SEnginePerformance perfRegister;
	CFsmTest::SEnginePerformance perfFsm1;
	CFsmTest::SEnginePerformance perfFsm4;
	CFsmTest::SEnginePerformance perfFsm8;

	// obsolete
	int nFsmStates;
	unsigned int dwTableSize;
	unsigned int dwMinTableSize;
	unsigned int dwTableNibbleSize;
	unsigned int dwTableByteSize;
	double dFsmRate;
	double dFsmNibbleRate;
	double dFsmByteRate;
	double dRegisterRate;
};

STestResult TestSpeed(const TPatterns patterns) {
	CFsmTest tester;
	printf("\nSearch FSM for patterns:\n");
	printf("Creating FSM...\r");
	PrintPatterns(patterns);
	try {
		tester.CreateFsm(patterns, true, true);
	}
	catch(...) {
		try {
			puts("Failed to create byte SearchFSM - to big tables");
			tester.CreateFsm(patterns, true, false);
		}
		catch(...) {
			puts("Failed to create even nibble SearchFSM - to big tables");
			try {
				tester.CreateFsm(patterns, false, false);
			}
			catch(...) {
				puts("Failed to create bit SearchFSM!");
				STestResult resultNo;
				return resultNo;
			}
		}
	}

	printf("\nTest correctness...");
	unsigned int dwHits;
	bool fOk = tester.TestCorrectness(g_nFastTestCorrectnessBytes, 0, &dwHits);
	puts(fOk? "OK" : "FAIL");
	Print(QString("Tested on %1 data, found %2 entries\n").arg(DataSizeToString(g_nFastTestCorrectnessBytes)).arg(dwHits));

	Print(QString("\nSpeed tests (on %1 data):\n").arg(DataSizeToString(g_nTestSpeedBytes)));
	STestResult result;
	CFsmTest::SEnginePerformance performance;
	long double dFsmRate = -1;
	bool fSuccess = tester.TestBitFsmRate(g_nTestSpeedBytes, &performance);
	PrintEnginePerformance("Bit SearchFSM (FSM-1)", fSuccess, performance);
	result.perfFsm1 = performance;
	dFsmRate = performance.dRate;

	long double dFsmNibbleRate = -1;
	fSuccess = tester.TestNibbleFsmRate(g_nTestSpeedBytes, &performance);
	PrintEnginePerformance("Nibble SearchFSM (FSM-4)", fSuccess, performance);
	result.perfFsm4 = performance;
	dFsmNibbleRate = performance.dRate;

	fSuccess = tester.TestOctetFsmRate(g_nTestSpeedBytes, &performance);
	PrintEnginePerformance("Octet SearchFSM (FSM-8)", fSuccess, performance);
	result.perfFsm8 = performance;
	long double dFsmByteRate = performance.dRate;

	fSuccess = tester.TestRegisterRate2(g_nTestSpeedBytes, &performance);
	PrintEnginePerformance("Register search", fSuccess, performance);
	result.perfRegister = performance;
	long double dRegisterRate = performance.dRate;

	result.nFsmStates = tester.GetStatesCount();
	result.dwTableSize = tester.GetTableSize().dwTotalSize;
	result.dwMinTableSize = tester.GetMinimalTableSize().dwTotalSize;
	result.dwTableNibbleSize = tester.GetNibbleTableSize().dwTotalSize;
	result.dwTableByteSize = tester.GetByteTableSize().dwTotalSize;
	result.dFsmRate = dFsmRate;
	result.dFsmNibbleRate = dFsmNibbleRate;
	result.dFsmByteRate = dFsmByteRate;
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

	printf("\nbit FSM rate");
	for (idx = 0; idx < list.count(); idx++) {
		printf("\t%g", list[idx].result.dFsmRate / g_dwMebi);
	}

	printf("\nnibble table");
	for (idx = 0; idx < list.count(); idx++) {
		Print(QString("\t%1").arg(DataSizeToString(list[idx].result.dwTableNibbleSize)));
	}

	printf("\nnibble FSM rate");
	for (idx = 0; idx < list.count(); idx++) {
		printf("\t%g", list[idx].result.dFsmNibbleRate / g_dwMebi);
	}

	printf("\nbyte table");
	for (idx = 0; idx < list.count(); idx++) {
		Print(QString("\t%1").arg(DataSizeToString(list[idx].result.dwTableByteSize)));
	}

	printf("\nbyte FSM rate");
	for (idx = 0; idx < list.count(); idx++) {
		printf("\t%g", list[idx].result.dFsmByteRate / g_dwMebi);
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
	BunchTest(4, false, 0, 4);
	BunchTest(5, false, 4, 1);
	BunchTest(0, true);
	BunchTest(1, true, 4, 3);
	BunchTest(2, true, 4, 4);
	BunchTest(3, true, 4, 5);
	BunchTest(4, true, 5);
	BunchTest(5, true, 5);

	return 0;
}
