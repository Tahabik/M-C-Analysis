#!/bin/bash

taskset -c 1 qemu-system-aarch64 \
  -M virt \
  -cpu max \
  -smp 1 \
  -m 2G \
  -bios /usr/share/edk2/aarch64/QEMU_EFI.fd \
  -drive if=none,file=debian-13-nocloud-arm64.qcow2,id=hd0 \
  -device virtio-blk-device,drive=hd0 \
  -netdev user,id=net0,hostfwd=tcp:127.0.0.1:2223-:22 \
  -device virtio-net-device,netdev=net0 \
  -nographic
