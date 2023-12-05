
WARN=-Wall -Wextra -Wno-unused-function -Wno-unused-parameter

# freestanding using any recent clang build
#CC=clang
#CFLAGS=--target=riscv64 -march=rv64gcv_zfh -O3 ${WARN} -nostdlib -fno-builtin -ffreestanding

# full cross compilation toolchain
CC=riscv64-linux-gnu-gcc
CFLAGS=-march=rv64gcv_zfh -O3 -static ${WARN} -nostdlib -fno-builtin -ffreestanding

# native build
#CC=cc
#CFLAGS=-march=rv64gcv -O3 ${WARN}

