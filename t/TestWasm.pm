package t::TestWasm;

use strict;
use Test::Nginx::Socket::Lua -Base;
use Cwd qw(cwd);

our $pwd = cwd();
our $crates = "work/lib/wasm";
our $buildroot = "../../work/buildroot";
our $nginxbin = $ENV{TEST_NGINX_BINARY} || 'nginx';
our $nginxV = eval { `$nginxbin -V 2>&1` };
our @nginx_modules;

our @EXPORT = qw(
    $pwd
    $crates
    $buildroot
    $nginxbin
    $nginxV
    skip_no_debug
    load_nginx_modules
);

$ENV{TEST_NGINX_HTML_DIR} = html_dir;
$ENV{TEST_NGINX_CRATES_DIR} = $crates;

sub skip_no_debug {
    if ($nginxV !~ m/--with-debug/) {
        plan(skip_all => "--with-debug required (NGX_BUILD_DEBUG=1)");
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

    if (!defined $block->main_config) {
        $block->set_value("main_config", "wasm {}");
    }

    if (!defined $block->config) {
        $block->set_value("config", "location /t { return 200; }");
    }

    if (!defined $block->request) {
        $block->set_value("request", "GET /t");
    }

    if (!defined $block->no_error_log) {
        $block->set_value("no_error_log", "[error]");
    }

    # --- load_nginx_modules: ngx_http_echo_module

    my @dyn_modules = @nginx_modules;

    my $load_nginx_modules = $block->load_nginx_modules;
    if (defined $load_nginx_modules) {
        splice @dyn_modules, 0, 0, split('/\s+/', $load_nginx_modules);
    }

    if (@dyn_modules) {
        my @modules = map { "load_module $buildroot/$_.so;" } @dyn_modules;
        $block->set_value("main_config",
                          (join "\n", @modules) . "\n\n" . $block->main_config);
    }

    # --- skip_no_debug: 3

    if (defined $block->skip_no_debug
        && !defined $block->skip_eval
        && $block->skip_no_debug =~ m/\s*(\d+)/)
    {
        $block->set_value("skip_eval", sprintf '%d: $::nginxV !~ m/--with-debug/', $1);
    }
});

no_long_string();

1;
