##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

package genHeapPerfCode;
use strict;

#setup constants
*TAB = \"    ";

our ($TAB);

#These variables are defined at the end of this file
our ($codeHeader,
     $codeGenWarningStart,
     $codeGenWarningEnd,
     $break,
     $fileBriefCPP,
     $setupHeapPerfRatingsComment);

sub new
{
    my $invocant = shift;
    my $class   = ref($invocant) || $invocant;
    my $self = { @_ };

    if(!defined($self->{inDir}))
    {
        die "Error in genHeapPerfCode::new:".
             "You must specify 'inDir' key in hash when calling.";
    }
    if(!defined($self->{outFileName}))
    {
        die "Error in genHeapPerfCode::new:".
             "You must specify 'outFileName' key in hash when calling.";
    }

    $self->{ifCases} = "";
    $self->{curDefs} = "";
    $self->{caseIdx} = 0;

    return bless $self, $class;
}

sub startPerfTable
{
    my $self = shift;
    my $asic = shift;
    my $defs = shift;

    if (!defined($asic))
    {
        die("Code generator error.");
    }

    if (!defined($defs))
    {
        $defs = ""
    }

    my $prevHadDefs = $self->{curDefs} ne "";

    if ($prevHadDefs && $self->{curDefs} ne $defs)
    {
        $self->{ifCases}.= "\n#endif\n"
    }

    if ($self->{curDefs} ne $defs && $defs ne "")
    {
        if ($self->{caseIdx} > 0 && $self->{curDefs} eq "")
        {
            $self->{ifCases}.= "\n";
        }

        $self->{ifCases}.= "#if ${defs}\n   ";
    }
    elsif ($self->{caseIdx} == 0 || $self->{curDefs} ne $defs)
    {
        $self->{ifCases}.= "   ";
    }

    $self->{curDefs} = $defs;

    #my $revFunc =
    $self->{ifCases}.= " if (Is${asic}(*(static_cast<Pal::Device*>(m_pDevice))))\n";
    $self->{ifCases}.= "${TAB}\{\n";
}

sub addPerfPair
{
    my $self = shift;
    my $varName = shift;
    my $varVal = shift;

    if (!defined($varName) || !defined($varVal))
    {
        die( "Code generator error.");
    }

    if ($varVal != 0)
    {
        my $valStr = sprintf("%f", $varVal);
        $valStr =~ s/0+$//g;

        $self->{ifCases}.= "${TAB}${TAB}pSettings->${varName} = ${valStr}f;\n";
    }
}

sub endPerfTable
{
    my $self = shift;

    $self->{ifCases}.= "${TAB}}\n${TAB}else";

    $self->{caseIdx}++;
}

sub finish
{
    my $self = shift;

    #setup output file
    my $outputDir = "";
    if(defined($self->{dir}))
    {
        $outputDir = "$self->{dir}";
    }
    $self->{outputFileCPP} = $outputDir.$self->{outFileName}.".cpp";

    #open file
    open ($self->{fhCPPFile}, ">$self->{outputFileCPP}")
         || die "Can't open file $self->{outputFileCPP}: $!";

    #Add headers
    print {$self->{fhCPPFile}} $codeHeader;
    print {$self->{fhCPPFile}} $codeGenWarningStart. "// ". $self->{inDir}. $codeGenWarningEnd;
    print {$self->{fhCPPFile}} "\n// ${break}";
    print {$self->{fhCPPFile}} "// ${fileBriefCPP}\n\n";

########## CPP file #####################################

    print {$self->{fhCPPFile}} "#include \"core/settingsLoader.h\"\n";
    print {$self->{fhCPPFile}} "#include \"core/device.h\"\n";
    print {$self->{fhCPPFile}} "#include \"palInlineFuncs.h\"\n";
        print {$self->{fhCPPFile}} "\n";
    print {$self->{fhCPPFile}} "namespace Pal {\n\n";

    #add setup function
    print {$self->{fhCPPFile}} "// ${break}";
    print {$self->{fhCPPFile}} "// ${setupHeapPerfRatingsComment}";

    print {$self->{fhCPPFile}} "void SettingsLoader::SetupHeapPerfRatings(\n";
    print {$self->{fhCPPFile}} "${TAB}PalSettings* pSettings)\n";
    print {$self->{fhCPPFile}} "{\n";

    print {$self->{fhCPPFile}} $self->{ifCases};
    if ($self->{curDefs} ne "")
    {
        print {$self->{fhCPPFile}} "\n#endif"
    }
    print {$self->{fhCPPFile}} "\n";
    print {$self->{fhCPPFile}} "${TAB}\{\n";
    print {$self->{fhCPPFile}} "${TAB}${TAB}PAL_NOT_IMPLEMENTED();\n";
    print {$self->{fhCPPFile}} "${TAB}}\n";
    print {$self->{fhCPPFile}} "}\n";
    print {$self->{fhCPPFile}} "} // Pal\n";

    #close file
    close ($self->{fhCPPFile}) || die "Can't close file: $!";
}

########## Variable Defines #####################################
use File::Basename;
my $copyrightFilePath = dirname(__FILE__) . "/../pal-copyright-template.txt";
open (my $fh, '<', "$copyrightFilePath")
    or die "Can't open copyright file $copyrightFilePath. $!";
$codeHeader = do { local $/; <$fh> };
close ($fh);

$codeGenWarningStart = <<'EOF';
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!
//
// This code has been generated automatically. Do not hand-modify this code.
//
// When changes are needed, modify the tools generating this module in the tools\generate directory
// OR the perf logs in the
EOF

$codeGenWarningEnd = <<'EOF';
 directory.
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!
///////////////////////////////////////////////////////////////////////////////////////////////////
EOF

$break = <<'EOF';
=====================================================================================================================
EOF

$fileBriefCPP = <<'EOF';
Contains the implementation for SettingLoader::SetupHeapPerfRatings.
EOF

$setupHeapPerfRatingsComment = <<'EOF';
Sets the heap performance ratings based on baked-in values.
EOF

1;
