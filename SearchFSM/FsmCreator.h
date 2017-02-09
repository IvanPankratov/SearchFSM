#line 2 "FsmCreator.h" // Make __FILE__ omit the path

#ifndef FSMCREATOR_H
#define FSMCREATOR_H

#include <QList>
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

public:
	CFsmCreator(const TPatterns &patterns);

public:
	bool GenerateTables(bool fVerbose = false);
	int GetStatesCount() const;
	STableRow GetTableRow(int nRow) const;

private:
	struct SPrefix {
		int nLength;
		int nErrors;
	};

	struct SStatePart {
		QList<SPrefix> prefixes; // found prefixes ordered by length from the least to the biggest
	};

	struct SBitResultForPattern {
		SStatePart newStatePart;
		bool fFound;
		int nErrors; // valid only if fFound = true
	};

	struct SStateDescription {
		QList<SStatePart> parts;
	};

private:
	STableCell TransitState(const SStateDescription &state, unsigned char bBit);
	static SBitResultForPattern ProcessBitForPattern(const SStatePart &part, const SPattern &pattern, unsigned char bBit);

private:
	static bool AreEqual(const SStateDescription &state1, const SStateDescription &state2);
	static bool AreEqual(const SStatePart &part1, const SStatePart &part2);
	void DumpState(const SStateDescription &state);
	void DumpStatePart(const SStatePart &part);
	void DumpOutput(const TOutputList &output);

private:
	const TPatterns m_patterns;
	QList<SStateDescription> m_states;
	QList<STableRow> m_table;
};

#endif // FSMCREATOR_H
