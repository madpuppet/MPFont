#pragma once

#include <string.h>
#include <stdio.h>
#include <format>

// Shad class lets resources directly create/read/write shad files
// Resources can use this to convert a text file into some sort of asset binary
//
// Non-resource classes such as managers or arbitrary game code should create ShadBin resources to load text files using the asset system

// case insensitive equal
inline bool StringEqual(const char* str1, const char* str2)
{
#if defined(_WIN32)
	return str1 && str2 && !_stricmp(str1, str2);
#else
	return str1 && str2 && !strcasecmp(str1, str2);
#endif
}

// case insensitive equal
inline bool StringEqual(const std::string& str1, const std::string& str2)
{
	return StringEqual(str1.c_str(), str2.c_str());
}

inline bool StringToBool(const std::string& str) { return StringEqual(str, "true") || str == "1"; }
inline f32 StringToF32(const std::string& str) { return (f32)atof(str.c_str()); }
inline i32 StringToI32(const std::string& str) { return (i32)atoi(str.c_str()); }
u32 StringToHex(const std::string& str);

struct vec2
{
	float x, y;
};
struct vec3
{
	float x, y, z;
};
struct vec4
{
	float x, y, z, w;
};
struct ivec2
{
	int x, y;
};
struct ivec3
{
	int x, y, z;
};
struct ivec4
{
	int x, y, z, w;
};


struct ShadNode
{
	int indent = 0;
	std::string field;
	std::vector<std::string> values;
	std::vector<ShadNode*> children;

	bool IsName(const std::string& str) { return StringEqual(field, str); }
	bool IsName(const char* str) { return StringEqual(field.c_str(), str); }

	std::vector<ShadNode*>& GetChildren() { return children; }

	u32 GetValueCount() const { return (u32)values.size(); }
	std::string GetValue(int idx) const { return idx < values.size() ? values[idx] : ""; }
	std::vector<std::string> GetValues() { return values; }

	std::string GetString(int idx = 0) const { return idx < values.size() ? values[idx] : ""; }
	bool GetBool(int idx = 0) const { return idx < values.size() ? StringToBool(values[idx]) : false; }
	f32 GetF32(int index = 0) const { return StringToF32(GetValue(index)); }
	i32 GetI32(int index = 0) const { return StringToI32(GetValue(index)); }
	vec2 GetVector2(int index = 0) const { return { GetF32(index), GetF32(index + 1) }; }
	vec3 GetVector3(int index = 0) const { return { GetF32(index), GetF32(index + 1), GetF32(index + 2) }; }
	vec4 GetVector4(int index = 0) const { return { GetF32(index), GetF32(index + 1), GetF32(index + 2), GetF32(index + 3) }; }
	ivec2 GetVector2i(int index = 0) const { return { GetI32(index), GetI32(index + 1) }; }
	ivec3 GetVector3i(int index = 0) const { return { GetI32(index), GetI32(index + 1), GetI32(index + 2) }; }
	ivec4 GetVector4i(int index = 0) const { return { GetI32(index), GetI32(index + 1), GetI32(index + 2), GetI32(index + 3) }; }
	vec2 GetRange(int index = 0) const { vec2 val; if (values.size() <= (index + 1)) { val.x = val.y = GetF32(); } else { val = GetVector2(index); }; return val; }

	ShadNode* AddChild(const std::string& field)
	{
		ShadNode* child = new ShadNode;
		child->field = field;
		child->indent = indent++;
		children.push_back(child);
		return child;
	}

	template <typename T>
	ShadNode* AddChild(const std::string& field, const T &value)
	{
		ShadNode* child = new ShadNode;
		child->field = field;
		child->values.push_back(std::format("{}", value));
		child->indent = indent++;
		children.push_back(child);
		return child;
	}
};

class Shad
{
	std::vector<ShadNode*> m_roots;

public:
	void Parse(const char* mem, u32 size);
	void Write(char* &outmem, u32 &size);

	const std::vector<ShadNode*>& GetRoots() const { return m_roots; }
	void AddRoot(ShadNode* node) { m_roots.push_back(node); }

private:
	void WriteAsciiNodes(std::vector<ShadNode*>& nodes, std::vector<char>& stream, int indent);
	void AddNodeChildren(ShadNode* parent, std::vector<ShadNode*>& nodes, int& idx);
};


