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

	int nCurrentState;
	for (nCurrentState = 0; nCurrentState < m_states.count(); nCurrentState++) {
		const SStateDescription &state = m_states[nCurrentState];
		int nState0 = TransitState(state, 0);
		int nState1 = TransitState(state, 1);
		printf("%i: ", nCurrentState);
		DumpState(state);
		printf("\n%i =0=> %i\n%i =1=> %i\n\n", nCurrentState, nState0, nCurrentState, nState1);
	}

	return true;
}

int CFsmCreator::TransitState(const CFsmCreator::SStateDescription &state, unsigned char bBit) {
	// create next state
	int idx, nCount = state.parts.count();
	SStateDescription newState;
	for (idx = 0; idx < nCount; idx++) {
		newState.parts << ProcessBitForPattern(state.parts[idx], m_patterns[idx], bBit);
	}

	// look for the same state in the list of the states
	int nStatesCount = m_states.count();
	for (idx = 0; idx < nStatesCount; idx++) {
		if (AreEqual(newState, m_states[idx])) { // found the same state
			return idx;
		}
	}

	// the state is not found - store it
	m_states.append(newState);
	int nNewStateIdx = m_states.count() - 1;
	return nNewStateIdx;
}

CFsmCreator::SStatePart CFsmCreator::ProcessBitForPattern(const CFsmCreator::SStatePart &part, const SPattern &pattern, unsigned char bBit) {
	SStatePart newPart;
	// process errors in the very first bit - position 0
	int nErrors0 = 0;
	if (GetMaskBit(pattern, 0) != 0) {
		// first bit is significnt
		nErrors0 += bBit ^ GetBit(pattern, 0);
	}
	newPart.nsErrors << nErrors0;

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
			newPart.nsErrors << nErrors;
		} else if (nErrors <= pattern.nMaxErrors){
			printf("*");
		}
	}

	while (!newPart.nsErrors.isEmpty()) {
		if (newPart.nsErrors.last() > pattern.nMaxErrors) {
			newPart.nsErrors.removeLast();
		} else {
			break;
		}
	}

	return newPart;
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
