##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

package heapPerfParser;
use strict;

#setup global variables
our $exporter;

#function to set\get exporter object
sub exporter
{
    if (@_)
    {
        $exporter = shift;
    }
    return $exporter;
}

sub trimLine
{
    my $line = shift;

    #strip whitespace from head and tail
    if ($line =~ /^\s*(.*)\s*$/o)
    {
        return $1;
    }
    else
    {
        print "Error trying to strip whitespace from '$line'\n";
        return "";
    }
}

sub parseFile
{
    my $file = shift;
    my $asic = "";

    if ($file =~ /asic(.+)\.txt$/)
    {
        $asic = $1;
    }

    open (FH, $file) || die "Can't open file $file: $!";

    #the first line names each performance test
    my $first = <FH>;
    my @tests = split(/\s+/, trimLine($first));
    my @heaps = ();
    my @table = ();
    my @defs  = ();

    #extract each heap name and put the rest in the table; extract list of required defines
    foreach my $line (<FH>)
    {
        if ($line =~ /^\s+$/)
        {
            next;
        }
        elsif ($line =~ /^Required defines: (.+)$/)
        {
            @defs = split(/,/, trimLine($1));
        }
        else
        {
            my @row = split(/\s+/, trimLine($line));

            push(@heaps, shift @row);
            push(@table, [@row]);
        }
    }

    close (FH) || die "Can't close file: $!";

    for my $defIdx (0..$#defs)
    {
        $defs[$defIdx] = trimLine($defs[$defIdx]);
    }

    $exporter->startPerfTable($asic, join(" && ", @defs));

    #build each possible variable/value pair
    for my $heapIdx (0..$#heaps)
    {
        for my $testIdx (0..$#tests)
        {
            my $name = $tests[$testIdx]."PerfFor".$heaps[$heapIdx];
            my $value = $table[$heapIdx][$testIdx];

            $exporter->addPerfPair($name, $value);
        }
    }

    $exporter->endPerfTable();
}

1;
