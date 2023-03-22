#pragma once

#include "Int.h"

#include <string>
#include <vector>

namespace Gensys
{
	class StringUtility
	{
	public:
		static std::wstring FormatString(std::wstring const format, ...);
		static std::string FormatString(std::string const format, ...);
		static std::wstring LowerCase(std::wstring const value);
		static std::wstring UpperCase(std::wstring const value);
		static bool StartsWith(std::wstring const value, std::wstring const startWith, bool const caseInsensitive);
		static bool EndsWith(std::wstring const value, std::wstring const endsWith, bool const caseInsensitive);
		static std::wstring Replace(std::wstring const value, std::wstring const search, std::wstring const with);
		static std::wstring LeftTrim(std::wstring const value, wchar_t const trimChar);
		static std::wstring RightTrim(std::wstring const value, wchar_t const trimChar);
		static std::wstring Trim(std::wstring const value, wchar_t const trimChar);
		static std::vector<std::wstring> Split(std::wstring const value, wchar_t const delimiter);
		static std::wstring Convert(int const value);
		static std::wstring Convert(long const value);
		static std::wstring Convert(bool const value);
		static std::wstring Convert(float const value);
		static std::wstring ToWideString(std::string const value);
		static std::string ToString(std::wstring const value);
		static uint32_t ToInt(std::wstring const value);
		static std::vector<uint8_t> StringToByteArray(std::wstring value);
		static uint32_t FNVHashString(std::wstring value);



	};
}