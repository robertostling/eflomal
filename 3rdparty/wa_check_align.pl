#!/usr/bin/perl -w
#
#
# Usage: wa_check_align.pl <submission_file>
#     where submission_file is the name of the file containing the
#         automatically produced word alignments 
#
# Check the validity of a word alignment file, and reports on various 
# errors, including:
#  - incorrect line format
#  - duplicate alignments
# 
# Note that this code does not check the validity of the token numbers
# included in the alignment. 
# 
# Each line in the word alignment file should respect the following format 
# (see the guidelines)
# sentence_no position_L1 position_L2 [S|P] [confidence]
#
# March 06, 2003
# Send comments, suggestions to: Rada Mihalcea, rada@cs.unt.edu
# This software is provided free of charge, AS IS. Feel free to use it
# and/or modify it.


scalar(@ARGV==1) || die "Usage: wa_check_submission.pl submission_file\n";

$submission = $ARGV[0];

# open the file
if ( (! -e $submission) || (! open SUBMISSIONFILE, "<$submission") ) {
    die "Can't find/open submission file `$submission': $!\n";
}

# read all the entries in the submission file
# 1. check the validity of the lines
# 2. check for duplicates in alignments 

$lineNo = 0;

while($line = <SUBMISSIONFILE>) {
    chomp $line;

    $line =~ s/^\s+//;
    $line =~ s/\s+$//;

    $lineNo++;

    
    # get all line components
    # format should be
    # sentence_no position_L1 position_L2 [S|P] [confidence]
    @components = split /\s+/, $line;
    if(scalar(@components) < 3 ||
       scalar(@components) > 5) {
	print STDERR "Error: too few or too many fields in submission file `$submission`, line $lineNo\n";
	exit;
    }
    

    ##### CHECK THE VALIDITY OF THIS LINE FORMAT ######
    
    # first three components have all to be numerical
    if($components[0] !~ /^\d+/ ||
       $components[1] !~ /^\d+/ ||
       $components[2] !~ /^\d+/) {
	print STDERR "Error: first three fields have to be numerical, in submission file `$submission`, line $lineNo\n";
	exit;
    }
    
    # fourth and fifth field are optional, if they exist,
    # one has to be [S|P], one has to be numerical
    if( (scalar (@components) == 4) &&
	($components[3] ne 'S' &&
	 $components[3] ne 'P' &&
	 $components[3] !~ /^[\d\.]+$/)) {
	print STDERR "Error: field 4 has to be [S|P] or numerical, in submission file `$submission`, line $lineNo\n";
	exit;
    }
    
    if( (scalar (@components) == 5) &&
	($components[3] ne 'S' &&
	 $components[3] ne 'P' ||
	 $components[4] !~ /^[\d\.]+$/)) {
	print STDERR "Error: field 4 has to be [S|P], field 5 has to be numerical, in submission file `$submission`, line $lineNo\n";
	exit;
    }

    ##### END CHECK VALIDITY ######

    ##### CHECK FOR DUPLICATES ######

    $alignment = $components[0]." ".$components[1]." ".$components[2];
    
    if(defined($allAlignments{$alignment})) {
	print STDERR "Error: alignment at line $lineNo already defined in line $allAlignments{$alignment}\n";
	exit;
    } 
    else {
	$allAlignments{$alignment} = $lineNo;
    }

    ##### END CHECK FOR DUPLICATES ######
    

    
}

print "File format is OK.\n";
