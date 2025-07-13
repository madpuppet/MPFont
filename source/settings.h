#pragma once

#include "main.h"
#include <map>
#include <format>

class Settings
{
	std::map<std::string, std::string> dic;

public:
	template<typename T> void Set(const std::string& key, const T &value)
	{
		dic[key] = std::format("{}", value);
	}

	std::string GetString(const std::string& key)
	{
		auto result = dic.find(key);
		return (result != dic.end()) ? result->second : "";
	}

	bool GetBool(const std::string& key, bool defaultValue)
	{
		auto result = dic.find(key);
		if (result == dic.end()) return defaultValue;
		return result->second == "1" || result->second == "true";
	}

	float GetFloat(const std::string& key, float defaultValue)
	{
		auto result = dic.find(key);
		if (result == dic.end()) return defaultValue;
		return (float)atof(result->second.c_str());
	}

	int GetInt(const std::string& key, int defaultValue)
	{
		auto result = dic.find(key);
		if (result == dic.end()) return defaultValue;
		return atoi(result->second.c_str());
	}

	void Save();
	void Load();
};
extern Settings gSettings;
