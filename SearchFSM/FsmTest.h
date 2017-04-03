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
	typedef CSearchFsmByte<g_nNibbleLength, TStateIdx, TOutputIdx> TSearchFsmNibble;
	typedef CSearchFsmByte<g_nByteLength, TStateIdx, TOutputIdx> TSearchFsmByte;

	// ancillary types
	typedef QVector<TSearchFsm::STableRow> TFsmTable;
	typedef QVector<TSearchFsmNibble::STableRow> TFsmNibbleTable;
	typedef QVector<TSearchFsmByte::STableRow> TFsmByteTable;
	typedef QVector<TSearchFsm::SOutput> TOutputTable;

	// time measurement results structure
	struct STimeings { // tim
		long double dTotalTime;
		long double dCpuTime;
		long double dKernelTime;
	};

	struct SEnginePerformance {
		STimeings timInitialization;
		STimeings timOperating;
		unsigned int dwBytesCount; // overall bytes processed
		unsigned int dwMemoryRequirements; // total memory requirements
		unsigned int dwStatesCount; // for FSMs only
		unsigned int dwHits;

		// obsolete
		long double dRate; // bytes per second
		long double dCpuUsage;
		long double dCpuKernelUsage;
	};

public:
	CFsmTest();
	~CFsmTest();

public:
	bool CreateFsm(const TPatterns &patterns, bool fCreateNibbleFsm, bool fCreateByteFsm, bool fVerbose = false);
	unsigned int GetCollisionsCount() const;
	bool TraceFsm(int nDataLength);
	bool TestCorrectness(unsigned int dwTestBytesCount, int nPrintHits, /* out, optional */ unsigned int *pdwHits = NULL);
	// rate is measured in bytes per second
	SEnginePerformance TestFsmRate(unsigned int dwTestBytesCount, /* out, optional */ unsigned int *pdwHits = NULL);
	SEnginePerformance TestFsmRate2(unsigned int dwTestBytesCount);
	SEnginePerformance TestFsmNibbleRate(unsigned int dwTestBytesCount, /* out, optional */ unsigned int *pdwHits = NULL);
	SEnginePerformance TestFsmByteRate(unsigned int dwTestBytesCount, /* out, optional */ unsigned int *pdwHits = NULL);
	SEnginePerformance TestRegisterRate(unsigned int dwTestBytesCount, /* out, optional */ unsigned int *pdwHits = NULL);
	SEnginePerformance TestRegisterRate2(unsigned int dwTestBytesCount);
	void ReleaseFsm();

public: // table size quering methods
	struct STableSize {
		unsigned int dwTotalSize;
		unsigned int dwFsmTableSize;
		unsigned int dwOutputTableSize;
	};

	unsigned int GetStatesCount() const {return m_rows.count();}
	unsigned int GetOutputElementsCount() const {return m_dwFsmOutputsCount;}
	STableSize GetTableSize() const;
	STableSize GetMinimalTableSize() const; // using the shortest integral types
	STableSize GetNibbleTableSize() const;
	STableSize GetByteTableSize() const;

private:
	static unsigned int GetMinimalDataSize(unsigned int nMaxValue);

	template <class TSearchEngine>
	bool TestEngine(unsigned int dwTestBytesCount, /* out */ SEnginePerformance *pResult);

private:
	class CBitFsmSearch;
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
	int m_nMaxErrorsCount;
	unsigned int m_dwCollisions;
	unsigned int m_dwFsmOutputsCount;
	unsigned int m_dwFsmNibbleOutputsCount;
	TSearchFsm *m_pFsm;
	TSearchFsmNibble *m_pFsmNibble;
	TSearchFsmByte *m_pFsmByte;

	// members for storing and releasing the tables
	const TFsmTable m_rows;
	const TFsmNibbleTable m_rowsNibble;
	const TFsmByteTable m_rowsByte;
	const TOutputTable m_outputs;
};

#endif // FSMTEST_H
