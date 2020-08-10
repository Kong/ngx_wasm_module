package t::TestWasm;

use Test::Nginx::Socket::Lua -Base;
use Cwd qw(cwd);

our $pwd = cwd();
our $crates = "work/lib/wasm";
our $buildroot = "../../work/buildroot";

our @EXPORT = qw(
    $pwd
    $crates
    $buildroot
);

$ENV{TEST_NGINX_HTML_DIR} = html_dir;
$ENV{TEST_NGINX_CRATES_DIR} = $crates;

add_block_preprocessor(sub {
    my $block = shift;

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

    # --- load_modules: ngx_http_echo_module

    my $dyn_modules = $block->load_modules;
    if (defined $dyn_modules) {
        my @modules = map { "load_module $buildroot/$_.so;" } grep { $_ } split /\s+/, $dyn_modules;
        if (@modules) {
            $block->set_value("main_config",
                              (join " ", @modules) . "\n" . $block->main_config);
        }
    }
});

1;
