#include <QCoreApplication>

#include "FsmCreator.h"

int main(int argc, char *argv[]) {
	QCoreApplication a(argc, argv);

	SPattern pat1;
	pat1.data << 0x7;
	pat1.mask << 0x7;
	pat1.nLength = 3;
	pat1.nMaxErrors = 1;

	SPattern pat2;
	pat2.data << 0x5;
	pat2.mask << 0x7;
	pat2.nLength = 3;
	pat2.nMaxErrors = 1;

	TPatterns patterns;
	patterns << pat1 << pat2;

	CFsmCreator fsm(patterns);
	fsm.GenerateTables();

	return 0;
}
