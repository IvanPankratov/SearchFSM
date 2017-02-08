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
	bool TraceFsm(int nDataLength);
	void ReleaseFsm();

private:
	void DumpFinding(int nBitsProcessed, const TSearchFsm::SOutput &out);

private: // pseudo-random related members
	unsigned int NextRandomEntity();
	unsigned int NextRandom15Bits();
	unsigned char RandomByte();
	void ResetLCG();

private: // output table handling
	static TOutputIdx StoreOutputList(const CFsmCreator::TOutputList &outputList, /* in-out */ TOutputTable *pOutputTable);
	static TOutputIdx StoreOutput(const TSearchFsm::SOutput &output, /* in-out */ TOutputTable *pOutputTable);
	static bool IsEqual(const TSearchFsm::SOutput &output1, const TSearchFsm::SOutput &output2);

private:
	TPatterns m_patterns;
	TSearchFsm *m_pFsm;

	// members for storing and releasing the tables
	const TFsmTable m_rows;
	const TOutputTable m_outputs;

	// random related data
	unsigned int m_dwRandomSeed; // pseudo-random sequence entity
};

#endif // FSMTEST_H
