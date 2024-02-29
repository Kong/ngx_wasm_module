# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: resolver_add directive - empty address
--- config
    resolver     1.1.1.1 ipv6=off;
    resolver_add '' hostname;
--- error_log eval
qr/\[emerg\] .*? invalid address value ""/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 2: resolver_add directive - empty host
--- config
    resolver     1.1.1.1 ipv6=off;
    resolver_add 127.0.0.1 '';
--- error_log eval
qr/\[emerg\] .*? invalid host value ""/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 3: resolver_add directive - bad IPv4 address
--- config
    resolver     1.1.1.1 ipv6=off;
    resolver_add 127.0.0 hostname;
--- error_log eval
qr/\[emerg\] .*? invalid address value "127.0.0"/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 4: resolver_add directive - bad IPv6 address
--- config
    resolver     1.1.1.1 ipv6=off;
    resolver_add 0:0:0:0:0:0 hostname;
--- error_log eval
qr/\[emerg\] .*? invalid address value "0:0:0:0:0:0"/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 5: resolver_add directive - no resolver
--- config
    resolver_add 127.0.0.1 hostname;
--- error_log eval
qr/\[emerg\] .*? no resolver defined and no default resolver/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 6: resolver_add directive - no resolver in configuration context
--- config
    resolver 1.1.1.1;

    location /t {
        resolver_add 127.0.0.1 hostname;
    }
--- error_log eval
qr/\[emerg\] .*? no resolver defined and no default resolver/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 7: resolver_add directive - no context resolver, adding to default resolver in wasm{} block
--- main_config
    wasm {}
--- config
    resolver_add 127.0.0.1 hostname;

    location /t {
        return 200;
    }
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 8: resolver_add directive - with context resolver, also adding to default resolver in wasm{} block
--- main_config
    wasm {}
--- config
    resolver     1.1.1.1;
    resolver_add 127.0.0.1 hostname;

    location /t {
        return 200;
    }
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 9: resolver_add directive - with context resolver, also adding to main resolver in wasm{} block
--- main_config
    wasm {
        resolver 8.8.8.8;
    }
--- config
    resolver     1.1.1.1;
    resolver_add 127.0.0.1 hostname;

    location /t {
        return 200;
    }
--- no_error_log
[error]
[crit]
[emerg]
