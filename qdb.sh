#!/usr/bin/env bash
make qemu-nox-gdb < /dev/null > /dev/null & gdb ; pkill qemu
