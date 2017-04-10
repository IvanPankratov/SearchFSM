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
	typedef CSearchFsm<TStateIdx, TOutputIdx> TSearchFsm;
	typedef CSearchFsmByte<g_nNibbleLength, TStateIdx, TOutputIdx> TNibbleSearchFsm;
	typedef CSearchFsmByte<g_nByteLength, TStateIdx, TOutputIdx> TOctetSearchFsm;

	// ancillary types
	typedef QVector<TSearchFsm::STableRow> TBitFsmTable;
	typedef QVector<TNibbleSearchFsm::STableRow> TNibbleFsmTable;
	typedef QVector<TOctetSearchFsm::STableRow> TOctetFsmTable;
	typedef QVector<TSearchFsm::SOutput> TOutputTable;

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
	CFsmTest();
	~CFsmTest();

public:
	bool CreateFsm(const TPatterns &patterns, bool fCreateNibbleFsm, bool fCreateByteFsm, bool fVerbose = false);
	bool TraceFsm(int nDataLength);
	bool TestCorrectness(unsigned int dwTestBytesCount, int nPrintHits, /* out, optional */ unsigned int *pdwHits = NULL);

	// test engines' performance
	bool TestBitFsmRate(unsigned int dwTestBytesCount, /* out */ SEnginePerformance *pResult);
	bool TestNibbleFsmRate(unsigned int dwTestBytesCount, /* out */ SEnginePerformance *pResult);
	bool TestOctetFsmRate(unsigned int dwTestBytesCount, /* out */ SEnginePerformance *pResult);
	bool TestRegisterRate(unsigned int dwTestBytesCount, /* out */ SEnginePerformance *pResult);

	// obsolete
	void ReleaseFsm();

public: // table size calculating methods
	template <class TSearchFsm_>
	static SFsmTableSize GetTableSize(const CFsmCreator::SFsmWrap<TSearchFsm_>& wrap);

	template <class TSearchFsm_>
	static SFsmTableSize GetMinimalTableSize(const CFsmCreator::SFsmWrap<TSearchFsm_>& fsm);

	static SPatternsStats AnalysePatterns(const TPatterns& patterns);

private:
	static unsigned int GetMinimalDataSize(unsigned int nMaxValue);

	template <class TSearchEngine>
	bool TestEnginePerformance(unsigned int dwTestBytesCount, /* out */ SEnginePerformance *pResult);

private:
	class CBitFsmSearch;
	class CNibbleFsmSearch;
	class COctetFsmSearch;
	class CRegisterSearch;

	struct SFinding {
		int nPatternIdx;
		int nErrors;
		unsigned int dwPosition;
	};
	typedef QList<SFinding> TFindingsList;

private:
	TFindingsList ProcessBitByFsm(unsigned int dwProcessedBits, unsigned char bBit);
	TFindingsList ProcessNibbleByFsm(unsigned int dwProcessedBits, unsigned char bNibble);
	TFindingsList ProcessByteByFsm(unsigned int dwProcessedBits, unsigned char bData);
	static bool AreEqual(const TFindingsList &list1, const TFindingsList &list2);
	static bool AreEqual(const SFinding &finding1, const SFinding &finding2);
	void DumpFinding(int nBitsProcessed, const TSearchFsm::SOutput &out);
	void AnalysePatterns();

private: // output table handling
	static TOutputIdx StoreOutputList(const CFsmCreator::TOutputList &outputList, /* in-out */ TOutputTable *pOutputTable);
	static TOutputIdx StoreOutput(const TSearchFsm::SOutput &output, /* in-out */ TOutputTable *pOutputTable);
	static bool IsEqual(const TSearchFsm::SOutput &output1, const TSearchFsm::SOutput &output2);

private:
	TPatterns m_patterns;
	int m_nMaxPatternLength;
	unsigned int m_dwFsmOutputsCount;
	unsigned int m_dwFsmNibbleOutputsCount;
	TSearchFsm *m_pFsm;
	TNibbleSearchFsm *m_pFsmNibble;
	TOctetSearchFsm *m_pFsmByte;

	// members for storing and releasing the tables
	const TBitFsmTable m_rows;
	const TNibbleFsmTable m_rowsNibble;
	const TOctetFsmTable m_rowsByte;
	const TOutputTable m_outputs;
};

#endif // FSMTEST_H
