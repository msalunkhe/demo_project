set -e
set -u


SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/sinter_functions.sh"


OPTUSAGE="
  -b STRING    Build number
  -c CONFIG    Config for building management
  -p DIR       fio_prefix install directory
  -s DIR       Management source directory
  -r REV       Management source revision to build
"

BUILD_NUMBER=
SOURCE_DIR=
SOURCE_REV=
FIO_PREFIX=
CONFIG=

opt_parse()
{
    case "$1" in
        (b)
        BUILD_NUMBER="$OPTARG"
        ;;

        (c)
        CONFIG="$OPTARG"
        ;;

        (p)
        FIO_PREFIX="$OPTARG"
        ;;

        (s)
        SOURCE_DIR="$OPTARG"
        ;;

        (r)
        SOURCE_REV="$OPTARG"
        ;;

        (*)
        usage >&2
        exit 1
        ;;
    esac
}
OPTPARSE=opt_parse
OPTSTRING="b:c:k:p:r:s:"


check_args()
{
    local failure=

    if [ -z "$FIO_PREFIX" ]; then
        printf "ERROR: fio_prefix directory option '-p' is required\n" >&2
        failure=1
    fi

    if [ -z "$SOURCE_REV" ]; then
        printf "ERROR: revision option '-r' is required\n" >&2
        failure=1
    fi

    if [ -z "$SOURCE_DIR" ]; then
        printf "ERROR: source location '-s' is required\n" >&2
        failure=1
    fi

    if [ -n "$failure" ]; then
        exit 1
    fi
}


main()
{
    local rc=0

    if [ "$#" -gt 0 ]; then
        parse_options "$@"
    fi
    check_args

    local build_dir="$WORK_DIR/build/$LABEL/management"
    local build_number_option=${BUILD_NUMBER:+--build-number=${BUILD_NUMBER}}
    local variant="$NAME"

    if [ -e "$build_dir" ]; then
        (
            set -e

            cd "$build_dir"
            hg pull
            hg update -C -r "$SOURCE_REV"
            rm -rf "$variant"
        )
    else
        mkdir -p "$(dirname "$build_dir")"
        hg clone "$SOURCE_DIR" "$build_dir" -r "$SOURCE_REV"
    fi

    if [ -n "$CONFIG" ]; then
        cp "$CONFIG" "$build_dir/fio-$variant.config"
    fi

    # This is a bit interesting since we can't build the whole tree yet
    # (waiting on the PDP) and we're selectively delivering pieces of
    # management.
    env \
        PATH="$FIO_PREFIX/bin:$PATH" \
        C_INCLUDE_PATH="$FIO_PREFIX/include" \
        CPLUS_INCLUDE_PATH="$FIO_PREFIX/include" \
        LIBRARY_PATH="$FIO_PREFIX/lib" \
        scons \
        -k \
        -C "$build_dir" \
        -u \
        --variant="$variant" \
        ${VERBOSE:+V=1} \
        $build_number_option

    # management build uses an old version of fio scons and does not create SETTINGS
    #local management_version="$(get_management_version "$build_dir/$variant/SETTINGS")"
    local codename="$(sed -e 's/\..*//;' "$build_dir/software_version" | tr [A-Z] [a-z])"
    local version="$(sed -e 's/^[^.]*\.//;' "$build_dir/software_version")${BUILD_NUMBER:+.$BUILD_NUMBER}"
    local source_hg_id="$(hg id -i "$build_dir")"
    local productname="fio-management-$version"
    local destdir="$WORK_DIR/build/$LABEL/$productname"
    local productdir="$WORK_DIR/products/$LABEL"

    # Remove any old destdir so that cruft doesn't acumulated in the tar.

