#pragma once

#include <string>
#include <vector>

namespace Gensys
{
	class StringUtility
	{
	public:
		static std::wstring FormatString(std::wstring const format, ...);
		static std::wstring LowerCase(std::wstring const value);
		static std::wstring UpperCase(std::wstring const value);
		static bool StartsWith(std::wstring const value, std::wstring const starts_with, bool const case_insensitive);
		static std::wstring Replace(std::wstring const value, std::wstring const search, std::wstring const with);
		static std::wstring LeftTrim(std::wstring const value, wchar_t const trim_char);
		static std::wstring RightTrim(std::wstring const value, wchar_t const trim_char);
		static std::wstring Trim(std::wstring const value, wchar_t const trim_char);
		static std::vector<std::wstring> Split(std::wstring const value, wchar_t const delimiter);
		static std::wstring Convert(int const value);
		static std::wstring Convert(long const value);
		static std::wstring Convert(bool const value);
		static std::wstring Convert(float const value);
		static std::wstring ToWideString(std::string const value);
		static std::string ToString(std::wstring const value);
		static int ToInt(std::wstring const value);
	};
}