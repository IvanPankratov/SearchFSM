#ifndef FSMCREATOR_H
#define FSMCREATOR_H

#include <QList>

typedef QList <unsigned char> TData;
unsigned char GetBit(const TData &data, int nBit);

struct SPattern {
	int nLength; // bits count
	int nMaxErrors; // maximum errors count acceptable by the searching FSM
	TData data; // first bit is LSB of data[0]
	TData mask; // positions marked by zero bits are insignificant - optional field
};

typedef QList<SPattern> TPatterns;

struct SOutput {
	int nPatternIdx;
	int nErrors;
	int nPosition;
};
typedef QList<SOutput> TOutputList;

struct STableCell {
	int nNextState;
	TOutputList output;
};
struct STableRow {
	STableCell cell0, cell1;
};


//////////////////////////////////////////////////////////////////////////
/// \brief The CFsmCreator class - builds tables for FSM
class CFsmCreator {
public:
	CFsmCreator(const TPatterns &patterns);

public:
	bool GenerateTables();

private:
	struct SStatePart {
		QList<int> nsErrors; // errors in pattern prefixes
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
	void DumpState(const SStateDescription &state);
	void DumpStatePart(const SStatePart &part, const SPattern &pattern);
	void DumpOutput(const TOutputList &output);

private:
	const TPatterns m_patterns;
	QList<SStateDescription> m_states;
	QList<STableRow> m_table;
};

#endif // FSMCREATOR_H
