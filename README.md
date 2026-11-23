# sys-monit-anime

Dashboard pemantau perangkat dengan animasi kipas. C++20 dan Qt 6 untuk Linux, Windows, dan macOS. Data sensor mengikuti dukungan OS dan driver.

Pilih tema bawaan atau salah satu dari 15 tema karakter di **Pengaturan → Tampilan**.

## Cara menjalankan

Butuh kompiler C++20, CMake 3.22+, Ninja, dan Qt 6.4+ dengan modul Core, Widgets, dan SVG. Linux juga perlu Qt DBus. Di Windows, gunakan Developer PowerShell dengan arsitektur yang sesuai dengan SDK Qt dan tambahkan direktori `bin` Qt ke `PATH`.

Jalankan dari direktori repo. Ganti `/path/to/qt` dengan direktori SDK Qt.

```sh
cmake --preset release -DCMAKE_PREFIX_PATH="/path/to/qt"
cmake --build --preset release
```

| OS | Perintah |
| --- | --- |
| Linux | `./build/release/sys-monit-anime` |
| Windows | `.\build\release\sys-monit-anime.exe` |
| macOS | `open build/release/sys-monit-anime.app` |

[Dukungan perangkat](docs/PLATFORMS.md) · [Lisensi MIT](LICENSE)
