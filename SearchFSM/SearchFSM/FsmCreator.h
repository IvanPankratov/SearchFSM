#line 2 "FsmCreator.h" // Make __FILE__ omit the path

#ifndef FSMCREATOR_H
#define FSMCREATOR_H

#include <QList>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QString>

#include "Common.h"

QString PatternToString(const SPattern &pattern);

//////////////////////////////////////////////////////////////////////////
/// \brief The CFsmCreator class - builds tables for Searching FSM
class CFsmCreator {
public:
	struct SOutput {
		int nPatternIdx;
		int nErrors;
		int nStepBack;
	};
	typedef QList<SOutput> TOutputList;

	struct STableCell {
		int nNextState;
		TOutputList output;
	};
	struct STableRow {
		STableCell cell0, cell1;
	};

	// byte FSM structures
	struct SByteTableRow {
		QVector<STableCell> cells;
	};

	struct TByteTable {
		QVector<SByteTableRow> rows;
	};

	enum EBitOrder {
		bitOrder_MsbFirst, // as in patterns
		bitOrder_LsbFirst
	};

	// SearchFSM structures - to create FSM at once and store the tables inside the structure
	template <class TSearchFsm>
	struct SFsmWrap {
		// the fsm itself - mustn't be used out of the whole structure's scope
		TSearchFsm fsm;

		// members for storing and releasing the tables
		const QVector<typename TSearchFsm::STableRow> m_rows;
		const QVector<typename TSearchFsm::TOutput> m_outputTable;
	};

public:
	CFsmCreator(const TPatterns &patterns);

public:
	bool GenerateTables(bool fVerbose = false);
	bool OptimizeTables(bool fVerbose = false);
	QSet<int> FindUnessentialStates(bool fVerbose = false) const;
	TByteTable CreateByteTable(int nBitsAtOnce, EBitOrder bitOrder = bitOrder_MsbFirst) const;
	int GetStatesCount() const;
	unsigned int GetCollisionsCount() const;
	const STableRow &GetTableRow(int nRow) const;

	// create SearchFSM at once - with stored tables
	template <class TSearchFsm>
	SFsmWrap<TSearchFsm> CreateFsmWrap() const;

	template <class TSearchFsm>
	SFsmWrap<TSearchFsm> CreateByteFsmWrap(CFsmCreator::EBitOrder bitOrder = bitOrder_MsbFirst) const;

private: // output table handling
	template <class TSearchFsm>
	static typename TSearchFsm::TOutputIdx StoreOutputList(const TOutputList &outputList,
		/* in-out */ QVector<typename TSearchFsm::TOutput> *pOutputTable);

	template <class TSearchFsm>
	static typename TSearchFsm::TOutputIdx StoreOutput(const typename TSearchFsm::TOutput &output,
		/* in-out */ QVector<typename TSearchFsm::TOutput> *pOutputTable);

	template <class TSearchFsm>
	static bool AreEqual(const typename TSearchFsm::TOutput &output1, const typename TSearchFsm::TOutput &output2);

private:
	struct SPrefix {
		int nLength;
		int nErrors;
	};

	struct SStatePart {
		QVector<SPrefix> prefixes; // found prefixes ordered by length from the least to the biggest
	};

	struct SBitResultForPattern {
		SStatePart newStatePart;
		bool fFound;
		int nErrors; // valid only if fFound = true
	};

	struct SStateDescription {
		QVector<SStatePart> parts;
	};

	typedef unsigned int TStateHash;
	typedef QList<int> TIndexList;

private:
	STableCell TransitState(const SStateDescription &state, unsigned char bBit);
	int AddState(const SStateDescription &state);
	static SBitResultForPattern ProcessBitForPattern(const SStatePart &part, const SPattern &pattern, unsigned char bBit);

private:
	static bool AreEqual(const SStateDescription &state1, const SStateDescription &state2);
	static bool AreEqual(const SStatePart &part1, const SStatePart &part2);
	static TStateHash Hash(const SStateDescription &state);
	void DumpState(const SStateDescription &state);
	void DumpStatePart(const SStatePart &part);
	void DumpOutput(const TOutputList &output);

private:
	const TPatterns m_patterns;
	QList<SStateDescription> m_states;
	QHash<TStateHash, TIndexList> m_idxStates; // hash -> indexes list
	unsigned int m_dwCollisions;
	QVector<STableRow> m_table;
};

// inline template members
template<class TSearchFsm>
CFsmCreator::SFsmWrap<TSearchFsm> CFsmCreator::CreateFsmWrap() const {
	QVector<typename TSearchFsm::STableRow> rows(GetStatesCount());
	QVector<typename TSearchFsm::TOutput> outputs;
	int nRow;
	for (nRow = 0; nRow < GetStatesCount(); nRow++) {
		const STableRow &row = GetTableRow(nRow);
		typename TSearchFsm::TTableCell cell0;
		cell0.idxNextState = row.cell0.nNextState;
		cell0.idxOutput = StoreOutputList<TSearchFsm>(row.cell0.output, &outputs);

		typename TSearchFsm::TTableCell cell1;
		cell1.idxNextState = row.cell1.nNextState;
		cell1.idxOutput = StoreOutputList<TSearchFsm>(row.cell1.output, &outputs);

		typename TSearchFsm::STableRow fsmRow;
		fsmRow.cell0 = cell0;
		fsmRow.cell1 = cell1;
		rows[nRow] = fsmRow;
	}

	// release redundant memory
	outputs.squeeze();

	// create the structure describing bit SearchFSM table
	typename TSearchFsm::STable table = {rows.constData(), outputs.constData(),
		(unsigned int)rows.count(), (unsigned int)outputs.count()};
	SFsmWrap<TSearchFsm> fsm = {table, rows, outputs};
	return fsm;
}

template<class TSearchFsm>
CFsmCreator::SFsmWrap<TSearchFsm> CFsmCreator::CreateByteFsmWrap(CFsmCreator::EBitOrder bitOrder) const {
	QVector<typename TSearchFsm::STableRow> rows(GetStatesCount());
	QVector<typename TSearchFsm::TOutput> outputs;

	CFsmCreator::TByteTable fsmTable = CreateByteTable(TSearchFsm::g_nBitsAtOnce, bitOrder);
	int nRow;
	for (nRow = 0; nRow < GetStatesCount(); nRow++) {
		const CFsmCreator::SByteTableRow &row = fsmTable.rows[nRow];
		int nColumn;
		for (nColumn = 0; nColumn < row.cells.count(); nColumn++) {
			STableCell &cell = fsmTable.rows[nRow].cells[nColumn];
			rows[nRow].cells[nColumn].idxNextState = cell.nNextState;
			rows[nRow].cells[nColumn].idxOutput = StoreOutputList<TSearchFsm>(cell.output, &outputs);
		}
	}

	// release redundant memory
	outputs.squeeze();

	// create the structure describing bit SearchFSM table
	typename TSearchFsm::STable table = {rows.constData(), outputs.constData(),
		(unsigned int)rows.count(), (unsigned int)outputs.count()};
	SFsmWrap<TSearchFsm> fsm = {table, rows, outputs};
	return fsm;
}

// output table handling
template<class TSearchFsm>
typename TSearchFsm::TOutputIdx CFsmCreator::StoreOutputList(const TOutputList &outputList,
	QVector<typename TSearchFsm::TOutput> *pOutputTable)
{
	if (outputList.isEmpty()) {
		return TSearchFsm::sm_outputNull;
	}

	int idx;
	typename TSearchFsm::TOutputIdx idxNext = TSearchFsm::sm_outputNull;
	for (idx = outputList.count() - 1; idx >= 0; idx--) {
		CFsmCreator::SOutput out = outputList[idx];
		typename TSearchFsm::TOutput outputNew;
		outputNew.patternIdx = out.nPatternIdx;
		outputNew.errorsCount = out.nErrors;
		outputNew.stepBack = out.nStepBack;
		outputNew.idxNextOutput = idxNext;
		idxNext = StoreOutput<TSearchFsm>(outputNew, pOutputTable);
	}

	return idxNext;
}

template<class TSearchFsm>
typename TSearchFsm::TOutputIdx CFsmCreator::StoreOutput(const typename TSearchFsm::TOutput &output,
	QVector<typename TSearchFsm::TOutput> *pOutputTable)
{
	int idx;
	for (idx = 0; idx < pOutputTable->count(); idx++) {
		if (AreEqual<TSearchFsm>(output, pOutputTable->at(idx))) {
			return idx;
		}
	}

	pOutputTable->push_back(output);
	return pOutputTable->count() - 1;
}

template<class TSearchFsm>
bool CFsmCreator::AreEqual(const typename TSearchFsm::TOutput &output1, const typename TSearchFsm::TOutput &output2) {
	return (output1.patternIdx == output2.patternIdx) && (output1.errorsCount == output2.errorsCount) &&
		(output1.stepBack == output2.stepBack) && (output1.idxNextOutput == output2.idxNextOutput);
}

#endif // FSMCREATOR_H
