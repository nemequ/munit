#!/bin/bash

## This script is just to get the compiler we're interested in
## installed on Travis.  You should not be using this.

case "${TRAVIS_OS_NAME}" in
    "linux")
	. /etc/lsb-release

	sudo apt-get update -qq
	sudo apt-get install -qq python-software-properties
	sudo apt-add-repository -y ppa:ubuntu-toolchain-r/test

	sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys AF4F7421
	# for CLANG_VERSION in 3.4 3.5 3.6 3.7; do
        #     sudo apt-add-repository -y \
	# 	 "deb http://llvm.org/apt/${DISTRIB_CODENAME}/ llvm-toolchain-${DISTRIB_CODENAME}-${CLANG_VERSION} main"
	# done

	sudo apt-get update -qq

	case "${COMPILER}" in
            "gcc-"*)
		sudo apt-get install -qq "${COMPILER}"
		;;
            "clang-"*)
		sudo apt-get install -qq "${COMPILER}"
		;;
            "x86_64-w64-mingw32-gcc")
		sudo apt-get install -qq mingw-w64
		;;
            "i686-w64-mingw32-gcc")
		sudo apt-get install -qq mingw-w64
		;;
	    "tcc")
		sudo apt-get install -qq tcc
		;;
	esac
	;;

    "osx")
        brew update
        brew install \
             ragel \
             glib

        case "${COMPILER}" in
            "gcc-4.6")
                which gcc-4.6 || brew install homebrew/versions/gcc46
                ;;
            "gcc-4.8")
                which gcc-4.8 || brew install homebrew/versions/gcc48
                ;;
            "gcc-5")
                which gcc-5 || brew install homebrew/versions/gcc5
                ;;
        esac
        ;;
esac
