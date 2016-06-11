#include "FsmCreator.h"

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

	int nCurrentState = 0;
	while (nCurrentState < m_states.count()) {
		const SStateDescription &state = m_states[nCurrentState];
		int nState0 = TransitState(state, 0);
		int nState1 = TransitState(state, 1);
		printf("%i =0=> %i\n%i =1=> %i\n\n", nCurrentState, nState0, nCurrentState, nState1);
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
		if (Compare(newState, m_states[idx])) { // found the same state
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
	if (GetBit(pattern.mask, 0) != 0) {
		// first bit is significnt
		nErrors0 += bBit ^ GetBit(pattern.data, 0);
	}
	newPart.nsErrors << nErrors0;

	// process all the other bits
	int idx, nCount = part.nsErrors.count();
	for (idx = 0; idx < nCount; idx++) {

	}

	return newPart;
}
