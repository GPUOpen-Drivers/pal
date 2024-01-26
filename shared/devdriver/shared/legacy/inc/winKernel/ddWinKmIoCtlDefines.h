/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#define DD_IOCTL_NUTCRACKER_AMDLOG_DEVDRIVER CTL_CODE (40000, 0x904, METHOD_BUFFERED, FILE_ALL_ACCESS)
#define DD_IOCTL_NUTCRACKER_AMDLOG_DEVDRIVER_IN_DIRECT                                                                 \
    CTL_CODE(40000, 0x905, METHOD_IN_DIRECT, FILE_READ_DATA | FILE_WRITE_DATA)

#pragma pack(1)

// ---------  IOCTL_NUTCRACKER_DEVDRIVER  defs -------------------------
struct nc_amdlog_devdriver_input
{
    uint32_t dev_mode_cmd;

    uint32_t process_id;
    uint32_t cmd_data_size;
    uint8_t  cmd_data[1]; // Start of command specific data buffer, see ddDevModeControlCmds.h for details
};

#pragma pack()
