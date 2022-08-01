#!/usr/bin/env perl

use strict;
use warnings;

sub output($);
sub replace_quotes($);

my %files;
my $space_with = 4;

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

            # comment // */
            if ($line =~ /^\s*\/\//) {
                output "avoid C99-style comments ( // ), use C89-style ( /* */ )";
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
}


sub replace_quotes ($) {
    my ($str) = @_;
    while ($str =~ s/[^z]/z/g) {}
    $str;
}

sub output ($) {
    my ($str) = @_;

    # skip *_lua_lex.c
    if ($infile =~ /_lua_lex.c$/) {
        return;
    }

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
