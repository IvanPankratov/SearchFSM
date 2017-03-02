#line 2 "SearchFsm.h" // Make __FILE__ omit the path

#ifndef SEARCHFSM_H
#define SEARCHFSM_H

#ifndef ASSERT
#define ASSERT(x)
#endif


//////////////////////////////////////////////////////////////////////////
/// \brief CSearchFsm<TStateIdx, TOutputIdx, TPatternIdx, TStepBack, TErrorsCount> class - template for SearchFSM
template <class TStateIdx_ = unsigned int, class TOutputIdx_ = unsigned int, class TPatternIdx = unsigned int,
	class TStepBack = unsigned int, class TErrorsCount = unsigned int>
class CSearchFsm {
public:
	typedef TStateIdx_ TStateIdx;
	typedef TOutputIdx_ TOutputIdx;

	static const TOutputIdx sm_outputNull = (TOutputIdx)(-1);

	// FSM table structures
	struct STableCell {
		TStateIdx idxNextState;
		TOutputIdx idxOutput;
	};
	struct STableRow {
		STableCell cell0, cell1;
	};

	// Output table structures
	struct SOutput {
		TPatternIdx patternIdx;
		TStepBack stepBack;
		TErrorsCount errorsCount;
		TOutputIdx idxNextOutput; // used for output chains, when found more than one pattern at once
	};

	// Whole automaton table structure
	struct STable {
		const STableRow *pTableRows;
		const SOutput *pOutputs;
		TStateIdx statesCount;
		TOutputIdx outputsCount;
	};

public: // constructor
	CSearchFsm(const STable &table): m_table(table) {
		Reset();
	}

public: // working methods
	void Reset(TStateIdx state = 0) {
		ASSERT(state < m_table.statesCount);
		m_state = state;
	}

	TOutputIdx PushBit(unsigned char bBit) {
		const STableRow &row = m_table.pTableRows[m_state];
		const STableCell &cell = (bBit == 0)? row.cell0 : row.cell1;
		m_state = cell.idxNextState;
		return cell.idxOutput;
	}

	TStateIdx GetState() const {
		return m_state;
	}

	const SOutput& GetOutput(TOutputIdx idxOutput) const {
		ASSERT(idxOutput<m_table.outputsCount);
		return m_table.pOutputs[idxOutput];
	}

private:
	const STable m_table;
	TStateIdx m_state;
};


//////////////////////////////////////////////////////////////////////////
/// \brief CSearchFsmByte<nBitsAtOnce, TStateIdx, TOutputIdx, TPatternIdx, TStepBack, TErrorsCount>
/// template class for byte SearchFSM
template <const int nBitsAtOnce, class TStateIdx_ = unsigned int, class TOutputIdx_ = unsigned int,
	class TPatternIdx = unsigned int, 	class TStepBack = unsigned int, class TErrorsCount = unsigned int>
class CSearchFsmByte {
public:
	typedef TStateIdx_ TStateIdx;
	typedef TOutputIdx_ TOutputIdx;

	static const TOutputIdx sm_outputNull = (TOutputIdx)(-1);
	static const int g_nColumnsCount = (1 << nBitsAtOnce);
	static const unsigned int g_dwByteMask = g_nColumnsCount - 1;

	// alias to the similar CSearchFsm and its types
	typedef CSearchFsm<TStateIdx, TOutputIdx, TPatternIdx, TStepBack, TErrorsCount> TSearchFsm;
	typedef typename TSearchFsm::STableCell TTableCell;
	typedef typename TSearchFsm::SOutput TOutput;

	// FSM table structures
	struct STableRow {
		TTableCell cells[g_nColumnsCount];
	};

	// Whole automaton table structure
	struct STable {
		const STableRow *pTableRows;
		const TOutput *pOutputs;
		TStateIdx statesCount;
		TOutputIdx outputsCount;
	};

public: // constructor
	CSearchFsmByte(const STable &table): m_table(table) {
		Reset();
	}

public: // working methods
	void Reset(TStateIdx state = 0) {
		ASSERT(state < m_table.statesCount);
		m_state = state;
	}

	TOutputIdx PushByte(unsigned int dwValue) {
		const STableRow &row = m_table.pTableRows[m_state];
		const TTableCell &cell = row.cells[dwValue & g_dwByteMask];
		m_state = cell.idxNextState;
		return cell.idxOutput;
	}

	TStateIdx GetState() const {
		return m_state;
	}

	const typename TSearchFsm::SOutput& GetOutput(TOutputIdx idxOutput) const {
		ASSERT(idxOutput<m_table.outputsCount);
		return m_table.pOutputs[idxOutput];
	}

private:
	const STable m_table;
	TStateIdx m_state;
};

#endif // SEARCHFSM_H
