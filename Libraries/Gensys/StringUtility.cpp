#include "StringUtility.h"

#include "Int.h"

#include <algorithm>
#include <sstream>
#include <cstdarg>

using namespace Gensys;

std::wstring StringUtility::FormatString(std::wstring const format, ...)
{
	va_list va;
	va_start(va, format);
	char buffer[1024];
	vsprintf(buffer, StringUtility::ToString(format).c_str(), va);	
	va_end(va);
	return ToWideString(std::string(buffer));
}

std::string StringUtility::FormatString(std::string const format, ...)
{
	va_list va;
	va_start(va, format);
	char buffer[1024];
	vsprintf(buffer, format.c_str(), va);	
	va_end(va);
	return std::string(buffer);
}

std::wstring StringUtility::LowerCase(std::wstring const value)
{
	std::wstring result = std::wstring(value);
	std::transform(result.begin(), result.end(), result.begin(), tolower);
	return result;
}

std::wstring StringUtility::UpperCase(std::wstring const value)
{
	std::wstring result = std::wstring(value);
	std::transform(result.begin(), result.end(), result.begin(), toupper);
	return result;
}

bool StringUtility::StartsWith(std::wstring const value, std::wstring const startsWith, bool const caseInsensitive)
{
	if (caseInsensitive) {
		return LowerCase(value).find(LowerCase(startsWith)) == 0;
	}
	return value.find(startsWith, 0) == 0;
}

bool StringUtility::EndsWith(std::wstring const value, std::wstring const endsWith, bool const caseInsensitive)
{
	if (caseInsensitive) {
		return LowerCase(value).rfind(LowerCase(endsWith)) == value.length() - endsWith.length();
	}
	return value.rfind(endsWith) == value.length() - endsWith.length();
}

std::wstring StringUtility::Replace(std::wstring const value, std::wstring const from, std::wstring const to)
{
	if (from.empty()) {
		return value;
	}
	std::wstring result = std::wstring(value);
	for(size_t pos = 0; (pos = result.find(from, pos)) != std::wstring::npos; pos += to.size()) {
		result.replace(pos, from.size(), to);
	}
    return result;
}

std::wstring StringUtility::LeftTrim(std::wstring const value, wchar_t const trimChar)
{
	std::wstring new_value(value);
	new_value.erase(0, new_value.find_first_not_of(trimChar));
	return new_value;
}

std::wstring StringUtility::RightTrim(std::wstring const value, wchar_t const trimChar)
{
	std::wstring new_value(value);
	new_value.erase(new_value.find_last_not_of(trimChar) + 1);
	return new_value;
}

std::wstring StringUtility::Trim(std::wstring const value, wchar_t const trimChar)
{
	std::wstring new_value(value);
	new_value.erase(0, new_value.find_first_not_of(trimChar));
	new_value.erase(new_value.find_last_not_of(trimChar) + 1);
	return new_value;
}


std::vector<std::wstring> StringUtility::Split(std::wstring const value, wchar_t const delimiter)
{
	std::wstring buffer;
	std::vector<std::wstring> items;
	for (size_t i = 0; i < value.size(); i++)
	{
		if (value.at(i) == delimiter)
		{
			items.push_back(buffer);
			buffer.clear();
			continue;
		}
		buffer.push_back(value.at(i));
	}
	if (!buffer.empty())
	{
		items.push_back(buffer);
	}
	return items;
}


std::wstring StringUtility::Convert(int const value)
{
	std::wstringstream ss;
	ss << value;
	std::wstring result;
	ss >> result;
	return result;
}

std::wstring StringUtility::Convert(long const value)
{
	std::wstringstream ss;
	ss << value;
	std::wstring result;
	ss >> result;
	return result;
}

std::wstring StringUtility::Convert(bool const value)
{
	return value ? L"true" : L"false";
}

std::wstring StringUtility::Convert(float const value)
{
	std::wstringstream ss;
	ss << value;
	std::wstring result;
	ss >> result;
	return result;
}

std::wstring StringUtility::ToWideString(std::string const value)
{
	if (value.length() == 0)
	{
		return std::wstring(L"");
	}
	const char* value_char = value.c_str();
	size_t length = value.length();
	std::wstring result(length, L'#');
	mbstowcs(&result[0], value_char, length);
	return result;
}

std::string StringUtility::ToString(std::wstring const value)
{
	if (value.length() == 0)
	{
		return std::string("");
	}
	const wchar_t* value_char = value.c_str();
	size_t length = wcslen(value_char);
	std::string result(length, '#');
	wcstombs(&result[0], value_char, length);
	return result;
}

uint32_t StringUtility::ToInt(std::wstring const value)
{
	uint32_t result = 0;
	for (uint32_t i = 0; i < (uint32_t)value.length(); i++)
	{
		result = result * 10;
		result += value.at(i) - 48;
	}
	return result;
}

std::vector<uint8_t> StringUtility::StringToByteArray(std::wstring value) 
{
	const uint8_t* data = (const uint8_t*)(value.c_str());
	const uint32_t size = (uint32_t)(value.size() * sizeof(wchar_t));
	return std::vector<uint8_t>(data, data + size);
}

uint32_t StringUtility::FNVHashString(std::wstring value) 
 {
	uint32_t hash = 2166136261u;
	std::vector<uint8_t> bytes = StringToByteArray(value);
	for (uint32_t i = 0; i < bytes.size(); i++)
    {
        hash ^= bytes.at(i);
        hash *= 16777619u;
    }
    return hash;
}