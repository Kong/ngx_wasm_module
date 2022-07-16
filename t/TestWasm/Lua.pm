package t::TestWasm::Lua;

use strict;
use t::TestWasm -Base;

our $lua_package_path = "${buildroot}/prefix/?.lua;${buildroot}/prefix/?/init.lua;./lib/?.lua;;";

our @EXPORT = qw(
    $lua_package_path
    skip_no_openresty
);

sub skip_no_openresty {
    my $build_name = (split /\n/, $nginxV)[0];

    if ($build_name !~ m/nginx version: openresty/) {
        plan skip_all => "skipped (not an OpenResty binary)";
    }
}

add_block_preprocessor(sub {
    my $block = shift;
    my $http_config = $block->http_config || '';

    $http_config .= <<_EOC_;
    lua_package_path '$lua_package_path';
_EOC_

    $block->set_value("http_config", $http_config);
});

1;
