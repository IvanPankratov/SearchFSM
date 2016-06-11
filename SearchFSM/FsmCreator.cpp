#include "FsmCreator.h"

#ifndef BITS_IN_BYTE
#define BITS_IN_BYTE 8
#endif

unsigned char GetBit(const TData &data, int nBit) {
	int nByteIdx = nBit / BITS_IN_BYTE;
	int nBitIdx = nBit % BITS_IN_BYTE;
	unsigned char bData = data[nByteIdx];
	return (bData >> nBitIdx) & 0x01;
}

unsigned char GetBit(const SPattern &pattern, int nBit) {
	return GetBit(pattern.data, nBit);
}

unsigned char GetMaskBit(const SPattern &pattern, int nBit) {
	if (pattern.mask.isEmpty()) { // no mask given
		return 0x01; // aways true
	}

	return GetBit(pattern.mask, nBit);
}


//////////////////////////////////////////////////////////////////////////
// CFsmCreator
CFsmCreator::CFsmCreator(const TPatterns &patterns):
	m_patterns(patterns)
{}

bool CFsmCreator::GenerateTables() {
	m_states.clear();
	SStateDescription state0;
	{ // create initial state - all the parts are empty
		SStatePart part0;
		int idx, nCount = m_patterns.count();
		for (idx = 0; idx < nCount; idx++) {
			state0.parts << part0;
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

	return true;
}

int CFsmCreator::GetStatesCount() const {
	return m_states.count();
}

STableRow CFsmCreator::GetTableRow(int nRow) const {
	return m_table[nRow];
}

STableCell CFsmCreator::TransitState(const CFsmCreator::SStateDescription &state, unsigned char bBit) {
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
			output.nPosition = m_patterns[idx].nLength;
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
	SStatePart newStatePart;
	// process errors in the very first bit - position 0
	int nErrors0 = 0;
	if (GetMaskBit(pattern, 0) != 0) {
		// first bit is significnt
		nErrors0 += bBit ^ GetBit(pattern, 0);
	}
	newStatePart.nsErrors << nErrors0;

	// process all the other bits
	int idx, nCount = part.nsErrors.count();
	for (idx = 0; idx < nCount; idx++) {
		int nErrors = part.nsErrors[idx];
		int nPosition = idx + 1;
		if (nErrors <= pattern.nMaxErrors) {
			if (GetMaskBit(pattern, nPosition)) {
				nErrors += bBit ^ GetBit(pattern, nPosition);
			}
		}

		if (nPosition < pattern.nLength - 1) {
			newStatePart.nsErrors << nErrors;
		} else if (nErrors <= pattern.nMaxErrors) { // found!
			result.fFound = true;
			result.nErrors = nErrors;
		}
	}

	while (!newStatePart.nsErrors.isEmpty()) {
		if (newStatePart.nsErrors.last() > pattern.nMaxErrors) {
			newStatePart.nsErrors.removeLast();
		} else {
			break;
		}
	}

	result.newStatePart = newStatePart;
	return result;
}

bool CFsmCreator::AreEqual(const CFsmCreator::SStateDescription &state1, const CFsmCreator::SStateDescription &state2) {
	int nPart, nCount = state1.parts.count();
	if (state2.parts.count() != nCount) {
		puts("WTF? O_o");
		return false;
	}
	for (nPart = 0; nPart < nCount; nPart++) {
		// compare parts
		const SStatePart &part1 = state1.parts[nPart];
		const SStatePart &part2 = state2.parts[nPart];
		if (part1.nsErrors != part2.nsErrors) {
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
		DumpStatePart(state.parts[nPart], m_patterns[nPart]);
	}
	printf("}");
}

void CFsmCreator::DumpStatePart(const CFsmCreator::SStatePart &part, const SPattern &pattern) {
	int nPos, nCount = part.nsErrors.count();
	bool fFirst = true;
	for (nPos = nCount - 1; nPos >= 0; nPos--) {
		int nErrors = part.nsErrors[nPos];
		if (nErrors <= pattern.nMaxErrors) {
			if (fFirst) {
				fFirst = false;
			} else {
				printf(", ");
			}
			if (nErrors == 0 ) {
				printf("%i", nPos + 1);
			} else {
				printf("%i-%i", nPos + 1, nErrors);
			}
		}
	}
}

void CFsmCreator::DumpOutput(const TOutputList &output) {
	if (output.isEmpty()) {
		return;
	}

	printf(" {");
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
