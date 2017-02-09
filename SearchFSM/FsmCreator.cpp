#line 2 "FsmCreator.cpp" // Make __FILE__ omit the path

#include "FsmCreator.h"

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
	SStateDescription state0;
	{ // create initial state - all the parts are empty
		SStatePart emptyPart;
		int idx, nCount = m_patterns.count();
		for (idx = 0; idx < nCount; idx++) {
			state0.parts << emptyPart;
		}
	}

	m_states << state0;

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

	return true;
}

int CFsmCreator::GetStatesCount() const {
	return m_states.count();
}

CFsmCreator::STableRow CFsmCreator::GetTableRow(int nRow) const {
	return m_table[nRow];
}

CFsmCreator::STableCell CFsmCreator::TransitState(const CFsmCreator::SStateDescription &state, unsigned char bBit) {
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

	// look for the same state in the list of the states
	int nStatesCount = m_states.count();
	int nNewStateIdx = -1;
	for (idx = 0; idx < nStatesCount; idx++) {
		if (AreEqual(newState, m_states[idx])) { // found the same state
			nNewStateIdx = idx;
			break;
		}
	}

	// the state is not found - store it
	if (nNewStateIdx == -1) {
		m_states.append(newState);
		nNewStateIdx = m_states.count() - 1;
	}

	STableCell cell;
	cell.nNextState = nNewStateIdx;
	cell.output = outputList;
	return cell;
}

CFsmCreator::SBitResultForPattern CFsmCreator::ProcessBitForPattern(const CFsmCreator::SStatePart &part, const SPattern &pattern, unsigned char bBit) {
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

bool CFsmCreator::AreEqual(const CFsmCreator::SStateDescription &state1, const CFsmCreator::SStateDescription &state2) {
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

bool CFsmCreator::AreEqual(const CFsmCreator::SStatePart &part1, const CFsmCreator::SStatePart &part2) {
	if (part1.prefixes.count() != part2.prefixes.count()) {
		return false;
	}
	int idx;
	for (idx = 0; idx < part1.prefixes.count(); idx++) {
		if (part1.prefixes[idx].nLength != part2.prefixes[idx].nLength ||
			part1.prefixes[idx].nErrors != part2.prefixes[idx].nErrors)
		{
			return false;
		}
	}

	return true;
}

void CFsmCreator::DumpState(const CFsmCreator::SStateDescription &state) {
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

void CFsmCreator::DumpStatePart(const CFsmCreator::SStatePart &part) {
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
