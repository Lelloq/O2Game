#pragma once
#include <filesystem>
#include <vector>

struct O2Sample {
	char8_t FileName[32];
	uint32_t RefValue;
	std::vector<uint8_t> AudioData;

	~O2Sample() {
		AudioData.clear();
	}
};

class OJM {
public:
	~OJM();

	void Load(std::filesystem::path& fileName);
	bool IsValid();

	std::vector<O2Sample> Samples;
private:
	void LoadM30Data(std::fstream& fs);
	void LoadOJMData(std::fstream& fs, bool encrypted);
	
	bool m_valid;
};