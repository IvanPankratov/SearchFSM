#line 2 "FsmTest.h" // Make __FILE__ omit the path

#ifndef FSMTEST_H
#define FSMTEST_H

#include <QVector>

#include "SearchFsm.h"
#include "FsmCreator.h"

class CFsmTest {
public:
	// parameters
	typedef unsigned int TStateIdx;
	typedef unsigned int TOutputIdx;

	// constants
	static const int g_nNibbleLength = 4;
	static const int g_nByteLength = 8;

	// FSM types
	typedef CSearchFsm<TStateIdx, TOutputIdx> TBitSearchFsm;
	typedef CSearchFsmByte<g_nNibbleLength, TStateIdx, TOutputIdx> TNibbleSearchFsm;
	typedef CSearchFsmByte<g_nByteLength, TStateIdx, TOutputIdx> TOctetSearchFsm;

	// time measurement results structure
	struct STimeings { // tim
		long double dTotalTime;
		long double dCpuTime;
		long double dKernelTime;
	};

	struct SFsmTableSize {
		unsigned int dwTotalSize;
		unsigned int dwMainTableSize;
		unsigned int dwOutputTableSize;
	};

	struct SFsmStatistics {
		unsigned int dwStatesCount;
		unsigned int dwOutputCellsCount;
		unsigned int dwCollisionsCount;
		SFsmTableSize tableSize;
		SFsmTableSize tableMinSize;
	};

	struct SEnginePerformance {
		bool fSuccess;
		STimeings timInitialization;
		STimeings timOperating;
		unsigned int dwBytesCount; // overall bytes processed
		unsigned int dwHits;

		// memory requirements and other statistics
		unsigned int dwMemoryRequirements; // total memory requirements
		bool fIsFsm; // true for FSMs
		SFsmStatistics fsmStatistics; // for FSMs only
	};

	struct SPatternsStats {
		int nMaxLength;
		int nMaxErrorsCount;
	};

public:
	CFsmTest(const TPatterns &patterns);
	~CFsmTest();

public:
	bool CreateFsm(bool fVerbose = false);
	bool TraceFsm(int nDataLength);
	bool TestCorrectness(unsigned int dwTestBytesCount, int nPrintHits, /* out, optional */ unsigned int *pdwHits = NULL);

	// test engines' performance
	bool TestBitFsmRate(unsigned int dwTestBytesCount, bool fOptimize, /* out */ SEnginePerformance *pResult);
	bool TestNibbleFsmRate(unsigned int dwTestBytesCount, /* out */ SEnginePerformance *pResult);
	bool TestOctetFsmRate(unsigned int dwTestBytesCount, /* out */ SEnginePerformance *pResult);
	bool TestRegisterRate(unsigned int dwTestBytesCount, /* out */ SEnginePerformance *pResult);

public: // table size calculating methods
	template <class TSearchFsm>
	static SFsmTableSize GetTableSize(const CFsmCreator::SFsmWrap<TSearchFsm>& wrap);

	template <class TSearchFsm>
	static SFsmTableSize GetMinimalTableSize(const CFsmCreator::SFsmWrap<TSearchFsm>& fsm);

private:
	static SPatternsStats AnalysePatterns(const TPatterns& patterns);
	static unsigned int GetMinimalDataSize(unsigned int nMaxValue);

	template <class TSearchEngine>
	bool TestEnginePerformance(unsigned int dwTestBytesCount, /* out */ SEnginePerformance *pResult);

private:
	template <bool fOptimize> class CBitFsmSearch;
	class CNibbleFsmSearch;
	class COctetFsmSearch;
	class CRegisterSearch;

private:
	struct SFinding {
		int nPatternIdx;
		int nErrors;
		unsigned int dwPosition;
	};
	typedef QList<SFinding> TFindingsList;

	static bool AreEqual(const TFindingsList &list1, const TFindingsList &list2);
	static bool AreEqual(const SFinding &finding1, const SFinding &finding2);
	void DumpFinding(int nBitsProcessed, const TBitSearchFsm::TOutput &out);

private:
	TPatterns m_patterns;
};

#endif // FSMTEST_H
