### Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All rights reserved. ###

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
