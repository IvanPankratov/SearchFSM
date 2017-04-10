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

CFsmTest::CFsmTest(const TPatterns &patterns) {
	m_patterns = patterns;
}

CFsmTest::~CFsmTest() {}

bool CFsmTest::CreateFsm(bool fVerbose) {
	// generate tables
	CFsmCreator fsm(m_patterns);
	return fsm.GenerateTables(fVerbose);
}

bool CFsmTest::TraceFsm(int nDataLength) {
	CFsmCreator fsm(m_patterns);
	fsm.GenerateTables();
	CFsmCreator::SFsmWrap<TSearchFsm> wrap = fsm.CreateFsmWrap<TSearchFsm>();

	CDoubleLcg lcg;
	unsigned char bData = 0;
	int idx;
	for (idx = 0; idx < nDataLength; idx++) {
		int nState = wrap.fsm.GetState();
		int nBit = idx % 8;
		if (nBit == 0) { // new byte begins => update bData
			bData = lcg.RandomByte();
		}
		unsigned char bBit = GetHiBit(bData, nBit);
		unsigned int nOut = wrap.fsm.PushBit(bBit);
		printf("%i: %i =%i=> %i", idx, nState, bBit, wrap.fsm.GetState());
		if (nOut != TSearchFsm::sm_outputNull) {
			printf(" found ");
			while (nOut != TSearchFsm::sm_outputNull) {
				const TSearchFsm::SOutput &out = wrap.fsm.GetOutput(nOut);
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
		unsigned int dwCheckLengthBytes = AnalysePatterns(m_patterns).nMaxLength / BITS_IN_BYTE + 1;
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
