package t::TestBuild;

use strict;
use Cwd qw( cwd );
use IPC::Run qw( run );
use File::Temp qw( tempdir );
use File::Path qw( make_path );
use Test::Base -Base;
use Test::LongString;

$ENV{NGX_BUILD_DIR_BUILDROOT} = tempdir(CLEANUP => 1);
$ENV{NGX_BUILD_DIR_SRCROOT} = tempdir(CLEANUP => 1);

our $buildroot = $ENV{NGX_BUILD_DIR_BUILDROOT};

our @EXPORT = qw(
    run_tests
    $buildroot
);

my $dry_run = 0;
my $cwd = cwd;

sub bail_out (@) {
    Test::More::BAIL_OUT(@_);
}

sub trim ($) {
    my $s = shift;
    return undef if !defined $s;
    chomp $s;
    $s =~ s/^\s+|\s+$//g;
    $s =~ s/\n/ /gs;
    $s =~ s/\s{2,}/ /gs;
    $s;
}

sub fmt_str ($) {
    my $s = shift;
    return undef if !defined $s;
    chomp $s;
    $s =~ s/"/\\"/g;
    $s =~ s/\r/\\r/g;
    $s =~ s/\n/\\n/g;
    $s;
}

sub exp_to_patterns ($) {
    my $exp = shift;

    if (!ref $exp) {
        chomp $exp;
        return split /\n+/, $exp;
    }

    if (ref $exp eq 'Regexp') {
        return [$exp];
    }

    return @$exp;
}

sub run_test ($) {
    my $block = shift;
    my $name = $block->name;
    my $exp_rc = $block->exit_code // 0;
    my $exp_grep_nginxV = $block->grep_nginxV;
    my $exp_no_grep_nginxV = $block->no_grep_nginxV;
    my $cmd = trim($block->run_cmd);
    my $exp_no_grep_cmd = $block->no_grep_cmd;
    my $exp_grep_cmd = $block->grep_cmd;
    my $exp_build = trim($block->build) or
                        bail_out "$name - No '--- build' specified";

    my ($in, $out, $err, $rc);
    run ["sh", "-c", $exp_build], \$in, \$out, \$err;
    $rc = $?;

    # --- exit_code: 0

    is $exp_rc, $rc >> 8, "$name - exit code ok";

    if (defined $err && $err ne '') {
        warn "$name - stderr:\n$err";
    }

    run ["$ENV{NGX_BUILD_DIR_BUILDROOT}/nginx", "-V"], \$in, \$out, \$err;

    my $nginxV = $err;

    # --- grep_nginxV

    if (defined $exp_grep_nginxV) {
        my @grep_nginxV = exp_to_patterns $exp_grep_nginxV;

        for my $pat (@grep_nginxV) {
            next if !defined $pat;

            my $p = fmt_str($pat);
            my $v = fmt_str($nginxV);

            SKIP: {
                skip "$name - grep_nginxV - test skipped", 1 if $dry_run;

                if ((ref $pat && $nginxV =~ /$pat/) || $nginxV =~ /\Q$pat\E/) {
                    pass("$name - pattern \"$p\" matches 'nginx -V'");

                } else {
                    fail("$name - pattern \"$p\" should match 'nginx -V' but does not match \"$v\"");
                }
            }
        }
    }

    # --- no_grep_nginxV

    if (defined $exp_no_grep_nginxV) {
        my %found;
        my @no_grep_nginxV = exp_to_patterns $exp_no_grep_nginxV;

        for my $pat (@no_grep_nginxV) {
            next if !defined $pat;

            if ((ref $pat && $nginxV =~ /$pat/) || $nginxV =~ /\Q$pat\E/) {
                if ($found{$pat}) {
                    my $tb = Test::More->builder;
                    $tb->no_ending(1);

                } else {
                    $found{$pat} = 1;
                }

                my $p = fmt_str($pat);
                my $v = fmt_str($nginxV);

                SKIP: {
                    skip "$name - no_grep_nginxV - test skipped", 1 if $dry_run;
                    fail("$name - pattern \"$p\" should not match 'nginx -V' but matches \"$v\"");
                }
            }
        }

        for my $pat (@no_grep_nginxV) {
            next if $found{$pat};
            next if !defined $pat;

            my $p = fmt_str($pat);

            SKIP: {
                skip "$name - no_grep_nginxV - test skipped", 1 if $dry_run;
                pass("$name - pattern \"$p\" does not match 'nginx -V'");
            }
        }
    }

    # --- cmd

    if (defined $cmd) {
        run ["sh", "-c", $cmd], \$in, \$out, \$err or die "$cmd: $? (".trim($err).")";

        # --- grep_cmd

        if (defined $exp_grep_cmd) {
            my @grep_cmd = exp_to_patterns $exp_grep_cmd;

            for my $pat (@grep_cmd) {
                next if !defined $pat;

                my $p = fmt_str($pat);
                my $c = fmt_str($cmd);
                my $v = fmt_str($out);

                SKIP: {
                    skip "$name - grep_cmd - test skipped", 1 if $dry_run;

                    if ((ref $pat && $out =~ /$pat/) || $out =~ /\Q$pat\E/) {
                        pass("$name - pattern \"$p\" matches \"$c\" output");

                    } else {
                        fail("$name - pattern \"$p\" should match output of \"$c\" but does not match \"$v\"");
                    }
                }
            }
        }

        # --- no_grep_cmd

        if (defined $exp_no_grep_cmd) {
            my %found;
            my @no_grep_cmd = exp_to_patterns $exp_no_grep_cmd;

            for my $pat (@no_grep_cmd) {
                next if !defined $pat;

                if ((ref $pat && $out =~ /$pat/) || $out =~ /\Q$pat\E/) {
                    if ($found{$pat}) {
                        my $tb = Test::More->builder;
                        $tb->no_ending(1);

                    } else {
                        $found{$pat} = 1;
                    }

                    my $p = fmt_str($pat);
                    my $c = fmt_str($cmd);
                    my $v = fmt_str($out);

                    SKIP: {
                        skip "$name - no_grep_cmd - test skipped", 1 if $dry_run;
                        fail("$name - pattern \"$p\" should not match output of \"$c\" but matches \"$v\"");
                    }
                }
            }

            for my $pat (@no_grep_cmd) {
                next if $found{$pat};
                next if !defined $pat;

                my $p = fmt_str($pat);
                my $c = fmt_str($cmd);

                SKIP: {
                    skip "$name - no_grep_cmd - test skipped", 1 if $dry_run;
                    pass("$name - pattern \"$p\" does not match output of \"$c\"");
                }
            }


        }
    }

    chdir $cwd or die $!;
}

sub run_tests () {
    for my $block (blocks()) {
        run_test($block);
    }
}

1;
