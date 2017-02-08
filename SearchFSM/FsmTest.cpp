#line 2 "FsmTest.cpp" // Make __FILE__ omit the path

#include <stdlib.h>

#include "FsmTest.h"
#include "ShiftRegister.h"
#include "WinTimer.h"

class CLcg { // linear congruent generator
public:
	CLcg() {
		Reset();
	}

public:
	unsigned int NextRandomEntity() {
		// constants for LCG (stated to be good ones)
		const unsigned int s_dwLcgConstA = 214013;
		const unsigned int s_dwLcgConstC = 2531011;

		// switch to the next member
		m_dwSeed = s_dwLcgConstA * m_dwSeed + s_dwLcgConstC;
		return m_dwSeed;
	}

	unsigned int NextRandom15Bits() {
		const unsigned int s_dw15BitsMask = 0x00001fff;

		// take bits 15-29 which are said to be the best distributed
		return (NextRandomEntity() >> 15) & s_dw15BitsMask;
	}

	unsigned char RandomByte() {
		// take only 8 bits
		return NextRandom15Bits();
	}

	void Reset() {
		m_dwSeed = 0;
	}

private:
	unsigned int m_dwSeed; // pseudo-random sequence entity
};

CFsmTest::CFsmTest() {
	m_nMaxPatternLength = 0;
	m_nMaxErrorsCount = 0;
	m_pFsm = NULL;
}

CFsmTest::~CFsmTest() {
	ReleaseFsm();
}

bool CFsmTest::CreateFsm(const TPatterns &patterns, bool fVerbose) {
	ReleaseFsm();

	// store the patterns for the future
	m_patterns = patterns;
	AnalysePatterns();

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

bool CFsmTest::TraceFsm(int nDataLength) {
	if (m_pFsm == NULL) {
		return false;
	}

	m_pFsm->Reset();
	CLcg lcg;

	unsigned char bData;
	int idx;
	for (idx = 0; idx < nDataLength; idx++) {
		int nState = m_pFsm->GetState();
		int nBit = idx % 8;
		if (nBit == 0) { // new byte begins => update bData
			bData = lcg.RandomByte();
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

bool CFsmTest::TestCorrectness(unsigned int dwTestBitsCount, unsigned int *pdwHits) {
	// prepare shift register and test patterns
	CShiftRegister testRegister(m_nMaxPatternLength);
	QList<CShiftRegister::SPattern> registerPatterns;
	int idx;
	for (idx = 0; idx < m_patterns.count(); idx++) {
		registerPatterns.append(testRegister.ConvertPattern(m_patterns[idx]));
	}

	// start test
	m_pFsm->Reset();
	CLcg lcg;

	bool fCorrect = true;
	unsigned int dwBit = 0;
	unsigned int cHits = 0;
	while (dwBit < dwTestBitsCount) {
		unsigned char bData = lcg.RandomByte();
		int nBit;
		for (nBit = 0; nBit < BITS_IN_BYTE && dwBit < dwTestBitsCount; nBit++) {
			// obtain next bit
			unsigned char bBit = GetHiBit(bData, nBit);
			dwBit++;

			// process bit by FSM
			QVector<int> nsPatternErrors;
			nsPatternErrors.fill(m_nMaxErrorsCount + 1, m_patterns.count());
			unsigned int dwOut = m_pFsm->PushBit(bBit);
			while (dwOut != TSearchFsm::sm_outputNull) {
				cHits++;
				const TSearchFsm::SOutput &out = m_pFsm->GetOutput(dwOut);
				nsPatternErrors[out.patternIdx] = out.errorsCount;
				dwOut = out.idxNextOutput;
			}

			// check FSM's output with the test register
			testRegister.PushBit(bBit);
			for (idx = 0; idx <registerPatterns.count(); idx++) {
				int nErrorsReg = testRegister.TestPattern(registerPatterns[idx]);
				int nErrorsFsm = nsPatternErrors[idx];
				int nMaxErrors = m_patterns[idx].nMaxErrors;
				if ((nErrorsReg <= nMaxErrors || nErrorsFsm <= nMaxErrors) && dwBit >= (unsigned int)m_patterns[idx].nLength) {
					if (nErrorsFsm != nErrorsReg) {
						unsigned int dwPosition = dwBit - m_patterns[idx].nLength;
						printf("FAIL! At %i, pattern #%i, FSM = %i errors, register = %i errors)\n", dwPosition, idx, nErrorsFsm, nErrorsReg);
						fCorrect = false;
					}
				}
			}
		}

	}

	if (pdwHits != NULL) {
		*pdwHits = cHits;
	}
	return fCorrect;
}

long double CFsmTest::TestFsmRate(unsigned int dwTestBytesCount, unsigned int *pdwHits) {
	m_pFsm->Reset();
	CLcg lcg;

	// start test
	CWinTimer timer;
	unsigned int dwBit = 0;
	unsigned int cHits = 0;
	unsigned int dwByteIdx;
	for (dwByteIdx = 0; dwByteIdx < dwTestBytesCount; dwByteIdx++) {
		unsigned char bData = lcg.RandomByte();
		int nBit;
		for (nBit = 0; nBit < BITS_IN_BYTE; nBit++) {
			// obtain next bit
			unsigned char bBit = GetHiBit(bData, nBit);
			dwBit++;

			// process bit by FSM
			unsigned int dwOut = m_pFsm->PushBit(bBit);
			while (dwOut != TSearchFsm::sm_outputNull) {
				cHits++;
				const TSearchFsm::SOutput &out = m_pFsm->GetOutput(dwOut);
				dwOut = out.idxNextOutput;
			}
		}
	}
	timer.Stop();
	CWinTimer::TTime tSeconds = timer.GetDuration();
	long double dRate = dwTestBytesCount / tSeconds;

	if (pdwHits != NULL) {
		*pdwHits = cHits;
	}
	return dRate;
}

long double CFsmTest::TestRegisterRate(unsigned int dwTestBytesCount, unsigned int *pdwHits) {
	// prepare shift register and test patterns
	CShiftRegister testRegister(m_nMaxPatternLength);
	QList<CShiftRegister::SPattern> registerPatterns;
	int idx;
	for (idx = 0; idx < m_patterns.count(); idx++) {
		registerPatterns.append(testRegister.ConvertPattern(m_patterns[idx]));
	}
	CLcg lcg;

	// start test
	CWinTimer timer;
	unsigned int dwBit = 0;
	unsigned int cHits = 0;
	unsigned int dwByteIdx;
	for (dwByteIdx = 0; dwByteIdx < dwTestBytesCount; dwByteIdx++) {
		unsigned char bData = lcg.RandomByte();
		int nBit;
		for (nBit = 0; nBit < BITS_IN_BYTE; nBit++) {
			// obtain next bit
			unsigned char bBit = GetHiBit(bData, nBit);
			dwBit++;

			// process bit with the register
			testRegister.PushBit(bBit);
			for (idx = 0; idx <registerPatterns.count(); idx++) {
				int nErrorsReg = testRegister.TestPattern(registerPatterns[idx]);
				int nMaxErrors = m_patterns[idx].nMaxErrors;
				unsigned int dwPatternLength = m_patterns[idx].nLength;
				if (nErrorsReg <= nMaxErrors && dwBit >= dwPatternLength) {
					cHits++;
				}
			}
		}
	}
	timer.Stop();
	CWinTimer::TTime tSeconds = timer.GetDuration();
	long double dRate = dwTestBytesCount / tSeconds;

	if (pdwHits != NULL) {
		*pdwHits = cHits;
	}
	return dRate;
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
	unsigned int dwStepBackSize = GetMinimalDataSize(m_nMaxPatternLength);
	unsigned int dwErrorsSize = GetMinimalDataSize(m_nMaxErrorsCount);
	unsigned int dwOutputCellSize = dwPatternIdxSize + dwStepBackSize + dwErrorsSize + dwOutputIndexSize;

	size.dwOutputTableSize = m_outputs.count() * dwOutputCellSize;
	size.dwTotalSize = size.dwFsmTableSize + size.dwOutputTableSize + sizeof(TSearchFsm::STable);

	return size;
}

// private
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

void CFsmTest::AnalysePatterns() {
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

	m_nMaxPatternLength = nMaxPatternLength;
	m_nMaxErrorsCount = nMaxErrorsCount;
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
