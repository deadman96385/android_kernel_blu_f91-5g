# export PATH=$PWD/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:$PWD/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9.1/bin:$PWD/prebuilts/gcc/linux-x86/aarch64/gcc-arm-8.3-2019.03-x86_64-aarch64-elf/bin:$PWD/prebuilts/gcc/linux-x86/host/x86_64-w64-mingw32-4.8/bin:$PWD/prebuilts/gcc/linux-x86/host/x86_64-linux-glibc2.17-4.8/bin:$PATH

PATH=$PWD/prebuilts/clang/host/linux-x86/clang-r383902/bin:$PWD/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:/usr/bin/:/bin:$PATH

        cd kernel-4.14
		make ARCH=arm64 CROSS_COMPILE=aarch64-linux-androidkernel- CLANG_TRIPLE=aarch64-linux-gnu- LD=ld.lld LD_LIBRARY_PATH=$PWD/prebuilts/clang/host/linux-x86/clang-r383902/lib64:\$$LD_LIBRARY_PATH NM=llvm-nm OBJCOPY=llvm-objcopy CC=clang O=out clean
        make ARCH=arm64 CROSS_COMPILE=aarch64-linux-androidkernel- CLANG_TRIPLE=aarch64-linux-gnu- LD=ld.lld LD_LIBRARY_PATH=$PWD/prebuilts/clang/host/linux-x86/clang-r383902/lib64:\$$LD_LIBRARY_PATH NM=llvm-nm OBJCOPY=llvm-objcopy CC=clang O=out k6833v1_64_defconfig
        make ARCH=arm64 CROSS_COMPILE=aarch64-linux-androidkernel- CLANG_TRIPLE=aarch64-linux-gnu- LD=ld.lld LD_LIBRARY_PATH=$PWD/prebuilts/clang/host/linux-x86/clang-r383902/lib64:\$$LD_LIBRARY_PATH NM=llvm-nm OBJCOPY=llvm-objcopy CC=clang O=out -j32 2>&1 | tee build_kernel.log

