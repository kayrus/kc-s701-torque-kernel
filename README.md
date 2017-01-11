# Build KC-S701 kernel

```sh
git clone https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6
export PATH=${PWD}/arm-eabi-4.6/bin:$PATH
export CROSS_COMPILE=arm-eabi-
cd kernel
./scripts/build-all.py -j8 -m bzImage noperf
ls -la ../all-kernels/msm8226/arch/arm/boot/zImage
```

# Build other kernels

## arm64

```sh
git clone https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-android-
export CROSS_COMPILE64=aarch64-linux-android-
export PATH=${PWD}/aarch64-linux-android-4.9/bin:$PATH
cd kernel
chmod +x scripts/link-vmlinux.sh
sed -i 's#struct dentry, d_alias#struct dentry, d_u.d_alias#' ./drivers/platform/msm/pft.c
make mrproper
./scripts/build-all.py -j8 msm-64
```

## Issues

### Perl

```sh
sed -i 's/!defined(@val)/!@val/' kernel/timeconst.pl
```

### Non clean build dir

`kernel is not clean, please run 'make mrproper'`

```sh
rm -rf ../all-kernels
rm -rf .config
rm -rf include/config
```

### asm/asm-generic

```sh
cd include
ln -s asm-generic asm
```

### `undefined reference to`

Most probably you've selected invalid defconfig. Make sure you use the correct one.

### `warning: argument to 'sizeof' in 'memcpy' call is the same expression`

Edit `Makefile` and add `-Wno-sizeof-pointer-memaccess` into the `HOSTCFLAGS` variable.

### Extract module symver

https://github.com/glandium/extract-symvers

```
unpackbootimg -i boot.img -o boot
python extract-symvers/extract-symvers.py -e le -B 0xc0008000 boot.img-zImage
```
