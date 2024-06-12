#!/usr/bin/env perl

use strict;
use warnings;
use local::lib "work/downloads/cpanm";
use Regexp::Common;

sub output($);
sub replace_quotes($);

my %files;
my $max_width = 80;
my $space_width = 4;
my $balanced = $RE{balanced}{-parens => '()'};

my ($infile, $lineno, $line);

##################################################################
# begin new rules - var tracking
##################################################################

sub save_var();
sub var_output($);

my ($var_lineno, $var_line);

##################################################################
# end new rules - var tracking
##################################################################

for my $file (@ARGV) {
    $infile = $file;
    #print "$infile\n";

    open my $in, $infile or die $!;

    # comment flags
    my ($full_comment, $one_line_comment, $half_line_comment, $comment_not_end) = (0, 0, 0, 0);

    # empty line before else code block
    my ($cur_line_is_empty, $prev_line_is_empty, $consecutive_empty_lines) = (0, 0, 0);

    $lineno = 0;

    my $level = 0;

    ##################################################################
    # begin new rules - flags
    ##################################################################

    # label flags
    my ($in_label, $label_blanks, $prev_label_blanks) = (0);

    # preprocessor directives
    my ($hash_comment, $prev_hash_comment) = (0);

    # variable declaration spacing
    my ($after_function, $max_var_type, $min_var_space, $n_vars, $diff_types) = (0, 0, 0, 0, 0);

    # initial section with defines and includes
    my ($include_section, $prev_consecutive_empty_lines) = (1, 0);

    # function declaration
    my ($func_decl_multiline, $in_func_decl);

    # switch indentation levels
    my @switch_level = ();

    ##################################################################
    # end new rules - flags
    ##################################################################

    while (<$in>) {
        $line = $_;

        $lineno++;

        #print "$lineno: $line";

        $prev_line_is_empty = $cur_line_is_empty;
        if ($line =~ /^\n$/) {
            $cur_line_is_empty = 1;
            $consecutive_empty_lines++;

        } else {
            $cur_line_is_empty = 0;
            $consecutive_empty_lines = 0;
        }

        # one line comment
        if ($line =~ /^#/) {
            $full_comment = 1;

            if ($line =~ /\\$/) {
                $comment_not_end = 1;
            } else {
                $one_line_comment = 1;
            }
        }

        # comment /* */
        if ($line =~ /(\S*) \s* \/\* .* \*\/ /x) {
            $one_line_comment = 1;

            if ($1 ne "") {
                $half_line_comment = 1;

            } else {
                $full_comment = 1;
            }
        }

        # comment block: /* or #if 0
        if ($line =~ /\/\*((?!\*\/).)*$/ || $line =~ /^#if 0/) {
            $full_comment = 1;
            #$print "enter comment block at line: $lineno\n";
        }

        ##################################################################
        # begin new rules - including comment lines
        ##################################################################

        $prev_hash_comment = $hash_comment;

        if ($line =~ /^#/) {
            $hash_comment = 1;

        } else {
            $hash_comment = 0;
        }

        # ignore contents of '#if 0', treat as a comment
        if ($line =~ /^#if 0/) {
            $one_line_comment = 0;
        }

        # in the Nginx codebase, the presence of #if lines generally
        # doesn't count towards the number of blanks around labels
        if (!$hash_comment) {

            if ($cur_line_is_empty) {
                $label_blanks++;

            } else {
                $prev_label_blanks = $label_blanks;
                $label_blanks = 0;
            }

            if ($in_label) {
                if (!$cur_line_is_empty) {
                    if ($prev_label_blanks == 0) {
                        output "need a blank line after a label.";

                    } elsif ($prev_label_blanks > 1) {
                        output "need one blank line after a label,"
                        . " got $prev_label_blanks.";
                    }

                    $in_label = 0;
                }
            }
        }

        ##################################################################
        # end new rules - including comment lines
        ##################################################################

        # not full_comment
        if ($full_comment == 0) {

            ##################################################################
            # begin new rules - excluding comment lines
            ##################################################################

            if ($include_section == 1) {
                if (!$cur_line_is_empty) {
                    if ($prev_consecutive_empty_lines != 2) {
                        output "needs 2 blank lines after #include section.";
                    }
                    $include_section = 0;
                }
            }

            # detect function start */
            if ($line =~ /^{$/) {
                $after_function = 1;
                $min_var_space = 99999;
                $max_var_type = 0;
                $n_vars = 0;
                $diff_types = 0;

            } elsif ($after_function == 1) {
                if ($line =~ /^\s+(([\w\(\)]+(\s\w+)?(\s\w+)?)\s+)\**\w+(\[|,|;| =)/) {
                    $n_vars++;
                    if ($min_var_space > length($1)) {
                        $min_var_space = length($1);
                    }
                    if ($max_var_type > 0 && $max_var_type != length($2)) {
                        $diff_types = 1;
                    }
                    if ($max_var_type < length($2)) {
                        $max_var_type = length($2);
                    }
                    $var_lineno = $lineno;
                    $var_line = $line;
                    save_var();

                } elsif (!$cur_line_is_empty) {
                    if ($n_vars > 0 && $min_var_space - $max_var_type > 3) {
                        var_output "excessive spacing in variable alignment: needs 2 spaces from longest type to first name/pointer.";
                    }
                    if ($diff_types == 1 && $min_var_space - $max_var_type < 2) {
                        var_output "too little spacing in variable alignment: needs 2 spaces from longest type to first name/pointer.";
                    }
                    $after_function = 0;
                }
            }

            # lines over 80 characters

            if ($line =~ /^.{\Q$max_width\E}.*?\S/
                && $line !~ /\*\/$/)
            {
                output "line too long (max $max_width columns).";
            }

            # comment // */

            if ($line =~ /^\s*\/\//) {
                output "avoid C99-style comments ( // ), use C89-style ( /* */ ).";
            }

            # top level indent

            if ($line =~ /^\S/) {
                # not a struct or an assignment
                if (!($line =~ /^(typedef|struct)/ || $line =~ /=/)) {

                    if ($line =~ /\)\s*{$/) {
                        output "function declaration needs { in its own line.";
                    }
                }
            }

            # keyword statements need a space

            if ($line =~ /^\s*(if|while|for)\(/) {
                output "keywords needs a space after open parentheses.";
            }

            # '} else {' unless after a preprocessor directive

            if (!$prev_hash_comment && $line =~ /^\s*else\s+{/) {
                output "else needs to be in the same line as closing bracket like so: '} else {'";
            }

            # for

            if ($line =~ /^\s*for[ (]/) {
                if ($line =~ /\(;;\)/) {
                    output "infinite for loop should be 'for ( ;; )'";

                } elsif (!($line =~ /\( ;; \)/) && ($line =~ /;;/)) {
                    output "for loop without a condition needs '; /* void */ ;'";
                }
            }

            # labels need a single blank before and afterwards

            if (!($line =~ /default:/) && $line =~ /^\s*[a-zA-Z_][a-zA-Z0-9_]+:/) {
                $in_label = 1;

                ## Not the case in the Nginx codebase.
                #if ($line =~ /^\s+/) {
                #    output "label needs to be at first column.";
                #}

                if ($prev_label_blanks == 0) {
                    output "need a blank line before a label.";
                }

                if ($prev_label_blanks > 1) {
                    output "need one blank line before a label,"
                    . " got $prev_label_blanks.";
                }
            }

            # no spaces when or'ing constants

            if ($line =~ /[A-Z_][A-Z_]+ \| [A-Z_][A-Z_]+/) {
                output "the | operator takes no space when joining flag constants";
            }

            $prev_consecutive_empty_lines = $consecutive_empty_lines;

            # function declarations

            if ($infile =~ /\.c$/) {

                # return types are followed by a line break

                if ($line =~ /^((static\s+)?[a-z_]+\s+(\*+\s*)?)[a-z_]+\(/) {
                    if ($line =~ /\)$/) {
                        output "function declaration needs line break after return type.";

                    } elsif ($line !~ /;$/) {
                        $func_decl_multiline = $line;
                        save_var();
                    }

                } elsif (defined $func_decl_multiline) {
                    # declaration spans multiple lines
                    if ($line =~ /\)$/) {
                        var_output "function declaration needs line break after return type.";

                    } elsif ($line =~ /;$/) {
                        undef $func_decl_multiline;
                    }
                }

                # proper identation of multi-line declarations

                if ($in_func_decl) {
                    if ($line =~ /\(/) {
                        # func_name(...
                        if ($line !~ /^\S/) {
                            output "incorrect front spaces.";
                        }

                    } else {
                        # catch case when arguments are double-indented
                        # (accepted by ngx-style.pl)
                        my $invalid_width = $space_width * 2;

                        if ($line =~ /^\s{\Q$invalid_width\E}\S/) {
                            output "incorrect front spaces, bad indentation.";
                        }
                    }

                    if ($line =~ /\)$/) {
                        undef $in_func_decl;
                    }
                }

                if ($line =~ /^(static\s+)?[a-z_]+\s+(\*+\s*)?$/) {
                    # declaration spans multiple lines, check next line
                    $in_func_decl = 1;
                }
            }

            # expressions have their opening brackets on the same line

            if ($line =~ /(?:for|if|switch)\s+(\(.*?\))$/) {
                my $expr = $1;

                if ($expr =~ /^$balanced$/) {
                    # parenthesis are balanced, expression is complete in a single line

                    if ($line !~ /({|}|;|\*\/)$/) {
                        # line does not end with '{'
                        # (nor '}', ';', '*/' for loops)
                        output "single-line expression must open bracket on same line.";
                    }
                 }
            }

            # zero/null tests

            if ($line =~ /!= NULL/) {
                output "use 'if (ptr)' instead of if (ptr != NULL)";
            }

            # switch/case tests

            if ($line =~ /^(\s*)switch/) {
                push(@switch_level, length($1));

            } elsif ((scalar @switch_level) > 0) {
                if ($line =~ /^(\s*)case/) {
                    if (length($1) != $switch_level[-1]) {
                        output "use 'case' at the same indent level as 'switch'";
                    }

                } elsif ($line =~ /^(\s*)}/) {
                    if (length($1) == $switch_level[-1]) {
                        pop(@switch_level);
                    }
                }
            }

            ##################################################################
            # end new rules - excluding comment lines
            ##################################################################

        }

        # leave comment after the comment block end
        if ($line =~ /^((?<!\*\/).)*\*\// || $line =~ /^#endif/) {
            $full_comment = 0;
            #print "out comment block at line: $lineno\n";
        }

        # leave comment continue
        if ($comment_not_end == 1 && !($line =~ /\\$/)) {
            $full_comment = 0;
            $comment_not_end = 0;
        }

        # leave comment after the line handled
        if ($one_line_comment == 1) {
            $full_comment = 0;
            $one_line_comment = 0;
            $half_line_comment = 0;
        }

    }

    if ($cur_line_is_empty == 1) {
        output "file should not end with a blank line.";
    }
}

sub replace_quotes ($) {
    my ($str) = @_;
    while ($str =~ s/[^z]/z/g) {}
    $str;
}

sub output ($) {
    my ($str) = @_;

    if (!exists($files{$infile})) {
        print "\n$infile:\n";
        $files{$infile} = 1;
    }

    print "\033[31;1m$str\033[0m\n";
    print "$lineno: $line";
}

##################################################################
# begin new rules - var tracking
##################################################################

sub save_var () {
    $var_line = $line;
    $var_lineno = $lineno;
}

sub var_output ($) {
    my ($msg) = @_;
    my ($save_line, $save_lineno) = ($line, $lineno);
    $line = $var_line;
    $lineno = $var_lineno;
    output($msg);
    $line = $save_line;
    $lineno = $save_lineno;
}

##################################################################
# end new rules - var tracking
##################################################################
