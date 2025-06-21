#!/usr/bin/env bash
set -e
make -j$(nproc)

qemu-system-x86_64 \
  -machine q35,accel=tcg -nographic -bios none \
  -kernel stage0.bin \
  -nic user,model=rtl8139,ipv6=off,tftp=./tftp_root,bootfile=boot.img
