#line 2 "FsmTest.h" // Make __FILE__ omit the path

#ifndef FSMTEST_H
#define FSMTEST_H

#include <QVector>

#include "SearchFsm.h"
#include "FsmCreator.h"

class CFsmTest {
public:
	typedef unsigned int TStateIdx;
	typedef unsigned int TOutputIdx;
	typedef CSearchFsm<TStateIdx, TOutputIdx> TSearchFsm;

	typedef QVector<TSearchFsm::STableRow> TFsmTable;
	typedef QVector<TSearchFsm::SOutput> TOutputTable;

public:
	CFsmTest();
	~CFsmTest();

public:
	bool CreateFsm(const TPatterns &patterns, bool fVerbose = false);
	unsigned int GetCollisionsCount() const;
	bool TraceFsm(int nDataLength);
	bool TestCorrectness(unsigned int dwTestBytesCount, int nPrintHits, /* out, optional */ unsigned int *pdwHits = NULL);
	// rate is measured in bytes per second
	long double TestFsmRate(unsigned int dwTestBytesCount, /* out, optional */ unsigned int *pdwHits = NULL);
	long double TestRegisterRate(unsigned int dwTestBytesCount, /* out, optional */ unsigned int *pdwHits = NULL);
	void ReleaseFsm();

public: // table size quering methods
	struct STableSize {
		unsigned int dwTotalSize;
		unsigned int dwFsmTableSize;
		unsigned int dwOutputTableSize;
	};

	unsigned int GetStatesCount() const {return m_rows.count();}
	unsigned int GetOutputElementsCount() const {return m_outputs.count();}
	STableSize GetTableSize() const;
	STableSize GetMinimalTableSize() const; // using the shortest integral types

private:
	static unsigned int GetMinimalDataSize(unsigned int nMaxValue);

private:
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
	TSearchFsm *m_pFsm;

	// members for storing and releasing the tables
	const TFsmTable m_rows;
	const TOutputTable m_outputs;
};

#endif // FSMTEST_H
