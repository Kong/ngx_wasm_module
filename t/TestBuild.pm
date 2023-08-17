package t::TestBuild;

use strict;
use Cwd qw(cwd);
use IPC::Run qw(run);
use File::Temp qw(tempdir);
use File::Path qw(make_path);
use Test::Base -Base;
use Test::LongString;

$ENV{TEST_NGINX_NPROC} ||= 0;
$ENV{NGX_WASM_RUNTIME} ||= 'wasmtime';
$ENV{NGX_BUILD_DIR_BUILDROOT} ||= tempdir(CLEANUP => 1);

our $buildroot = $ENV{NGX_BUILD_DIR_BUILDROOT};

our @EXPORT = qw(
    $buildroot
    run_tests
    get_variable_from_makefile
);

my $dry_run = 0;
my $pwd = cwd();
my ($out, $err);
my $cmd = <<"_CMD_";
    export NGX_WASM_DIR=$pwd &&
    . $pwd/util/_lib.sh &&
    get_default_runtime_dir $ENV{NGX_WASM_RUNTIME}
_CMD_

run ["bash", "-c", $cmd], \undef, \$out, \$err;
if (defined $err && $err ne '') {
    warn "failed to get default runtime_dir: $err";
}

chomp $out;

$ENV{NGX_WASM_RUNTIME_DIR} = $out;

sub bail_out (@) {
    Test::More::BAIL_OUT(@_);
}

sub get_variable_from_makefile($) {
    my $name = shift;

    my $cmd = <<"_CMD_";
        export NGX_WASM_DIR=$pwd &&
        . $pwd/util/_lib.sh &&
        get_variable_from_makefile $name
_CMD_

    run ["bash", "-c", $cmd], \undef, \$out, \$err;
    if (defined $err && $err ne '') {
        warn "failed to get variable from Makefile: $err";
    }

    chomp $out;

    return $out;
}

sub get_libs () {
    my ($out, $err);

    if (-e "$buildroot/nginx") {
        if ($^O eq 'darwin') {
            run ["otool", "-L", "$buildroot/nginx"], \undef, \$out, \$err;

        } else {
            run ["ldd", "$buildroot/nginx"], \undef, \$out, \$err;
        }

        if (defined $err && $err ne '') {
            warn "failed to read libs\n$err";
        }
    }

    return $out;
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

sub grep_something ($$$) {
    my $block = shift;
    my ($grep_name, $out) = @_;
    my $name = $block->name;
    my $ex = $block->{$grep_name}->[0];
    return if !defined $ex;
    my @patterns = exp_to_patterns $ex;

    if ($grep_name =~ /^no_grep/) {
        my %found;

        for my $pat (@patterns) {
            next if !defined $pat;

            if ((ref $pat && $out =~ /$pat/) || $out =~ /\Q$pat\E/) {
                if ($found{$pat}) {
                    my $tb = Test::More->builder;
                    $tb->no_ending(1);

                } else {
                    $found{$pat} = 1;
                }

                my $p = fmt_str($pat);
                my $v = fmt_str($out);

                SKIP: {
                    skip "$name - $grep_name - test skipped", 1 if $dry_run;
                    fail "$name - $grep_name pattern \"$p\" should not match output \"$v\"";
                }
            }
        }

        for my $pat (@patterns) {
            next if !defined $pat;
            next if $found{$pat};

            my $p = fmt_str($pat);
            my $v = fmt_str($out);

            SKIP: {
                skip "$name - $grep_name - test skipped", 1 if $dry_run;
                pass "$name - $grep_name pattern \"$p\" does not match output \"$v\"";
            }
        }

    } else {
        for my $pat (@patterns) {
            next if !defined $pat;

            my $p = fmt_str($pat);
            my $v = fmt_str($out);

            SKIP: {
                skip "$name - $grep_name - test skipped", 1 if $dry_run;

                if ((ref $pat && $out =~ /$pat/) || $out =~ /\Q$pat\E/) {
                    pass "$name - $grep_name pattern \"$p\" matches output";

                } else {
                    fail "$name - $grep_name pattern \"$p\" should match output but does not match \"$v\"";
                }
            }
        }
    }
}

sub run_test ($) {
    my $block = shift;
    my $name = $block->name;
    my $exp_rc = $block->exit_code // 0;
    my $skip_eval = $block->skip_eval;

    if (defined $skip_eval
        && $skip_eval =~ m{^ \s* (\d+) \s* : \s* (.*)}xs)
    {
        my $ntests = $1;
        my $should_skip = eval $2;
        if ($@) {
            bail_out("$name - skip_eval: failed to eval \"$2\": $@");
        }

        if ($should_skip) {
            SKIP: {
                skip "$name - skip_eval: test skipped", $ntests;
            }

            return;
        }
    }

    my $cmd = trim($block->run_cmd);
    my $build = trim($block->build) or
                     bail_out "$name - No '--- build' specified";
    if ($build =~ m{make \s* $}xs) {
        $build .= " -j$ENV{TEST_NGINX_NPROC}";
    }

    my ($out, $err, $rc);
    run ["rm", "-rf", $buildroot];
    run ["sh", "-c", $build], \undef, \$out, \$err;
    $rc = $? >> 8;

    # --- exit_code: 0

    is $rc, $exp_rc, "$name - exit code matches";

    if ($rc != 0 && defined $err && $err ne '') {
        warn "$name - stderr:\n$out\n$err" unless defined $block->expect_stderr;
    }

    # --- grep_build

    if (defined $out) {
        grep_something($block, "grep_build", $out);
        grep_something($block, "no_grep_build", $out);

        # --- grep_nginxV

        my $nginxV;

        if (-e "$buildroot/nginx") {
            run ["$buildroot/nginx", "-V"], \undef, \$out, \$err;
            $nginxV = $err;
        }

        if (defined $nginxV) {
            grep_something($block, "grep_nginxV", $nginxV);
            grep_something($block, "no_grep_nginxV", $nginxV);
        }
    }

    # --- cmd

    if (defined $cmd) {
        run ["sh", "-c", $cmd], \undef, \$out, \$err
            or die "$cmd: $? (".trim($err).")";

        # --- grep_cmd

        if (defined $out) {
            grep_something($block, "grep_cmd", $out);
            grep_something($block, "no_grep_cmd", $out);
        }
    }

    # --- grep_libs

    if (defined $block->grep_libs || defined $block->no_grep_libs) {
        my $libs = get_libs();

        if (defined $libs) {
            grep_something($block, "grep_libs", $libs);
            grep_something($block, "no_grep_libs", $libs);
        }
    }

    chdir $pwd or die $!;
}

sub run_tests () {
    for my $block (blocks()) {
        run_test($block);
    }
}

1;
