#!/usr/local/bin/perl

##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

use strict;
use warnings;

use English;

BEGIN
{
    #add the dk\win\perl\5.6.1\site\lib path to the include path
    foreach my $dir (@INC)
    {
        if($dir =~ m|(.*perl/[^/]*/)lib|)
        {
            push @INC, $1."site/lib";
            last;
        }
    }

    #add the path that holds this script file to the Perl include path
    my @fullpath = split( /[\\\/]/, $PROGRAM_NAME);
    pop @fullpath;
    my $pathName = join( '/', @fullpath);
    if($pathName)
    {
        push @INC, $pathName;
    }
}

use genHeapPerfCode;
use heapPerfParser;

if(scalar(@ARGV) < 2)
{
    print "ERROR:  Please specify the path to the directroy to search for perf logs ".
          "followed by the output file name prefix and output directory.\n".
          "Example: perl genSettingsMain.pl ../../icd/platform/settings/perf ".
          "g_heapPerf ../../icd/platform/settings\n";
    die "Error";
}

my $inDir = $ARGV[0];
my $outputFilePrefix = $ARGV[1];
my $outDir = ".";

if(scalar(@ARGV) >= 3)
{
    $outDir = $ARGV[2];
}

#make sure the output directory has a slash at the end
if( $outDir !~ m|/$| )
{
    $outDir .= "/";
}

#check for existence of the input directory first
if(! -d $inDir)
{
    die "Directory Error";
}

#allocate exporter object
my $exporter = genHeapPerfCode::->new(inDir => $inDir,
                                      dir => $outDir,
                                      outFileName => $outputFilePrefix);

#tell parser about the exporter object
heapPerfParser::exporter($exporter);

#tell the parser to parse each perf log file
foreach (glob "$inDir/asic*.txt")
{
    heapPerfParser::parseFile($_);
}

#tell the code generator that we are done
$exporter->finish();
