#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/home/hosa200/linux_img/
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

echo $SYSROOT
if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

sudo rm  -rf ${OUTDIR}
mkdir -p ${OUTDIR}
#
cd ${OUTDIR}
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
    git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

     #TODO: Add your kernel build steps here
     # deep clean
     if [ -f .config ]; then
         make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mproper
     fi
     # build config files
     make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
     # build image 
     make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
     # build modules
     #make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
     # build device tree
     make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in ${OUTDIR}"
pwd
sudo mkdir -p -m 777 ../Image/ 
FLD=$(pwd)
sudo cp -a -L -r ${FLD}/arch/arm64/boot/Image ../Image
ls -l
cd /Image
ls -l


echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf rootfs
    echo "Creating rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo mkdir -m 777 rootfs
else
    echo "Creating rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo mkdir -m 777 rootfs
fi

# TODO: Create necessary base directories
cd rootfs/
sudo mkdir -m 777 -p bin etc dev home lib lib64 proc sbin sys tmp usr var
sudo mkdir -m 777 -p usr/bin usr/lib usr/sbin
sudo mkdir -m 777 -p var/log
#tree -d

cd "$OUTDIR"
if [ ! -d "busybox" ]
then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox

else
    cd busybox/
fi

# TODO: Make and install busybox
    # deep clean
    make distclean
    # build config files
    make defconfig
    # build image 
    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
    # install image
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

    #move them to rootfs
    sudo cp -a _install/* ../rootfs/

echo "Library dependencies" 
${CROSS_COMPILE}readelf -a busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
echo "Copying Library dependencies for ${SYSROOT} to ${OUTDIR}/rootfs"
pwd
cd ../rootfs
#if [ -f $SYSROOT/lib/ld-linux-aarch64.so.1 ]; then
    echo "found ld-linux-aarch64.so.1"
    sudo cp -a $SYSROOT/lib/ld-linux-aarch64.so.1 lib
#fi
#if [ -f $SYSROOT/lib64/libm.so.6 ]; then
    echo "found libm.so.6 "
    sudo cp -a $SYSROOT/lib64/libm.so.6 lib64
#fi
#if [ -f $SYSROOT/lib64/libresolv.so.2 ]; then
    echo "found libresolv.so.2"
    sudo cp -a $SYSROOT/lib64/libresolv.so.2 lib64
#fi
#if [ -f $SYSROOT/lib64/libc.so.6 ]; then
    echo "found libc.so.6"
    sudo cp -a $SYSROOT/lib64/libc.so.6 lib64
#fi

# TODO: Make device nodes
echo "Make device nodes"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

# TODO: Clean and build the writer utility
echo "Clean and build the writer utility"
cd ${FINDER_APP_DIR}
make CROSS_COMPILE=${CROSS_COMPILE} clean
make CROSS_COMPILE=${CROSS_COMPILE} all

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "Copy the finder related scripts and executables to the /home directory"
echo "on the target rootfs"
cd ${OUTDIR}/rootfs
cp -a -L -r ${FINDER_APP_DIR}/* home
ls -l

# TODO: Chown the root directory
cd ..
sudo chmod 755 rootfs
sudo chmod 777 rootfs/home/conf

# TODO: Create initramfs.cpio.gz
echo "Create initramfs.cpio.gz"
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ..
gzip initramfs.cpio

echo "Done"