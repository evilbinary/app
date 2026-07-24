#ifndef CPU_H
#define CPU_H

#include <string>
#include <vector>

class Cpu {
private:
	unsigned long defaultAppClock;
	std::vector<unsigned long> frequencies;

public:
	Cpu();

	unsigned long getDefaultAppClock() { return defaultAppClock; }

	std::vector<std::string> getFrequencies();
	void setCpuSpeed(unsigned long khz);

	std::string freqStr(unsigned long khz);
	unsigned long freqFromStr(const std::string& str);
};

#endif
