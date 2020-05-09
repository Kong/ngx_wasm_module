package t::TestWasm;

use Test::Nginx::Socket::Lua -Base;
use Cwd qw(cwd);

our $pwd = cwd();

our @EXPORT = qw(
    $pwd
);

add_block_preprocessor(sub {
    my $block = shift;

    if (!defined $block->request) {
        $block->set_value("request", "GET /");
    }
});

1;
