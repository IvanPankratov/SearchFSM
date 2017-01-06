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
	struct SWrapFsm {
		TSearchFsm fsm; // uses \var rows and \var output fields, do NOT use this field separately
		TFsmTable rows;
		TOutputTable outputs;
	};

public:
	static SWrapFsm CreateFsm(const CFsmCreator &fsmCreator);
	static bool TestFsm(TSearchFsm fsm);

private:
	static TOutputIdx StoreOutputList(const CFsmCreator::TOutputList &outputList, /* in-out */ TOutputTable *pOutputTable);
	static TOutputIdx StoreOutput(const TSearchFsm::SOutput &output, /* in-out */ TOutputTable *pOutputTable);
	static bool IsEqual(const TSearchFsm::SOutput &output1, const TSearchFsm::SOutput &output2);
};

#endif // FSMTEST_H
