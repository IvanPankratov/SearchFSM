#include <QCoreApplication>
#include <QVector>

#include "FsmCreator.h"
#include "SearchFsm.h"

void Print(const SPattern &pattern) {
	puts(PatternToString(pattern).toLocal8Bit().constData());
}

int main(int argc, char *argv[]) {
	QCoreApplication a(argc, argv);

	SPattern pat1;
	pat1.data << 0x1e;
//	pat1.mask << 0x1f;
	pat1.nLength = 5;
	pat1.nMaxErrors = 2;

	SPattern pat2;
	pat2.data << 0xaa;
//	pat2.mask << 0x1f;
	pat2.nLength = 8;
	pat2.nMaxErrors = 2;

	SPattern patTwoParts; // 1011----1101
	patTwoParts.data << 0xb0 << 0x0d;
	patTwoParts.mask << 0xf0 << 0x0f;
	patTwoParts.nLength = 12;
	patTwoParts.nMaxErrors = 1;

	Print(patTwoParts);
	Print(pat1);
	Print(pat2);

	TPatterns patterns;
	patterns /*<< patTwoParts*/ << pat1 << pat2;

	CFsmCreator fsm(patterns);
	fsm.GenerateTables();

	// try to create tables acceptable by CSearchFsm
	typedef CSearchFsm<unsigned int, unsigned int> TSearchFsm;
	QVector<TSearchFsm::STableRow> rows;
	QVector<TSearchFsm::SOutput> outputs;
	int nRow;
	for (nRow = 0; nRow < fsm.GetStatesCount(); nRow++) {
		STableRow row = fsm.GetTableRow(nRow);
		TSearchFsm::STableCell cell0;
		cell0.idxNextState = row.cell0.nNextState;
		cell0.idxOutput = TSearchFsm::sm_outputNull; // no output yet

		TSearchFsm::STableCell cell1;
		cell1.idxNextState = row.cell1.nNextState;
		cell1.idxOutput = TSearchFsm::sm_outputNull; // no output yet

		TSearchFsm::STableRow fsmRow = {cell0, cell1};
		rows.append(fsmRow);
	}
	TSearchFsm::STable table = {rows.constData(), outputs.constData(), rows.count(), outputs.count()};
	TSearchFsm searchFsm(table);
	searchFsm.PushBit(0);

	return 0;
}
