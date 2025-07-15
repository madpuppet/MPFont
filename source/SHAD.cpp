#include "main.h"
#include "SHAD.h"
#include <format>

u32 StringToHex(const std::string & str)
{
	u32 result = 0;
	const char* in = str.c_str();
	char ch;
	while ((ch = *in++) != 0)
	{
		result = result << 4;
		if (ch >= 'a' && ch <= 'f')
			result += ((int)ch - 'a' + 10);
		else if (ch >= 'A' && ch <= 'F')
			result += ((int)ch - 'A' + 10);
		else if (ch >= '0' && ch <= '9')
			result += ((int)ch - '0');
	}
	return result;
}


void GrabToken(const char*& mem, const char* end, char* buffer, char* bufferEnd, bool isField, char*& tokenEnd, bool& isEOL)
{
	isEOL = false;

	// skip white space
	while (mem < end && (*mem == ' ' || *mem == '\t' || *mem == 10 || *mem == 13))
		mem++;

	// skip comments
	if (*mem == '#')
	{
		while (mem < end && (*mem != 10 || *mem != 13))
			mem++;
		while (mem < end && (*mem == 10 || *mem == 13))
			mem++;
		isEOL = true;
	}

	char* out = buffer;
	bool quotes = false;
	while (mem < end)
	{
		if (*mem == '"')
		{
			quotes = !quotes;
			mem++;

			// move buffer forward so we can't rewind past this point when trimming end white space
			buffer = out;
		}
		else if (!quotes && ((isField && *mem == ':') || (!isField && *mem == ',')))
		{
			mem++;
			break;
		}
		else if ((!quotes && *mem == '#') || *mem == 13 || *mem == 10)
		{
			break;
		}
		else if (out < bufferEnd)
		{
			*out++ = *mem++;
		}
	}

	// skip trailing white space
	while (mem < end && (*mem == ' ' || *mem == '\t'))
		mem++;

	// skip trailing comments
	if (*mem == '#')
	{
		while (mem < end && (*mem != 10 || *mem != 13))
			mem++;
		while (mem < end && (*mem == 10 || *mem == 13))
			mem++;
		isEOL = true;
	}

	while (*mem == 10 || *mem == 13)
	{
		isEOL = true;
		mem++;
	}

	// skip trailing white space
	while (out > buffer && (out[-1] == ' ' || out[-1] == '\t'))
		out--;

	tokenEnd = out;
}


ShadNode* ParseNode(const char*& mem, const char* end)
{
	char lineBuffer[2048];

	// skip eol
	while (mem < end && (*mem == 13 || *mem == 10))
		mem++;

	int indent = 0;
	while ((*mem == ' ' || *mem == '\t') && (mem < end))
	{
		mem++;
		indent++;
	}

	if (mem == end)
		return nullptr;

	bool isEOL;
	char* tokenEnd;
	GrabToken(mem, end, lineBuffer, &lineBuffer[2048], true, tokenEnd, isEOL);
	if (tokenEnd == lineBuffer)
		return nullptr;

	auto node = new ShadNode;
	node->indent = indent;
	std::string field(lineBuffer, tokenEnd);
	node->field = std::move(field);

	while (!isEOL && mem < end)
	{
		GrabToken(mem, end, lineBuffer, &lineBuffer[2048], false, tokenEnd, isEOL);
		if (tokenEnd != lineBuffer)
		{
			std::string field(lineBuffer, tokenEnd);
			node->values.push_back(std::move(field));
		}
	}
	return node;
}

void AddNode(std::vector<ShadNode*>& children, ShadNode* o)
{
	if (children.empty() || children[0]->indent == o->indent)
	{
		children.push_back(o);
	}
	else
	{
		AddNode(children.back()->children, o);
	}
}

void Shad::AddNodeChildren(ShadNode* parent, std::vector<ShadNode*>& nodes, int& idx)
{
	while (idx < nodes.size() && nodes[idx]->indent > parent->indent)
	{
		ShadNode* node = nodes[idx++];
		parent->children.push_back(node);
		AddNodeChildren(node, nodes, idx);
	}
}

void Shad::Parse(const char* mem, u32 size)
{
	std::vector<ShadNode*> nodes;
	const char* ptr = mem;
	const char* endPtr = mem + size;
	while (ptr != endPtr)
	{
		auto node = ParseNode(ptr, endPtr);
		nodes.push_back(node);
	}

	int idx = 0;
	while (idx < (int)nodes.size())
	{
		ShadNode* node = nodes[idx++];
		m_roots.push_back(node);
		AddNodeChildren(node, nodes, idx);
	}
}

void Shad::Write(char*& outmem, u32& size)
{
	std::vector<char> out;
	WriteAsciiNodes(m_roots, out, 0);
	outmem = new char[out.size()];
	memcpy(outmem, out.data(), out.size());
	size = (u32)out.size();
}

void Shad::WriteAsciiNodes(std::vector<ShadNode*>& nodes, std::vector<char> &stream, int indent)
{
	for (auto node : nodes)
	{
		std::string out = std::format("{:{}}{}", " ", indent, node->field);
		u32 valueCount = node->GetValueCount();
		if (valueCount > 0)
		{
			out += ": ";
			for (u32 i = 0; i < valueCount; i++)
			{
				out += node->GetString(i);
				if (i < valueCount - 1)
					out += ", ";
			}
		}
		out += "\n";

		for (auto ch : out)
			stream.push_back(ch);

		auto& children = node->GetChildren();
		if (children.size())
		{
			WriteAsciiNodes(children, stream, indent + 4);
		}
	}
}
