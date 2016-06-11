#include <QCoreApplication>

#include "FsmCreator.h"

int main(int argc, char *argv[]) {
	QCoreApplication a(argc, argv);

	SPattern pat1;
	pat1.data << 0x1f;
//	pat1.mask << 0x1f;
	pat1.nLength = 5;
	pat1.nMaxErrors = 2;

	SPattern pat2;
	pat2.data << 0x0a;
//	pat2.mask << 0x1f;
	pat2.nLength = 5;
	pat2.nMaxErrors = 2;

	TPatterns patterns;
	patterns << pat1 << pat2;

	CFsmCreator fsm(patterns);
	fsm.GenerateTables();

	return 0;
}
