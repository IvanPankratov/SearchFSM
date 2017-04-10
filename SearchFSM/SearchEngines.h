#ifndef SEARCHENGINES_H
#define SEARCHENGINES_H

#include "FsmTest.h"
#include "ShiftRegister.h"

/// CFsmTest::CBitFsmSearch - search with a bit SearchFSM
class CFsmTest::CBitFsmSearch {
public: // data
	struct TSearchData {
		CFsmCreator::SFsmWrap<TSearchFsm> wrap;
		unsigned int dwCollisionsCount;
		unsigned int dwBits;
	};

public: // initialization & statictics
	static TSearchData InitEngine(const TPatterns& patterns);
	static unsigned int GetMemoryRequirements(const TSearchData &data);
	static bool IsFsm() {return true;}
	static CFsmTest::SFsmStatistics GetFsmStatistics(const TSearchData &data);

public: // working methods
	static CFsmTest::TFindingsList ProcessByte(const unsigned char bData, TSearchData *pSearchData);
	static unsigned int ProcessByteIdle(const unsigned char bData, TSearchData *pSearchData);
};

// implementation
CFsmTest::CBitFsmSearch::TSearchData CFsmTest::CBitFsmSearch::InitEngine(const TPatterns &patterns) {
	CFsmCreator fsm(patterns);
	fsm.GenerateTables();
	TSearchData data = {fsm.CreateFsmWrap<TSearchFsm>(), fsm.GetCollisionsCount(), 0};
	data.wrap.fsm.Reset();

	return data;
}

unsigned int CFsmTest::CBitFsmSearch::GetMemoryRequirements(const CFsmTest::CBitFsmSearch::TSearchData &data) {
	return CFsmTest::GetTableSize(data.wrap).dwTotalSize;
}

CFsmTest::SFsmStatistics CFsmTest::CBitFsmSearch::GetFsmStatistics(const CFsmTest::CBitFsmSearch::TSearchData &data) {
	CFsmTest::SFsmStatistics stats;
	stats.dwStatesCount = data.wrap.m_rows.count();
	stats.dwOutputCellsCount = data.wrap.m_outputTable.count();
	stats.dwCollisionsCount = data.dwCollisionsCount;
	stats.tableSize = CFsmTest::GetTableSize(data.wrap);
	stats.tableMinSize = CFsmTest::GetMinimalTableSize(data.wrap);

	return stats;
}

CFsmTest::TFindingsList CFsmTest::CBitFsmSearch::ProcessByte(const unsigned char bData, CFsmTest::CBitFsmSearch::TSearchData *pSearchData) {
	TFindingsList result;
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
				SFinding finding;
				finding.nPatternIdx = out.patternIdx;
				finding.nErrors = out.errorsCount;
				unsigned int dwPosition = pSearchData->dwBits - out.stepBack;
				finding.dwPosition = dwPosition;
				result << finding;
				dwOut = out.idxNextOutput;
			}
			dwOut = out.idxNextOutput;
		}
	}

	return result;
}

unsigned int CFsmTest::CBitFsmSearch::ProcessByteIdle(const unsigned char bData, CFsmTest::CBitFsmSearch::TSearchData *pSearchData) {
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
		unsigned int dwCollisionsCount;
		unsigned int dwBits;
	};

public: // initialization & statictics
	static TSearchData InitEngine(const TPatterns& patterns);
	static unsigned int GetMemoryRequirements(const TSearchData &data);
	static bool IsFsm() {return true;}
	static CFsmTest::SFsmStatistics GetFsmStatistics(const TSearchData &data);

public: // working methods
	static CFsmTest::TFindingsList ProcessByte(const unsigned char bData, TSearchData *pSearchData);
	static unsigned int ProcessByteIdle(const unsigned char bData, TSearchData *pSearchData);
};

// implementation
CFsmTest::CNibbleFsmSearch::TSearchData CFsmTest::CNibbleFsmSearch::InitEngine(const TPatterns &patterns) {
	CFsmCreator fsm(patterns);
	fsm.GenerateTables();
	TSearchData data = {fsm.CreateByteFsmWrap<TNibbleSearchFsm>(), fsm.GetCollisionsCount(), 0};
	data.wrap.fsm.Reset();

	return data;
}

unsigned int CFsmTest::CNibbleFsmSearch::GetMemoryRequirements(const CFsmTest::CNibbleFsmSearch::TSearchData &data) {
	return CFsmTest::GetTableSize(data.wrap).dwTotalSize;
}

CFsmTest::SFsmStatistics CFsmTest::CNibbleFsmSearch::GetFsmStatistics(const CFsmTest::CNibbleFsmSearch::TSearchData &data) {
	CFsmTest::SFsmStatistics stats;
	stats.dwStatesCount = data.wrap.m_rows.count();
	stats.dwOutputCellsCount = data.wrap.m_outputTable.count();
	stats.dwCollisionsCount = data.dwCollisionsCount;
	stats.tableSize = CFsmTest::GetTableSize(data.wrap);
	stats.tableMinSize = CFsmTest::GetMinimalTableSize(data.wrap);

	return stats;
}

CFsmTest::TFindingsList CFsmTest::CNibbleFsmSearch::ProcessByte(const unsigned char bData, CFsmTest::CNibbleFsmSearch::TSearchData *pSearchData) {
	// process the two nibbles of the byte
	TFindingsList result;

	// Higher nibble
	unsigned int dwOut = pSearchData->wrap.fsm.PushByte(HiNibble(bData));
	pSearchData->dwBits += g_nNibbleLength;
	while (dwOut != TNibbleSearchFsm::sm_outputNull) {
		const TSearchFsm::SOutput &out = pSearchData->wrap.fsm.GetOutput(dwOut);
		if (out.stepBack <= pSearchData->dwBits) { // enough data
			SFinding finding;
			finding.nPatternIdx = out.patternIdx;
			finding.nErrors = out.errorsCount;
			unsigned int dwPosition = pSearchData->dwBits - out.stepBack;
			finding.dwPosition = dwPosition;
			result << finding;
		}
		dwOut = out.idxNextOutput;
	}
	// Lowwer nibble
	dwOut = pSearchData->wrap.fsm.PushByte(LoNibble(bData));
	pSearchData->dwBits += g_nNibbleLength;
	while (dwOut != TNibbleSearchFsm::sm_outputNull) {
		const TSearchFsm::SOutput &out = pSearchData->wrap.fsm.GetOutput(dwOut);
		if (out.stepBack <= pSearchData->dwBits) { // enough data
			SFinding finding;
			finding.nPatternIdx = out.patternIdx;
			finding.nErrors = out.errorsCount;
			unsigned int dwPosition = pSearchData->dwBits - out.stepBack;
			finding.dwPosition = dwPosition;
			result << finding;
		}
		dwOut = out.idxNextOutput;
	}

	return result;

}

unsigned int CFsmTest::CNibbleFsmSearch::ProcessByteIdle(const unsigned char bData, CFsmTest::CNibbleFsmSearch::TSearchData *pSearchData) {
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
		unsigned int dwCollisionsCount;
		unsigned int dwBits;
	};

public: // initialization & statictics
	static TSearchData InitEngine(const TPatterns& patterns);
	static unsigned int GetMemoryRequirements(const TSearchData &data);
	static bool IsFsm() {return true;}
	static CFsmTest::SFsmStatistics GetFsmStatistics(const TSearchData &data);

public: // working methods
	static CFsmTest::TFindingsList ProcessByte(const unsigned char bData, TSearchData *pSearchData);
	static unsigned int ProcessByteIdle(const unsigned char bData, TSearchData *pSearchData);
};

// implementation
CFsmTest::COctetFsmSearch::TSearchData CFsmTest::COctetFsmSearch::InitEngine(const TPatterns &patterns) {
	CFsmCreator fsm(patterns);
	fsm.GenerateTables();
	TSearchData data = {fsm.CreateByteFsmWrap<TOctetSearchFsm>(), fsm.GetCollisionsCount(), 0};
	data.wrap.fsm.Reset();

	return data;
}

unsigned int CFsmTest::COctetFsmSearch::GetMemoryRequirements(const CFsmTest::COctetFsmSearch::TSearchData &data) {
	return CFsmTest::GetTableSize(data.wrap).dwTotalSize;
}

CFsmTest::SFsmStatistics CFsmTest::COctetFsmSearch::GetFsmStatistics(const CFsmTest::COctetFsmSearch::TSearchData &data) {
	CFsmTest::SFsmStatistics stats;
	stats.dwStatesCount = data.wrap.m_rows.count();
	stats.dwOutputCellsCount = data.wrap.m_outputTable.count();
	stats.dwCollisionsCount = data.dwCollisionsCount;
	stats.tableSize = CFsmTest::GetTableSize(data.wrap);
	stats.tableMinSize = CFsmTest::GetMinimalTableSize(data.wrap);

	return stats;
}

CFsmTest::TFindingsList CFsmTest::COctetFsmSearch::ProcessByte(const unsigned char bData, CFsmTest::COctetFsmSearch::TSearchData *pSearchData) {
	// process the byte at once
	TFindingsList result;
	unsigned int dwOut = pSearchData->wrap.fsm.PushByte(bData);
	pSearchData->dwBits += g_nByteLength;
	while (dwOut != TNibbleSearchFsm::sm_outputNull) {
		const TSearchFsm::SOutput &out = pSearchData->wrap.fsm.GetOutput(dwOut);
		if (out.stepBack <= pSearchData->dwBits) { // enough data
			SFinding finding;
			finding.nPatternIdx = out.patternIdx;
			finding.nErrors = out.errorsCount;
			unsigned int dwPosition = pSearchData->dwBits - out.stepBack;
			finding.dwPosition = dwPosition;
			result << finding;
		}
		dwOut = out.idxNextOutput;
	}

	return result;
}

unsigned int CFsmTest::COctetFsmSearch::ProcessByteIdle(const unsigned char bData, CFsmTest::COctetFsmSearch::TSearchData *pSearchData) {
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
	static CFsmTest::TFindingsList ProcessByte(const unsigned char bData, TSearchData *pSearchData);
	static unsigned int ProcessByteIdle(const unsigned char bData, TSearchData *pSearchData);
};

// implementation
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

CFsmTest::TFindingsList CFsmTest::CRegisterSearch::ProcessByte(const unsigned char bData, CFsmTest::CRegisterSearch::TSearchData *pSearchData) {
	TFindingsList result;
	int nBit;
	for (nBit = 0; nBit < BITS_IN_BYTE; nBit++) {
		// obtain next bit
		unsigned char bBit = GetHiBit(bData, nBit);

		// process bit with the register
		pSearchData->reg.PushBit(bBit);
		pSearchData->dwBits++;
		unsigned int dwPatternIdx;
		for (dwPatternIdx = 0; dwPatternIdx < pSearchData->dwPatternsCount; dwPatternIdx++) {
			unsigned int dwErrors = 0;
			if (pSearchData->reg.TestPattern(pSearchData->patterns[dwPatternIdx], &dwErrors)) {
				unsigned int dwPatternLength = pSearchData->initialPatterns[dwPatternIdx].nLength;
				if (dwPatternLength <= pSearchData->dwBits) { // register is full
					SFinding finding;
					finding.nPatternIdx = dwPatternIdx;
					finding.nErrors = dwErrors;
					unsigned int dwPosition = pSearchData->dwBits - dwPatternLength;
					finding.dwPosition = dwPosition;
					result << finding;
				}
			}
		}
	}

	return result;
}

unsigned int CFsmTest::CRegisterSearch::ProcessByteIdle(const unsigned char bData, CFsmTest::CRegisterSearch::TSearchData *pSearchData) {
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


#endif // SEARCHENGINES_H
