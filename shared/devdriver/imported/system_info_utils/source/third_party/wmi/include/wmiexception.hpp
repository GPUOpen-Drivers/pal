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

#ifndef WMIEXCEPTION_HPP
#define WMIEXCEPTION_HPP

#include <sstream>
#include <string>

namespace Wmi
{

	struct WmiException
	{

		WmiException(const std::string &str_errorMessage, long l_errorCode) :
			errorMessage(str_errorMessage),
			errorCode(l_errorCode)
		{}

		std::string hexErrorCode() const
		{
			std::stringstream ss;
			ss<<"0x"<<std::hex<<errorCode;
			return ss.str();
		}

		std::string errorMessage;

		long errorCode;

	}; //end struct WmiException

}; //end namespace Wmi

#endif
