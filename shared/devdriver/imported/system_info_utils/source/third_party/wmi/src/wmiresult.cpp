/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/

/**
  *
  * WMI
  * @author Thomas Sparber (2016)
  *
 **/

#include <algorithm>
#include <codecvt>
#include <locale>

#include <wmiresult.hpp>

using std::codecvt_utf8;
using std::string;
using std::transform;
using std::vector;
using std::wstring;
using std::wstring_convert;

using namespace Wmi;

wstring unescape(wstring str)
{
	size_t start_pos = 0;
	const wstring from = L"\\\"";
	const wstring to = L"\"";

    while((start_pos = str.find(from, start_pos)) != string::npos)
	{
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }

    return str;
}

bool tokenize(const wstring &str, vector<wstring> &out)
{
	size_t start_pos = str.find(L"[");

	if(start_pos == string::npos)return false;

	do
	{
		start_pos++;
		size_t end_pos = start_pos;
		size_t temp_pos = start_pos;

		while(end_pos != start_pos && temp_pos != string::npos && temp_pos == end_pos)
		{
			end_pos = str.find(L"\"", temp_pos+2);
			temp_pos = str.find(L"\\\"", temp_pos);
		}

		temp_pos = str.find(L",", end_pos);
		if(temp_pos == string::npos)temp_pos = str.find(L"]", end_pos);
		end_pos = temp_pos;

		wstring token = str.substr(str[start_pos] == L'\"' ? start_pos+1 : start_pos, str[start_pos] == L'\"' ? (end_pos - start_pos - 2) : (end_pos - start_pos));
		out.push_back(unescape(token));

		start_pos = end_pos;
		end_pos = start_pos;
		temp_pos = start_pos;
	}
	while(start_pos != string::npos && start_pos+1 != str.length());

	return true;
}

void WmiResult::set(std::size_t index, wstring name, const wstring &value)
{
	while(index >= result.size())result.emplace_back();

	transform(name.begin(), name.end(), name.begin(), ::tolower);
	result[index][name] = value;
}

bool WmiResult::extract(std::size_t index, const string &name, wstring &out) const
{
	if(index >= result.size())return false;

	wstring key(name.cbegin(), name.cend());
	transform(key.begin(), key.end(), key.begin(), ::tolower);

	auto found = result[index].find(key);
	if(found == result[index].cend())return false;

	out = found->second;
	return true;
}

bool WmiResult::extract(std::size_t index, const string &name, string &out) const
{
	wstring temp;
	if(!extract(index, name, temp))return false;

	wstring_convert<codecvt_utf8<wchar_t>> myconv;
	out = myconv.to_bytes(temp);
	return true;
}

bool WmiResult::extract(std::size_t index, const string &name, int &out) const
{
	string temp;
	if(!extract(index, name, temp))return false;

	char *test;
	out = strtol(temp.c_str(), &test, 0);
	return (test == temp.c_str() + temp.length());
}

bool WmiResult::extract(std::size_t index, const string &name, bool &out) const
{
	string temp;
	if(!extract(index, name, temp))return false;

	transform(temp.begin(), temp.end(), temp.begin(), ::tolower);
	if(temp == "true" || temp == "1")out = true;
	else if(temp == "false" || temp == "0")out = false;
	else return false;

	return true;
}

bool WmiResult::extract(std::size_t index, const string &name, uint64_t &out) const
{
	string temp;
	if(!extract(index, name, temp))return false;

	char *test;
	out = strtoull(temp.c_str(), &test, 0);
	return (test == temp.c_str() + temp.length());
}

bool WmiResult::extract(std::size_t index, const string &name, uint32_t &out) const
{
	string temp;
	if(!extract(index, name, temp))return false;

	char *test;
	out = (uint32_t)std::strtoul(temp.c_str(), &test, 0);
	return (test == temp.c_str() + temp.length());
}

bool WmiResult::extract(std::size_t index, const string &name, uint16_t &out) const
{
	string temp;
	if(!extract(index, name, temp))return false;

	char *test;
	out = (uint16_t)std::strtoul(temp.c_str(), &test, 0);
	return (test == temp.c_str() + temp.length());
}

bool WmiResult::extract(std::size_t index, const string &name, uint8_t &out) const
{
	string temp;
	if(!extract(index, name, temp))return false;

	char *test;
	out = (uint8_t)std::strtoul(temp.c_str(), &test, 0);
	return (test == temp.c_str() + temp.length());
}

bool WmiResult::extract(std::size_t index, const string &name, vector<wstring> &out) const
{
	wstring temp;
	if(!extract(index, name, temp))return false;

	if(!tokenize(temp, out))return false;

	return true;
}

bool WmiResult::extract(std::size_t index, const string &name, vector<string> &out) const
{
	vector<wstring> tokens;
	if(!extract(index, name, tokens))return false;

	out.resize(tokens.size());
	for(std::size_t i = 0; i < tokens.size(); ++i)
	{
		wstring_convert<codecvt_utf8<wchar_t>> myconv;
		const wstring &temp = tokens[i];
		out[i] = myconv.to_bytes(temp);
	}

	return true;
}

bool WmiResult::extract(std::size_t index, const string &name, vector<int> &out) const
{
	vector<string> tokens;
	if(!extract(index, name, tokens))return false;

	out.resize(tokens.size());
	for(std::size_t i = 0; i < tokens.size(); ++i)
	{
		char *test;
		out[i] = strtol(tokens[i].c_str(), &test, 0);
		return (test == tokens[i].c_str() + tokens[i].length());
	}

	return true;
}

bool WmiResult::extract(std::size_t index, const string &name, vector<bool> &out) const
{
	vector<string> tokens;
	if(!extract(index, name, tokens))return false;

	out.resize(tokens.size());
	for(std::size_t i = 0; i < tokens.size(); ++i)
	{
		string temp = tokens[i];
		transform(temp.begin(), temp.end(), temp.begin(), ::tolower);
		if(temp == "true" || temp == "1")out[i] = true;
		else if(temp == "false" || temp == "0")out[i] = false;
		else return false;
	}

	return true;
}

bool WmiResult::extract(std::size_t index, const string &name, vector<uint64_t> &out) const
{
	vector<string> tokens;
	if(!extract(index, name, tokens))return false;

	out.resize(tokens.size());
	for(std::size_t i = 0; i < tokens.size(); ++i)
	{
		char *test;
		out[i] = strtoull(tokens[i].c_str(), &test, 0);
		if(test != tokens[i].c_str() + tokens[i].length()) return false;
	}

	return true;
}

bool WmiResult::extract(std::size_t index, const string &name, vector<uint32_t> &out) const
{
	vector<string> tokens;
	if(!extract(index, name, tokens))return false;

	out.resize(tokens.size());
	for(std::size_t i = 0; i < tokens.size(); ++i)
	{
		char *test;
		out[i] = (uint32_t)strtoul(tokens[i].c_str(), &test, 0);
		if (test != tokens[i].c_str() + tokens[i].length()) return false;
	}

	return true;
}

bool WmiResult::extract(std::size_t index, const string &name, vector<uint16_t> &out) const
{
	vector<string> tokens;
	if(!extract(index, name, tokens))return false;

	out.resize(tokens.size());
	for(std::size_t i = 0; i < tokens.size(); ++i)
	{
		char *test;
		out[i] = (uint16_t)strtoul(tokens[i].c_str(), &test, 0);
		if (test != tokens[i].c_str() + tokens[i].length()) return false;
	}

	return true;
}

bool WmiResult::extract(std::size_t index, const string &name, vector<uint8_t> &out) const
{
	vector<string> tokens;
	if(!extract(index, name, tokens))return false;

	out.resize(tokens.size());
	for(std::size_t i = 0; i < tokens.size(); ++i)
	{
		char *test;
		out[i] = (uint8_t)strtoul(tokens[i].c_str(), &test, 0);
		if (test != tokens[i].c_str() + tokens[i].length()) return false;
	}

	return true;
}
