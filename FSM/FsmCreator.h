#ifndef FSMCREATOR_H
#define FSMCREATOR_H

#include <QList>

typedef QList <unsigned char> TData;
unsigned char GetBit(const TData &data, int nBit);

struct SPattern {
	int nLength; // bits count
	int nMaxErrors; // maximum errors count acceptable by the searching FSM
	TData data; // first bit is LSB of data[0]
	TData mask; // positions marked by zero bits are insignificant
};

typedef QList<SPattern> TPatterns;

class CFsmCreator {
public:
	CFsmCreator(const TPatterns &patterns);

public:
	bool GenerateTables();

private:
	struct SStatePart {
		QList<int> nsErrors; // errors in pattern prefixes
	};

	struct SStateDescription {
		QList<SStatePart> parts;
	};

private:
	int TransitState(const SStateDescription &state, unsigned char bBit);
	static SStatePart ProcessBitForPattern(const SStatePart &part, const SPattern &pattern, unsigned char bBit);

private:
	static bool AreEqual(const SStateDescription &state1, const SStateDescription &state2);
	void DumpState(const SStateDescription &state);
	void DumpStatePart(const SStatePart &part, const SPattern &pattern);

private:
	const TPatterns m_patterns;
	QList<SStateDescription> m_states;
};

#endif // FSMCREATOR_H
