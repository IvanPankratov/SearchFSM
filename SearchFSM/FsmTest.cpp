#line 2 "FsmTest.cpp" // Make __FILE__ omit the path

#include <stdlib.h>

#include "FsmTest.h"
#include "SearchEngines.h"
#include "WinTimer.h"

class CLcg { // linear congruent generator, produces period 1 GiB
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
	// prepare engines (register and bit SearchFSM - must be created, Nibble and octet SearchFSM - try)
	CRegisterSearch::TSearchData searchDataRegister = CRegisterSearch::InitEngine(m_patterns);
	CBitFsmSearch::TSearchData searchDataBitFsm = CBitFsmSearch::InitEngine(m_patterns);
	CNibbleFsmSearch::TSearchData *pSearchDataNibbleFsm = NULL;
	try {
		pSearchDataNibbleFsm = new CNibbleFsmSearch::TSearchData(CNibbleFsmSearch::InitEngine(m_patterns));
	}
	catch(...) {
		puts("Failed to build Nibble SearchFSM!");
		pSearchDataNibbleFsm = NULL;
	}

	COctetFsmSearch::TSearchData *pSearchDataOctetFsm = NULL;
	try {
		pSearchDataOctetFsm = new COctetFsmSearch::TSearchData(COctetFsmSearch::InitEngine(m_patterns));
	}
	catch(...) {
		puts("Failed to build Octet SearchFSM!");
		pSearchDataOctetFsm = NULL;
	}

	// start test
	CDoubleLcg lcg;
	bool fCorrect = true;
	unsigned int cHits = 0;
	int cPrinted = 0;
	unsigned int dwBytes;
	for (dwBytes = 0; dwBytes < dwTestBytesCount; dwBytes++) {
		unsigned char bData = lcg.RandomByte();
		TFindingsList finReg = CRegisterSearch::ProcessByte(bData, &searchDataRegister);
		TFindingsList finBitFsm = CBitFsmSearch::ProcessByte(bData, &searchDataBitFsm);

		if (!AreEqual(finBitFsm, finReg)) {
			puts("FAIL! Bit SearchFSM != Test register!");
//			printf("FAIL! At %i, pattern #%i, FSM = %i errors, register = %i errors)\n", dwPosition, idx, nErrorsFsm, nErrors);
			fCorrect = false;
		}

		if (pSearchDataNibbleFsm != NULL) { // Nibble SearchFSM is built
			TFindingsList finNibbleFsm = CNibbleFsmSearch::ProcessByte(bData, pSearchDataNibbleFsm);
			if (!AreEqual(finBitFsm, finNibbleFsm)) {
				puts("FAIL! Bit SearchFSM != Nibble SearchFSM!");
				fCorrect = false;
			}
		}

		if (pSearchDataOctetFsm != NULL) { // Octet SearchFSM is built
			TFindingsList finOctetFsm = COctetFsmSearch::ProcessByte(bData, pSearchDataOctetFsm);
			if (!AreEqual(finBitFsm, finOctetFsm)) {
				puts("FAIL! Bit SearchFSM != Octet SearchFSM!");
				fCorrect = false;
			}
		}

		unsigned int dwMask = (int)cHits >> 31; // either all ones or null
		cHits += finReg.count();
		cHits |= dwMask;

		while (cPrinted < nPrintHits && !finReg.isEmpty()) {
			SFinding finding = finReg.takeFirst();
			printf("Found #%i at %i with %i errors\n", finding.nPatternIdx, finding.dwPosition, finding.nErrors);
			cPrinted++;
		}
	}

	if (pdwHits != NULL) {
		*pdwHits = cHits;
	}

	return fCorrect;
}

bool CFsmTest::TestBitFsmRate(unsigned int dwTestBytesCount, CFsmTest::SEnginePerformance *pResult) {
	return TestEnginePerformance<CBitFsmSearch>(dwTestBytesCount, pResult);
}

bool CFsmTest::TestNibbleFsmRate(unsigned int dwTestBytesCount, CFsmTest::SEnginePerformance *pResult) {
	return TestEnginePerformance<CNibbleFsmSearch>(dwTestBytesCount, pResult);
}

bool CFsmTest::TestOctetFsmRate(unsigned int dwTestBytesCount, CFsmTest::SEnginePerformance *pResult) {
	return TestEnginePerformance<COctetFsmSearch>(dwTestBytesCount, pResult);
}

bool CFsmTest::TestRegisterRate(unsigned int dwTestBytesCount, CFsmTest::SEnginePerformance *pResult) {
	return TestEnginePerformance<CRegisterSearch>(dwTestBytesCount, pResult);
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
bool CFsmTest::TestEnginePerformance(unsigned int dwTestBytesCount, SEnginePerformance *pResult) {
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
			unsigned int dwMask = (int)cHits >> 31; // either all ones or null
			TFindingsList findings = TSearchEngine::ProcessByte(bData, &searchData);
			cHits += findings.count();
			cHits |= dwMask;
		}
		for (; dwBytes < dwTestBytesCount; dwBytes++) {
			unsigned char bData = lcg.RandomByte();
			unsigned int dwMask = (int)cHits >> 31; // either all ones or null
			cHits += TSearchEngine::ProcessByteIdle(bData, &searchData);
			cHits |= dwMask;
		}
		timer.Stop();
		performance.timOperating = GetTimings(timer);
		performance.dwBytesCount = dwTestBytesCount;
		performance.dwHits = cHits;

		performance.fSuccess = true;
		*pResult = performance;
	}
	catch(...) {
		pResult->fSuccess = false;
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
	int idx;
	for (idx = 0; idx < m_patterns.count(); idx++) {
		int nLength = m_patterns[idx].nLength;
		if (nMaxPatternLength < nLength) {
			nMaxPatternLength = nLength;
		}
	}

	m_nMaxPatternLength = nMaxPatternLength;
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

