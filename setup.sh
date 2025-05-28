# Setup
export INSTALL_PATH=/tmp/initramfs_new
export INITRAMFS_ORIG=~/course/kernel_programming/initramfs.cpio.gz
export FS_NAME=lab2  # Change this

# Extract and prepare
# rm -rf $INSTALL_PATH
mkdir -p $INSTALL_PATH
cd $INSTALL_PATH && gunzip -c $INITRAMFS_ORIG | cpio -id

# Copy files
mkdir -p lib/modules bin
cp /path/to/your/*.ko lib/modules/          # For kernel modules
cp /path/to/your/executables bin/           # For programs
chmod +x bin/*

# Package
find . -print0 | cpio --null -ov --format=newc | gzip -9 > ~/course/kernel_programming/initramfs_$FS_NAME.cpio.gz

# Test in QEMU
qemu-system-x86_64 \
  -kernel ~/course/kernel_programming/linux/arch/x86/boot/bzImage \
  -initrd ~/course/kernel_programming/initramfs_$FS_NAME.cpio.gz \
  -append "console=ttyS0 nokaslr" \
  -nographic -m 512M