#!/bin/bash

echo "Starting Debian 13 Generic ARM64 VM..."

cd ~/qemu-project/images
taskset -c 1 qemu-system-aarch64 \
  -M virt \
  -cpu cortex-a72 \
  -smp 1 \
  -m 2G \
  -bios /usr/share/qemu-efi-aarch64/QEMU_EFI.fd \
  -drive file=debian-13-generic-arm64-20260525-2489.qcow2,if=virtio,format=qcow2 \
  -netdev user,id=net0,hostfwd=tcp::2223-:22 \
  -device virtio-net-device,netdev=net0 \
  -nographic 
