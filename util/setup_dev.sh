#!/usr/bin/env bash
set -e

SCRIPT_NAME=$(basename $0)
NGX_WASM_DIR=${NGX_WASM_DIR:-"$(
    cd $(dirname $(dirname ${0}))
    pwd -P
)"}
if [[ ! -f "${NGX_WASM_DIR}/util/_lib.sh" ]]; then
    echo "Unable to source util/_lib.sh" >&2
    exit 1
fi

source $NGX_WASM_DIR/util/_lib.sh

###############################################################################

mkdir -p $DIR_CPANM $DIR_BIN $DIR_TESTS_LIB_WASM

pushd $DIR_CPANM
    if [[ ! -x "cpanm" ]]; then
        notice "downloading cpanm..."
        download cpanm https://cpanmin.us/
        chmod +x cpanm
    fi

    export PERL5LIB=$DIR_CPANM
    PERL_ARGS="--notest -L $DIR_CPANM"

    if [[ -n "$CI" ]]; then
        PERL_ARGS="--reinstall $PERL_ARGS"
    fi

    notice "downloading Test::Nginx dependencies..."
    HOME=$DIR_CPANM ./cpanm $PERL_ARGS Test::Nginx
    HOME=$DIR_CPANM ./cpanm $PERL_ARGS IPC::Run

    set +e
    patch --forward --ignore-whitespace lib/perl5/Test/Nginx/Util.pm <<'EOF'
    @@ -484,6 +484,7 @@ sub master_process_enabled (@) {
     }

     our @EXPORT = qw(
    +    gen_rand_port
         use_http2
         use_http3
         env_to_nginx
EOF
    patch --forward --ignore-whitespace lib/perl5/Test/Nginx/Util.pm <<'EOF'
    @@ -960,5 +960,5 @@ sub write_config_file ($$$) {
             bail_out "Can't open $ConfFile for writing: $!\n";
    +    print $out "daemon $DaemonEnabled;" if ($DaemonEnabled eq 'off');
         print $out <<_EOC_;
     worker_processes  $Workers;
    -daemon $DaemonEnabled;
     master_process $MasterProcessEnabled;
EOF
    patch --forward --ignore-whitespace lib/perl5/Test/Nginx/Util.pm <<'EOF'
    @@ -2783,7 +2783,7 @@ END {

         check_prev_block_shutdown_error_log();

    -    if ($Randomize) {
    +    if ($Randomize && !$ENV{TEST_NGINX_NO_CLEAN}) {
             if (defined $ServRoot && -d $ServRoot && $ServRoot =~ m{/t/servroot_\d+}) {
                 system("rm -rf $ServRoot");
             }
EOF
    patch --forward --ignore-whitespace lib/perl5/Test/Nginx/Socket.pm <<'EOF'
    @@ -813,6 +813,10 @@ again:

         test_stap($block, $dry_run);

    +    if ($repeated_req_idx == 0) {
    +        $repeated_req_idx = $req_idx - 1;
    +    }
    +
         check_error_log($block, $res, $dry_run, $repeated_req_idx, $need_array);

         if (!defined $block->ignore_response) {
EOF

    set -e
popd

pushd $DIR_BIN
    notice "downloading the reindex script..."
    download reindex https://raw.githubusercontent.com/openresty/openresty-devel-utils/master/reindex
    chmod +x reindex

    notice "downloading the style script..."
    download style https://raw.githubusercontent.com/openresty/openresty-devel-utils/master/ngx-style.pl
    chmod +x style
popd

if [[ -d "$DIR_NGX_ECHO_MODULE" ]]; then
    notice "updating the echo-nginx-module repository..."
    pushd $DIR_NGX_ECHO_MODULE
        git fetch
        git reset --hard origin/master
    popd
else
    notice "cloning the echo-nginx-module repository..."
    git clone https://github.com/openresty/echo-nginx-module.git $DIR_NGX_ECHO_MODULE
fi

if [[ -d "$DIR_NGX_HEADERS_MORE_MODULE" ]]; then
    notice "updating the headers-more-nginx-module repository..."
    pushd $DIR_NGX_HEADERS_MORE_MODULE
        git fetch
        git reset --hard origin/master
    popd
else
    notice "cloning the headers-more-nginx-module repository..."
    git clone https://github.com/openresty/headers-more-nginx-module.git $DIR_NGX_HEADERS_MORE_MODULE
fi

if [[ -d "$DIR_MOCKEAGAIN" ]]; then
    notice "updating the mockeagain repository..."
    pushd $DIR_MOCKEAGAIN
        git fetch
        git reset --hard origin/master
    popd
else
    notice "cloning the mockeagain repository..."
    git clone https://github.com/openresty/mockeagain.git $DIR_MOCKEAGAIN
fi

pushd $DIR_MOCKEAGAIN
    make
popd

get_no_pool_nginx 1

if [[ -n "$NGX_WASM_RUNTIME" ]] && ! [[ -n "$NGX_WASM_RUNTIME_LIB" ]]; then
    notice "fetching the \"$NGX_WASM_RUNTIME\" runtime..."
    $NGX_WASM_DIR/util/runtime.sh -R "$NGX_WASM_RUNTIME"
fi

notice "done"

# vim: ft=sh st=4 sts=4 sw=4:
