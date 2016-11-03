#!/usr/bin/perl -w

# Usage: wa_eval.pl <answer_file> <submission_file>
#     where answer_file is the name of the file containing the gold 
#         standard word alignments
#     and submission_file is the name of the file containing the
#         automatically produced word alignments 
#
# Evaluation is performed using: 
#  - Standard Precision, Recall, F-measure, separate 
#    for S (Sure) and P (Probable) cases
#  - AER measure, defined as 
#    AER = 1 - ( |A & S| + |A & P| ) / ( |A| + |S| )
#    [where A represents the alignment, S and P represent the
#     S (Sure) and P (Probable) gold standard alignments]
#
# March 06, 2003
# Send comments, suggestions to: Rada Mihalcea, rada@cs.unt.edu
# This software is provided free of charge, AS IS. Feel free to use it
# and/or modify it.

scalar(@ARGV==2) || die "Usage: wa_eval.pl answer_file submission_file\n";

$answer = $ARGV[0];
$submission = $ARGV[1];

# open the two files
if ( (! -e $answer) || (! open ANSWERFILE, "<$answer") ) {
    die "Can't find/open answer file `$answer': $!\n";
}

if ( (! -e $submission) || (! open SUBMISSIONFILE, "<$submission") ) {
    die "Can't find/open submission file `$submission': $!\n";
}

# read all the entries in the answer file
# sure alignments and probable alignments are stored separately

while($line = <ANSWERFILE>) {
    chomp $line;

    $line =~ s/^\s+//;
    $line =~ s/\s+$//;

    # get all line components
    # format should be
    # sentence_no position_L1 position_L2 [S|P] [confidence]
    @components = split /\s+/, $line;
    
    if(scalar(@components) < 3) {
	print STDERR "Incorrect format in answer file `$answer`\n";
	exit;
    }
    
    $alignment = $components[0]." ".$components[1]." ".$components[2];
    
    # identify the S[ure] alignments
     if( scalar (@components) == 3 ||
	(scalar (@components) == 4 && 
	 ($components[3] =~ /^[\d\.]+$/ ||
	  $components[3] eq 'S')) ||
	(scalar (@components) == 5 && 
	 ($components[3] eq 'S' ||
	  $components[4] eq 'S'))) {
	 $sureAnswer{$alignment} = 1; 
    }
    
    # identify the P[robable] alignments
     if( (scalar (@components) == 4 && 
	 $components[3] eq 'P') ||
	(scalar (@components) == 5 && 
	 ($components[3] eq 'P' ||
	  $components[4] eq 'P'))) {
	 $probableAnswer{$alignment} = 1;
     }
}


# read all the entries in the submission file
# sure alignments and probable alignments are stored separately

while($line = <SUBMISSIONFILE>) {
    chomp $line;

    $line =~ s/^\s+//;
    $line =~ s/\s+$//;

    # get all line components
    # format should be
    # sentence_no position_L1 position_L2 [S|P] [confidence]
    @components = split /\s+/, $line;
    if(scalar(@components) < 3) {
	print STDERR "Incorrect format in submission file `$submission`: $!\n";
	exit;
    }
    
    $alignment = $components[0]." ".$components[1]." ".$components[2];
    
    # identify the S[ure] alignments
    if( scalar (@components) == 3 ||
       (scalar (@components) == 4 && 
	($components[3] =~ /^[\d\.]+$/ ||
	 $components[3] eq 'S')) ||
       (scalar (@components) == 5 && 
	($components[3] eq 'S' ||
	 $components[4] eq 'S'))) {
	$sureSubmission{$alignment} = 1; 
    }
    
    # identify the P[probable] alignments
    if( (scalar (@components) == 4 && 
	 $components[3] eq 'P') ||
	(scalar (@components) == 5 && 
	 ($components[3] eq 'P' ||
	  $components[4] eq 'P'))) {
	$probableSubmission{$alignment} = 1;
    }
}


	
# now determine the S[ure] matches 
$sureMatch = 0;
foreach $alignment (keys %sureSubmission) {
    if(defined($sureAnswer{$alignment})) {
	$sureMatch++;
    }
}

# and the [P]robable matches 
# these are checked against both S[ure] and P[robable] correct alignments
$probableMatch = 0;
foreach $alignment (keys %probableSubmission, keys %sureSubmission) {
    if(defined($sureAnswer{$alignment}) ||
       defined($probableAnswer{$alignment})) {
	$probableMatch++;
    }
}

# and also the intersection between all submitted alignments 
# and the S [Sure] correct alignments -- as needed by AER
$probableMatchSure = 0;
foreach $alignment (keys %probableSubmission, keys %sureSubmission) {
    if(defined($sureAnswer{$alignment})) {
	$probableMatchSure++;
    }
}


# now determine the precision, recall, and F-measure for [S]ure alignments
if(scalar(keys %sureSubmission) != 0) {
    $surePrecision = $sureMatch / scalar(keys %sureSubmission);
}
else {
    $surePrecision = 0;
}

if(scalar(keys %sureAnswer) != 0) {
    $sureRecall = $sureMatch / scalar(keys %sureAnswer);
}
else {
    $sureRecall = 0;
}

if($sureRecall != 0 && $surePrecision != 0) {
    $sureFMeasure = 2 * $sureRecall * $surePrecision / 
	($sureRecall + $surePrecision);
} 
else {
    $sureFMeasure = 0;
}


# and now determine the precision, recall, and F-measure for [P]robable alignments
if(scalar(keys %sureSubmission) + scalar(keys %probableSubmission) != 0) {
    $probablePrecision = $probableMatch / (scalar(keys %sureSubmission)  + 
				   scalar(keys %probableSubmission));
}
else {
    $probablePrecision = 0;
}

if(scalar(keys %sureAnswer) + scalar(keys %probableAnswer)!= 0) {
    $probableRecall = $probableMatch / (scalar(keys %sureAnswer) +
				+ scalar(keys %probableAnswer));
}
else {
    $probableRecall = 0;
}

if($probableRecall != 0 && $probablePrecision != 0) {
    $probableFMeasure = 2 * $probableRecall * $probablePrecision / 
	($probableRecall + $probablePrecision);
} 
else {
    $probableFMeasure = 0;
}


# and determine the AER
if(scalar(keys %sureSubmission) + scalar(keys %probableSubmission) != 0) {
    $AER = 1 - ($probableMatchSure + $probableMatch) / (scalar(keys %sureSubmission) + scalar(keys %probableSubmission) + scalar(keys %sureAnswer));
}
else {
    $AER = 0;
}

# and finally - print out the results
printf("\nThe following results assume correct file formats.\n");
printf("Make sure you have previously checked the file format\n");
printf("using wa_check_submission.pl\n\n");
printf("\n    Word Alignment Evaluation   \n");
printf("----------------------------------\n");
printf("   Evaluation of SURE alignments \n");
printf("   Precision = %5.4f  \n", $surePrecision);
printf("   Recall    = %5.4f\n",$sureRecall);
printf("   F-measure = %5.4f\n",$sureFMeasure);
printf("-----------------------------------\n");
printf("   Evaluation of PROBABLE alignments\n");
printf("   Precision = %5.4f\n",$probablePrecision);
printf("   Recall    = %5.4f\n",$probableRecall);
printf("   F-measure = %5.4f\n",$probableFMeasure);
printf("-----------------------------------\n");
printf("   AER       = %5.4f\n",$AER);


