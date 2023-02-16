package t::TestWasm;

use strict;
use Test::Nginx::Socket::Lua -Base;
use Test::Nginx::Util qw(gen_rand_port);
use List::Util qw(max);
use Cwd qw(cwd);

our $pwd = cwd();
our $crates = "$pwd/work/lib/wasm";
our $buildroot = "$pwd/work/buildroot";
our $nginxbin = $ENV{TEST_NGINX_BINARY} || 'nginx';
our $nginxV = eval { `$nginxbin -V 2>&1` };
our $exttimeout = $ENV{TEST_NGINX_EXTERNAL_TIMEOUT} || '60s';
our $extresolver = $ENV{TEST_NGINX_EXTERNAL_RESOLVER} || '8.8.8.8';
our @nginx_modules;

our @EXPORT = qw(
    $pwd
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

$ENV{TEST_NGINX_USE_HUP} ||= 0;
$ENV{TEST_NGINX_HTML_DIR} = html_dir();
$ENV{TEST_NGINX_DATA_DIR} = "$pwd/t/data";
$ENV{TEST_NGINX_CRATES_DIR} = $crates;
$ENV{TEST_NGINX_UNIX_SOCKET} = html_dir() . "/nginx.sock";
# TODO: drop TEST_NGINX_SERVER_PORT2 when tcp_sock supports
# unix: sockets to avoid port conflicts in randomized tests
$ENV{TEST_NGINX_SERVER_PORT2} = gen_rand_port(1000);

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

    if (!defined $ENV{TEST_NGINX_USE_VALGRIND_ALL}) {
        plan skip_all => 'slow with Valgrind (set TEST_NGINX_USE_VALGRIND_ALL to run)';

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
            my $main_config = $block->main_config || '';
            my $wasm_config = "wasm {\n" .
                              "    " . (join "\n", @arr) . "\n";

            if (defined $compiler) {
                $wasm_config = $wasm_config .
                               "    compiler " . $compiler . ";\n";
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
        && !defined $ENV{TEST_NGINX_USE_VALGRIND_ALL}
        && $block->skip_valgrind =~ m/\s*(\d+)/)
    {
        $block->set_value("skip_eval", sprintf '%d: $ENV{TEST_NGINX_USE_VALGRIND} && !defined $ENV{TEST_NGINX_USE_VALGRIND_ALL}', $1);
    }

    # --- timeout_no_valgrind: 1

    if (!defined $block->timeout
        && defined $block->timeout_no_valgrind
        && $block->timeout_no_valgrind =~ m/\s*(\S+)/
        && !defined $ENV{TEST_NGINX_USE_VALGRIND})
    {
        my $timeout = $1;

        if ($nginxV =~ m/wasmtime/ || $nginxV =~ m/v8/) {
            # Wasmtime and V8 (TurboFan) are much slower to load modules
            $timeout += 30;
            $timeout = max($timeout, 45);
        }

        $block->set_value("timeout", $timeout);
    }
});

no_long_string();

1;
