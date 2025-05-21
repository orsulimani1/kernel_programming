Adding Lab Examples to QEMU Image
Method 1: Add to Initramfs

Copy lab files to initramfs directory:
bash# Assuming you're in your kernel_programming directory
mkdir -p /tmp/initramfs_new/lab1
cp labs/lab1/* /tmp/initramfs_new/lab1/

# Copy the current initramfs content
cd /tmp/initramfs_new
gunzip -c /path/to/kernel_programming/initramfs.cpio.gz | cpio -id

Add build tools to initramfs:
bash# Add kernel headers and build tools if needed
mkdir -p /tmp/initramfs_new/lib/modules/$(make -C /path/to/linux kernelrelease)
cp -r /path/to/linux/include /tmp/initramfs_new/
cp -r /path/to/linux/Module.symvers /tmp/initramfs_new/

Repack the initramfs:
bashcd /tmp/initramfs_new
find . -print0 | cpio --null -ov --format=newc | gzip -9 > \
  /path/to/kernel_programming/initramfs_lab1.cpio.gz

Boot QEMU with the new initramfs:
bashqemu-system-x86_64 \
  -kernel /path/to/linux/arch/x86/boot/bzImage \
  -initrd /path/to/kernel_programming/initramfs_lab1.cpio.gz \
  -append "console=ttyS0 nokaslr" \
  -nographic \
  -m 512M