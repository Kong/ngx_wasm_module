# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestBuild;

static_only();

our $buildroot = $t::TestBuild::buildroot;

plan tests => 4 * blocks();

run_tests();

__DATA__

=== TEST 1: build with static runtime
--- build: make
--- run_cmd eval: qq{ldd $::buildroot/nginx}
--- no_grep_cmd eval
[
    qr/libwasmtime/,
    qr/libwasmer/,
    qr/libwasm/,
]
