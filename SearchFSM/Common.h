#line 2 "Common.h" // Make __FILE__ omit the path

#ifndef COMMON_H
#define COMMON_H

#include <QList>

// bits manipulation routine
#ifndef BITS_IN_BYTE
#define BITS_IN_BYTE 8
#endif

inline unsigned char GetHiBit(const unsigned char bData, int nBit) {
	return (bData >> (BITS_IN_BYTE - nBit - 1)) & 0x01;
}

inline unsigned char HiNibble(const unsigned char bData) {
	return bData >> 4;
}

inline unsigned char LoNibble(const unsigned char bData) {
	return bData & 0x0f;
}


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
	typedef QList <unsigned char> TData;

	int nLength; // bits count
	int nMaxErrors; // maximum errors count acceptable by the searching FSM
	TData data; // first bit is LSB of data[0]
	TData mask; // positions marked by zero bits are insignificant - optional field
};

typedef QList<SPattern> TPatterns;

inline unsigned char GetBit(const SPattern::TData &data, int nBit, int nLength) {
	int nBytesCount = (nLength - 1) / BITS_IN_BYTE + 1;
	int nByteIdx = nBit / BITS_IN_BYTE;
	int nBitIdx = nBit % BITS_IN_BYTE;
	if (nByteIdx == nBytesCount - 1) { // last byte (possibly not whole)
		int nBitsInLastByte = (nLength - 1) % BITS_IN_BYTE + 1;
		nBitIdx += BITS_IN_BYTE - nBitsInLastByte;
	}
	unsigned char bData = data[nByteIdx];
	return GetHiBit(bData, nBitIdx);
}

inline unsigned char GetBit(const SPattern &pattern, int nBit) {
	return GetBit(pattern.data, nBit, pattern.nLength);
}

inline unsigned char GetMaskBit(const SPattern &pattern, int nBit) {
	if (pattern.mask.isEmpty()) { // no mask given
		return 0x01; // aways true
	}

	return GetBit(pattern.mask, nBit, pattern.nLength);
}

#endif // COMMON_H
