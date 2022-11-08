/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef WMI_HPP
#define WMI_HPP

#include <string>
#include <type_traits>

#include <wmiexception.hpp>
#include <wmiresult.hpp>

namespace Wmi
{

	//SFINAE test
	//default wmi path if wmi class does not implement getWmiPath
	template <class WmiClass>
	std::string CallGetWmiPath(...)
	{
		return "cimv2";
	}

	template <class WmiClass>
	typename std::enable_if<std::is_function<decltype(WmiClass::getWmiPath)>::value, std::string>::type
	CallGetWmiPath(int /* required, otherwise we get an ambiguitiy error... */)
	{
	   return WmiClass::getWmiPath();
	}

	void query(const std::string& q, const std::string& p, WmiResult &out);

	inline WmiResult query(const std::string& q, const std::string& p)
	{
		WmiResult result;
		query(q, p, result);
		return result;
	}

	template <class WmiClass>
	inline void retrieveWmi(WmiClass &out)
	{
		WmiResult result;
		const std::string q = std::string("Select * From ") + WmiClass::getWmiClassName();
		query(q, CallGetWmiPath<WmiClass>(0), result);
		out.setProperties(result, 0);
	}

	template <class WmiClass>
	inline void retrieveWmi(WmiClass &out, std::string columns)
	{
		WmiResult result;
		const std::string q = std::string("Select ") + columns + std::string(" From ") + WmiClass::getWmiClassName();
		query(q, CallGetWmiPath<WmiClass>(0), result);
		out.setProperties(result, 0);
	}

	template <class WmiClass>
	inline WmiClass retrieveWmi()
	{
		WmiClass temp;
		retrieveWmi(temp);
		return temp;
	}

	template <class WmiClass>
	inline WmiClass retrieveWmi(std::string columns)
	{
		WmiClass temp;
		retrieveWmi(temp, columns);
		return temp;
	}

	template <class WmiClass>
	inline void retrieveAllWmi(std::vector<WmiClass> &out)
	{
		WmiResult result;
		const std::string q = std::string("Select * From ") + WmiClass::getWmiClassName();
		query(q, CallGetWmiPath<WmiClass>(0), result);

		out.clear();
		for(std::size_t index = 0; index < result.size(); ++index)
		{
			WmiClass temp;
			temp.setProperties(result, index);
			out.push_back(std::move(temp));
		}
	}

	template <class WmiClass>
	inline void retrieveAllWmi(std::vector<WmiClass> &out, std::string columns)
	{
		WmiResult result;
		const std::string q = std::string("Select ") + columns + std::string(" From ") + WmiClass::getWmiClassName();
		query(q, CallGetWmiPath<WmiClass>(0), result);

		out.clear();
		for(std::size_t index = 0; index < result.size(); ++index)
		{
			WmiClass temp;
			temp.setProperties(result, index);
			out.push_back(std::move(temp));
		}
	}

	template <class WmiClass>
	inline std::vector<WmiClass> retrieveAllWmi()
	{
		std::vector<WmiClass> ret;
		retrieveAllWmi(ret);

		return ret;
	}

	template <class WmiClass>
	inline std::vector<WmiClass> retrieveAllWmi(std::string columns)
	{
		std::vector<WmiClass> ret;
		retrieveAllWmi(ret, columns);

		return ret;
	}

}; //end namespace Wmi

#endif
