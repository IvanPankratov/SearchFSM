#include <QCoreApplication>

#include "FsmCreator.h"

int main(int argc, char *argv[]) {
	QCoreApplication a(argc, argv);

	SPattern pat1;
	pat1.data << 0xff;
//	pat1.mask << 0x1f;
	pat1.nLength = 8;
	pat1.nMaxErrors = 2;

	SPattern pat2;
	pat2.data << 0xaa;
//	pat2.mask << 0x1f;
	pat2.nLength = 8;
	pat2.nMaxErrors = 2;

	SPattern patTwoParts; // 1011****1101
	patTwoParts.data << 0x0d << 0x0b;
	patTwoParts.mask << 0x0f << 0x0f;
	patTwoParts.nLength = 12;
	patTwoParts.nMaxErrors = 1;

	TPatterns patterns;
	patterns /*<< patTwoParts*/ << pat1 << pat2;

	CFsmCreator fsm(patterns);
	fsm.GenerateTables();

	return 0;
}
