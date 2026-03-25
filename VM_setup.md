# VM Setting for Kernel Development

We use QEMU for virtual machine for modifying and tesing kernel codes.

```bash
sudo apt-get install -y qemu-kvm libvirt-daemon-system virtinst virt-manager bridge-utils
mkdir -p ~/qemu-vm
cd ~/qemu-vm
qemu-img create -f qcow2 ubuntu-kernel-dev2.qcow2 50G
wget https://releases.ubuntu.com/22.04/ubuntu-22.04.5-live-server-amd64.iso
```

Load VM from image file using the below.

```bash
qemu-system-x86_64 \
    -enable-kvm \
    -m 8G \
    -smp 4 \
    -hda ~/qemu-vm/ubuntu-kernel-dev2.qcow2 \
    -cdrom ~/qemu-vm/ubuntu-22.04.5-live-server-amd64.iso \
    -boot d \
    -nographic \
    -serial mon:stdio
```

When boot complete from ISO file, press `e` in GRUB and add `console=ttyS0` next to `linux   /casper/vmlinuz --- ` for console output.

Connect VM from disk (without `cdrom`).
```bash
qemu-system-x86_64 \
    -enable-kvm \
    -m 8G \
    -smp 4 \
    -hda ~/qemu-vm/ubuntu-kernel-dev2.qcow2 \
    -nographic \
    -serial mon:stdio \
    -machine q35 \
    -net nic \
    -net user,hostfwd=tcp::2222-:22
```
We can connect via ssh using `ssh -p 2222 dev@localhost`

To set the serial console setup persistently, follow the below.
```bash
vi /etc/default/grub
```
And modify to `GRUB_CMDLINE_LINUX="console=ttyS0"` and then `sudo update-grub`.

# Kernel Source Download
```bash
sudo apt-get install -y \
    build-essential libncurses-dev bison flex \
    libssl-dev libelf-dev bc dwarves \
    git wget curl vim

wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.8.tar.xz 
tar -xf linux-6.8.tar.xz

cd ~/linux-6.8
cp /boot/config-$(uname -r) .config
make olddefconfig

scripts/config --disable SYSTEM_TRUSTED_KEYS
scripts/config --disable SYSTEM_REVOCATION_KEYS
scripts/config --set-str CONFIG_SYSTEM_TRUSTED_KEYS ""
scripts/config --set-str CONFIG_SYSTEM_REVOCATION_KEYS ""
```