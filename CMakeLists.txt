cmake_minimum_required(VERSION 3.25)

project(MGCORE LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(MGCORE_BUILD_ISO "Build bootable ISO image" ON)
option(MGCORE_ENABLE_USERSPACE "Build userspace samples and initramfs payload" ON)
option(MGCORE_REQUIRE_NASM "Require NASM for boot/userspace assembly objects" ON)
set(MGCORE_ELF_TARGET "x86_64-elf" CACHE STRING "Target triple for freestanding kernel and userspace objects")
set(MGCORE_BOOTLOADER "AUTO" CACHE STRING "Bootloader provider: AUTO, LIMINE, or GRUB")
set_property(CACHE MGCORE_BOOTLOADER PROPERTY STRINGS AUTO LIMINE GRUB)
set(LIMINE_DIR "" CACHE PATH "Path to a Limine binary release directory")

set(MGCORE_KERNEL_NAME kernel.elf)
set(MGCORE_ISO_NAME mgcore.iso)
set(MGCORE_ROOTFS_DIR ${CMAKE_BINARY_DIR}/rootfs)
set(MGCORE_ISO_DIR ${CMAKE_BINARY_DIR}/iso)
set(MGCORE_INITRAMFS ${CMAKE_BINARY_DIR}/initramfs.cpio)

find_program(CLANG_EXECUTABLE NAMES clang REQUIRED)
find_program(LLD_LINK_EXECUTABLE NAMES ld.lld lld-link ld REQUIRED)
find_program(NASM_EXECUTABLE NAMES nasm)
find_program(GRUB_MKRESCUE_EXECUTABLE NAMES grub-mkrescue)
find_program(QEMU_EXECUTABLE NAMES qemu-system-x86_64)
find_program(XORRISO_EXECUTABLE NAMES xorriso)
find_package(Python3 COMPONENTS Interpreter)

if(NASM_EXECUTABLE)
elseif(MGCORE_REQUIRE_NASM)
  message(FATAL_ERROR "NASM not found. Install nasm or configure with -DMGCORE_REQUIRE_NASM=OFF for source-only setup.")
else()
  message(WARNING "NASM not found; skipping build target generation.")
  return()
endif()

set(MGCORE_COMMON_INCLUDE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/lib/libc/include
)

set(MGCORE_KERNEL_CFLAGS
  -ffreestanding
  -fno-pie
  -fno-stack-protector
  -fno-pic
  -m64
  -mno-red-zone
  -mcmodel=large
  -Wall
  -Wextra
  -Wpedantic
  -Wno-unused-parameter
)

set(MGCORE_USER_CFLAGS
  -ffreestanding
  -fno-pie
  -fno-stack-protector
  -fno-pic
  -m64
  -mno-red-zone
  -Wall
  -Wextra
  -Wpedantic
)

set(MGCORE_NASM_FLAGS)

set(CMAKE_C_LINK_EXECUTABLE
  "${LLD_LINK_EXECUTABLE} <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>"
)
set(CMAKE_C_STANDARD_LIBRARIES "")
set(CMAKE_C_LINKER_WRAPPER_FLAG "")
set(CMAKE_C_LINKER_WRAPPER_FLAG_SEP "")
set(CMAKE_C_LINKER_DEPFILE_SUPPORTED FALSE)
set(CMAKE_C_LINK_DEPENDS_USE_LINKER FALSE)

if(MGCORE_BOOTLOADER STREQUAL "AUTO")
  if(WIN32)
    set(MGCORE_BOOTLOADER_SELECTED "LIMINE")
  else()
    set(MGCORE_BOOTLOADER_SELECTED "GRUB")
  endif()
else()
  set(MGCORE_BOOTLOADER_SELECTED "${MGCORE_BOOTLOADER}")
endif()

set(MGCORE_LIMINE_SEARCH_DIRS)
if(LIMINE_DIR)
  list(APPEND MGCORE_LIMINE_SEARCH_DIRS "${LIMINE_DIR}")
endif()
if(DEFINED ENV{LIMINE_DIR} AND NOT "$ENV{LIMINE_DIR}" STREQUAL "")
  list(APPEND MGCORE_LIMINE_SEARCH_DIRS "$ENV{LIMINE_DIR}")
endif()
list(APPEND MGCORE_LIMINE_SEARCH_DIRS
  "${CMAKE_SOURCE_DIR}/tools/limine"
  "${CMAKE_SOURCE_DIR}/vendor/limine"
)

set(MGCORE_LIMINE_DIR "")
foreach(candidate IN LISTS MGCORE_LIMINE_SEARCH_DIRS)
  if(EXISTS "${candidate}/limine-bios.sys")
    set(MGCORE_LIMINE_DIR "${candidate}")
    break()
  endif()
  if(EXISTS "${candidate}/bin/limine-bios.sys")
    set(MGCORE_LIMINE_DIR "${candidate}/bin")
    break()
  endif()
endforeach()

if(MGCORE_LIMINE_DIR)
  find_program(LIMINE_TOOL_EXECUTABLE NAMES limine limine.exe PATHS "${MGCORE_LIMINE_DIR}" "${MGCORE_LIMINE_DIR}/.." NO_DEFAULT_PATH)
endif()

function(mgcore_add_nasm_object out_var source)
  string(REPLACE "/" "_" object_stem "${source}")
  string(REPLACE "\\" "_" object_stem "${object_stem}")
  set(object_file "${CMAKE_CURRENT_BINARY_DIR}/${object_stem}.o")
  add_custom_command(
    OUTPUT ${object_file}
    COMMAND ${NASM_EXECUTABLE}
      -f elf64
      -I${CMAKE_SOURCE_DIR}/include/
      -I${CMAKE_SOURCE_DIR}/lib/libc/include/
      ${MGCORE_NASM_FLAGS}
      -o ${object_file}
      ${CMAKE_SOURCE_DIR}/${source}
    DEPENDS ${CMAKE_SOURCE_DIR}/${source}
    VERBATIM
  )
  set(${out_var} ${object_file} PARENT_SCOPE)
endfunction()

set(KERNEL_SOURCES
  src/kernel/console.c
  src/kernel/interrupt.c
  src/kernel/main.c
  src/kernel/pmm.c
  src/kernel/vmm.c
  src/kernel/task.c
  src/kernel/scheduler.c
  src/kernel/swap.c
  src/kernel/safeboot.c
  src/kernel/syscall.c
  src/kernel/syscall_arch.c
  src/kernel/fs.c
  src/kernel/tmpfs.c
  src/kernel/elf.c
  src/kernel/net.c
  src/kernel/signal.c
  src/kernel/string.c
  src/kernel/util.c
  src/cli/shell.c
  src/cli/commands/builtin.c
  src/syscall/syscall_table.c
  src/syscall/syscall_impl.c
  drivers/keyboard.c
  drivers/mouse.c
)

mgcore_add_nasm_object(KERNEL_BOOT_OBJ boot/boot.asm)
mgcore_add_nasm_object(KERNEL_BOOT64_OBJ boot/boot64.asm)
mgcore_add_nasm_object(KERNEL_ISR_OBJ src/kernel/interrupt_stubs.asm)
mgcore_add_nasm_object(KERNEL_SYSCALL_ENTRY_OBJ src/kernel/syscall_entry.asm)

add_executable(kernel.elf
  ${KERNEL_SOURCES}
  ${KERNEL_BOOT_OBJ}
  ${KERNEL_BOOT64_OBJ}
  ${KERNEL_ISR_OBJ}
  ${KERNEL_SYSCALL_ENTRY_OBJ}
)
target_include_directories(kernel.elf PRIVATE ${MGCORE_COMMON_INCLUDE})
target_compile_options(kernel.elf PRIVATE
  $<$<COMPILE_LANGUAGE:C>:--target=${MGCORE_ELF_TARGET}>
  $<$<COMPILE_LANGUAGE:C>:${MGCORE_KERNEL_CFLAGS}>
)
set_target_properties(kernel.elf PROPERTIES SUFFIX "")
target_link_options(kernel.elf PRIVATE
  -m elf_x86_64
  -T ${CMAKE_SOURCE_DIR}/boot/linker.ld
  -z max-page-size=0x1000
  --build-id=none
)

function(mgcore_add_userspace name)
  mgcore_add_nasm_object(CRT0_OBJECT lib/crt/crt0.asm)
  add_executable(${name}
    ${CRT0_OBJECT}
    lib/libc/syscall.c
    lib/libc/unistd.c
    lib/libc/string.c
    ${ARGN}
  )
  target_include_directories(${name} PRIVATE ${MGCORE_COMMON_INCLUDE})
  target_compile_options(${name} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:--target=${MGCORE_ELF_TARGET}>
    $<$<COMPILE_LANGUAGE:C>:${MGCORE_USER_CFLAGS}>
  )
  set_target_properties(${name} PROPERTIES SUFFIX "" OUTPUT_NAME ${name})
  target_link_options(${name} PRIVATE
    -m elf_x86_64
    -T ${CMAKE_SOURCE_DIR}/userspace/user.ld
    --build-id=none
  )
  add_custom_command(TARGET ${name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MGCORE_ROOTFS_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${name}> ${MGCORE_ROOTFS_DIR}/bin/${name}
  )
endfunction()

if(MGCORE_ENABLE_USERSPACE)
  mgcore_add_userspace(init userspace/init.c)
  mgcore_add_userspace(shell userspace/shell.c)
  mgcore_add_userspace(hello_c userspace/test_programs/hello.c)

  add_custom_target(rootfs_files ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MGCORE_ROOTFS_DIR}/etc
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MGCORE_ROOTFS_DIR}/scripts
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/userspace/test_programs/hello.py ${MGCORE_ROOTFS_DIR}/scripts/hello.py
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/docs/syscall-notes.md ${MGCORE_ROOTFS_DIR}/etc/syscalls.txt
    DEPENDS init shell hello_c
  )

  if(Python3_FOUND)
    add_custom_command(
      OUTPUT ${MGCORE_INITRAMFS}
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/mkinitramfs.py ${MGCORE_ROOTFS_DIR} ${MGCORE_INITRAMFS}
      DEPENDS rootfs_files init shell hello_c ${CMAKE_SOURCE_DIR}/scripts/mkinitramfs.py
      COMMENT "Packing initramfs"
    )
    add_custom_target(initramfs ALL DEPENDS ${MGCORE_INITRAMFS})
  else()
    add_custom_target(initramfs
      COMMAND ${CMAKE_COMMAND} -E echo "Python3 not found; creating placeholder initramfs"
      COMMAND ${CMAKE_COMMAND} -E touch ${MGCORE_INITRAMFS}
    )
  endif()
else()
  add_custom_target(initramfs
    COMMAND ${CMAKE_COMMAND} -E echo "Userspace disabled; creating placeholder initramfs"
    COMMAND ${CMAKE_COMMAND} -E touch ${MGCORE_INITRAMFS}
  )
endif()

add_dependencies(kernel.elf initramfs)

if(MGCORE_BUILD_ISO AND GRUB_MKRESCUE_EXECUTABLE)
endif()

if(MGCORE_BUILD_ISO AND MGCORE_BOOTLOADER_SELECTED STREQUAL "LIMINE")
  if(MGCORE_LIMINE_DIR AND XORRISO_EXECUTABLE AND LIMINE_TOOL_EXECUTABLE)
    add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR}/${MGCORE_ISO_NAME}
      COMMAND ${CMAKE_COMMAND} -E rm -rf ${MGCORE_ISO_DIR}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${MGCORE_ISO_DIR}/boot
      COMMAND ${CMAKE_COMMAND} -E make_directory ${MGCORE_ISO_DIR}/EFI/BOOT
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:kernel.elf> ${MGCORE_ISO_DIR}/boot/kernel.elf
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MGCORE_INITRAMFS} ${MGCORE_ISO_DIR}/boot/initramfs.cpio
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/boot/limine.conf ${MGCORE_ISO_DIR}/limine.conf
      COMMAND ${CMAKE_COMMAND} -E copy ${MGCORE_LIMINE_DIR}/limine-bios.sys ${MGCORE_ISO_DIR}/limine-bios.sys
      COMMAND ${CMAKE_COMMAND} -E copy ${MGCORE_LIMINE_DIR}/limine-bios-cd.bin ${MGCORE_ISO_DIR}/limine-bios-cd.bin
      COMMAND ${CMAKE_COMMAND} -E copy ${MGCORE_LIMINE_DIR}/limine-uefi-cd.bin ${MGCORE_ISO_DIR}/limine-uefi-cd.bin
      COMMAND ${CMAKE_COMMAND} -E copy ${MGCORE_LIMINE_DIR}/BOOTX64.EFI ${MGCORE_ISO_DIR}/EFI/BOOT/BOOTX64.EFI
      COMMAND ${XORRISO_EXECUTABLE} -as mkisofs
              -b limine-bios-cd.bin
              -no-emul-boot
              -boot-load-size 4
              -boot-info-table
              --efi-boot limine-uefi-cd.bin
              -efi-boot-part
              --efi-boot-image
              --protective-msdos-label
              ${MGCORE_ISO_DIR}
              -o ${CMAKE_BINARY_DIR}/${MGCORE_ISO_NAME}
      COMMAND ${LIMINE_TOOL_EXECUTABLE} bios-install ${CMAKE_BINARY_DIR}/${MGCORE_ISO_NAME}
      DEPENDS kernel.elf initramfs ${CMAKE_SOURCE_DIR}/boot/limine.conf
      COMMENT "Building Limine ISO image"
      VERBATIM
    )
    add_custom_target(iso ALL DEPENDS ${CMAKE_BINARY_DIR}/${MGCORE_ISO_NAME})
  else()
    add_custom_target(iso
      COMMAND ${CMAKE_COMMAND} -E echo "Limine boot selected, but required files are missing."
      COMMAND ${CMAKE_COMMAND} -E echo "Expected a Limine binary release in tools/limine or -DLIMINE_DIR=... containing limine-bios.sys, limine-bios-cd.bin, limine-uefi-cd.bin, BOOTX64.EFI, and the limine host tool."
      COMMAND ${CMAKE_COMMAND} -E echo "Use scripts/setup-limine.ps1 or scripts/setup-limine.sh to fetch a binary release."
    )
  endif()
elseif(MGCORE_BUILD_ISO AND MGCORE_BOOTLOADER_SELECTED STREQUAL "GRUB" AND GRUB_MKRESCUE_EXECUTABLE)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/${MGCORE_ISO_NAME}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${MGCORE_ISO_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MGCORE_ISO_DIR}/boot/grub
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:kernel.elf> ${MGCORE_ISO_DIR}/boot/kernel.elf
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/boot/grub.cfg ${MGCORE_ISO_DIR}/boot/grub/grub.cfg
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MGCORE_INITRAMFS} ${MGCORE_ISO_DIR}/boot/initramfs.cpio
    COMMAND ${GRUB_MKRESCUE_EXECUTABLE} -o ${CMAKE_BINARY_DIR}/${MGCORE_ISO_NAME} ${MGCORE_ISO_DIR}
    DEPENDS kernel.elf initramfs ${CMAKE_SOURCE_DIR}/boot/grub.cfg
    COMMENT "Building GRUB ISO image"
    VERBATIM
  )
  add_custom_target(iso ALL DEPENDS ${CMAKE_BINARY_DIR}/${MGCORE_ISO_NAME})
else()
  add_custom_target(iso
    COMMAND ${CMAKE_COMMAND} -E echo "No boot image provider is ready."
    COMMAND ${CMAKE_COMMAND} -E echo "Selected bootloader: ${MGCORE_BOOTLOADER_SELECTED}"
    COMMAND ${CMAKE_COMMAND} -E echo "For Windows/MSYS2, use Limine via scripts/setup-limine.ps1 then rebuild."
  )
endif()

add_custom_target(run
  COMMAND ${CMAKE_COMMAND} -E echo "Use ./run.sh build gui or ./scripts/run.ps1 -BuildDir build -Mode gui after building"
  DEPENDS iso
)
