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
	};

public: // constructor
	CShiftRegister(unsigned int dwLength);

public: // preparing methods
	SPattern ConvertPattern(const ::SPattern &pattern);

public: // working methods
	void PushBit(unsigned char bBit);
	unsigned int TestPattern(const SPattern &pattern) const;

private:
	static void PushBit(unsigned char bBit, /* in-out */ TData *pData);
	static unsigned int Weight(TChunk vector);

private:
	unsigned int m_dwLength;
	TData m_Data;
};

#endif // SHIFTREGISTER_H
