package t::TestWasm;

use strict;
use Test::Nginx::Socket::Lua -Base;
use Test::Nginx::Util qw(gen_rand_port $RunTestHelper);
use List::Util qw(max);
use Cwd qw(cwd);
use Config;

our $pwd = cwd();
our $osname = $Config{"osname"};
our $crates = "$pwd/work/lib/wasm";
our $buildroot = "$pwd/work/buildroot";
our $nginxbin = $ENV{TEST_NGINX_BINARY} || 'nginx';
our $nginxV = eval { `$nginxbin -V 2>&1` };
our $exttimeout = $ENV{TEST_NGINX_EXTERNAL_TIMEOUT} || '60s';
our $extresolver = $ENV{TEST_NGINX_EXTERNAL_RESOLVER} || '8.8.8.8';
our @nginx_modules;

our @EXPORT = qw(
    $pwd
    $osname
    $crates
    $buildroot
    $nginxbin
    $nginxV
    $exttimeout
    $extresolver
    load_nginx_modules
    skip_no_ssl
    skip_no_debug
    skip_no_tinygo
    skip_valgrind
);

our $TestNginxRunTestHelper = $RunTestHelper;

$RunTestHelper = \&run_test_helper;

$ENV{TEST_NGINX_USE_HUP} ||= 0;
$ENV{TEST_NGINX_USE_VALGRIND} ||= 0;
$ENV{TEST_NGINX_USE_VALGRIND_ALL} ||= 0;
$ENV{TEST_NGINX_HTML_DIR} = html_dir();
$ENV{TEST_NGINX_DATA_DIR} = "$pwd/t/data";
$ENV{TEST_NGINX_CRATES_DIR} = $crates;
$ENV{TEST_NGINX_UNIX_SOCKET} = html_dir() . "/nginx.sock";
$ENV{TEST_NGINX_SERVER_PORT2} = gen_rand_port(1000);

my %colors = (
    UNEXPECTED => "\033[1;31m",
    ERROR => "\033[1;33;41m",
    FOUND => "\033[32m",
    MATCH => "\033[1;41m"
);

sub skip_valgrind (@) {
    my ($skip) = @_;

    if (!$ENV{TEST_NGINX_USE_VALGRIND}) {
        return;
    }

    if (defined $skip) {
        my $nver = (split /\n/, $nginxV)[0];

        if ($nver =~ m/$skip/) {
            plan skip_all => "skipped with Valgrind (nginx -V matches '$skip')";
        }

        return;
    }

    if (!$ENV{TEST_NGINX_USE_VALGRIND_ALL}) {
        plan skip_all => 'slow with Valgrind (set TEST_NGINX_USE_VALGRIND_ALL=1 to run)';
    }
}

sub skip_no_debug {
    if ($nginxV !~ m/--with-debug/) {
        plan(skip_all => "--with-debug required (NGX_BUILD_DEBUG=1)");
    }
}

sub skip_no_ssl {
    if ($nginxV !~ m/built with \S+SSL/) {
        plan(skip_all => "SSL support required (NGX_BUILD_SSL=1)");
    }
}

sub skip_no_tinygo {
    my @files = glob($ENV{TEST_NGINX_CRATES_DIR} . '/go_*.wasm');
    if (!@files && !defined($ENV{CI})) {
        plan(skip_all => "Missing Go .wasm bytecode files in $ENV{TEST_NGINX_CRATES_DIR}");
    }
}

sub load_nginx_modules (@) {
    splice @nginx_modules, 0, 0, @_;
}

add_block_preprocessor(sub {
    my $block = shift;

    if (!defined $block->log_level) {
        $block->set_value("log_level", "debug");
    }

    if (!defined $block->config) {
        $block->set_value("config", "location /t { return 200; }");
    }

    if (!defined $block->request) {
        $block->set_value("request", "GET /t");
    }

    # --- env variables

    #my $main_config = $block->main_config || '';
    #$block->set_value("main_config",
    #                  "env WASMTIME_BACKTRACE_DETAILS=1;\n"
    #                  . $main_config);

    # --- load_nginx_modules: ngx_http_echo_module

    my @arr;
    my @dyn_modules = @nginx_modules;
    my $load_nginx_modules = $block->load_nginx_modules;
    if (defined $load_nginx_modules) {
        @dyn_modules = split /\s+/, $load_nginx_modules;
    }

    # ngx_wasm_module.so injection

    if (defined $ENV{NGX_BUILD_DYNAMIC_MODULE}
        && $ENV{NGX_BUILD_DYNAMIC_MODULE} == 1
        && -e "$buildroot/ngx_wasm_module.so")
    {
        push @dyn_modules, "ngx_wasm_module";
    }

    if (@dyn_modules) {
        @arr = map { "load_module $buildroot/$_.so;" } @dyn_modules;
        my $main_config = $block->main_config || '';
        $block->set_value("main_config",
                          (join "\n", @arr)
                          . "\n\n"
                          . $main_config);
    }

    # compiler override when '--- wasm_modules' block is specified

    my $compiler;

    if ($nginxV =~ m/wasmer/) {
        $compiler = "singlepass";
    }

    # --- wasm_modules: on_phases

    my $wasm_modules = $block->wasm_modules;
    if (defined $wasm_modules) {
        @arr = split /\s+/, $wasm_modules;
        if (@arr) {
            @arr = map { "module $_ $crates/$_.wasm;" } @arr;
            my $wasm_config = "wasm {\n" .
                              "    " . (join "\n", @arr) . "\n";

            if (defined $compiler) {
                $wasm_config = $wasm_config .
                               "    compiler " . $compiler . ";\n";
            }

            my $backtraces = $block->backtraces;
            if (defined $backtraces) {
                $wasm_config = $wasm_config .
                               "    backtraces on;\n";
            }

            my $tls_skip_verify = $block->tls_skip_verify;
            my $tls_skip_host_check = $block->tls_skip_host_check;
            my $tls_trusted_certificate = $block->tls_trusted_certificate;

            if (defined $tls_skip_verify) {
                $wasm_config = $wasm_config .
                               "    tls_skip_verify " . $tls_skip_verify . ";\n";
            }

            if (defined $tls_skip_host_check) {
                $wasm_config = $wasm_config .
                               "    tls_skip_host_check " . $tls_skip_host_check . ";\n";
            }

            if (defined $tls_trusted_certificate) {
                $wasm_config = $wasm_config .
                               "    tls_trusted_certificate " . $tls_trusted_certificate . ";\n";
            }

            # --- shm_kv

            my $shm_kv = $block->shm_kv;
            if (defined $shm_kv) {
                @arr = split /,/, $shm_kv;
                @arr = map { "    shm_kv $_;" } @arr;
                $wasm_config = $wasm_config . (join "\n", @arr);
            }

            # --- shm_queue

            my $shm_queue = $block->shm_queue;
            if (defined $shm_queue) {
                @arr = split /,/, $shm_queue;
                @arr = map { "    shm_queue $_;" } @arr;
                $wasm_config = $wasm_config . (join "\n", @arr);
            }

            $wasm_config = $wasm_config . "}\n";

            my $main_config = $block->main_config || '';
            $block->set_value("main_config", $main_config . $wasm_config);
        }
    }

    # --- skip_no_debug: 3

    if (defined $block->skip_no_debug
        && !defined $block->skip_eval
        && $block->skip_no_debug =~ m/\s*(\d+)/)
    {
        $block->set_value("skip_eval", sprintf '%d: $::nginxV !~ m/--with-debug/', $1);
    }

    # --- skip_valgrind: 3

    if (defined $block->skip_valgrind
        && $ENV{TEST_NGINX_USE_VALGRIND}
        && $block->skip_valgrind =~ m/\s*(\d+)/)
    {
        $block->set_value("skip_eval", sprintf '%d: $ENV{TEST_NGINX_USE_VALGRIND} && !$ENV{TEST_NGINX_USE_VALGRIND_ALL}', $1);
    }

    # --- timeout_expected: 1

    if (!defined $block->timeout
        && defined $block->timeout_expected
        && $block->timeout_expected =~ m/\s*(\S+)/)
    {
        my $timeout = $1;

        if ($ENV{TEST_NGINX_USE_VALGRIND}) {
            if ($nginxV =~ m/wasmtime/ || $nginxV =~ m/v8/) {
                # Wasmtime and V8 (TurboFan) are much slower to load modules
                # min timeout: 45s
                $timeout += 30;
                $timeout = max($timeout, 45);

            } else {
                # Wasmer
                # min timeout: 30s
                $timeout += 30;
                $timeout = max($timeout, 30);
            }
        }

        $block->set_value("timeout", $timeout);
    }
});

sub error_log_data () {
    open my $in, $Test::Nginx::Util::ErrLogFile or
        return undef;

    my @lines = <$in>;

    close $in;
    return \@lines;
}

sub push_err($$$) {
    my ($err_out, $str, $color) = @_;

    if ($color ne "") {
        push(@$err_out, $color);
    }
    push(@$err_out, $str);
    if ($color ne "") {
        push(@$err_out, "\033[0m");
    }
}

sub run_test_helper ($$$) {
    my ($block, $dry_run, $repeated_req_idx) = @_;
    my $name = $block->name;

    $TestNginxRunTestHelper->(@_);

    if (defined $block->match_error_log) {
        my $match_error_log = $block->match_error_log;

        if (ref $match_error_log
            && ref $match_error_log eq 'ARRAY'
            && ref $match_error_log->[0]
            && ref $match_error_log->[0] eq 'ARRAY'
        ) {
            $match_error_log = $match_error_log->[$repeated_req_idx];
        }

        my @substrings;
        if (ref $match_error_log eq 'ARRAY') {
            @substrings = @$match_error_log;
        } else {
            @substrings = split("\n", $match_error_log);
        }
        my $numSubstrings = @substrings;

        my $match_pat;
        my $match_pats = $block->match_error_log_grep;
        if (!defined $match_pats) {
            bail_out("$name - No --- match_error_log_grep defined but --- match_error_log is defined");
        }

        if (ref $match_pats && ref $match_pats eq 'ARRAY') {
            $match_pat = $match_pats->[$repeated_req_idx];

        } else {
            $match_pat = $match_pats;
        }

        SKIP: {
            skip "$name - error_log - tests skipped due to $dry_run", 1 if $dry_run;

            my $lines;

            $lines ||= error_log_data();

            my @err_out = ();

            my $i = 0;
            my $failed = 0;
            my $newline = 1;
            my @missing = ();
            for my $line (@$lines) {
                if ($newline == 0 ||
                    (ref $match_pat &&
                     $line =~ /$match_pat/ ||
                     $line =~ /\Q$match_pat\E/)
                ) {
                    if ($i >= $numSubstrings) {
                        push_err(\@err_out, $line, "");
                        if ($newline == 1) {
                            push_err(\@err_out, "^---- no match declared for the line above.", $colors{"ERROR"});
                            push_err(\@err_out, "\n", "");
                        }
                        $newline = 1;
                        next;
                    }

                    my $expected_at = index($line, $substrings[$i]);

                    my $earliest_i = -1;
                    my $earliest_at = 2**32;
                    for (my $j = 0; $j < $numSubstrings; $j++) {
                        my $at = index($line, $substrings[$j]);
                        if ($at != -1) {
                            if ($at < $earliest_at) {
                                $earliest_at = $at;
                                $earliest_i = $j;
                            }
                        }
                    }


                    if ($earliest_i != -1) {
                        my $earliest_len = length($substrings[$earliest_i]);
                        my $expected_len = length($substrings[$i]);

                        if ($expected_at != -1 && $earliest_at + $earliest_len >= $expected_at) {
                            $earliest_i = $i;
                            $earliest_len = $expected_len;
                        }

                        my $rest_at = $earliest_at + $earliest_len;

                        push_err(\@err_out, substr($line, 0, $earliest_at), "");
                        my $color;
                        if ($earliest_i == $i) {
                            $color = $colors{"FOUND"};
                            $i++;
                        } else {
                            $color = $colors{"UNEXPECTED"};
                            $failed = 1;
                            push(@missing, $substrings[$i]);
                            $i++;
                        }
                        push_err(\@err_out, substr($line, $earliest_at, $earliest_len), $color);

                        $line = substr($line, $rest_at);
                        if (length($line) > 0) {
                            $newline = 0;
                            redo;
                        }

                    } else {
                        if ($newline) {
                            $failed = 1;

                            push(@missing, $substrings[$i]);
                            $i++;
                        }
                    }

                    push_err(\@err_out, $line, "");
                    for my $p (@missing) {
                        push_err(\@err_out, "^---- expected ", $colors{"ERROR"});
                        push_err(\@err_out, $p, $colors{"MATCH"});
                        push_err(\@err_out, " in line above.", $colors{"ERROR"});
                        push_err(\@err_out, "\n", "");
                    }
                    @missing = ();
                    $newline = 1;
                }
            }

            # fail if we didn't find them all
            if ($i < $numSubstrings) {
                $failed = 1;
                for (my $j = $i; $j < $numSubstrings; $j++) {
                    push_err(\@err_out, "---- missing a match for ", $colors{"ERROR"});
                    push_err(\@err_out, $substrings[$j], $colors{"MATCH"});
                    push_err(\@err_out, "\n", "");
                }
            }

            if ($failed == 1) {
                fail("$name - match_error_log:\n" . join("", @err_out));
            } else {
                pass("$name - all substrings were matched in order in error.log");
            }

            # fail if we didn't find them all
            if ($i < $numSubstrings) {

                my $ord = ordinate($i);
                my $missing = $substrings[$i];
                SKIP: {
                    skip "$name - error_log - tests skipped due to $dry_run", 1 if $dry_run;
                }
            } else {
            }
        }
    } else {
        if (defined $block->match_error_log_grep) {
            bail_out("$name - No --- match_error_log defined but --- match_error_log_grep is defined");
        }
    }
}

no_long_string();

1;
