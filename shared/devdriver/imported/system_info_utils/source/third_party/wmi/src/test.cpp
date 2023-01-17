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

#include <iostream>
#include <unistd.h>

#include <wmi.hpp>
#include <wmiclasses.hpp>

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::wcout;

using namespace Wmi;

int main(int /*argc*/, char */*args*/[])
{
	try {
		Win32_ComputerSystem computer = retrieveWmi<Win32_ComputerSystem>();
		Win32_ComputerSystemProduct product  = retrieveWmi<Win32_ComputerSystemProduct>();
		SoftwareLicensingService liscense  = retrieveWmi<SoftwareLicensingService>();
		Win32_OperatingSystem os_info  = retrieveWmi<Win32_OperatingSystem>();

		cout<<"Computername: "<<computer.Name<<" Domain: "<<computer.Domain<<endl;
		cout<<"Product: "<<product.Name<<" UUID:"<<product.UUID<<endl;
		cout<<"Architecture: "<<os_info.OSArchitecture<<std::endl;
		cout<<"Roles: "<<endl;
		for(const string &role : computer.Roles)
		{
			cout<<" - "<<role<<std::endl;
		}
		cout<<endl;
		cout<<"Machine Id:"<<liscense.ClientMachineID<<" Kmsid:"<<liscense.KeyManagementServiceProductKeyID<<std::endl;
		cout<<"Installed services:"<<endl;

		// gets all rows and all columns
		for(const Win32_Service &service : retrieveAllWmi<Win32_Service>())
		{
			cout<<service.Caption<<" started:"<<service.Started<<" state:"<<service.State<<  endl;
		}

		// gets all rows and only specified columns(better performance)
		for(const Win32_Service &service : retrieveAllWmi<Win32_Service>("Caption,Started,State"))
		{
			cout<<service.Caption<<" started:"<<service.Started<<" state:"<<service.State<<  endl;
		}

		//Example for using a class that has a non default root (securitycenter2)
		//This can be accombplished by implementing getWmiPath in the wmi class
		cout << "Antivirus installed:" << endl;
		for (const AntiVirusProduct& antivirus : retrieveAllWmi<AntiVirusProduct>())
		{
			cout << antivirus.DisplayName << " | path:" << antivirus.PathToSignedProductExe << " state:" << antivirus.ProductState << " time: " << antivirus.Timestamp << endl;
		}

		// Example for Windows Embedded
		//UWF_Filter filter = retrieveWmi<UWF_Filter>();
		//cout << "UWF Filter enabled:" << filter.CurrentEnabled << std::endl;
	} catch (const WmiException &ex) {
		cerr<<"Wmi error: "<<ex.errorMessage<<", Code: "<<ex.hexErrorCode()<<endl;
		return 1;
	}

	return 0;
}
