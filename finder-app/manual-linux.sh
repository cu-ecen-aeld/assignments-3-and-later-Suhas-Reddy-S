#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.
# Updated by Suhas-Reddy-S

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR} 

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    make clean
    echo "emove previous kernel build artifacts and configuration using 'mrproper'"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    echo "Configure the kernel using the default configuration for the specified ARM4 architecture"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    ech "Build the kernel and modules in parallel using multiple jobs"
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    echo "Build device tree blobs (DTBs) for the specified architecture"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs 
fi

cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
# Create the root filesystem directory if it doesn't exist
mkdir -p ${OUTDIR}/rootfs
# Move into the root filesystem directory
cd ${OUTDIR}/rootfs
# Create essential directories in the root filesystem
mkdir -p lib etc dev proc sys bin sbin tmp usr var lib64 home
# Create additional directories in the usr and var paths
mkdir -p usr/bin usr/lib usr/sbin var/log


cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    # Clean the BusyBox build directory, removing any previous build artifacts
    make distclean
    # Configure BusyBox using the default configuration
    make defconfig

else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install

cd ${OUTDIR}/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
# Copying ld-linux-aarch64.so.1 to the specified directory
# The find command searches for ld-linux-aarch64.so.1 within the specified $SYSROOT directory
# The result is passed to ${} to extract the value, and then the cp command is used to copy it to "${OUTDIR}/rootfs/lib"
cp ${$(find $SYSROOT -name ld-linux-aarch64.so.1)} "${OUTDIR}/rootfs/lib"
# Copying libm.so.6, libresolv.so.2, and libc.so.6 to the specified directory
# The find command is used to locate libm.so.6, libresolv.so.2, and libc.so.6 within the $SYSROOT directory
# The results are passed to the cp command to copy these libraries to "${OUTDIR}/rootfs/lib64"
cp $(find $SYSROOT -name libm.so.6) $(find $SYSROOT -name libresolv.so.2) $(find $SYSROOT -name libc.so.6) "${OUTDIR}/rootfs/lib64"

# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 600 ${OUTDIR}/rootfs/dev/tty c 5 1

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make  CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp -rf ${FINDER_APP_DIR}/finder.sh ${FINDER_APP_DIR}/conf ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home/

# Modify the finder-test.sh script to reference conf/assignment.txt instead of ../conf/assignment.txt
# cp -f ${FINDER_APP_DIR}/conf/assignment.txt ${FINDER_APP_DIR}/conf/username.txt ${OUTDIR}/rootfs/home/conf/

# Copy autorun-qemu.sh script into the outdir/rootfs/home directory
cp ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home/

# TODO: Chown the root directory
cd ${OUTDIR}/rootfs
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio

echo "Build completed successfully!"
