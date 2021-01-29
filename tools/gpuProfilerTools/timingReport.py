##
 #######################################################################################################################
 #
 #  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

import collections
import csv
import glob
import os
import re
import sys

try:
    dict.iteritems
except AttributeError:
    # Python 3
    def itervalues(d):
        return iter(d.values())
    def iteritems(d):
        return iter(d.items())
else:
    # Python 2
    def itervalues(d):
        return d.itervalues()
    def iteritems(d):
        return d.iteritems()

QueueCallCol     = 0
CmdBufIndexCol   = QueueCallCol + 1
CmdBufCallCol    = CmdBufIndexCol + 1
SubQueueIdxCol   = CmdBufCallCol + 1
StartClockCol    = SubQueueIdxCol + 1
EndClockCol      = StartClockCol + 1
TimeCol          = EndClockCol + 1
PipelineHashCol  = TimeCol + 1
CompilerHashCol  = PipelineHashCol + 1
VsCsCol          = CompilerHashCol + 1
HsCol            = VsCsCol + 1
DsCol            = HsCol + 1
GsCol            = DsCol + 1
PsCol            = GsCol + 1
VertsThdGrpsCol  = PsCol + 1
InstancesCol     = VertsThdGrpsCol + 1
CommentsCol      = InstancesCol + 1

def isValidHash(string):
    # A valid hash is a non-empty string that represents a non-zero hex value.
    return string and (int(string, 16) != 0)

def DeterminePipelineType(row):
    if not row[CompilerHashCol]:
        return "No Pipeline (BLT, Barrier, etc.)"
    else:
        if re.search("Dispatch", row[2]):
            return "Cs"
        elif isValidHash(row[HsCol]) and isValidHash(row[GsCol]):
            return "VsHsDsGsPs"
        elif isValidHash(row[HsCol]):
            return "VsHsDsPs"
        elif isValidHash(row[GsCol]):
            return "VsGsPs"
        else:
            return "VsPs"

enPrintAllPipelines = False

if len(sys.argv) > 3 or len(sys.argv) < 2:
    sys.exit("Usage: timingReport.py <full path to log folder> [-all].")
elif len(sys.argv) == 3:
    if sys.argv[2] == "-all":
        enPrintAllPipelines = True
    else:
        sys.exit("Usage: timingReport.py <full path to log folder>. [-all]")

gpuFrameTime = 0

os.chdir(sys.argv[1])
files = glob.glob("frame*.csv")

if (len(files) == 0):
    sys.exit("ERROR: Looking at directory <{0}> but cannot find any files that match the \"frame*.csv\" pattern.".format(os.getcwd()))

frames               = { }  # Frame num -> [ tsFreq, cmdBufClockPairs, total barrier time ]
perCallTable         = { }  # Device -> Engine -> QueueId -> Call -> [ count, totalTime ]
perPipelineTypeTable = { }  # PipelineType -> [ count, totalTime ]
perPipelineTable     = { }  # Pipeline Hash -> [ type, count, totalTime, vs/csHash, hsHash, dsHash, gsHash, psHash ]
perPsTable           = { }  # PS Hash -> [ count, totalTime ]
pipelineRangeTable   = { }  # Frame num -> EngineType -> PipelineHash -> [(startClock1, endClock1, time1), (startClock2, endClock2, time2), ...]

frameCount           = 0
submitCount          = 0
cmdBufCount          = 0

filesProcessedSoFar  = 0 # For printing parsing progress.

for file in files:
    if sys.stdout.isatty():
        sys.stdout.write("Parsing input files.  {0:.0f}% Complete.\r".format((filesProcessedSoFar / float(len(files))) * 100))
    filesProcessedSoFar += 1

    # Decode file name.
    searchObj  = re.search("frame([0-9]*)Dev([0-9]*)Eng(\D*)([0-9]*)-([0-9]*)\.csv", file)
    frameNum   = int(searchObj.group(1))
    deviceNum  = int(searchObj.group(2))
    engineType = searchObj.group(3)
    engineId   = int(searchObj.group(4))
    queueId    = int(searchObj.group(5))

    # Track the fact we've never seen this frame before:
    # - Zero out the time spend in barriers for it.
    # - Place some empty maps in the pipelineRangeTable.
    if not frameNum in frames:
        frames[frameNum] = [0, [], 0]
        pipelineRangeTable[frameNum] = { "Ace" : {}, "Dma" : {}, "Gfx" : {} }

    # Expand C,D,U to full engine type name.
    if engineType == "Ace":
        engineKey = "Compute"
    elif engineType == "Dma":
        engineKey = "DMA"
    elif engineType == "Gfx":
        engineKey = "Universal"
    else:
        continue

    # Create readable keys for perCallTable, will be displayed later.
    deviceKey = "Device " + str(deviceNum)
    engineKey = engineKey + " Engine " + str(engineId)
    queueKey  = "Queue " + str(queueId)

    if not deviceKey in perCallTable.keys():
        perCallTable[deviceKey] = { }
    if not engineKey in perCallTable[deviceKey].keys():
        perCallTable[deviceKey][engineKey] = { }
    if not queueKey in perCallTable[deviceKey][engineKey].keys():
        perCallTable[deviceKey][engineKey][queueKey] = { }

    with open(file) as csvFile:
        reader = csv.reader(csvFile, skipinitialspace=True)
        headers = next(reader)

        tsFreqSearch        = re.search(".*Frequency: (\d+).*", headers[TimeCol])
        frames[frameNum][0] = int(tsFreqSearch.group(1))

        for row in reader:
            if row[QueueCallCol] == "Submit()":
                submitCount += 1
            if row[CmdBufCallCol] == "Begin()" and row[StartClockCol]:
                frames[frameNum][1].append((int(row[StartClockCol]), int(row[EndClockCol])))
                cmdBufCount += 1
            if row[TimeCol]:
                if row[CmdBufCallCol] in perCallTable[deviceKey][engineKey][queueKey].keys():
                    perCallTable[deviceKey][engineKey][queueKey][row[CmdBufCallCol]][0] += 1
                    perCallTable[deviceKey][engineKey][queueKey][row[CmdBufCallCol]][1] += float(row[TimeCol])
                else:
                    perCallTable[deviceKey][engineKey][queueKey][row[CmdBufCallCol]] = [ 1, float(row[TimeCol]) ]

                pipelineType = DeterminePipelineType(row)
                if pipelineType in perPipelineTypeTable:
                    perPipelineTypeTable[pipelineType][0] += 1
                    perPipelineTypeTable[pipelineType][1] += float(row[TimeCol])
                else:
                    perPipelineTypeTable[pipelineType] = [ 1, float(row[TimeCol]) ]

                if row[CompilerHashCol]:
                    # Update the perPipelineTable totals.
                    # Note that in practice the compiler hash is most useful because it's in all of the pipeline dumps.
                    if row[CompilerHashCol] in perPipelineTable:
                        perPipelineTable[row[CompilerHashCol]][1] += 1
                        perPipelineTable[row[CompilerHashCol]][2] += float(row[TimeCol])
                    else:
                        perPipelineTable[row[CompilerHashCol]] = [ pipelineType, 1, float(row[TimeCol]), row[VsCsCol], row[HsCol], row[DsCol], row[GsCol], row[PsCol] ]

                    # Record the start and end clocks and the time of this shader work in the pipelineRangeTable.
                    # Note that we may divide by zero later unless we exclude rows with identical start and end clocks.
                    startClock = int(row[StartClockCol])
                    endClock   = int(row[EndClockCol])
                    if endClock - startClock > 0:
                        if row[CompilerHashCol] in pipelineRangeTable[frameNum][engineType]:
                            pipelineRangeTable[frameNum][engineType][row[CompilerHashCol]].append((startClock, endClock, float(row[TimeCol])))
                        else:
                            pipelineRangeTable[frameNum][engineType][row[CompilerHashCol]] = [(startClock, endClock, float(row[TimeCol]))]

                if row[PsCol]:
                    if row[PsCol] in perPsTable:
                        perPsTable[row[PsCol]][0] += 1
                        perPsTable[row[PsCol]][1] += float(row[TimeCol])
                    else:
                        perPsTable[row[PsCol]] = [ 1, float(row[TimeCol]) ]

                if row[CmdBufCallCol] == "CmdBarrier()":
                    frames[frameNum][2] += float(row[TimeCol])

        csvFile.close

# Compute the sum of all GPU frame times, where the time of a single frame is the amount of time the GPU spent being busy.
# We can do this by creating a list of all GPU clock ranges when the GPU was busy from the list of all command buffer clock ranges like so:
# - For the current frame, sort the list of top-level command buffer (begin, end) clocks by increasing begin time.
# - Pop the top (begin, end) pair and use it to start a new GPU busy range.
# - While the top pair overlaps with the busy range, update the busy range with the latest ending time and pop the top pair.
# - Once there are no more overlapping ranges, push the current range as a complete busy range and repeat.
# Once that is done we can simply sum the busy ranges to get the ammount of time the GPU was busy for the current frame.
gpuFrameTime = 0
for frame in frames.keys():
    tsFreq        = frames[frame][0]
    orderedRanges = sorted(frames[frame][1], key=lambda x: x[0])
    busyRanges    = []
    while orderedRanges:
        (curBegin, curEnd) = orderedRanges.pop(0)
        while orderedRanges and orderedRanges[0][0] <= curEnd:
            curEnd = max(curEnd, orderedRanges[0][1])
            orderedRanges.pop(0)
        busyRanges.append((curBegin, curEnd))
    for (begin, end) in busyRanges:
        gpuFrameTime += ((1000000 * (end - begin)) / tsFreq)

frameCount    = int(len(frames))
gpuFrameTime /= frameCount

print("Average GPU busy time per frame: {0:.3f}ms ({1:,d} frames)".format(gpuFrameTime / 1000.0, frameCount))
print("Average submits per frame:         " + str(submitCount / frameCount))
print("Average command buffers per frame: " + str(cmdBufCount / frameCount))
print("")

for deviceKey in iter(sorted(perCallTable)):
    print("== Frame Breakdown By Command Buffer Call =======================================================================================================\n")
    if len(perCallTable) > 1:
        print(" + " + deviceKey + ":")
    for engineKey in iter(sorted(perCallTable[deviceKey])):
        for queueKey in iter(sorted(perCallTable[deviceKey][engineKey])):
            print("   {0:37s}| Avg. Call Count | Avg. GPU Time [us] | Avg. Frame % ".format(engineKey + " (" + queueKey + ") Calls"))
            print("  --------------------------------------+-----------------+--------------------+--------------")
            totalQueueCount = 0
            totalQueueTime = 0
            for callId in collections.OrderedDict(sorted(perCallTable[deviceKey][engineKey][queueKey].items(), key=lambda x: x[1][1], reverse=True)):
                count = perCallTable[deviceKey][engineKey][queueKey][callId][0] / frameCount
                totalQueueCount += count
                time = perCallTable[deviceKey][engineKey][queueKey][callId][1] / frameCount
                totalQueueTime += time
                print("  {0:38s}|    {1:12,.2f} |       {2:12,.2f} |      {3:5.2f} %".
                      format(callId, count, time, (time / gpuFrameTime) * 100))
            print("  --------------------------------------+-----------------+--------------------+--------------")
            print("   Total                                |    {0:12,.2f} |       {1:>12,.2f} |      {2:5.2f} %\n\n".
                  format(totalQueueCount, totalQueueTime, (totalQueueTime / gpuFrameTime) * 100))

print("== Frame Breakdown By Pipeline Type =============================================================================================================\n")
print("   Pipeline Type                        | Avg. Call Count | Avg. GPU Time [us] | Avg. Frame %")
print("  --------------------------------------+-----------------+--------------------|--------------")
for pipelineType in collections.OrderedDict(sorted(perPipelineTypeTable.items(), key=lambda x: x[1][1], reverse=True)):
    timePerFrame = perPipelineTypeTable[pipelineType][1] / frameCount
    pctOfFrame   = (timePerFrame / gpuFrameTime) * 100
    print("  {0:37s} |    {1:12,.2f} |       {2:>12,.2f} |      {3:5.2f} %".
        format(pipelineType,
               perPipelineTypeTable[pipelineType][0] / frameCount,
               timePerFrame,
               pctOfFrame))
print("\n")

print("== Top Pipelines (>= 1%) ========================================================================================================================\n")
pipelineNum = 0
hidden = 0
print("   Compiler Hash         | Type         | Avg. Call Count | Avg. GPU Time [us] | Avg. Frame %")
print("  -----------------------+--------------+-----------------+--------------------|--------------")
for pipeline in collections.OrderedDict(sorted(perPipelineTable.items(), key=lambda x: x[1][2], reverse=True)):
    pipelineNum += 1
    timePerFrame = perPipelineTable[pipeline][2] / frameCount
    pctOfFrame   = (timePerFrame / gpuFrameTime) * 100
    if pctOfFrame < 1.0 and not enPrintAllPipelines:
        hidden += 1
    else:
        print("  {0:2d}. {1:s} | {2:10s}   |    {3:12,.2f} |       {4:>12,.2f} |      {5:5.2f} %".
            format(pipelineNum,
                   pipeline,
                   perPipelineTable[pipeline][0],
                   perPipelineTable[pipeline][1] / frameCount,
                   timePerFrame,
                   pctOfFrame))
if hidden > 0:
    print("\n  + {0:d} pipelines not shown (< 1%).".format(hidden))
print("\n")

print("== Top Pipeline/Shader Hashes (>= 1%) ===========================================================================================================\n")
pipelineNum = 0
hidden = 0
print("   Compiler Hash         | Type       | VS/CS Hash                         | HS Hash                            | DS Hash                            | GS Hash                            | PS Hash                            ")
print("  -----------------------+------------+------------------------------------+------------------------------------+------------------------------------+------------------------------------+------------------------------------")
for pipeline in collections.OrderedDict(sorted(perPipelineTable.items(), key=lambda x: x[1][2], reverse=True)):
    pipelineNum += 1
    timePerFrame = perPipelineTable[pipeline][2] / frameCount
    pctOfFrame   = (timePerFrame / gpuFrameTime) * 100
    if pctOfFrame < 1.0 and not enPrintAllPipelines:
        hidden += 1
    else:
        pipelineHashes = perPipelineTable[pipeline]
        vsCsHash       = pipelineHashes[3] if isValidHash(pipelineHashes[3]) else ""
        hsHash         = pipelineHashes[4] if isValidHash(pipelineHashes[4]) else ""
        dsHash         = pipelineHashes[5] if isValidHash(pipelineHashes[5]) else ""
        gsHash         = pipelineHashes[6] if isValidHash(pipelineHashes[6]) else ""
        psHash         = pipelineHashes[7] if isValidHash(pipelineHashes[7]) else ""
        print("  {0:2d}. {1:18s} | {2:10s} | {3:34s} | {4:34s} | {5:34s} | {6:34s} | {7:34s} ".
            format(pipelineNum,
                   pipeline,
                   pipelineHashes[0],
                   vsCsHash, hsHash, dsHash, gsHash, psHash))
if hidden > 0:
    print("\n  + {0:d} pipelines not shown (< 1%).".format(hidden))
print("\n")

print("== Top Pixel Shaders (>= 1%) ====================================================================================================================\n")
psNum = 0
hidden = 0
print("   PS Hash                                | Avg. Call Count | Avg. GPU Time [us] | Avg. Frame %")
print("  ----------------------------------------+-----------------+--------------------|--------------")
for ps in collections.OrderedDict(sorted(perPsTable.items(), key=lambda x: x[1][1], reverse=True)):
    psNum += 1
    timePerFrame = perPsTable[ps][1] / frameCount
    pctOfFrame   = (timePerFrame / gpuFrameTime) * 100
    if pctOfFrame < 1.0 and not enPrintAllPipelines:
        hidden += 1
    else:
        print("  {0:2d}. {1:36s}|    {2:12,.2f} |       {3:>12,.2f} |      {4:5.2f} %".
            format(psNum,
                   ps,
                   perPsTable[ps][0] / frameCount,
                   timePerFrame,
                   pctOfFrame))
if hidden > 0:
    print("\n  + {0:d} pixel shaders not shown (< 1%).".format(hidden))

print("\n")

# Identify frame with median time spent in barriers.
medianBarrierFrame = list(collections.OrderedDict(sorted(list(iteritems(frames)), key=lambda x: x[1][2])).keys())[int(frameCount / 2)]

barrierTime = 0
barrierReportTable = [ ] # [time, [desc, ...] ]
for file in files:
    # Decode file name.
    searchObj  = re.search("frame([0-9]*)Dev([0-9]*)Eng(\D*)([0-9]*)-([0-9]*)\.csv", file)
    frameNum   = int(searchObj.group(1))
    engineType = searchObj.group(3)

    if not (engineType == "Ace" or engineType == "Dma" or engineType == "Gfx"):
        continue

    if frameNum == medianBarrierFrame:
        with open(file) as csvFile:
            reader = csv.reader(csvFile, skipinitialspace=True)
            next(reader)
            for row in reader:
                if row[CmdBufCallCol] == "CmdBarrier()":
                    barrierTime += float(row[TimeCol])
                    entry = [float(row[TimeCol]), [ ] ]

                    if row[CommentsCol] == "":
                        entry[1].append(["-", "", 0, 0])
                    else:
                        actionList = row[CommentsCol].split("\n")
                        for action in actionList:
                            if ('CacheMask' not in action) and ('OldLayout' not in action) and ('NewLayout' not in action):
                                searchObj = re.search("(.*): ([0-9]*)x([0-9]*) (.*)", action)
                                if searchObj != None:
                                    actionType = searchObj.group(1)
                                    width = int(searchObj.group(2))
                                    height = int(searchObj.group(3))
                                    format = searchObj.group(4)

                                    entry[1].append([actionType, format, width, height])
                                else:
                                    entry[1].append([action, "", 0, 0])

                    barrierReportTable.append(entry)
            csvFile.close

print("== Median Frame Top CmdBarrier() Calls (>= 10us): ===============================================================================================\n")
print("Frame #{0:d} total barrier time: {1:,.2f} us\n".format(medianBarrierFrame, barrierTime))
print("     Layout Transition(s)                                         |  Format                      | Dimensions  | Time [us]")
print("  ----------------------------------------------------------------+------------------------------+-------------+-----------")
barrierNum = 0
hidden = 0
for barrier in sorted(barrierReportTable, key=lambda x: x[0], reverse=True):
    barrierNum +=1
    if barrier[0] < 10:
        hidden += 1
    else:
        firstLine = True
        actions = sorted(barrier[1], key=lambda x: x[3], reverse=True)
        for action in actions:
            dimensions = "{0:4d} x {1:4d}".format(action[2], action[3]) if action[2] != 0 and action[3] != 0 else "           "
            if firstLine:
                print("  {0:2d}. {2:58s}  | {3:26s}   | {4:s} | {1:>8,.2f}".
                    format(barrierNum, barrier[0], action[0], action[1], dimensions))
            else:
                print("      {0:58s}  | {1:26s}   | {2:s} |".
                    format(action[0], action[1], dimensions))

            firstLine = False

if hidden > 0:
    print("\n  + {0:d} CmdBarrier() calls not shown (< 10us).\n".format(hidden))

print("\n")

asyncOverlapTable = { } # computePipelineHash -> [totalOverlapTime, universalPipelineHash -> overlapTime]
for frameNum in iter(sorted(pipelineRangeTable)):
    # Check for overlap between all pairs of compute and universal executions in this frame.
    for cPipeline, cClocks in pipelineRangeTable[frameNum]["Ace"].items():
        if cPipeline not in asyncOverlapTable:
            asyncOverlapTable[cPipeline] = [0, { }]
        for cStart, cEnd, cTime in cClocks:
            for uPipeline, uClocks in pipelineRangeTable[frameNum]["Gfx"].items():
                for uStart, uEnd, uTime in uClocks:
                    # If these clock ranges intersect, compute the portion of the compute time that overlaps with the universal work.
                    # Note that we treat the clocks as dimensionless numbers, we never need to know the clock frequency.
                    if uStart < cEnd and uEnd > cStart:
                        overlapTime = cTime * (min(cEnd, uEnd) - max(cStart, uStart)) / (cEnd - cStart)
                        asyncOverlapTable[cPipeline][0] += overlapTime
                        if uPipeline in asyncOverlapTable[cPipeline][1]:
                            asyncOverlapTable[cPipeline][1][uPipeline] += overlapTime
                        else:
                            asyncOverlapTable[cPipeline][1][uPipeline] = overlapTime

if len(asyncOverlapTable.keys()) > 0:
    print("== Async Compute Overlap ========================================================================================================================\n")
    print("   Async Compiler Hash    | Gfx Compiler Hash    | Avg. GPU Time [us] | Avg. Frame %")
    print("  ------------------------+----------------------+--------------------+--------------")
    pipelineNum = 0
    for cPipeline in collections.OrderedDict(sorted(asyncOverlapTable.items(), key=lambda x: x[1][0], reverse=True)):
        pipelineNum += 1
        timePerFrame = asyncOverlapTable[cPipeline][0] / frameCount
        pctOfFrame   = (timePerFrame / gpuFrameTime) * 100
        print("  {0:2d}. {1:s}  |  Total               |       {2:>12,.2f} |      {3:5.2f} %".
            format(pipelineNum, cPipeline, timePerFrame, pctOfFrame))
        numTrailing       = 0
        trailingTimeTotal = 0
        for uPipeline in collections.OrderedDict(sorted(asyncOverlapTable[cPipeline][1].items(), key=lambda x: x[1], reverse=True)):
            timePerFrame = asyncOverlapTable[cPipeline][1][uPipeline] / frameCount
            pctOfFrame   = (timePerFrame / gpuFrameTime) * 100
            if pctOfFrame < 0.10:
                numTrailing       += 1
                trailingTimeTotal += timePerFrame
            else:
                print("                          |  {0:s}  |       {1:>12,.2f} |      {2:5.2f} %".
                    format(uPipeline, timePerFrame, pctOfFrame))
        pctOfFrame = (trailingTimeTotal / gpuFrameTime) * 100
        print("                          |  Num Hidden: {0:<6d}  |       {1:>12,.2f} |      {2:5.2f} %".
            format(numTrailing, trailingTimeTotal, pctOfFrame))
        timePerFrame = (perPipelineTable[cPipeline][2] - asyncOverlapTable[cPipeline][0]) / frameCount
        pctOfFrame   = (timePerFrame / gpuFrameTime) * 100
        print("                          |  No Overlap          |       {0:>12,.2f} |      {1:5.2f} %".
            format(timePerFrame, pctOfFrame))
        print("  ------------------------+----------------------+--------------------+--------------")
