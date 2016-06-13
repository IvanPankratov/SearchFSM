#ifndef FSMTEST_H
#define FSMTEST_H

#include <QVector>

#include "SearchFsm.h"
#include "FsmCreator.h"

class CFsmTest {
public:
	typedef CSearchFsm<unsigned int, unsigned int> TSearchFsm;
	struct SWrapFsm {
		TSearchFsm fsm; // uses \var rows and \var output fields, do NOT use this field separately
		QVector<TSearchFsm::STableRow> rows;
		QVector<TSearchFsm::SOutput> outputs;
	};

public:
	static SWrapFsm CreateFsm(const CFsmCreator &fsmCreator);
	static bool TestFsm(TSearchFsm fsm);
};

#endif // FSMTEST_H
