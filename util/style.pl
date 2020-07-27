#!/usr/bin/env perl

# Borrowed from OpenResty's ngx-style.pl script:
# https://github.com/openresty/openresty-devel-utils

use strict;
use warnings;

sub output($);
sub replace_quotes($);

my $exit_status = 0;

my %files;
my $space_width = 4;
my ($infile, $lineno, $line);

for my $file (@ARGV) {
    $infile = $file;
    #print "$infile\n";

    open my $in, $infile or die $!;

    $lineno = 0;

    my $level = 0;

    # block flags
    my ($just_enter_block, $just_leave_block, $blank_between_block) = (0, 0, 0);

    # comment flags
    my ($full_comment, $one_line_comment, $half_line_comment, $comment_not_end) = (0, 0, 0, 0);

    # bracket flags
    my ($unclosed_brackets, $unclosed_brackets_lineno, $unfinished_space) = (0, 0, 0);

    # level flags
    my ($next_level, $next_level_space) = (0, 0);

    # macro flags
    my ($macro_defined) = (0);

    # variable align flags
    my ($need_variable_align, $variable_align_space) = (0, 0, 0);

    while (<$in>) {
        $line = $_;

        $lineno++;

        #print "$lineno: $line";

        if ($line =~ /\r\n$/) {
            output "found DOS line ending";
        }

        if ($line =~ /(\s+)[\r]?\n$/) {
            output "found line tailing spaces";
        }

        # force point convert
        if ($line =~ /\(\w+\*\)/) {
            output "need space before *";
        }

        if ($line =~ /^typedef struct \w+( *)(\w+);/) {
            if (length($1) != 2) {
                output "need two space before $2";
            }
        }

        # function close or struct close
        if ($line =~ /^}/) {
            $just_leave_block = 1;
            $blank_between_block = 2;

            if ($need_variable_align == 1) {
                $need_variable_align = 0;
                $variable_align_space = 0;
            }
        }

        # one line comment
        if ($line =~ /^#/) {
            $full_comment = 1;
            $just_leave_block = 0;

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

        if ($line =~ /^#if/) {
            $macro_defined = 1;
            #print "enter macro_defined\n";
        }

        my $space = 0;
        if ($line =~ /^( *).*$/) {
            $space = length $1;
        }

        my $line_without_quote = $line;
        #while ($line_without_quote =~ s/"[^"]+"//g) {}
        while ($line_without_quote =~ s#"(?:\\.|[^"]*)*"#replace_quotes($&)#ge) {}

        my $line_without_brackets = $line_without_quote;
        while ($line_without_brackets =~ s#\([^()]*\)#replace_quotes($&)#ge) {}
        #print $line_without_brackets;

        # check this only without any comment
        if ($full_comment == 0 && $half_line_comment == 0) {
            # 1.
            # space before '+', "&&", '=', '*', '-'
            # space after  '+', "&&", '=', ','
            foreach my $symbol ('\+', '-', '&&', '=', '\*') {
                #print "$symbol\n";
                while ($line_without_quote =~ /\w+(\s?)($symbol)(\s?)\w+/g) {
                    if (length($1) == 0) {
                        output "need space before $2";
                    }
                    if (length($3) == 0 && $2 ne "*" && $2 ne "-") {
                        output "need space after $2";
                    }
                }
            }

            # 2.
            while ($line_without_quote =~ /\w+(\s?)(,)(\s?)\w+/g) {
                if (length($1) != 0) {
                    output "do not need space before ,";
                }
                if (length($3) != 1) {
                    # print $line_without_quote;
                    output "need one space after ,";
                }
            }

            # 3.
            if ($line =~ /\(\w+\)\w+/) {
                output "need space after )";
            }
        }

        # not full_comment
        if ($full_comment == 0) {

            # check the front space

            # not empty line
            if ($line ne "\n") {
                # check the indent after unclosed bracket
                if ($unclosed_brackets
                    && (
                        $space != $unfinished_space
                        && $space != $space_width
                        && $space != $space_width * 2)

                    # skip: line too long
                    && length($line) - 1 - $space + $unfinished_space <= 80

                    # only check the next line
                    && $lineno == $unclosed_brackets_lineno + 1

                    # skip: start with ')'
                    && $line_without_brackets !~ /^\s*\)/)
                {
                    output "incorrect front spaces, unclosed bracket";
                }

                # we only check the next line after '{' for now
                if ($next_level == 1) {
                    $next_level = 0;

                    if ($space != $next_level_space) {
                        if (!($line =~ /^\s*\}/) && !($line =~ /^\w+:$/)) {
                            output "incorrect front spaces, level indent";
                        }
                    }
                }

                if ($space % 4 != 0) {
                    #output "warning: wrong front space?";
                }

            }

            # enter next level state
            if ($macro_defined == 0 && $line =~ /^((?<!switch).)*{\n$/) {
                $next_level = 1;
                $next_level_space = $space + 4;
            }

            # enter unclosed bracket state; only check one level
            if ($line_without_brackets =~ /^([^(]*?\()[^(]*$/g) {
                #print "$1\n";
                $unclosed_brackets = 1;
                $unclosed_brackets_lineno = $lineno;
                $unfinished_space = length $1;
            }

            # leave unclosed bracket state
            if ($unclosed_brackets == 1) {
                if ($line_without_brackets =~ /\)([^)]*$)/g) {
                    $unclosed_brackets = 0;
                }
            }
        }

        if ($need_variable_align == 1) {
            # struct skip empty line; but function break on empty line
            if ($line eq "\n" || $full_comment == 1) {
                $variable_align_space = 0;
            } else {
                if ($line =~ /(^\s+\w+(\s\w+)?\s+\**)\w+(,|;| =)/) {
                    if ($variable_align_space > 0 && $variable_align_space != length($1)) {
                        output "variable name should align with the previous line";
                    }
                    $variable_align_space = length($1);
                } else {
                    $need_variable_align = 0;
                    $variable_align_space = 0;
                }
            }
        }

        if ($macro_defined != 0) {
            if ($line =~ /^#endif/) {
                $macro_defined = 0;
            }
            if ($macro_defined == 1) {
                $macro_defined = 2;
            } else {
                $macro_defined = 0;
            }
        }

        if ($just_enter_block == 1) {
            if ($line eq "\n") {
                output "found blank line in first line";
            } else {
                $just_enter_block = 0;
            }
        }

        if ($just_leave_block == 1 && !($line =~ /^}/)) {
            if ($line eq "\n") {
                $blank_between_block--;
            } else {
                $just_leave_block = 0;

                #if ($line =~ /^\/\* vi:set/) {
                #    if ($blank_between_block != 1) {
                #        my $blank_line = 2 - $blank_between_block;
                #        output "found $blank_line blank line before vi:set";
                #    }
                #} else {
                #    if ($blank_between_block != 0) {
                #        my $blank_line = 2 - $blank_between_block;
                #        output "found $blank_line blank line between blocks";
                #    }
                #}
            }
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

        # function open or struct open
        if ($line =~ /^{$/ || $line =~ /struct \w+ \{$/) {
            $need_variable_align = 1;
            $variable_align_space = 0;
            $just_enter_block = 1;
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

    if (!exists($files{$infile})) {
        if ($exit_status != 0) {
            print "\n";
        }
        print "$infile:\n";
        $files{$infile} = 1;
    }

    print "\033[31;1m$str\033[0m\n";
    print "$lineno: $line";

    $exit_status = 1;
}

exit $exit_status;

# vim: ft=perl st=4 sts=4 sw=4:
