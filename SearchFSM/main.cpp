#line 2 "main.cpp" // Make __FILE__ omit the path

#include <QCoreApplication>

#include "SearchFsm.h"
#include "FsmCreator.h"
#include "FsmTest.h"

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

QString FileSizeToString(unsigned int dwSize) {
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
	Print(QString("FSM table: %1\n").arg(FileSizeToString(size.dwFsmTableSize)));
	Print(QString("Output table: %1\n").arg(FileSizeToString(size.dwOutputTableSize)));
	Print(QString("Total size: %1\n").arg(FileSizeToString(size.dwTotalSize)));
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

int main(int argc, char *argv[]) {
	QCoreApplication a(argc, argv);

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

	CFsmTest tester;
	printf("\nSearch FSM for patterns:\n");
	PrintPatterns(patterns);
	tester.CreateFsm(patterns);
	PrintFsmStats(tester);

	printf("\nTracing the FSM:\n");
	tester.TraceFsm(70);

	return 0;
}
