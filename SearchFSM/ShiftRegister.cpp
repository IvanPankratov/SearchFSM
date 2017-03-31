#line 2 "ShiftRegister.cpp" // Make __FILE__ omit the path

#include "Common.h"
#include "ShiftRegister.h"

static const int g_nChunkLength = sizeof(CShiftRegister::TChunk) * BITS_IN_BYTE;

// constructor
CShiftRegister::CShiftRegister() {
	m_dwLength = 0;
}

CShiftRegister::CShiftRegister(unsigned int dwLength) {
	Init(dwLength);
}

void CShiftRegister::Init(unsigned int dwLength) {
	m_dwLength = dwLength;
	int nChunks = dwLength / g_nChunkLength; // fully used chunks
	if (dwLength % g_nChunkLength != 0) { // some extra bits
		nChunks++;
	}
	m_Data.fill(0, nChunks);
}

// preparing methods
CShiftRegister::SPattern CShiftRegister::ConvertPattern(const ::SPattern &pattern) {
	SPattern result;
	result.pattern.fill(0, m_Data.length());
	result.mask.fill(0, m_Data.length());
	int nBit;
	for (nBit = 0; nBit < pattern.nLength; nBit++) {
		PushBit(GetBit(pattern, nBit), &result.pattern);
		PushBit(GetMaskBit(pattern, nBit), &result.mask);
	}

	result.dwMaxErrors = pattern.nMaxErrors;

	return result;
}

unsigned int CShiftRegister::RequiredMemorySize() const {
	return m_Data.count() * sizeof(TChunk);
}

// working methods
void CShiftRegister::PushBit(unsigned char bBit) {
	PushBit(bBit, &m_Data);
}

bool CShiftRegister::TestPattern(const SPattern &pattern, unsigned int *pdwErrors) const {
	const TChunk *pChunks = m_Data.data();
	const TChunk *pPatternChunks = pattern.pattern.data();
	const TChunk *pMaskChunks = pattern.mask.data();

	int idx;
	unsigned int cErrors = 0;
	for (idx = 0; idx < m_Data.length(); idx++) {
		const TChunk chunkDiff = (pChunks[idx] ^ pPatternChunks[idx]) & pMaskChunks[idx];
		cErrors += WeightTable(chunkDiff);
	}

	*pdwErrors = cErrors;
	return (cErrors <= pattern.dwMaxErrors);
}

bool CShiftRegister::TestPattern(const CShiftRegister::SPattern &pattern) const {
	unsigned int dwDummy;
	return TestPattern(pattern, &dwDummy);
}

// private
void CShiftRegister::PushBit(unsigned char bBit, CShiftRegister::TData *pData) {
	TChunk carry = bBit;
	TChunk *pChunks = pData->data();
	int idx;
	for (idx = 0; idx < pData->length(); idx++) {
		const TChunk chunk = pChunks[idx];
		pChunks[idx] = (chunk << 1) | carry;
		carry = (chunk >> (g_nChunkLength - 1)) & (TChunk)0x01;
	}
}

unsigned int CShiftRegister::Weight(CShiftRegister::TChunk vector) {
	const TChunk mask1 = 0x55555555;
	TChunk vector2 = (vector & mask1) + ((vector >> 1) & mask1);

	const TChunk mask2 = 0x33333333;
	TChunk vector4 = (vector2 & mask2) + ((vector2 >> 2) & mask2);

	const TChunk mask4 = 0x0f0f0f0f;
	TChunk vector8 = (vector4 + (vector4 >> 4)) & mask4;

	TChunk vector16 = (vector8 + (vector8 >> 8));

	const TChunk mask8n16 = 0x000000ff;
	TChunk vector32 = (vector16 + (vector16 >> 16)) & mask8n16;

	return vector32;
}

unsigned int CShiftRegister::WeightTable(CShiftRegister::TChunk vector) {
	union U32{
		TChunk dw;
		unsigned char bs[4];
	} u32 = {vector};

	int nWeight = ByteWeight(u32.bs[0]) + ByteWeight(u32.bs[1]) + ByteWeight(u32.bs[2]) + ByteWeight(u32.bs[3]);
	return nWeight;
}

unsigned int CShiftRegister::ByteWeight(unsigned char bVector) {
	static const unsigned int nWeight8Bit[] = {
		0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
	};

	return nWeight8Bit[bVector];
}
