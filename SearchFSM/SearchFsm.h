#ifndef SEARCHFSM_H
#define SEARCHFSM_H

#ifndef ASSERT
#define ASSERT(x)
#endif


//////////////////////////////////////////////////////////////////////////
/// \brief CSearchFsm<TStateIdx, TOutputIdx> class - template for SearchFSM
template <class TStateIdx = unsigned int, class TOutputIdx = unsigned int>
class CSearchFsm {
public:
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
		unsigned char bPatternIdx;
		unsigned char bErrors;
		unsigned char bPosition;
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

#endif // SEARCHFSM_H
