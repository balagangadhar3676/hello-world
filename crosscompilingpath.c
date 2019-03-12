#set up paths and environment for cross compiling for openwrt
export STAGING_DIR=/openwrt-18.06.1/staging_dir
export BUILD_DIR=/openwrt-8devices/build_dir
export TOOLCHAIN_DIR=$STAGING_DIR/toolchain-mips_24kc_gcc-7.3.0_musl
export LDCFLAGS=$TOOLCHAIN_DIR/bin
export LD_LIBRARY_PATH=$TOOLCHAIN_DIR/usr/lib
export LD_LIBRARY_PATH=/openwrt-8devices/staging_dir/target-mips_24kc_musl/usr/lib:$LD_LIBRARY_PATH
##export PATH=$TOOLCHAIN_DIR/bin:$PATH
export PATH=$TOOLCHAIN_DIR/bin:$BUILD_DIR/target-mips_24kc_musl:$PATH
