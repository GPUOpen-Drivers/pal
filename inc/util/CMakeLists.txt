##
 #######################################################################################################################
 #
 #  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
 #
 #  Permission is hereby granted, free of charge, to any person obtaining a copy
 #  of this software and associated documentation files (the "Software"), to deal
 #  in the Software without restriction, including without limitation the rights
 #  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 #  copies of the Software, and to permit persons to whom the Software is
 #  furnished to do so, subject to the following conditions:
 #
 #  The above copyright notice and this permission notice shall be included in all
 #  copies or substantial portions of the Software.
 #
 #  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 #  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 #  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 #  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 #  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 #  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 #  SOFTWARE.
 #
 #######################################################################################################################

target_include_directories(pal PUBLIC .)

target_sources(pal PRIVATE
    CMakeLists.txt
    palArchiveFile.h
    palArchiveFileFmt.h
    palArFile.h
    palAssert.h
    palAutoBuffer.h
    palBestFitAllocator.h
    palBestFitAllocatorImpl.h
    palBuddyAllocator.h
    palBuddyAllocatorImpl.h
    palByteSwap.h
    palConditionVariable.h
    palDbgLogger.h
    palDbgLogHelper.h
    palDbgLogMgr.h
    palDbgPrint.h
    palDeque.h
    palDequeImpl.h
    palElf.h
    palElfPackager.h
    palElfPackagerImpl.h
    palElfProcessor.h
    palElfProcessorImpl.h
    palElfReader.h
    palEvent.h
    palFile.h
    palFileMap.h
    palFunctionRef.h
    palHashBase.h
    palHashBaseImpl.h
    palHashLiteralString.h
    palHashMap.h
    palHashMapImpl.h
    palHashProvider.h
    palHashSet.h
    palHashSetImpl.h
    palInlineFuncs.h
    palIntervalTree.h
    palIntervalTreeImpl.h
    palIntrusiveList.h
    palIntrusiveListImpl.h
    palJsonWriter.h
    palLibrary.h
    palLinearAllocator.h
    palList.h
    palListImpl.h
    palLiterals.h
    palMath.h
    palMemMapFile.h
    palMemTracker.h
    palMemTrackerImpl.h
    palMetroHash.h
    palMsgPack.h
    palMsgPackImpl.h
    palMutex.h
    palOptional.h
    palPlatformKey.h
    palRingBuffer.h
    palRingBufferImpl.h
    palSemaphore.h
    palSettingsFileMgr.h
    palSettingsFileMgrImpl.h
    palSpan.h
    palSparseVector.h
    palSparseVectorImpl.h
    palStringBag.h
    palStringBagImpl.h
    palStringUtil.h
    palStringView.h
    palSysMemory.h
    palSystemEvent.h
    palSysUtil.h
    palThread.h
    palTime.h
    palTypeTraits.h
    palUtil.h
    palUuid.h
    palUuidLiteral.h
    palVector.h
    palVectorImpl.h
)
