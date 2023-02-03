#include "StringUtility.h"

#include <algorithm>
#include <sstream>
#include <cstdarg>

using namespace Gensys;

std::wstring StringUtility::FormatString(std::wstring const format, ...)
{
	va_list va;
	va_start(va, format);
	wchar_t buffer[1024];
	vswprintf(buffer, format.c_str(), va);	
	va_end(va);
	return std::wstring(buffer);
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

bool StringUtility::StartsWith(std::wstring const value, std::wstring const starts_with, bool const case_insensitive)
{
	if (case_insensitive) {
		return LowerCase(value).rfind(LowerCase(starts_with), 0) == 0;
	}
	return value.rfind(starts_with, 0) == 0;
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

std::wstring StringUtility::LeftTrim(std::wstring const value, wchar_t const trim_char)
{
	std::wstring new_value(value);
	new_value.erase(0, new_value.find_first_not_of(trim_char));
	return new_value;
}

std::wstring StringUtility::RightTrim(std::wstring const value, wchar_t const trim_char)
{
	std::wstring new_value(value);
	new_value.erase(new_value.find_last_not_of(trim_char) + 1);
	return new_value;
}

std::wstring StringUtility::Trim(std::wstring const value, wchar_t const trim_char)
{
	std::wstring new_value(value);
	new_value.erase(0, new_value.find_first_not_of(trim_char));
	new_value.erase(new_value.find_last_not_of(trim_char) + 1);
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
	size_t length = strlen(value_char);
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

int StringUtility::ToInt(std::wstring const value)
{
	int result = 0;
	for (size_t i = 0; i < value.length(); i++)
	{
		result = result * 10;
		result += value.at(i) - 48;
	}
	return result;
}