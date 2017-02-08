#line 2 "FsmCreator.h" // Make __FILE__ omit the path

#ifndef FSMCREATOR_H
#define FSMCREATOR_H

#include <QList>
#include <QString>

typedef QList <unsigned char> TData;

//////////////////////////////////////////////////////////////////////////
/// \brief The SPattern struct - struct describing searched pattern for SearchFSM
/// Bits are stored in the arrays of bytes (unsigned char) in the \p data field like this:
/// Bits in the whole bytes: 01234567, i.e. MSB is the bit 0, LSB is the bit 7.
/// Bits in partially used byte are stored in the LSBs: xxxx0123, i.e. MSB is unused,
/// LSB is the bit 3.
/// \p nMaxErrors field sets maximum deviation for the SearchFSM for this pattern, it means
/// that the SearchFSM will be able to find this pattern with the acceptable count of wrong bits.
/// \p nLength field indicates pattern length in bits.
///
/// Examples:
/// pattern 11001 is stored like this: 0x19, length = 5
/// pattern 11110101 is stored like this: 0xf5, length = 8
/// pattern 10101100'11001110 is stored like this: 0xac, 0xce, length = 16
/// pattern 11110000'111 is stored so: 0xf0, 0x07, lengnt = 11; (so 5 MSBs of the second byte are
/// unused!)
///
/// Optional \p mask field may indicate which bits are insignificant.
/// Appropriate bits in the mask should be set to zero.
/// This enables you to set partially defined patterns.
/// Bits order is exactly the same as in the pattern's bits.
/// Data's bits corresponding to zero bits in the mask have no meaning.
///
/// Examples:
/// pattern 011--111 is stored like this: data = 0x67, mask = 0xe7, length = 8.
struct SPattern {
	int nLength; // bits count
	int nMaxErrors; // maximum errors count acceptable by the searching FSM
	TData data; // first bit is LSB of data[0]
	TData mask; // positions marked by zero bits are insignificant - optional field
};

QString PatternToString(const SPattern &pattern);

typedef QList<SPattern> TPatterns;

// bits manipulation routine
#ifndef BITS_IN_BYTE
#define BITS_IN_BYTE 8
#endif

inline unsigned char GetHiBit(const unsigned char bData, int nBit) {
	return (bData >> (BITS_IN_BYTE - nBit - 1)) & 0x01;
}

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
