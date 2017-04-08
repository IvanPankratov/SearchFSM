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
		const unsigned int s_dw15BitsMask = 0x00007fff;

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

CFsmTest::STimeings GetTimings(const CWinTimer &timer) {
	CFsmTest::STimeings results;
	results.dTotalTime = timer.GetTotalDuration();
	results.dCpuTime = timer.GetThreadDuration();
	results.dKernelTime = timer.GetKernelDuration();
	return results;
}

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
	TBitFsmTable rows(fsm.GetStatesCount());
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
	TNibbleFsmTable rowsNibble;
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
	TOctetFsmTable rowsByte;
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
	const_cast<TBitFsmTable&>(m_rows) = rows;
	const_cast<TNibbleFsmTable&>(m_rowsNibble) = rowsNibble;
	const_cast<TOctetFsmTable&>(m_rowsByte) = rowsByte;
	const_cast<TOutputTable&>(m_outputs) = outputs;

	// create the structure describing bit SearchFSM table
	unsigned int dwRows = rows.count();
	unsigned int dwOutputs = outputs.count();
	TSearchFsm::STable table = {m_rows.constData(), m_outputs.constData(), dwRows, m_dwFsmOutputsCount};
	m_pFsm = new TSearchFsm(table);

	// the same for Nibble SearchFSM and byte SearchFSM if requested
	if (fCreateNibbleFsm) {
		TNibbleSearchFsm::STable tableNibble = {m_rowsNibble.constData(), m_outputs.constData(), dwRows, m_dwFsmNibbleOutputsCount};
		m_pFsmNibble = new TNibbleSearchFsm(tableNibble);
	}
	if (fCreateByteFsm) {
		TOctetSearchFsm::STable tableByte = {m_rowsByte.constData(), m_outputs.constData(), dwRows, dwOutputs};
		m_pFsmByte = new TOctetSearchFsm(tableByte);
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
			for (idx = 0; idx < registerPatterns.count(); idx++) {
				unsigned int dwErrors;
				if (testRegister.TestPattern(registerPatterns[idx], &dwErrors) && cBit >= (unsigned int)m_patterns[idx].nLength) {
					SFinding finding;
					finding.nPatternIdx = idx;
					finding.nErrors = dwErrors;
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

CFsmTest::SEnginePerformance CFsmTest::TestFsmRate(unsigned int dwTestBytesCount, unsigned int *pdwHits) {
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
	CWinTimer::TTime tSeconds = timer.GetTotalDuration();
	SEnginePerformance speed;
	speed.dRate = dwTestBytesCount / tSeconds;
	speed.dCpuUsage = timer.GetThreadDuration() / tSeconds;
	speed.dCpuKernelUsage = timer.GetKernelDuration() / tSeconds;

	if (pdwHits != NULL) {
		*pdwHits = cHits;
	}
	return speed;
}

bool CFsmTest::TestBitFsmRate(unsigned int dwTestBytesCount, CFsmTest::SEnginePerformance *pResult) {
	return TestEngine<CBitFsmSearch>(dwTestBytesCount, pResult);
}

bool CFsmTest::TestNibbleFsmRate(unsigned int dwTestBytesCount, CFsmTest::SEnginePerformance *pResult) {
	return TestEngine<CNibbleFsmSearch>(dwTestBytesCount, pResult);
}

CFsmTest::SEnginePerformance CFsmTest::TestFsmByteRate(unsigned int dwTestBytesCount, unsigned int *pdwHits) {
	if (m_pFsmByte == NULL) {
		if (pdwHits != NULL) {
			*pdwHits = 0;
		}
		SEnginePerformance performance = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		return performance;
	}

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
		while (dwOut != TOctetSearchFsm::sm_outputNull) {
			cHits++;
			const TOctetSearchFsm::TOutput &out = m_pFsmByte->GetOutput(dwOut);
			dwOut = out.idxNextOutput;
		}
	}
	timer.Stop();
	CWinTimer::TTime tSeconds = timer.GetTotalDuration();
	SEnginePerformance speed;
	speed.dRate = dwTestBytesCount / tSeconds;
	speed.dCpuUsage = timer.GetThreadDuration() / tSeconds;
	speed.dCpuKernelUsage = timer.GetKernelDuration() / tSeconds;

	if (pdwHits != NULL) {
		*pdwHits = cHits;
	}
	return speed;
}

bool CFsmTest::TestOctetFsmRate(unsigned int dwTestBytesCount, CFsmTest::SEnginePerformance *pResult) {
	return TestEngine<COctetFsmSearch>(dwTestBytesCount, pResult);
}

CFsmTest::SEnginePerformance CFsmTest::TestRegisterRate(unsigned int dwTestBytesCount, unsigned int *pdwHits) {
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
			for (idx = 0; idx < registerPatterns.count(); idx++) {
				unsigned int dwPatternLength = m_patterns[idx].nLength;
				if (testRegister.TestPattern(registerPatterns[idx]) && dwBit >= dwPatternLength) {
					cHits++;
				}
			}
		}
	}
	timer.Stop();
	CWinTimer::TTime tSeconds = timer.GetTotalDuration();
	SEnginePerformance speed;
	speed.dRate = dwTestBytesCount / tSeconds;
	speed.dCpuUsage = timer.GetThreadDuration() / tSeconds;
	speed.dCpuKernelUsage = timer.GetKernelDuration() / tSeconds;

	if (pdwHits != NULL) {
		*pdwHits = cHits;
	}
	return speed;
}

bool CFsmTest::TestRegisterRate2(unsigned int dwTestBytesCount, CFsmTest::SEnginePerformance *pResult) {
	return TestEngine<CRegisterSearch>(dwTestBytesCount, pResult);
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

CFsmTest::SPatternsStats CFsmTest::AnalysePatterns(const TPatterns &patterns) {
	SPatternsStats result;
	result.nMaxLength = 0;
	result.nMaxErrorsCount = 0;
	int idx;
	for (idx = 0; idx < patterns.count(); idx++) {
		int nLength = patterns[idx].nLength;
		if (result.nMaxLength < nLength) {
			result.nMaxLength = nLength;
		}

		int nErrorsCount = patterns[idx].nMaxErrors;
		if (result.nMaxErrorsCount < nErrorsCount) {
			result.nMaxErrorsCount = nErrorsCount;
		}
	}

	return result;
}

// table size calculating methods
template <class TSearchFsm_>
CFsmTest::SFsmTableSize CFsmTest::GetTableSize(const CFsmCreator::SFsmWrap<TSearchFsm_> &wrap) {
	SFsmTableSize size;
	size.dwMainTableSize = wrap.m_rows.count() * sizeof(typename TSearchFsm_::STableRow);
	size.dwOutputTableSize = wrap.m_outputTable.count() * sizeof(typename TSearchFsm_::TOutput);
	size.dwTotalSize = size.dwMainTableSize + size.dwOutputTableSize + sizeof(typename TSearchFsm_::STable);

	return size;

}

template <class TSearchFsm_>
CFsmTest::SFsmTableSize CFsmTest::GetMinimalTableSize(const CFsmCreator::SFsmWrap<TSearchFsm_> &wrap) {
	SFsmTableSize size;
	unsigned int dwStatesCount = wrap.m_rows.count();
	unsigned int dwOutputsCount = wrap.m_outputTable.count();
	//	struct STableCell {
	//		TStateIdx idxNextState;
	//		TOutputIdx idxOutput;
	//	};
	unsigned int dwStateIndexSize = GetMinimalDataSize(dwStatesCount - 1);
	// mustn't forget about CSearchFsm::sm_outputNull
	unsigned int dwOutputIndexSize = GetMinimalDataSize(dwOutputsCount);
	// table cell contains next state index and output index, and
	unsigned int dwTableCellSize = dwStateIndexSize + dwOutputIndexSize;
	unsigned int dwRowSize = dwTableCellSize * TSearchFsm_::g_nColumnsCount;
	size.dwMainTableSize = dwStatesCount * dwRowSize;

	//	struct SOutput {
	//		TPatternIdx patternIdx;
	//		TStepBack stepBack;
	//		TErrorsCount errorsCount;
	//		TOutputIdx idxNextOutput;
	//	};
	unsigned int dwMaxPatternIdx = 0, dwMaxStepBack = 0, dwMaxErrors = 0;
	int idx;
	for (idx = 0; idx < wrap.m_outputTable.count(); idx++) {
		const typename TSearchFsm_::TOutput& out = wrap.m_outputTable[idx];
		if (dwMaxPatternIdx < out.patternIdx) {
			dwMaxPatternIdx = out.patternIdx;
		}
		if (dwMaxStepBack < out.stepBack) {
			dwMaxStepBack = out.stepBack;
		}
		if (dwMaxErrors < out.errorsCount) {
			dwMaxErrors = out.errorsCount;
		}
	}
	unsigned int dwPatternIdxSize = GetMinimalDataSize(dwMaxPatternIdx);
	unsigned int dwStepBackSize = GetMinimalDataSize(dwMaxStepBack);
	unsigned int dwErrorsSize = GetMinimalDataSize(dwMaxErrors);
	unsigned int dwOutputCellSize = dwPatternIdxSize + dwStepBackSize + dwErrorsSize + dwOutputIndexSize;

	size.dwOutputTableSize = dwOutputsCount * dwOutputCellSize;
	size.dwTotalSize = size.dwMainTableSize + size.dwOutputTableSize + sizeof(typename TSearchFsm_::STable);

	return size;
}

// table size quering methods
CFsmTest::SFsmTableSize CFsmTest::GetTableSize() const {
	SFsmTableSize size;
	size.dwMainTableSize = m_rows.count() * sizeof(TBitFsmTable::value_type);
	size.dwOutputTableSize = m_dwFsmOutputsCount * sizeof(TOutputTable::value_type);
	size.dwTotalSize = size.dwMainTableSize + size.dwOutputTableSize + sizeof(TSearchFsm::STable);

	return size;
}

CFsmTest::SFsmTableSize CFsmTest::GetMinimalTableSize() const {
	SFsmTableSize size;

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
	size.dwMainTableSize = m_rows.count() * dwRowSize;

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
	size.dwTotalSize = size.dwMainTableSize + size.dwOutputTableSize + sizeof(TSearchFsm::STable);

	return size;
}

CFsmTest::SFsmTableSize CFsmTest::GetNibbleTableSize() const {
	SFsmTableSize size;
	size.dwMainTableSize = m_rowsNibble.count() * sizeof(TNibbleFsmTable::value_type);
	size.dwOutputTableSize = m_dwFsmNibbleOutputsCount * sizeof(TOutputTable::value_type);
	size.dwTotalSize = size.dwMainTableSize + size.dwOutputTableSize + sizeof(TNibbleSearchFsm::STable);

	return size;
}

CFsmTest::SFsmTableSize CFsmTest::GetByteTableSize() const {
	SFsmTableSize size;
	size.dwMainTableSize = m_rowsByte.count() * sizeof(TOctetFsmTable::value_type);
	size.dwOutputTableSize = m_outputs.count() * sizeof(TOutputTable::value_type);
	size.dwTotalSize = size.dwMainTableSize + size.dwOutputTableSize + sizeof(TOctetSearchFsm::STable);

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

template <class TSearchEngine>
bool CFsmTest::TestEngine(unsigned int dwTestBytesCount, SEnginePerformance *pResult) {
	try {
		// prepare engine
		CWinTimer timer;
		typename TSearchEngine::TSearchData searchData = TSearchEngine::InitEngine(m_patterns);
		timer.Stop();
		SEnginePerformance performance;
		performance.timInitialization = GetTimings(timer);
		performance.dwMemoryRequirements = TSearchEngine::GetMemoryRequirements(searchData);
		performance.fIsFsm = TSearchEngine::IsFsm();
		if (performance.fIsFsm) {
			performance.fsmStatistics = TSearchEngine::GetFsmStatistics(searchData);
		}

		// preparing the test
		unsigned int dwCheckLengthBytes = m_nMaxPatternLength / BITS_IN_BYTE + 1;
		if (dwTestBytesCount < dwCheckLengthBytes) {
			dwCheckLengthBytes = dwTestBytesCount;
		}

		// start test
		CLcg lcg;
		timer.Start();
		unsigned int cHits = 0;
		unsigned int dwBytes;
		for (dwBytes = 0; dwBytes < dwCheckLengthBytes; dwBytes++) {
			unsigned char bData = lcg.RandomByte();
			cHits += TSearchEngine::ProcessByteCheckLength(bData, &searchData);
		}
		for (; dwBytes < dwTestBytesCount; dwBytes++) {
			unsigned char bData = lcg.RandomByte();
			cHits += TSearchEngine::ProcessByte(bData, &searchData);
		}
		timer.Stop();
		performance.timOperating = GetTimings(timer);
		performance.dwBytesCount = dwTestBytesCount;
		performance.dwHits = cHits;

		// obsolete
		CWinTimer::TTime tSeconds = timer.GetTotalDuration();
		performance.dRate = dwTestBytesCount / tSeconds;
		performance.dCpuUsage = timer.GetThreadDuration() / tSeconds;
		performance.dCpuKernelUsage = timer.GetKernelDuration() / tSeconds;

		*pResult = performance;
	}
	catch(...) {
		return false;
	}

	return true;
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
	while (dwOut != TNibbleSearchFsm::sm_outputNull) {
		const TNibbleSearchFsm::TOutput &out = m_pFsmNibble->GetOutput(dwOut);
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
	while (dwOut != TOctetSearchFsm::sm_outputNull) {
		const TOctetSearchFsm::TOutput &out = m_pFsmByte->GetOutput(dwOut);
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

//////////////////////////////////////////////////////////////////////////////
// Search engine classes
//////////////////////////////////////////////////////////////////////////////

/// CFsmTest::CBitFsmSearch - search with a bit SearchFSM
class CFsmTest::CBitFsmSearch {
public: // data
	struct TSearchData {
		CFsmCreator::SFsmWrap<TSearchFsm> wrap;
		unsigned int dwBits;
	};

public: // initialization & statictics
	static TSearchData InitEngine(const TPatterns& patterns);
	static unsigned int GetMemoryRequirements(const TSearchData &data);
	static bool IsFsm() {return true;}
	static CFsmTest::SFsmStatistics GetFsmStatistics(const TSearchData &data);

public: // working methods
	static unsigned int ProcessByteCheckLength(const unsigned char bData, TSearchData *pSearchData);
	static unsigned int ProcessByte(const unsigned char bData, TSearchData *pSearchData);
};

CFsmTest::CBitFsmSearch::TSearchData CFsmTest::CBitFsmSearch::InitEngine(const TPatterns &patterns) {
	CFsmCreator fsm(patterns);
	fsm.GenerateTables();
	TSearchData data = {fsm.CreateFsmWrap<TSearchFsm>(), 0};
	data.wrap.fsm.Reset();

	return data;
}

unsigned int CFsmTest::CBitFsmSearch::GetMemoryRequirements(const CFsmTest::CBitFsmSearch::TSearchData &data) {
	return CFsmTest::GetTableSize(data.wrap).dwTotalSize;
}

CFsmTest::SFsmStatistics CFsmTest::CBitFsmSearch::GetFsmStatistics(const CFsmTest::CBitFsmSearch::TSearchData &data) {
	CFsmTest::SFsmStatistics stats;
	stats.dwStatesCount = data.wrap.m_rows.count();
	stats.tableSize = CFsmTest::GetTableSize(data.wrap);
	stats.tableMinSize = CFsmTest::GetMinimalTableSize(data.wrap);

	return stats;
}

unsigned int CFsmTest::CBitFsmSearch::ProcessByteCheckLength(const unsigned char bData, CFsmTest::CBitFsmSearch::TSearchData *pSearchData) {
	unsigned int cHits = 0;
	int nBit;
	for (nBit = 0; nBit < BITS_IN_BYTE; nBit++) {
		// obtain next bit
		unsigned char bBit = GetHiBit(bData, nBit);

		// process bit by FSM
		unsigned int dwOut = pSearchData->wrap.fsm.PushBit(bBit);
		pSearchData->dwBits++;
		while (dwOut != TSearchFsm::sm_outputNull) {
			const TSearchFsm::SOutput &out = pSearchData->wrap.fsm.GetOutput(dwOut);
			if (out.stepBack <= pSearchData->dwBits) { // enough data
				cHits++;
			}
			dwOut = out.idxNextOutput;
		}
	}

	return cHits;
}

unsigned int CFsmTest::CBitFsmSearch::ProcessByte(const unsigned char bData, CFsmTest::CBitFsmSearch::TSearchData *pSearchData) {
	unsigned int cHits = 0;
	int nBit;
	for (nBit = 0; nBit < BITS_IN_BYTE; nBit++) {
		// obtain next bit
		unsigned char bBit = GetHiBit(bData, nBit);

		// process bit by FSM
		unsigned int dwOut = pSearchData->wrap.fsm.PushBit(bBit);
		while (dwOut != TSearchFsm::sm_outputNull) {
			cHits++;
			const TSearchFsm::SOutput &out = pSearchData->wrap.fsm.GetOutput(dwOut);
			dwOut = out.idxNextOutput;
		}
	}

	return cHits;
}


/// CFsmTest::CNibbleFsmSearch - search with a 4-bit SearchFSM
class CFsmTest::CNibbleFsmSearch {
public: // data
	struct TSearchData {
		CFsmCreator::SFsmWrap<CFsmTest::TNibbleSearchFsm> wrap;
		unsigned int dwBits;
	};

public: // initialization & statictics
	static TSearchData InitEngine(const TPatterns& patterns);
	static unsigned int GetMemoryRequirements(const TSearchData &data);
	static bool IsFsm() {return true;}
	static CFsmTest::SFsmStatistics GetFsmStatistics(const TSearchData &data);

public: // working methods
	static unsigned int ProcessByteCheckLength(const unsigned char bData, TSearchData *pSearchData);
	static unsigned int ProcessByte(const unsigned char bData, TSearchData *pSearchData);
};

CFsmTest::CNibbleFsmSearch::TSearchData CFsmTest::CNibbleFsmSearch::InitEngine(const TPatterns &patterns) {
	CFsmCreator fsm(patterns);
	fsm.GenerateTables();
	TSearchData data = {fsm.CreateByteFsmWrap<TNibbleSearchFsm>(), 0};
	data.wrap.fsm.Reset();

	return data;
}

unsigned int CFsmTest::CNibbleFsmSearch::GetMemoryRequirements(const CFsmTest::CNibbleFsmSearch::TSearchData &data) {
	return CFsmTest::GetTableSize(data.wrap).dwTotalSize;
}

CFsmTest::SFsmStatistics CFsmTest::CNibbleFsmSearch::GetFsmStatistics(const CFsmTest::CNibbleFsmSearch::TSearchData &data) {
	CFsmTest::SFsmStatistics stats;
	stats.dwStatesCount = data.wrap.m_rows.count();
	stats.tableSize = CFsmTest::GetTableSize(data.wrap);
	stats.tableMinSize = CFsmTest::GetMinimalTableSize(data.wrap);

	return stats;
}

unsigned int CFsmTest::CNibbleFsmSearch::ProcessByteCheckLength(const unsigned char bData, CFsmTest::CNibbleFsmSearch::TSearchData *pSearchData) {
	// process the two nibbles of the byte
	unsigned int cHits = 0;

	// Higher nibble
	unsigned int dwOut = pSearchData->wrap.fsm.PushByte(HiNibble(bData));
	pSearchData->dwBits += g_nNibbleLength;
	while (dwOut != TNibbleSearchFsm::sm_outputNull) {
		const TSearchFsm::SOutput &out = pSearchData->wrap.fsm.GetOutput(dwOut);
		if (out.stepBack <= pSearchData->dwBits) { // enough data
			cHits++;
		}
		dwOut = out.idxNextOutput;
	}
	// Lowwer nibble
	dwOut = pSearchData->wrap.fsm.PushByte(LoNibble(bData));
	pSearchData->dwBits += g_nNibbleLength;
	while (dwOut != TNibbleSearchFsm::sm_outputNull) {
		const TSearchFsm::SOutput &out = pSearchData->wrap.fsm.GetOutput(dwOut);
		if (out.stepBack <= pSearchData->dwBits) { // enough data
			cHits++;
		}
		dwOut = out.idxNextOutput;
	}

	return cHits;
}

unsigned int CFsmTest::CNibbleFsmSearch::ProcessByte(const unsigned char bData, CFsmTest::CNibbleFsmSearch::TSearchData *pSearchData) {
	// process the two nibbles of the byte
	unsigned int cHits = 0;

	// Higher nibble
	unsigned int dwOut = pSearchData->wrap.fsm.PushByte(HiNibble(bData));
	while (dwOut != TNibbleSearchFsm::sm_outputNull) {
		const TSearchFsm::SOutput &out = pSearchData->wrap.fsm.GetOutput(dwOut);
		cHits++;
		dwOut = out.idxNextOutput;
	}
	// Lowwer nibble
	dwOut = pSearchData->wrap.fsm.PushByte(LoNibble(bData));
	while (dwOut != TNibbleSearchFsm::sm_outputNull) {
		const TSearchFsm::SOutput &out = pSearchData->wrap.fsm.GetOutput(dwOut);
		cHits++;
		dwOut = out.idxNextOutput;
	}

	return cHits;
}


/// CFsmTest::COctetFsmSearch - search with a 8-bit SearchFSM
class CFsmTest::COctetFsmSearch {
public: // data
	struct TSearchData {
		CFsmCreator::SFsmWrap<CFsmTest::TOctetSearchFsm> wrap;
		unsigned int dwBits;
	};

public: // initialization & statictics
	static TSearchData InitEngine(const TPatterns& patterns);
	static unsigned int GetMemoryRequirements(const TSearchData &data);
	static bool IsFsm() {return true;}
	static CFsmTest::SFsmStatistics GetFsmStatistics(const TSearchData &data);

public: // working methods
	static unsigned int ProcessByteCheckLength(const unsigned char bData, TSearchData *pSearchData);
	static unsigned int ProcessByte(const unsigned char bData, TSearchData *pSearchData);
};

CFsmTest::COctetFsmSearch::TSearchData CFsmTest::COctetFsmSearch::InitEngine(const TPatterns &patterns) {
	CFsmCreator fsm(patterns);
	fsm.GenerateTables();
	TSearchData data = {fsm.CreateByteFsmWrap<TOctetSearchFsm>(), 0};
	data.wrap.fsm.Reset();

	return data;
}

unsigned int CFsmTest::COctetFsmSearch::GetMemoryRequirements(const CFsmTest::COctetFsmSearch::TSearchData &data) {
	return CFsmTest::GetTableSize(data.wrap).dwTotalSize;
}

CFsmTest::SFsmStatistics CFsmTest::COctetFsmSearch::GetFsmStatistics(const CFsmTest::COctetFsmSearch::TSearchData &data) {
	CFsmTest::SFsmStatistics stats;
	stats.dwStatesCount = data.wrap.m_rows.count();
	stats.tableSize = CFsmTest::GetTableSize(data.wrap);
	stats.tableMinSize = CFsmTest::GetMinimalTableSize(data.wrap);

	return stats;
}

unsigned int CFsmTest::COctetFsmSearch::ProcessByteCheckLength(const unsigned char bData, CFsmTest::COctetFsmSearch::TSearchData *pSearchData) {
	// process the byte at once
	unsigned int cHits = 0;
	unsigned int dwOut = pSearchData->wrap.fsm.PushByte(bData);
	pSearchData->dwBits += g_nByteLength;
	while (dwOut != TNibbleSearchFsm::sm_outputNull) {
		const TSearchFsm::SOutput &out = pSearchData->wrap.fsm.GetOutput(dwOut);
		if (out.stepBack <= pSearchData->dwBits) { // enough data
			cHits++;
		}
		dwOut = out.idxNextOutput;
	}

	return cHits;
}

unsigned int CFsmTest::COctetFsmSearch::ProcessByte(const unsigned char bData, CFsmTest::COctetFsmSearch::TSearchData *pSearchData) {
	// process the byte at once
	unsigned int cHits = 0;
	unsigned int dwOut = pSearchData->wrap.fsm.PushByte(bData);
	while (dwOut != TNibbleSearchFsm::sm_outputNull) {
		const TSearchFsm::SOutput &out = pSearchData->wrap.fsm.GetOutput(dwOut);
		cHits++;
		dwOut = out.idxNextOutput;
	}

	return cHits;
}


/// CFsmTest::CRegisterSearch - simple search with a shift register
class CFsmTest::CRegisterSearch {
public: // data
	struct TSearchData {
		QList<CShiftRegister::SPattern> patterns;
		unsigned int dwPatternsCount;
		TPatterns initialPatterns;
		CShiftRegister reg;
		unsigned int dwBits;
	};

public: // initialization & statictics
	static TSearchData InitEngine(const TPatterns& patterns);
	static unsigned int GetMemoryRequirements(const TSearchData &data);
	static bool IsFsm() {return false;}
	static CFsmTest::SFsmStatistics GetFsmStatistics(const TSearchData &data);

public: // working methods
	static unsigned int ProcessByteCheckLength(const unsigned char bData, TSearchData *pSearchData);
	static unsigned int ProcessByte(const unsigned char bData, TSearchData *pSearchData);
};

CFsmTest::CRegisterSearch::TSearchData CFsmTest::CRegisterSearch::InitEngine(const TPatterns &patterns) {
	// prepare shift register and test patterns
	int nMaxPatternLength = AnalysePatterns(patterns).nMaxLength;
	TSearchData data;
	data.dwPatternsCount = patterns.count();
	data.initialPatterns = patterns;
	data.reg.Init(nMaxPatternLength);
	int idx;
	for (idx = 0; idx < patterns.count(); idx++) {
		data.patterns.append(data.reg.ConvertPattern(patterns[idx]));
	}
	data.dwBits = 0;

	return data;
}

unsigned int CFsmTest::CRegisterSearch::GetMemoryRequirements(const CFsmTest::CRegisterSearch::TSearchData &data) {
	return data.reg.RequiredMemorySize();
}

unsigned int CFsmTest::CRegisterSearch::ProcessByteCheckLength(const unsigned char bData, CFsmTest::CRegisterSearch::TSearchData *pSearchData) {
	unsigned int cHits = 0;
	int nBit;
	for (nBit = 0; nBit < BITS_IN_BYTE; nBit++) {
		// obtain next bit
		unsigned char bBit = GetHiBit(bData, nBit);

		// process bit with the register
		pSearchData->reg.PushBit(bBit);
		pSearchData->dwBits++;
		unsigned int dwPatternIdx;
		for (dwPatternIdx = 0; dwPatternIdx < pSearchData->dwPatternsCount; dwPatternIdx++) {
			if (pSearchData->reg.TestPattern(pSearchData->patterns[dwPatternIdx])) {
				unsigned int dwPatternLength = pSearchData->initialPatterns[dwPatternIdx].nLength;
				if (dwPatternLength <= pSearchData->dwBits) { // register is full
					cHits++;
				}
			}
		}
	}

	return cHits;
}

unsigned int CFsmTest::CRegisterSearch::ProcessByte(const unsigned char bData, CFsmTest::CRegisterSearch::TSearchData *pSearchData) {
	unsigned int cHits = 0;
	int nBit;
	for (nBit = 0; nBit < BITS_IN_BYTE; nBit++) {
		// obtain next bit
		unsigned char bBit = GetHiBit(bData, nBit);

		// process bit with the register
		pSearchData->reg.PushBit(bBit);
		unsigned int dwPatternIdx;
		for (dwPatternIdx = 0; dwPatternIdx < pSearchData->dwPatternsCount; dwPatternIdx++) {
			if (pSearchData->reg.TestPattern(pSearchData->patterns[dwPatternIdx])) {
				cHits++;
			}
		}
	}

	return cHits;
}
