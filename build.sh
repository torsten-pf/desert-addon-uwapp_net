#!/bin/bash

set -eo pipefail

ADDON=uwapp_net
ADDON_DIR=$(dirname `readlink -f $0`)

help_func()
{
  printf "%s - build DESERT ADDON $ADDON for DESERT\n" $(basename $0)
  printf "Usage: %s: -r root_dir -d dest_dir [-h]\n" $(basename $0)
  printf "  -r   DESERT root directory (where DESERT_Framework resides)\n"
  printf "  -b   DESERT build directory (where .buildHost resides)\n"
  printf "  -h   output this help text\n"
  printf "\n"
}

handle_args() {
	#-------------------------------------------------------------------
	#  Check for and handle command-line arguments
	#-------------------------------------------------------------------
	# since we don't want getopts to generate error
	# messages, but want this script to issue its
	# own messages, we will put, in the option list, a
	# leading ':' v---here to silence getopts.
	while getopts :r:b:h FOUND
	do
	  case $FOUND in
	  r)  ROOT_DESERT=$OPTARG;;
	  b)  DEST_FOLDER_LOCAL=$OPTARG;;
	  \:) echo "Argument missing from -$OPTARG option\n"
		  help_func
		  exit 2
		  ;;
	  \?) echo "Unknown option: -$OPTARG\n"
		  help_func
		  exit 2
		  ;;
	  h)  help_func
		  exit 2
		  ;;
	  esac >&2
	done
}

#***
# << DESERT_addon package >>
# -------
# This function allows the compilation/cross-compilation of the zlib package.
# Through the:
#    ${ARCH}
#    ${HOST}
# variables "build_DESERT_addon()" decides if do a compilation or a cross-compilation:
#    ${ARCH} = ${HOST}  -> compile
#    ${ARCH} != ${HOST} -> cross-compile

#TODO:
# (v) add the "return check" after each compile command. Moreover add "tail -n 50" command when a error compile happen.
#*
build_DESERT_addon() {
    info_L2 "INSTALLATION of ${1} as ADD-ON"
    src_addon_path="${DEST_FOLDER}/${DESERT_DIR}-${DESERT_VERSION}-ADDONS-src/${1}"
    start="$(date +%s)"
    if [ -d ${src_addon_path} ]; then
        (
            cd ${src_addon_path}
            ./autogen.sh > "${currentBuildLog}/DESERT_ADDON/${1}-${2}.log"  2>&1
        )
        if [ -e Makefile ] ; then
            info_L2 "make-clean [${2}]"
            make distclean >> "${currentBuildLog}/DESERT_ADDON/${1}-${2}.log"  2>&1
            if [ $? -ne 0 ] ; then
                err_L1 "Error during the make distclean of ${1}! Exiting ..."
                tail -n 50 ${currentBuildLog}/DESERT_ADDON/${1}-${2}.log
                exit 1
            fi
        fi

        (
            cd ${src_addon_path}
            ./autogen.sh >> "${currentBuildLog}/DESERT_ADDON/${1}-${2}.log"  2>&1
        )

        info_L2 "configure  [${2}]"
        case ${WITHWOSS} in
            0)
                CXXFLAGS="-Wno-write-strings"                                                                         \
                CFLAGS="-Wno-write-strings"                                                                                      \
                ${src_addon_path}/configure --target=$ARCH                                                                       \
                                            --host=$ARCH                                                                         \
                                            --build=$HOST                                                                        \
                                            --with-ns-allinone=${currentBuildLog}                                                \
                                            --with-nsmiracle=${currentBuildLog}/nsmiracle-${NSMIRACLE_VERSION}                   \
                                            --with-desert=${DEST_FOLDER}/${DESERT_DIR}-${DESERT_VERSION}-src                     \
                                            --with-desert-build=${DEST_FOLDER}/${DESERT_DIR}-${DESERT_VERSION}-build             \
                                            --with-desertAddon=${DEST_FOLDER}/${DESERT_DIR}-${DESERT_VERSION}-ADDONS-src         \
                                            --with-desertAddon-build=${DEST_FOLDER}/${DESERT_DIR}-${DESERT_VERSION}-ADDONS-build \
                                            --prefix=${DEST_FOLDER}                                                              \
                                            >> "${currentBuildLog}/DESERT_ADDON/${1}-${2}.log"  2>&1
                ;;
            1)
                CXXFLAGS="-Wno-write-strings"                                                                         \
                CFLAGS="-Wno-write-strings"                                                                                      \
                CPPFLAGS=-I${currentBuildLog}/netcdf-cxx-${NETCDFCXX_VERSION}/cxx                                                \
                LDFLAGS=-L${DEST_FOLDER}/lib                                                                                     \
                ${src_addon_path}/configure --target=$ARCH                                                                       \
                                            --host=$ARCH                                                                         \
                                            --build=$HOST                                                                        \
                                            --with-ns-allinone=${currentBuildLog}                                                \
                                            --with-nsmiracle=${currentBuildLog}/nsmiracle-${NSMIRACLE_VERSION}                   \
                                            --with-desert=${DEST_FOLDER}/${DESERT_DIR}-${DESERT_VERSION}-src                     \
                                            --with-desert-build=${DEST_FOLDER}/${DESERT_DIR}-${DESERT_VERSION}-build             \
                                            --with-desertAddon=${DEST_FOLDER}/${DESERT_DIR}-${DESERT_VERSION}-ADDONS-src         \
                                            --with-desertAddon-build=${DEST_FOLDER}/${DESERT_DIR}-${DESERT_VERSION}-ADDONS-build \
                                            --with-woss=${currentBuildLog}/woss-${WOSS_VERSION}                                  \
                                            --prefix=${DEST_FOLDER}                                                              \
                                            >> "${currentBuildLog}/DESERT_ADDON/${1}-${2}.log"  2>&1
                ;;
            *)
                exit 1
                ;;
        esac
        if [ $? -ne 0 ]; then
            err_L1 "Error during the configuration of ${1}! Exiting ..."
            tail -n 50 ${currentBuildLog}/DESERT_ADDON/${1}-${2}.log
            exit 1
        fi

        #info_L2 "patch      [${2}]"
        #(
        #    cat "${UNPACKED_FOLDER}/${PATCHES_DIR}/001-desert-2.0.0-addon-libtool-no-verbose.patch" | patch -p1 >> "${currentBuildLog}/DESERT_ADDON/${1}-${2}.log"  2>&1
        #)

        info_L2 "make       [${2}]"
        make >> "${currentBuildLog}/DESERT_ADDON/${1}-${2}.log"  2>&1
        if [ $? -ne 0 ]; then
            err_L1 "Error during the compilation of ${1}! Exiting ..."
            tail -n 50 ${currentBuildLog}/DESERT_ADDON/${1}-${2}.log
            exit 1
        fi

        info_L2 "make inst  [${2}]"
        make install >> "${currentBuildLog}/DESERT_ADDON/${1}-${2}.log"  2>&1
        if [ $? -ne 0 ]; then
            err_L1 "Error during the installation of ${1}! Exiting ..."
            tail -n 50 ${currentBuildLog}/DESERT_ADDON/${1}-${2}.log
            exit 1
        fi
    else
        err_L1 "No add-on '${1}' found. Is the name correct?"
        err_L1 "You have these add-ons available:"
        err_L1 "$(ls ../ | sed -e 's/\(ADDON_\)/                             - \1/' | grep ADDON_) "
        exit 1
    fi
    elapsed=`expr $(date +%s) - $start`
    ok_L1 "completed in ${elapsed}s"

    return 0
}

# ROOT_DESERT and DEST_FOLDER must be set via command line
handle_args $*

if [ -z "$ROOT_DESERT" ]; then
	echo "missing output directory argument (where software should be built)"
    help_func
    exit 2
fi
if [ ! -d "$ROOT_DESERT/DESERT_Framework" ]; then
	echo "wrong root directory given, missing DESERT_Framework in $ROOT_DESERT!"
    exit 2
fi

# set common variables 
pushd "$ROOT_DESERT/DESERT_Framework"
. ./commonVariables.sh
. ./commonFunctions.sh
popd
# overwrite default DEST_FOLDER
DEST_FOLDER=$DEST_FOLDER_LOCAL

if [ -z "$DEST_FOLDER" ]; then
	echo "missing output directory argument (where software should be built)"
    help_func
    exit 2
fi
if [ ! -d "$DEST_FOLDER/.buildHost" ]; then
	echo "wrong root directory given, missing .buildHost in $DEST_FOLDER!"
    exit 2
fi

WITHWOSS=0

BUILD_HOST="${DEST_FOLDER}/.buildHost"
BUILD_TARGET="${DEST_FOLDER}/.buildTarget"

# copy or link addon source to ${DEST_FOLDER}/${DESERT_DIR}-${DESERT_VERSION}-ADDONS-src/$ADDON
ADDON_DEST_DIR=${DEST_FOLDER}/${DESERT_DIR}-${DESERT_VERSION}-ADDONS-src/${ADDON}
[ -d ${ADDON_DEST_DIR} ] || (echo "Linking ${ADDON_DIR} to ${ADDON_DEST_DIR}..." &&  ln -s ${ADDON_DIR} ${ADDON_DEST_DIR})
handle_desert_ADDON host $ADDON
