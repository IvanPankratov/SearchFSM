#include <stdlib.h>

#include "FsmTest.h"

unsigned char RandomByte() {
	return rand();
}

CFsmTest::SWrapFsm CFsmTest::CreateFsm(const CFsmCreator &fsmCreator) {
	// convert tables to format acceptable by CSearchFsm
	QVector<TSearchFsm::STableRow> rows;
	QVector<TSearchFsm::SOutput> outputs;
	int nRow;
	for (nRow = 0; nRow < fsmCreator.GetStatesCount(); nRow++) {
		STableRow row = fsmCreator.GetTableRow(nRow);
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

	// create the FSM itself and wrap
	SWrapFsm fsm = {TSearchFsm(table), rows, outputs};

	return fsm;
}

bool CFsmTest::TestFsm(TSearchFsm fsm) {
	fsm.Reset();
	int nDataLength = 30;
	int idx;
	for (idx = 0; idx < nDataLength; idx++) {
		int nState = fsm.GetState();
		unsigned char bBit = RandomByte() & 0x01;
		int nOut = fsm.PushBit(bBit);
		printf("%i =%i=> %i\n", nState, bBit, fsm.GetState());
		while (nOut != TSearchFsm::sm_outputNull) {
			// stub for output processing
			break;
		}
	}

	return true;
}
