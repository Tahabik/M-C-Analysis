#!/bin/bash

taskset -c 0 qemu-system-x86_64 \
  --enable-kvm \
  -cpu host \
  -smp 1 \
  -m 2G \
  -drive file=debian-13-nocloud-amd64.qcow2,if=virtio,format=qcow2 \
  -device virtio-net-pci,netdev=net0 \
  -netdev user,id=net0,hostfwd=tcp::2222-:22 \
  -nographic
