# Syscall Itu Apaan Sih?

Dokumen ini bahas:

- apa itu syscall
- gimana syscall bekerja dari userspace ke kernel
- kenapa syscall bisa kejadian
- analogi biar kebayang
- contoh di konteks MGCORE

## TL;DR

Syscall itu cara resmi program user (ring 3) buat minta layanan ke kernel (ring 0).  
Program user gak boleh nyentuh hardware atau memori kernel langsung, jadi harus "ngetok pintu" lewat syscall.

## Apa Itu Syscall

Bayangin aplikasi lo jalan di area aman (`userspace`).  
Sementara kernel jalan di area superprivilege (`kernelspace`).

Karena ada batas keamanan, aplikasi gak bisa:

- baca keyboard controller langsung
- nulis ke disk controller langsung
- bikin proses baru seenaknya di level CPU privilege
- akses memori kernel mentah

Jadi aplikasi manggil API libc kayak `write()`, `read()`, `fork()`, `execve()`, dll.  
Di bawahnya, libc bakal ngubah itu jadi **instruksi syscall** ke CPU.

## Kenapa Syscall Bisa Terjadi

Karena OS modern pakai model proteksi:

- `ring 3`: userspace (terbatas)
- `ring 0`: kernelspace (privilege penuh)

CPU emang didesain buat pindah mode secara aman ketika ada event tertentu:

- interrupt hardware (misal keyboard, timer)
- exception/fault (misal page fault)
- **software trap/syscall** (request eksplisit dari program)

Jadi "bisa terjadi" karena:

1. Program user sengaja minta layanan kernel.
2. CPU punya mekanisme transisi privilege yang legal.
3. Kernel nyiapin handler buat nerima request itu.

## Flow Syscall Dari A sampai Z

Contoh mental model `write(1, "halo", 4)`:

1. Aplikasi manggil `write()` dari libc.
2. Wrapper libc isi register sesuai ABI syscall (`rax` nomor syscall, register lain argumen).
3. CPU eksekusi instruksi `syscall`.
4. CPU pindah dari ring 3 ke ring 0 dan lompat ke entry point syscall kernel.
5. Kernel simpan konteks penting, validasi argumen user pointer, cek permission.
6. Kernel dispatch berdasarkan nomor syscall ke fungsi handler yang cocok.
7. Handler ngerjain kerjaan (misal nulis ke console/fs/driver).
8. Nilai balik dimasukin ke register return.
9. Kernel balikin kontrol ke userspace pakai jalur return syscall.
10. Program lanjut jalan, seolah itu function call biasa.

## Analogi Gaul Biar Nempel

### Analogi 1: Apartemen + Security

- Userspace = penghuni apartemen
- Kernel = ruang kontrol gedung
- Syscall = interkom ke security

Penghuni gak bisa buka panel listrik utama sendiri.  
Kalau mau akses sesuatu yang privileged, dia tekan interkom:

- "Bang, tolong bukain pintu parkir" (`open`)
- "Bang, kirim paket ke unit X" (`write`)
- "Bang, saya pindah kamar" (`execve` secara konsep ganti image proses)

Security verifikasi dulu, baru eksekusi.  
Kalau request aneh atau gak valid, ditolak.

### Analogi 2: Restoran Open Kitchen

- User program = pelanggan
- Kernel = dapur + chef
- Syscall = pesanan via waiter

Lo gak boleh nyelonong ke dapur masak sendiri.  
Lo pesan via waiter (libc + syscall interface).  
Waiter bawa request dengan format yang bener.  
Chef proses, lalu hasilnya balik ke meja lo.

## Kenapa Gak Langsung Aja Akses Hardware?

Kalau semua app boleh akses hardware langsung:

- keamanan jebol
- satu app bisa ngerusak app lain
- debugging jadi neraka
- resource conflict brutal (disk, network, memory)

Syscall bikin semua akses sensitif lewat satu gerbang terkontrol.

## Komponen Penting di Jalur Syscall

- **Userspace API**: fungsi libc (`read`, `write`, dst.)
- **Syscall ABI**: aturan nomor syscall + register argumen
- **CPU transition mechanism**: instruksi `syscall`/`sysenter`/`int`
- **Kernel entry stub**: kode assembly awal buat setup konteks
- **Dispatcher**: switch nomor syscall -> handler C
- **Handler**: implementasi logika tiap syscall
- **Return path**: balik ke userspace dengan nilai return / errno

## Error Handling Singkat

Kalau kernel nolak atau belum implement:

- return negatif (di kernel) seperti `-EINVAL`, `-EFAULT`, `-ENOSYS`
- libc biasanya expose ke app sebagai `-1` + `errno`

## Relevansi Buat MGCORE

Di MGCORE, fondasi syscall udah ada:

- nomor syscall Linux-like di `src/syscall/unistd.h`
- table dispatch di `src/syscall/syscall_table.c`
- implementasi handler di `src/syscall/syscall_impl.c`
- transisi arch-specific di `src/kernel/syscall_entry.asm` dan `src/kernel/syscall_arch.c`

Daftar status syscall (implemented vs placeholder) tetap ada di:

- `docs/syscalls.md`

## Ringkasan

Syscall itu "jembatan resmi" antara dunia user dan kernel.  
Tanpa syscall, OS modern gak punya boundary keamanan yang waras.  
Dengan syscall, app bisa minta layanan powerful tanpa dikasih kunci root ke seluruh mesin.
