#include <QCoreApplication>

#include "SearchFsm.h"
#include "FsmCreator.h"
#include "FsmTest.h"

void Print(const SPattern &pattern) {
	QString sPattern = PatternToString(pattern);
	sPattern = QString("%1, %2 errors").arg(sPattern).arg(pattern.nMaxErrors);
	puts(sPattern.toLocal8Bit().constData());
}

void PrintPatterns(const TPatterns &patterns) {
	int idx;
	for (idx = 0; idx < patterns.count(); idx++) {
		Print(patterns[idx]);
	}
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
	PrintPatterns(patterns);
	tester.CreateFsm(patterns);
	tester.TestFsm(50);

	return 0;
}
