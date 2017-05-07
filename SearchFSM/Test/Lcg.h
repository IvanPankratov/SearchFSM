#ifndef LCG_H
#define LCG_H

class CLcg { // linear congruent generator, produces period 1 GiB
public:
	CLcg() {
		Reset();
	}

public:
	unsigned int NextRandomEntity() {
		// constants for LCG (stated to be good ones)
		const unsigned int s_dwLcgConstA = 214013;
		const unsigned int s_dwLcgConstC = 2531011;

		// switch to the next member
		m_dwSeed = s_dwLcgConstA * m_dwSeed + s_dwLcgConstC;
		return m_dwSeed;
	}

	unsigned int NextRandom15Bits() {
		const unsigned int s_dw15BitsMask = 0x00007fff;

		// take bits 15-29 which are said to be the best distributed
		return (NextRandomEntity() >> 15) & s_dw15BitsMask;
	}

	unsigned char RandomByte() {
		// construct 8 bits-value from 15 bits
		unsigned int dwValue = NextRandom15Bits();
		return (dwValue & 0xff) + (dwValue >> 8);
	}

	void Reset(unsigned int dwSeed = 0) {
		m_dwSeed = dwSeed;
	}

	unsigned int GetSeed() const {
		return m_dwSeed;
	}

private:
	unsigned int m_dwSeed; // pseudo-random sequence entity
};

class CDoubleLcg {
public:
	CDoubleLcg() {
		Reset();
	}

public:
	unsigned char RandomByte() {
		unsigned char bValue = m_lcg1.RandomByte() ^ m_bLcg2Value;
		unsigned int dwSeed28Bits = m_lcg1.GetSeed() & 0x0fffffff;
		if (dwSeed28Bits == 0) { // LCG-1 made a full cycle
			puts("update LCG-2");
			m_bLcg2Value = m_lcg2.RandomByte();
		}
		return bValue;
	}

	void Reset() {
		m_lcg1.Reset();
		m_lcg2.Reset();
		m_bLcg2Value = 0;
	}

private:
	CLcg m_lcg1, m_lcg2;
	unsigned char m_bLcg2Value;
};

#endif // LCG_H
