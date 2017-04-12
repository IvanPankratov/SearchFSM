#line 2 "FsmCreator.cpp" // Make __FILE__ omit the path

#include "FsmCreator.h"

#include <QQueue>

QString PatternToString(const SPattern &pattern) {
	QString sPattern;
	int nBit;
	for (nBit = 0; nBit < pattern.nLength; nBit++) {
		if (GetMaskBit(pattern, nBit) != 0) {
			sPattern.append(QString::number(GetBit(pattern, nBit)));
		} else {
			sPattern.append("-");
		}
	}

	return sPattern;
}


//////////////////////////////////////////////////////////////////////////
// CFsmCreator
CFsmCreator::CFsmCreator(const TPatterns &patterns):
	m_patterns(patterns)
{}

bool CFsmCreator::GenerateTables(bool fVerbose) {
	m_states.clear();
	m_idxStates.clear();
	m_dwCollisions = 0;

	{ // create initial state - all the parts are empty
		SStateDescription state0;
		SStatePart emptyPart;
		int idx, nCount = m_patterns.count();
		for (idx = 0; idx < nCount; idx++) {
			state0.parts << emptyPart;
		}
		AddState(state0);
	}

	m_table.clear();
	int nCurrentState;
	for (nCurrentState = 0; nCurrentState < m_states.count(); nCurrentState++) {
		STableRow row;
		const SStateDescription &state = m_states[nCurrentState];
		row.cell0 = TransitState(state, 0);
		row.cell1 = TransitState(state, 1);
		m_table.append(row);

		if (fVerbose) {
			// output (for debug reason)
			int nState0 = row.cell0.nNextState;
			int nState1 = row.cell1.nNextState;
			printf("%i: ", nCurrentState);
			DumpState(state);
			printf("\n%i =0=> %i", nCurrentState, nState0);
			DumpOutput(row.cell0.output);
			printf("\n%i =1=> %i", nCurrentState, nState1);
			DumpOutput(row.cell1.output);
			printf("\n\n");
		}
	}

	// no needed anymore
	m_states.clear();
	m_idxStates.clear();

	return true;
}

bool CFsmCreator::OptimizeTables(bool fVerbose) {
	const int g_nNoState = -1; // index for states to be removed

	QSet<int> nsUnessentialStates = FindUnessentialStates(fVerbose);
	if (nsUnessentialStates.isEmpty()) { // no unessential states
		return true;
	}

	// reindex essential states (for removing unessential ones)
	int nState, nStatesCount = m_table.count();
	QVector<int> nsStatesShift(nStatesCount); // old state -> new state
	int cEssentialStates = 0;
	for (nState = 0; nState < nStatesCount; nState++) {
		if (nsUnessentialStates.contains(nState)) { // unessential state -> -1
			nsStatesShift[nState] = g_nNoState;

		} else { // essential state
			nsStatesShift[nState] = cEssentialStates;
			cEssentialStates++;
		}
	}

	// create new FSM table (with new indexes)
	QVector<STableRow> tableNew(cEssentialStates);
	for (nState = 0; nState < nStatesCount; nState++) {
		int nNewStateIdx = nsStatesShift[nState];
		if (nNewStateIdx != g_nNoState) {
			STableRow row = m_table[nState];
			row.cell0.nNextState = nsStatesShift[row.cell0.nNextState];
			row.cell1.nNextState = nsStatesShift[row.cell1.nNextState];
			tableNew[nNewStateIdx] = row;
		}
	}

	// substitute FSM table with a newly created one
	m_table = tableNew;

	return true;
}

QSet<int> CFsmCreator::FindUnessentialStates(bool fVerbose) const {
	// count incoming arcs for each state
	int nStatesCount = m_table.count();
	QVector<int> nsIncomingArcs(nStatesCount, 0);
	int nState;
	for (nState = 0; nState < nStatesCount; nState++) {
		const STableRow &row = m_table[nState];
		nsIncomingArcs[row.cell0.nNextState]++;
		nsIncomingArcs[row.cell1.nNextState]++;
	}

	// locate unessential states
	QQueue<int> qsZeroStates; // states with zero incoming arcs
	if (nsIncomingArcs[0] == 0) { // only this state might be with zero incoming arcs at the beginning
		qsZeroStates.enqueue(0); // zero state
	}
	QSet<int> nsUnessentialStates;
	while (!qsZeroStates.isEmpty()) {
		int nZeroState = qsZeroStates.dequeue();
		nsUnessentialStates << nZeroState;
		const STableRow &row = m_table[nZeroState];
		int nNextState0 = row.cell0.nNextState;
		nsIncomingArcs[nNextState0]--;
		if (nsIncomingArcs[nNextState0] == 0) {
			qsZeroStates.enqueue(nNextState0);
		}
		int nNextState1 = row.cell1.nNextState;
		nsIncomingArcs[nNextState1]--;
		if (nsIncomingArcs[nNextState1] == 0) {
			qsZeroStates.enqueue(nNextState1);
		}

		if (fVerbose) {
			printf("remove state %i -> %i, %i\n", nZeroState, nNextState0, nNextState1);
		}
	}

	return nsUnessentialStates;
}

CFsmCreator::TByteTable CFsmCreator::CreateByteTable(int nBitsAtOnce, CFsmCreator::EBitOrder bitOrder) const {
	const unsigned int dwColumnsCount = 1 << nBitsAtOnce;
	const int nRowsCount = m_table.count();
	TByteTable table;
	table.rows.resize(nRowsCount);
	int nRow;
	for (nRow = 0; nRow < nRowsCount; nRow++) {
		table.rows[nRow].cells.resize(dwColumnsCount);
		unsigned int dwValue;
		for (dwValue = 0; dwValue < dwColumnsCount; dwValue++) {
			// in the state nRow got byte dwValue bit-by-bit
			int nState = nRow;
			TOutputList outputList;
			int nBit;
			for (nBit = 0; nBit < nBitsAtOnce; nBit++) {
				unsigned char bBit;
				if (bitOrder == bitOrder_LsbFirst) {
					bBit = (dwValue >> nBit) & 0x01;
				} else {
					bBit = (dwValue >> (nBitsAtOnce - nBit - 1)) & 0x01;
				}

				const STableRow &row = m_table[nState];
				const STableCell &cell = (bBit == 0)? row.cell0 : row.cell1;
				nState = cell.nNextState;
				int nBitsRemained = nBitsAtOnce - nBit - 1;
				int idxOut;
				for (idxOut = 0; idxOut < cell.output.count(); idxOut++) {
					SOutput output = cell.output[idxOut];
					output.nStepBack += nBitsRemained;
					outputList.append(output);
				}
			}

			STableCell cell;
			cell.nNextState = nState;
			cell.output = outputList;
			table.rows[nRow].cells[dwValue] = cell;
		}
	}

	return table;
}

int CFsmCreator::GetStatesCount() const {
	return m_table.count();
}

unsigned int CFsmCreator::GetCollisionsCount() const {
	return m_dwCollisions;
}

const CFsmCreator::STableRow &CFsmCreator::GetTableRow(int nRow) const {
	return m_table[nRow];
}

CFsmCreator::STableCell CFsmCreator::TransitState(const SStateDescription &state, unsigned char bBit) {
	// create next state
	int idx, nCount = state.parts.count();
	TOutputList outputList;
	SStateDescription newState;
	for (idx = 0; idx < nCount; idx++) {
		SBitResultForPattern bitResult = ProcessBitForPattern(state.parts[idx], m_patterns[idx], bBit);
		newState.parts << bitResult.newStatePart;
		if (bitResult.fFound) {
			// pattern found
			SOutput output;
			output.nPatternIdx = idx;
			output.nErrors = bitResult.nErrors;
			output.nStepBack = m_patterns[idx].nLength;
			outputList << output;
		}
	}

	STableCell cell;
	cell.nNextState = AddState(newState);
	cell.output = outputList;
	return cell;
}

int CFsmCreator::AddState(const CFsmCreator::SStateDescription &state) {
	TStateHash hash = Hash(state);
	TIndexList list = m_idxStates[hash];
	// look for the same state in the list of the states with the same hash
	int idx, nStatesCount = list.count();
	for (idx = 0; idx < nStatesCount; idx++) {
		int nStateIdx = list[idx];
		if (AreEqual(state, m_states[nStateIdx])) { // found the same state
			return nStateIdx;
		} else if (idx == nStatesCount - 1) { // no equal state in the list - hash collision
			m_dwCollisions++;
		}
	}

	// the state is not found - store it
	m_states.append(state);
	int nNewStateIdx = m_states.count() - 1;
	m_idxStates[hash] << nNewStateIdx;
	return nNewStateIdx;
}

CFsmCreator::SBitResultForPattern CFsmCreator::ProcessBitForPattern(const SStatePart &part, const SPattern &pattern, unsigned char bBit) {
	SBitResultForPattern result;
	result.fFound = false;
	result.nErrors = -1;

	SStatePart nextStatePart;
	// process prefix of length 1
	int nErrors1 = 0;
	if (GetMaskBit(pattern, 0) != 0) { // first bit is significnt
		nErrors1 += bBit ^ GetBit(pattern, 0);
	}
	if (nErrors1 <= pattern.nMaxErrors) {
		SPrefix prefix1 = {1, nErrors1};
		nextStatePart.prefixes << prefix1;
	}

	// process all the other bits
	int idx, nCount = part.prefixes.count();
	for (idx = 0; idx < nCount; idx++) {
		const SPrefix &prefix = part.prefixes[idx];
		int nNextLength = prefix.nLength + 1;
		int nNextErrors = prefix.nErrors;
		int nBit = nNextLength - 1;
		if (GetMaskBit(pattern, nBit) != 0) {
			nNextErrors += bBit ^ GetBit(pattern, nBit);
		}

		if (nNextErrors <= pattern.nMaxErrors) { // errors count is acceptable
			if (nNextLength == pattern.nLength) { // the last bit
				result.fFound = true;
				result.nErrors = nNextErrors;

			} else {
				SPrefix nextPrefix = {nNextLength, nNextErrors};
				nextStatePart.prefixes << nextPrefix;
			}
		}
	}

	result.newStatePart = nextStatePart;
	return result;
}

bool CFsmCreator::AreEqual(const SStateDescription &state1, const SStateDescription &state2) {
	int nPart, nCount = state1.parts.count();
	if (state2.parts.count() != nCount) {
		puts("WTF? O_o");
		return false;
	}
	for (nPart = 0; nPart < nCount; nPart++) {
		if (!AreEqual(state1.parts[nPart], state2.parts[nPart])) {
			return false;
		}
	}
	return true;
}

bool CFsmCreator::AreEqual(const SStatePart &part1, const SStatePart &part2) {
	if (part1.prefixes.count() != part2.prefixes.count()) {
		return false;
	}
	int nPrefix, nPrefixesCount = part1.prefixes.count();
	for (nPrefix = 0; nPrefix < nPrefixesCount; nPrefix++) {
		if (part1.prefixes[nPrefix].nLength != part2.prefixes[nPrefix].nLength ||
			part1.prefixes[nPrefix].nErrors != part2.prefixes[nPrefix].nErrors)
		{
			return false;
		}
	}

	return true;
}

CFsmCreator::TStateHash CFsmCreator::Hash(const SStateDescription &state) {
	const unsigned int g_dwHashMultiplier = 3571;
	const unsigned int g_dwHashStatePartMultiplier = 1907;

	unsigned int dwHash = 0;
	int nPart, nCount = state.parts.count();
	for (nPart = 0; nPart < nCount; nPart++) {
		dwHash += nPart;
		dwHash *= g_dwHashStatePartMultiplier;

		const SStatePart &part = state.parts[nPart];
		int nPrefix, nPrefixesCount = part.prefixes.count();
		for (nPrefix = 0; nPrefix < nPrefixesCount; nPrefix++) {
			const SPrefix &prefix = part.prefixes[nPrefix];
			dwHash += prefix.nLength;
			dwHash *= g_dwHashMultiplier;
			dwHash += prefix.nErrors;
			dwHash *= g_dwHashMultiplier;
		}
	}

	return dwHash;
}

void CFsmCreator::DumpState(const SStateDescription &state) {
	int nPart, nCount = state.parts.count();
	printf("{");
	for (nPart = 0; nPart < nCount; nPart++) {
		if (nPart > 0) { // separate state parts
			printf(" | ");
		}
		DumpStatePart(state.parts[nPart]);
	}
	printf("}");
}

void CFsmCreator::DumpStatePart(const SStatePart &part) {
	int idx, nCount = part.prefixes.count();
	bool fFirst = true;
	for (idx = nCount - 1; idx >= 0; idx--) {
		if (fFirst) {
			fFirst = false;
		} else {
			printf(", ");
		}
		int nErrors = part.prefixes[idx].nErrors;
		if (nErrors == 0 ) {
			printf("%i", idx + 1);
		} else {
			printf("%i-%i", idx + 1, nErrors);
		}
	}
}

void CFsmCreator::DumpOutput(const TOutputList &output) {
	if (output.isEmpty()) {
		return;
	}

	printf(" * {");
	int idx, nCount = output.count();
	bool fFirst = true;
	for (idx = 0; idx < nCount; idx++) {
		// print comma if necessary
		if (fFirst) {
			fFirst = false;
		} else {
			printf(", ");
		}

		SOutput out = output[idx];
		int nErrors = out.nErrors;
		if (nErrors == 0 ) {
			printf("%i", out.nPatternIdx);
		} else {
			printf("%i-%i", out.nPatternIdx, nErrors);
		}
	}
	printf("}");
}
