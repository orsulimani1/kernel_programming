# Complete Guide: Building Linux Kernel and BusyBox for QEMU

This guide walks through the complete process of building a custom Linux kernel with BusyBox, creating an initramfs, and booting it using QEMU. This is useful for kernel development, testing, and learning about Linux internals.

## Prerequisites

- Ubuntu 22.04 (or similar)
- Git
- Basic development tools (gcc, make, etc.)
- Approximately 10GB of free disk space

sudo apt-get install build-essential flex bison libssl-dev libelf-dev libncurses5-dev libncursesw5-dev

## 1. Clone the Repository with Submodules

Clone the project repository including all submodules:

```bash
# Clone the repository with all submodules in one command
git clone --recursive git@github.com:orsulimani1/kernel_programming.git

# Or if you've already cloned without --recursive
git clone git@github.com:orsulimani1/kernel_programming.git
cd kernel_programming/
git submodule init
git submodule update
```

## 2. Install Cross Compiler

Install the cross compiler (if not already installed):

```bash
# Install the cross compiler on Ubuntu/Debian
sudo apt-get install gcc-x86-64-linux-gnu binutils-x86-64-linux-gnu

# Set the cross-compiler environment variable
export CROSS_COMPILE=x86_64-linux-gnu-
```

## 3. Build the Linux Kernel

Navigate to the Linux source directory and build the kernel:

```bash
# Go to the repository's baseline directory
cd kernel_programming/linux/

# Apply the patch to fix realloc issues if needed
git apply ../realloc.patch

# Set the cross-compiler (if you're compiling on x86_64 for x86_64)
export CROSS_COMPILE=x86_64-linux-gnu-

# Configure the kernel for x86_64
make x86_64_defconfig

# Build the kernel (adjust the number based on your CPU cores)
make -j$(nproc)

# Verify the kernel image is created
ls arch/x86/boot/bzImage
```

## 4. Download and Build BusyBox

BusyBox provides the essential user space utilities:

```bash
# Go to the repository's baseline directory
cd kernel_programming

# Download BusyBox source
wget https://busybox.net/downloads/busybox-1.35.0.tar.bz2
tar xjf busybox-1.35.0.tar.bz2
cd busybox-1.35.0

# Install required ncurses libraries if missing
# sudo apt-get install libncurses5-dev libncursesw5-dev

# Fix the dialog Makefile issue by commenting out a line
sed -i 's/\(always         := $(hostprogs-y) dochecklxdialog\)/#\1/' scripts/kconfig/lxdialog/Makefile

# Create a default configuration
make defconfig

# Configure BusyBox to build statically
make menuconfig
# In the menuconfig interface:
# Navigate to "Settings" → Find "Build static binary (no shared libs)"
# Press 'Y' to enable it
# Exit and save the configuration

# Build BusyBox
make -j$(nproc)

# Install BusyBox
make install
```

## 5. Create an Initramfs

Now create a minimal filesystem for the kernel to boot:

```bash
# Create a dedicated directory
mkdir -p /tmp/initramfs
cd /tmp/initramfs

# Create the basic directory structure
mkdir -p {bin,sbin,etc,proc,sys,dev,usr/{bin,sbin},root}

# Copy the BusyBox binary
cp kernel_programming/busybox-1.35.0/busybox bin/

# Create essential symlinks
cd bin
ln -s busybox sh
cd ..

# Create a proper init script with explicit BusyBox paths
cp ${REPO_BASELINE}/_init ./init

# Make init executable
chmod +x init

# Create basic configuration files
cat > etc/passwd << 'EOF'
root::0:0:root:/root:/bin/sh
EOF

cat > etc/group << 'EOF'
root:x:0:
EOF
```

## 5. Package the Initramfs

Create the compressed initramfs image:

```bash
# Package the initramfs with a clear filename to avoid confusion
find . -print0 | cpio --null -ov --format=newc | gzip -9 > ${REPO_BASELINE}/kernel_programming/initramfs.cpio.gz

# Return to the project directory
cd ${REPO_BASELINE}/kernel_programming
```

## 6. Boot with QEMU

Finally, boot your custom Linux kernel with the initramfs using QEMU:

```bash
# Start QEMU with your kernel and initramfs
# Make sure to run this command from the kernel_programming directory
cd ${REPO_BASELINE}kernel_programming
qemu-system-x86_64 \
  -kernel linux/arch/x86/boot/bzImage \
  -initrd latest_initramfs.cpio.gz \
  -append "console=ttyS0 nokaslr" \
  -nographic \
  -m 512M
```

## 7. Working with Your Custom Linux System

### Basic Usage
After booting, you'll see a shell prompt (`/ #`). You can now use the various BusyBox commands.

### Exiting QEMU
To exit the QEMU session:
- Press `Ctrl+A` followed by `x`


qemu-system-x86_64 \
  -kernel linux/arch/x86/boot/bzImage \
  -initrd initramfs.cpio.gz \
  -append "console=ttyS0 nokaslr" \
  -nographic \
  -m 512M