package t::TestWasm;

use Test::Nginx::Socket::Lua -Base;
use Cwd qw(cwd);

our $pwd = cwd();

our @EXPORT = qw(
    $pwd
);

$ENV{TEST_NGINX_HTML_DIR} = html_dir;

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
});

1;
