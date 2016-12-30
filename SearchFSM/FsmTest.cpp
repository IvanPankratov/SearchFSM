#include <stdlib.h>

#include "FsmTest.h"

unsigned char RandomByte() {
	return rand();
}

CFsmTest::SWrapFsm CFsmTest::CreateFsm(const CFsmCreator &fsmCreator) {
	// convert tables to format acceptable by CSearchFsm
	TFsmTable rows;
	TOutputTable outputs;
	int nRow;
	for (nRow = 0; nRow < fsmCreator.GetStatesCount(); nRow++) {
		STableRow row = fsmCreator.GetTableRow(nRow);
		TSearchFsm::STableCell cell0;
		cell0.idxNextState = row.cell0.nNextState;
		TOutputIdx idxOutput0 = StoreOutputList(row.cell0.output, &outputs);
		cell0.idxOutput = idxOutput0;

		TSearchFsm::STableCell cell1;
		cell1.idxNextState = row.cell1.nNextState;
		TOutputIdx idxOutput1 = StoreOutputList(row.cell1.output, &outputs);
		cell1.idxOutput = idxOutput1;

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
	int nDataLength = 50;
	int idx;
	for (idx = 0; idx < nDataLength; idx++) {
		int nState = fsm.GetState();
		unsigned char bBit = RandomByte() & 0x01;
		int nOut = fsm.PushBit(bBit);
		printf("%i =%i=> %i", nState, bBit, fsm.GetState());
		if (nOut != TSearchFsm::sm_outputNull) {
			printf(" {");
			const TSearchFsm::SOutput &out = fsm.GetOutput(nOut);
			printf("(%i, %i, %i)", out.bPatternIdx, out.bErrors, out.bPosition);
			nOut = out.idxNextOutput;
			while (nOut != TSearchFsm::sm_outputNull) {
				const TSearchFsm::SOutput &out = fsm.GetOutput(nOut);
				printf(", (%i, %i, %i)", out.bPatternIdx, out.bErrors, out.bPosition);
				nOut = out.idxNextOutput;
			}
			printf("}");
		}
		printf("\n");
	}

	return true;
}

CFsmTest::TOutputIdx CFsmTest::StoreOutputList(const TOutputList &outputList, CFsmTest::TOutputTable *pOutputTable) {
	if (outputList.isEmpty()) {
		return TSearchFsm::sm_outputNull;
	}

	int idx;
	TOutputIdx idxNext = TSearchFsm::sm_outputNull;
	for (idx = outputList.count() - 1; idx >= 0; idx--) {
		SOutput out = outputList[idx];
		TSearchFsm::SOutput outputNew;
		outputNew.bPatternIdx = out.nPatternIdx;
		outputNew.bErrors = out.nErrors;
		outputNew.bPosition = out.nPosition;
		outputNew.idxNextOutput = idxNext;
		idxNext = StoreOutput(outputNew, pOutputTable);
	}

	return idxNext;
}

CFsmTest::TOutputIdx CFsmTest::StoreOutput(const TSearchFsm::SOutput &output, CFsmTest::TOutputTable *pOutputTable) {
	int idx;
	for (idx = 0; idx < pOutputTable->count(); idx++) {
		if (IsEqual(output, pOutputTable->at(idx))) {
			return idx;
		}
	}

	pOutputTable->push_back(output);
	return pOutputTable->count() - 1;
}

bool CFsmTest::IsEqual(const TSearchFsm::SOutput &output1, const TSearchFsm::SOutput &output2) {
	return (output1.bPatternIdx == output2.bPatternIdx) && (output1.bErrors == output2.bErrors) &&
		(output1.bPosition == output2.bPosition) && (output1.idxNextOutput == output2.idxNextOutput);
}
