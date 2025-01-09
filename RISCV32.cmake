set(CMAKE_SYSTEM_NAME Generic-ELF)

find_program(
    CLANG
    NAMES clang
    NO_DEFAULT_PATH
    PATHS /opt/homebrew/opt/llvm/bin
    REQUIRED
)

find_program(
    OBJCOPY
    NAMES llvm-objcopy
    NO_DEFAULT_PATH
    PATHS /opt/homebrew/opt/llvm/bin
    REQUIRED
)

set(CMAKE_C_COMPILER ${CLANG})

set(CMAKE_C_FLAGS_INIT "--target=riscv32 -ffreestanding -nostdlib")
set(CMAKE_EXE_LINKER_FLAGS "--target=riscv32 -ffreestanding -nostdlib")