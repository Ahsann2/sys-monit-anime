# Dukungan platform

Monitor Sistem memakai antarmuka Qt yang sama dan pembaca telemetri native
untuk setiap sistem operasi. Perangkat dibaca saat aplikasi berjalan.

| Fitur | Linux | Windows | macOS |
| --- | --- | --- | --- |
| Model, CPU, RAM, disk, dan jaringan | DMI, procfs, sysfs | Win32, PDH, IP Helper | sysctl, Mach, IOKit |
| CPU per thread | procfs | PDH jika tersedia | Mach |
| Baterai dan daya AC | power supply sysfs | Win32 | IOKit Power Sources |
| Daftar proses dan statistik yang dapat dibaca | Proses pengguna melalui procfs | Toolhelp dan Win32 | libproc |
| Adaptor grafis | DRM dan driver | DXGI, klasifikasi UMA jika D3D12 tersedia | Metal |
| Beban dan suhu GPU | NVIDIA NVML, beban aplikasi Intel i915, dan sensor AMD yang tersedia | NVIDIA NVML jika DLL driver dapat dimuat | Identitas GPU via Metal |
| Suhu CPU dan RPM | Kernel dan driver perangkat | Tergantung integrasi sensor tambahan, saat ini N/A | Tergantung integrasi sensor tambahan, saat ini N/A |
| Kontrol kipas | ASUS GL752VW melalui ASUS WMI dan autentikasi admin | — | — |
| Aksi proses, cache, dan login otomatis | Tersedia sesuai dukungan kernel dan desktop | Pemantauan | Pemantauan |

Sensor dan izin berbeda menurut model, firmware, driver, dan akun. Aplikasi
menampilkan `N/A` atau celah pada grafik bila data tidak tersedia. Angka RPM
hanya muncul saat kernel atau driver menyediakan pembacaan yang sah.
Beban aplikasi Intel dihitung dari kanal GPU tersibuk di antara proses yang
dapat dibaca oleh akun pengguna.
Pustaka NVML berasal dari driver NVIDIA pengguna. Aplikasi tidak memasang
driver, menaikkan hak akses saat membaca data, atau melakukan kontrol kipas
pada model yang belum divalidasi.

## Tampilan

Pilih karakter lewat **Pengaturan → Tampilan** untuk menerapkan palet, ikon,
potret, latar, dan rotor sekaligus. Semua tema sudah disertakan di aplikasi.
Untuk mengganti ilustrasi secara lokal, simpan `portrait.png` dan `scene.png`
di folder karakter di bawah `sys-monit-anime/characters` pada direktori data
pengguna, lalu buka ulang aplikasi.

| Karakter | Nama folder |
| --- | --- |
| Sung Jinwoo | `sung-jinwoo` |
| Sasuke Uchiha | `sasuke` |
| Tanjiro Kamado | `tanjiro` |
| Giyu Tomioka | `giyu` |
| Satoru Gojo | `gojo` |
| Sanemi Shinazugawa | `sanemi` |
| Muichiro Tokito | `muichiro` |
| Itachi Uchiha | `itachi` |
| Killua Zoldyck | `killua` |
| Naruto Uzumaki · Kurama | `naruto` |
| Shinobu Kocho | `shinobu` |
| Spider-Man | `spider-man` |
| Doma · Bulan Atas Dua | `doma` |
| Akaza · Bulan Atas Tiga | `akaza` |
| Ryomen Sukuna | `sukuna` |
| Megumi Fushiguro | `megumi` |
| Gusion · Soul Revelation | `gusion` |
| Julian · Scarlet Raven | `julian` |
| Yu Zhong · Exorcist | `yu-zhong` |

## Build desktop

Gunakan Qt 6.4+ (Core, Widgets, SVG dan DBus khusus Linux), CMake 3.22+, dan
kompiler C++20. Matriks CI menyasar Ubuntu 24.04, Windows x64 dan ARM64, serta
macOS Intel dan Apple Silicon. Paket dibuat untuk arsitektur dan OS tujuannya.

Untuk Linux dengan Qt di lokasi khusus, sertakan
`-DCMAKE_PREFIX_PATH=/lokasi/Qt` saat konfigurasi. Ubuntu dengan paket Qt
di bawah 6.4 memerlukan SDK yang terpisah atau build Qt yang kompatibel.

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Windows, melalui Developer PowerShell untuk Visual Studio:

```powershell
cmake -S . -B build/native -A x64 -DCMAKE_PREFIX_PATH="$env:QT_ROOT_DIR" -DVIVOGUARD_DEPLOY_QT=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure
cmake --install build/native --config Release --prefix "$PWD/dist"
```

Untuk Windows ARM64, pakai SDK Qt dan MSVC untuk ARM64 serta `-A ARM64`.

macOS, melalui Terminal dengan Xcode:

```sh
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DVIVOGUARD_DEPLOY_QT=ON
cmake --build build/native
ctest --test-dir build/native --output-on-failure
cmake --install build/native --prefix dist
```

`QT_ROOT_DIR` menunjuk direktori SDK Qt, misalnya direktori `clang_64` di
macOS. Hasil macOS berupa `sys-monit-anime.app`. Paket pengujian CI tidak
menggunakan sertifikat penandatanganan atau notarization. Untuk distribusi
biner publik, tinjau penandatanganan dan kewajiban lisensi Qt yang dipakai.
