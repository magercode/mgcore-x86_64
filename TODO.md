# TODO MGCORE

## Prioritas 1 (MVP Kernel + CLI)

- [x] Finalisasi struktur folder dan konvensi penamaan.
- [x] Entry boot minimal (early init + jump ke kernel).
- [x] Inisialisasi console/serial output.
- [ ] Shell CLI minimal dengan input line editing dasar.
- [ ] Command built-in: `help`, `clear`, `echo`.
- [ ] Build pipeline CMake + Ninja stabil.

## Prioritas 2 (Kernel Core)

- [ ] Manajemen memory sederhana (heap/kheap).
- [ ] Scheduler sederhana dan task management.
- [ ] Timer interrupt + timekeeping.
- [x] Driver keyboard dan handler input.
- [x] Logging sistem (levels: info/warn/error).

## Prioritas 3 (Filesystem + Utilities)

- [ ] Filesystem minimal (ramfs atau block fs sederhana).
- [ ] Command built-in: `ls`, `cat`, `pwd`.
- [ ] Path handling dan working directory.
- [ ] Command pipeline sederhana.

## Prioritas 4 (Ekosistem)

- [ ] Dokumentasi arsitektur kernel.
- [ ] Diagram boot flow.
- [ ] Standar coding style.
- [ ] Unit test untuk parser CLI.
- [ ] Tooling lint/format.

## Opsional / Eksperimen

- [ ] Multiboot support.
- [ ] Virtual file system (VFS) layer.
- [ ] Driver storage (ATA/virtio).
- [ ] Network stack minimal.
- [ ] Porting libc subset.
