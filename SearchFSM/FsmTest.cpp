#line 2 "FsmTest.cpp" // Make __FILE__ omit the path

#include <stdlib.h>

#include "FsmTest.h"
#include "ShiftRegister.h"
#include "WinTimer.h"

class CLcg { // linear congruent generator, produces period 256 MiB
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
		// construct 8 bits-value from 15 bits
		unsigned int dwValue = NextRandom15Bits();
		return (dwValue & 0xff) + (dwValue >> 8);
	}

	void Reset(unsigned int dwSeed = 0) {
		m_dwSeed = dwSeed;
	}

	unsigned int GetSeed() const {
		return m_dwSeed;
	}

private:
	unsigned int m_dwSeed; // pseudo-random sequence entity
};

class CDoubleLcg {
public:
	CDoubleLcg() {
		Reset();
	}

public:
	unsigned char RandomByte() {
		unsigned char bValue = m_lcg1.RandomByte() ^ m_bLcg2Value;
		unsigned int dwSeed28Bits = m_lcg1.GetSeed() & 0x0fffffff;
		if (dwSeed28Bits == 0) { // LCG-1 made a full cycle
			puts("update LCG-2");
			m_bLcg2Value = m_lcg2.RandomByte();
		}
		return bValue;
	}

	void Reset() {
		m_lcg1.Reset();
		m_lcg2.Reset();
		m_bLcg2Value = 0;
	}

private:
	CLcg m_lcg1, m_lcg2;
	unsigned char m_bLcg2Value;
};

CFsmTest::CFsmTest() {
	m_nMaxPatternLength = 0;
	m_nMaxErrorsCount = 0;
	m_dwCollisions = 0;
	m_dwFsmOutputsCount = 0;
	m_pFsm = NULL;
	m_pFsmNibble = NULL;
	m_pFsmByte = NULL;
}

CFsmTest::~CFsmTest() {
	ReleaseFsm();
}

bool CFsmTest::CreateFsm(const TPatterns &patterns, bool fCreateNibbleFsm, bool fCreateByteFsm, bool fVerbose) {
	ReleaseFsm();

	// store the patterns for the future
	m_patterns = patterns;
	AnalysePatterns();

	// generate tables
	CFsmCreator fsm(patterns);
	fsm.GenerateTables(fVerbose);
	m_dwCollisions = fsm.GetCollisionsCount();

	// convert tables to format acceptable by SearchFSMs
	TFsmTable rows(fsm.GetStatesCount());
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
		rows[nRow] = fsmRow;
	}

	// remember bit SearchFSM outputs count
	m_dwFsmOutputsCount = outputs.count();

	// create nibble SearchFSM
	TFsmNibbleTable rowsNibble;
	if (fCreateNibbleFsm) {
		rowsNibble.resize(fsm.GetStatesCount());
		CFsmCreator::TByteTable fsmNibble = fsm.CreateByteTable(g_nNibbleLength);
		for (nRow = 0; nRow < fsm.GetStatesCount(); nRow++) {
			const CFsmCreator::SByteTableRow &rowNibble = fsmNibble.rows[nRow];
			int nColumn;
			for (nColumn = 0; nColumn < rowNibble.cells.count(); nColumn++) {
				TSearchFsm::STableCell *pCell = &rowsNibble[nRow].cells[nColumn];
				CFsmCreator::STableCell &cell = fsmNibble.rows[nRow].cells[nColumn];
				pCell->idxNextState = cell.nNextState;
				pCell->idxOutput = StoreOutputList(cell.output, &outputs);
			}
		}
	}
	m_dwFsmNibbleOutputsCount = outputs.count();

	// create byte SearchFSM
	TFsmByteTable rowsByte;
	if (fCreateByteFsm) {
		rowsByte.resize(fsm.GetStatesCount());
		CFsmCreator::TByteTable fsmByte = fsm.CreateByteTable(g_nByteLength);
		for (nRow = 0; nRow < fsm.GetStatesCount(); nRow++) {
			const CFsmCreator::SByteTableRow &rowByte = fsmByte.rows[nRow];
			int nColumn;
			for (nColumn = 0; nColumn < rowByte.cells.count(); nColumn++) {
				TSearchFsm::STableCell *pCell = &rowsByte[nRow].cells[nColumn];
				CFsmCreator::STableCell &cell = fsmByte.rows[nRow].cells[nColumn];
				pCell->idxNextState = cell.nNextState;
				pCell->idxOutput = StoreOutputList(cell.output, &outputs);
			}
		}
	}

	// release redundant memory
	ASSERT(rows.capacity() == rows.count());
	outputs.squeeze();

	// store the tables (with const_cast to disable write-protection)
	const_cast<TFsmTable&>(m_rows) = rows;
	const_cast<TFsmNibbleTable&>(m_rowsNibble) = rowsNibble;
	const_cast<TFsmByteTable&>(m_rowsByte) = rowsByte;
	const_cast<TOutputTable&>(m_outputs) = outputs;

	// create the structure describing bit SearchFSM table
	unsigned int dwRows = rows.count();
	unsigned int dwOutputs = outputs.count();
	TSearchFsm::STable table = {m_rows.constData(), m_outputs.constData(), dwRows, m_dwFsmOutputsCount};
	m_pFsm = new TSearchFsm(table);

	// the same for Nibble SearchFSM and byte SearchFSM if requested
	if (fCreateNibbleFsm) {
		TSearchFsmNibble::STable tableNibble = {m_rowsNibble.constData(), m_outputs.constData(), dwRows, m_dwFsmNibbleOutputsCount};
		m_pFsmNibble = new TSearchFsmNibble(tableNibble);
	}
	if (fCreateByteFsm) {
		TSearchFsmByte::STable tableByte = {m_rowsByte.constData(), m_outputs.constData(), dwRows, dwOutputs};
		m_pFsmByte = new TSearchFsmByte(tableByte);
	}

	return true;
}

unsigned int CFsmTest::GetCollisionsCount() const {
	return m_dwCollisions;
}

bool CFsmTest::TraceFsm(int nDataLength) {
	if (m_pFsm == NULL) {
		return false;
	}

	m_pFsm->Reset();
	CDoubleLcg lcg;

	unsigned char bData = 0;
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

bool CFsmTest::TestCorrectness(unsigned int dwTestBytesCount, int nPrintHits, unsigned int *pdwHits) {
	// prepare shift register and test patterns
	CShiftRegister testRegister(m_nMaxPatternLength);
	QList<CShiftRegister::SPattern> registerPatterns;
	int idx;
	for (idx = 0; idx < m_patterns.count(); idx++) {
		registerPatterns.append(testRegister.ConvertPattern(m_patterns[idx]));
	}

	// start test
	m_pFsm->Reset();
	CDoubleLcg lcg;

	bool fCorrect = true;
	unsigned int cBit = 0;
	unsigned int cHits = 0;
	int cPrinted = 0;
	unsigned int dwByte;
	for (dwByte = 0; dwByte < dwTestBytesCount; dwByte++) {
		unsigned char bData = lcg.RandomByte();
		TFindingsList regFindings, fsmFindings, fsmNibbleFindings, fsmByteFindings;

		int nBit;
		for (nBit = 0; nBit < BITS_IN_BYTE; nBit++) {
			// obtain next bit
			unsigned char bBit = GetHiBit(bData, nBit);
			cBit++;

			// process bit by the registers
			testRegister.PushBit(bBit);
			for (idx = 0; idx <registerPatterns.count(); idx++) {
				int nErrors = testRegister.TestPattern(registerPatterns[idx]);
				int nMaxErrors = m_patterns[idx].nMaxErrors;
				if (nErrors <= nMaxErrors && cBit >= (unsigned int)m_patterns[idx].nLength) {
					SFinding finding;
					finding.nPatternIdx = idx;
					finding.nErrors = nErrors;
					unsigned int dwPosition = cBit - m_patterns[idx].nLength;
					finding.dwPosition = dwPosition;
					regFindings << finding;
				}
			}

			// process bit by bit SearchFSM
			fsmFindings << ProcessBitByFsm(cBit, bBit);
		}

		// compare search results
		if (!AreEqual(fsmFindings, regFindings)) {
			puts("FAIL! Bit SearchFSM != Test register!");
//			printf("FAIL! At %i, pattern #%i, FSM = %i errors, register = %i errors)\n", dwPosition, idx, nErrorsFsm, nErrors);
			fCorrect = false;
		}

		// process byte by nibble SearchFSM
		if (m_pFsmNibble != NULL) {
			fsmNibbleFindings << ProcessNibbleByFsm(cBit - g_nNibbleLength, HiNibble(bData));
			fsmNibbleFindings << ProcessNibbleByFsm(cBit, LoNibble(bData));

			if (!AreEqual(fsmFindings, fsmNibbleFindings)) {
				puts("FAIL! Bit SearchFSM != Nibble SearchFSM!");
				fCorrect = false;
			}
		}


		// process byte by byte SearchFSM
		if (m_pFsmByte != NULL) {
			fsmByteFindings = ProcessByteByFsm(cBit, bData);

			if (!AreEqual(fsmFindings, fsmByteFindings)) {
				puts("FAIL! Bit SearchFSM != Byte SearchFSM!");
				fCorrect = false;
			}
		}

		cHits += regFindings.count();
		while (cPrinted < nPrintHits && !regFindings.isEmpty()) {
			SFinding finding = regFindings.takeFirst();
			printf("Found #%i at %i with %i errors\n", finding.nPatternIdx, finding.dwPosition, finding.nErrors);
			cPrinted++;
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
	unsigned int cHits = 0;
	unsigned int dwByteIdx;
	for (dwByteIdx = 0; dwByteIdx < dwTestBytesCount; dwByteIdx++) {
		unsigned char bData = lcg.RandomByte();
		int nBit;
		for (nBit = 0; nBit < BITS_IN_BYTE; nBit++) {
			// obtain next bit
			unsigned char bBit = GetHiBit(bData, nBit);

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

long double CFsmTest::TestFsmNibbleRate(unsigned int dwTestBytesCount, unsigned int *pdwHits) {
	m_pFsmNibble->Reset();
	CLcg lcg;

	// start test
	CWinTimer timer;
	unsigned int cHits = 0;
	unsigned int dwByteIdx;
	for (dwByteIdx = 0; dwByteIdx < dwTestBytesCount; dwByteIdx++) {
		unsigned char bData = lcg.RandomByte();

		// process the two nibbles of the byte
		unsigned int dwOut = m_pFsmNibble->PushByte(HiNibble(bData));
		while (dwOut != TSearchFsmNibble::sm_outputNull) {
			cHits++;
			const TSearchFsmNibble::TOutput &out = m_pFsmNibble->GetOutput(dwOut);
			dwOut = out.idxNextOutput;
		}
		dwOut = m_pFsmNibble->PushByte(LoNibble(bData));
		while (dwOut != TSearchFsmNibble::sm_outputNull) {
			cHits++;
			const TSearchFsmNibble::TOutput &out = m_pFsmNibble->GetOutput(dwOut);
			dwOut = out.idxNextOutput;
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

long double CFsmTest::TestFsmByteRate(unsigned int dwTestBytesCount, unsigned int *pdwHits) {
	m_pFsmByte->Reset();
	CLcg lcg;

	// start test
	CWinTimer timer;
	unsigned int cHits = 0;
	unsigned int dwByteIdx;
	for (dwByteIdx = 0; dwByteIdx < dwTestBytesCount; dwByteIdx++) {
		unsigned char bData = lcg.RandomByte();

		// process the two nibbles of the byte
		unsigned int dwOut = m_pFsmByte->PushByte(bData);
		while (dwOut != TSearchFsmByte::sm_outputNull) {
			cHits++;
			const TSearchFsmByte::TOutput &out = m_pFsmByte->GetOutput(dwOut);
			dwOut = out.idxNextOutput;
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

	if (m_pFsmNibble != NULL) {
		delete m_pFsmNibble;
		m_pFsmNibble = NULL;
	}

	if (m_pFsmByte != NULL) {
		delete m_pFsmByte;
		m_pFsmByte = NULL;
	}
}

// table size quering methods
CFsmTest::STableSize CFsmTest::GetTableSize() const {
	STableSize size;
	size.dwFsmTableSize = m_rows.count() * sizeof(TFsmTable::value_type);
	size.dwOutputTableSize = m_dwFsmOutputsCount * sizeof(TOutputTable::value_type);
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
	unsigned int dwOutputIndexSize = GetMinimalDataSize(m_dwFsmOutputsCount);
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

	size.dwOutputTableSize = m_dwFsmOutputsCount * dwOutputCellSize;
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

CFsmTest::TFindingsList CFsmTest::ProcessBitByFsm(unsigned int dwProcessedBits, unsigned char bBit) {
	TFindingsList findingsList;

	unsigned int dwOut = m_pFsm->PushBit(bBit);
	while (dwOut != TSearchFsm::sm_outputNull) {
		const TSearchFsm::SOutput &out = m_pFsm->GetOutput(dwOut);
		unsigned int dwPosition = dwProcessedBits - out.stepBack;
		SFinding finding;
		finding.nPatternIdx = out.patternIdx;
		finding.nErrors = out.errorsCount;
		finding.dwPosition = dwPosition;
		findingsList << finding;
		dwOut = out.idxNextOutput;
	}

	return findingsList;
}

CFsmTest::TFindingsList CFsmTest::ProcessNibbleByFsm(unsigned int dwProcessedBits, unsigned char bNibble) {
	TFindingsList findingsList;

	unsigned int dwOut = m_pFsmNibble->PushByte(bNibble);
	while (dwOut != TSearchFsmNibble::sm_outputNull) {
		const TSearchFsmNibble::TOutput &out = m_pFsmNibble->GetOutput(dwOut);
		unsigned int dwPosition = dwProcessedBits - out.stepBack;
		SFinding finding;
		finding.nPatternIdx = out.patternIdx;
		finding.nErrors = out.errorsCount;
		finding.dwPosition = dwPosition;
		findingsList << finding;
		dwOut = out.idxNextOutput;
	}

	return findingsList;
}

CFsmTest::TFindingsList CFsmTest::ProcessByteByFsm(unsigned int dwProcessedBits, unsigned char bData) {
	TFindingsList findingsList;

	unsigned int dwOut = m_pFsmByte->PushByte(bData);
	while (dwOut != TSearchFsmByte::sm_outputNull) {
		const TSearchFsmByte::TOutput &out = m_pFsmByte->GetOutput(dwOut);
		unsigned int dwPosition = dwProcessedBits - out.stepBack;
		SFinding finding;
		finding.nPatternIdx = out.patternIdx;
		finding.nErrors = out.errorsCount;
		finding.dwPosition = dwPosition;
		findingsList << finding;
		dwOut = out.idxNextOutput;
	}

	return findingsList;
}

bool CFsmTest::AreEqual(const TFindingsList &list1, const TFindingsList &list2) {
	if (list1.count() != list2.count()) {
		return false;
	}
	int idx, nCount = list1.count();
	for (idx = 0; idx < nCount; idx++) {
		if (!AreEqual(list1[idx], list2[idx])) {
			return false;
		}
	}

	return true;
}

bool CFsmTest::AreEqual(const SFinding &finding1, const SFinding &finding2) {
	return (finding1.nPatternIdx == finding2.nPatternIdx) &&
		(finding1.nErrors == finding2.nErrors) && (finding1.dwPosition == finding2.dwPosition);
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
