#line 2 "FsmTest.cpp" // Make __FILE__ omit the path

#include <stdlib.h>

#include "FsmTest.h"

CFsmTest::CFsmTest() {
	m_pFsm = NULL;
	ResetLCG();
}

CFsmTest::~CFsmTest() {
	ReleaseFsm();
}

bool CFsmTest::CreateFsm(const TPatterns &patterns, bool fVerbose) {
	ReleaseFsm();

	// store the patterns for the future
	m_patterns = patterns;

	// generate tables
	CFsmCreator fsm(patterns);
	fsm.GenerateTables(fVerbose);

	// convert tables to format acceptable by CSearchFsm
	TFsmTable rows;
	rows.reserve(fsm.GetStatesCount());
	TOutputTable outputs;
	int nRow;
	for (nRow = 0; nRow < fsm.GetStatesCount(); nRow++) {
		CFsmCreator::STableRow row = fsm.GetTableRow(nRow);
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

	// release redundant memory
	ASSERT(rows.capacity() == rows.count());
	outputs.squeeze();

	// store the tables (with const_cast to disable write-protection)
	const_cast<TFsmTable&>(m_rows) = rows;
	const_cast<TOutputTable&>(m_outputs) = outputs;

	// create the table describing structure
	unsigned int dwRows = rows.count();
	unsigned int dwOutputs = outputs.count();
	TSearchFsm::STable table = {rows.constData(), outputs.constData(), dwRows, dwOutputs};

	// create the FSM itself
	m_pFsm = new TSearchFsm(table);

	return true;
}

void CFsmTest::DumpFinding(int nBitsProcessed, const TSearchFsm::SOutput &out) {
	int nPosition = nBitsProcessed - out.stepBack;
	if (out.errorsCount == 0) {
		printf("#%i at %i", out.patternIdx, nPosition);
	} else {
		printf("#%i at %i (%i errors)", out.patternIdx, nPosition, out.errorsCount);
	}

	if (out.idxNextOutput != TSearchFsm::sm_outputNull) {
		printf(", ");
	}
}

bool CFsmTest::TraceFsm(int nDataLength) {
	if (m_pFsm == NULL) {
		return false;
	}

	m_pFsm->Reset();
	ResetLCG();

	unsigned char bData;
	int idx;
	for (idx = 0; idx < nDataLength; idx++) {
		int nState = m_pFsm->GetState();
		int nBit = idx % 8;
		if (nBit == 0) { // new byte begins => update bData
			bData = RandomByte();
//			printf("0x%02x ", bData); // debug dumping
		}
		unsigned char bBit = GetHiBit(bData, nBit);
		unsigned int nOut = m_pFsm->PushBit(bBit);
		printf("%i: %i =%i=> %i", idx, nState, bBit, m_pFsm->GetState());
		if (nOut != TSearchFsm::sm_outputNull) {
			printf(" found ");
			while (nOut != TSearchFsm::sm_outputNull) {
				const TSearchFsm::SOutput &out = m_pFsm->GetOutput(nOut);
				DumpFinding(idx + 1, out);
				nOut = out.idxNextOutput;
			}
		}
		printf("\n");
	}

	return true;
}

void CFsmTest::ReleaseFsm() {
	if (m_pFsm != NULL) {
		delete m_pFsm;
		m_pFsm = NULL;
	}
}

// table size quering methods
CFsmTest::STableSize CFsmTest::GetTableSize() const {
	STableSize size;
	size.dwFsmTableSize = m_rows.count() * sizeof(TFsmTable::value_type);
	size.dwOutputTableSize = m_outputs.count() * sizeof(TOutputTable::value_type);
	size.dwTotalSize = size.dwFsmTableSize + size.dwOutputTableSize + sizeof(TSearchFsm::STable);

	return size;
}

CFsmTest::STableSize CFsmTest::GetMinimalTableSize() const {
	STableSize size;

//	struct STableCell {
//		TStateIdx idxNextState;
//		TOutputIdx idxOutput;
//	};
	unsigned int dwStateIndexSize = GetMinimalDataSize(m_rows.count() - 1);
	// mustn't forget about CSearchFsm::sm_outputNull
	unsigned int dwOutputIndexSize = GetMinimalDataSize(m_outputs.count());
	// table cell contains next state index and output index, and
	unsigned int dwTableCellSize = dwStateIndexSize + dwOutputIndexSize;
	unsigned int dwRowSize = dwTableCellSize * 2;
	size.dwFsmTableSize = m_rows.count() * dwRowSize;

//	struct SOutput {
//		TPatternIdx patternIdx;
//		TStepBack stepBack;
//		TErrorsCount errorsCount;
//		TOutputIdx idxNextOutput;
//	};
	unsigned int dwPatternIdxSize = GetMinimalDataSize(m_patterns.count() - 1);
	int nMaxPatternLength = 0;
	int nMaxErrorsCount = 0;
	int idx;
	for (idx = 0; idx < m_patterns.count(); idx++) {
		int nLength = m_patterns[idx].nLength;
		if (nMaxPatternLength < nLength) {
			nMaxPatternLength = nLength;
		}

		int nErrorsCount = m_patterns[idx].nMaxErrors;
		if (nMaxErrorsCount < nErrorsCount) {
			nMaxErrorsCount = nErrorsCount;
		}
	}
	unsigned int dwStepBackSize = GetMinimalDataSize(nMaxPatternLength);
	unsigned int dwErrorsSize = GetMinimalDataSize(nMaxErrorsCount);
	unsigned int dwOutputCellSize = dwPatternIdxSize + dwStepBackSize + dwErrorsSize + dwOutputIndexSize;

	size.dwOutputTableSize = m_outputs.count() * dwOutputCellSize;
	size.dwTotalSize = size.dwFsmTableSize + size.dwOutputTableSize + sizeof(TSearchFsm::STable);

	return size;
}

unsigned int CFsmTest::GetMinimalDataSize(unsigned int nMaxValue) {
	if (nMaxValue <= 0xff) { // single-byte integer is enough
		return 1;
	} else if (nMaxValue <= 0xffff) { // two-byte integer is enough
		return 2;
	} else if (nMaxValue <= 0xffffffff){ // four-byte integer is enough
		return 4;
	} else {
		return 8;
	}
}

// private
// pseudo-random related members
unsigned int CFsmTest::NextRandomEntity() {
	// constants for LCG (stated to be good ones)
	const unsigned int s_dwLcgConstA = 214013;
	const unsigned int s_dwLcgConstC = 2531011;

	// switch to the next member
	m_dwRandomSeed = s_dwLcgConstA * m_dwRandomSeed + s_dwLcgConstC;
	return m_dwRandomSeed;
}

unsigned int CFsmTest::NextRandom15Bits() {
	const unsigned int s_dw15BitsMask = 0x00001fff;

	// take bits 15-29 which are said to be the best distributed
	return (NextRandomEntity() >> 15) & s_dw15BitsMask;
}

unsigned char CFsmTest::RandomByte() {
	// take only 8 bits
	return NextRandom15Bits();
}

void CFsmTest::ResetLCG() {
	m_dwRandomSeed = 0;
}

// output table handling
CFsmTest::TOutputIdx CFsmTest::StoreOutputList(const CFsmCreator::TOutputList &outputList, CFsmTest::TOutputTable *pOutputTable) {
	if (outputList.isEmpty()) {
		return TSearchFsm::sm_outputNull;
	}

	int idx;
	TOutputIdx idxNext = TSearchFsm::sm_outputNull;
	for (idx = outputList.count() - 1; idx >= 0; idx--) {
		CFsmCreator::SOutput out = outputList[idx];
		TSearchFsm::SOutput outputNew;
		outputNew.patternIdx = out.nPatternIdx;
		outputNew.errorsCount = out.nErrors;
		outputNew.stepBack = out.nStepBack;
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
	return (output1.patternIdx == output2.patternIdx) && (output1.errorsCount == output2.errorsCount) &&
		(output1.stepBack == output2.stepBack) && (output1.idxNextOutput == output2.idxNextOutput);
}
