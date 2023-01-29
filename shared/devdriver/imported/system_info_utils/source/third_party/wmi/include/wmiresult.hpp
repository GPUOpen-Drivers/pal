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

#ifndef WMIRESULT_HPP
#define WMIRESULT_HPP

#include <map>
#include <string>
#include <vector>

namespace Wmi
{

	class WmiResult
	{

	public:
		WmiResult() :
			result()
		{}

		void set(std::size_t index, std::wstring name, const std::wstring &value);

		std::vector<std::map<std::wstring,std::wstring> >::iterator begin()
		{
			return result.begin();
		}

		std::vector<std::map<std::wstring,std::wstring> >::iterator end()
		{
			return result.end();
		}

		std::vector<std::map<std::wstring,std::wstring> >::const_iterator cbegin() const
		{
			return result.cbegin();
		}

		std::vector<std::map<std::wstring,std::wstring> >::const_iterator cend() const
		{
			return result.cend();
		}

		std::size_t size() const
		{
			return result.size();
		}

		bool extract(std::size_t index, const std::string &name, std::wstring &out) const;
		bool extract(std::size_t index, const std::string &name, std::string &out) const;
		bool extract(std::size_t index, const std::string &name, int &out) const;
		bool extract(std::size_t index, const std::string &name, bool &out) const;
		bool extract(std::size_t index, const std::string &name, uint64_t &out) const;
		bool extract(std::size_t index, const std::string &name, uint32_t &out) const;
		bool extract(std::size_t index, const std::string &name, uint16_t &out) const;
		bool extract(std::size_t index, const std::string &name, uint8_t &out) const;

		bool extract(std::size_t index, const std::string &name, std::vector<std::wstring> &out) const;
		bool extract(std::size_t index, const std::string &name, std::vector<std::string> &out) const;
		bool extract(std::size_t index, const std::string &name, std::vector<int> &out) const;
		bool extract(std::size_t index, const std::string &name, std::vector<bool> &out) const;
		bool extract(std::size_t index, const std::string &name, std::vector<uint64_t> &out) const;
		bool extract(std::size_t index, const std::string &name, std::vector<uint32_t> &out) const;
		bool extract(std::size_t index, const std::string &name, std::vector<uint16_t> &out) const;
		bool extract(std::size_t index, const std::string &name, std::vector<uint8_t> &out) const;

	private:
		std::vector<std::map<std::wstring,std::wstring> > result;

	}; //end class WmiResult

}; //end namespace Wmi

#endif
