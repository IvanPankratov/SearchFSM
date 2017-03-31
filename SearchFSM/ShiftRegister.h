#line 2 "ShiftRegister.h" // Make __FILE__ omit the path

#ifndef SHIFTREGISTER_H
#define SHIFTREGISTER_H

#include <QVector>

#include "Common.h"

class CShiftRegister {
public:
	typedef unsigned int TChunk;
	typedef QVector <TChunk> TData;

	struct SPattern { // local format for quick comparison
		TData pattern;
		TData mask;
		unsigned int dwMaxErrors;
	};

public: // constructor
	CShiftRegister();
	CShiftRegister(unsigned int dwLength);

public: // preparing methods
	void Init(unsigned int dwLength);
	SPattern ConvertPattern(const ::SPattern &pattern);
	unsigned int RequiredMemorySize() const;

public: // working methods
	void PushBit(unsigned char bBit);
	bool TestPattern(const SPattern &pattern, unsigned int *pdwErrors) const;
	bool TestPattern(const SPattern &pattern) const;

private:
	static void PushBit(unsigned char bBit, /* in-out */ TData *pData);
	static unsigned int Weight(TChunk vector);
	static unsigned int WeightTable(TChunk vector);
	static unsigned int ByteWeight(unsigned char bVector);

private:
	unsigned int m_dwLength;
	TData m_Data;
};

#endif // SHIFTREGISTER_H
