#line 2 "ShiftRegister.cpp" // Make __FILE__ omit the path

#include "Common.h"
#include "ShiftRegister.h"

static const int g_nChunkLength = sizeof(CShiftRegister::TChunk) * BITS_IN_BYTE;

// constructor
CShiftRegister::CShiftRegister(unsigned int dwLength) {
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

	return result;
}

// working methods
void CShiftRegister::PushBit(unsigned char bBit) {
	PushBit(bBit, &m_Data);
}

unsigned int CShiftRegister::TestPattern(const SPattern &pattern) const {
	const TChunk *pChunks = m_Data.data();
	const TChunk *pPatternChunks = pattern.pattern.data();
	const TChunk *pMaskChunks = pattern.mask.data();

	int idx;
	unsigned int cErrors = 0;
	for (idx = 0; idx < m_Data.length(); idx++) {
		const TChunk chunkDiff = (pChunks[idx] ^ pPatternChunks[idx]) & pMaskChunks[idx];
		cErrors += Weight(chunkDiff);
	}

	return cErrors;
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
