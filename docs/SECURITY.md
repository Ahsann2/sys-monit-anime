# Keamanan Monitor Sistem

## Privilege model

Monitor Sistem berjalan sebagai user biasa. Implementasi saat ini tidak memasang
setuid binary dan tidak menjalankan long-lived root daemon.

Akibatnya:

- Aplikasi membaca telemetri yang diekspos kernel kepada pengguna.
- Aplikasi dapat memberi sinyal hanya kepada proses dengan UID yang sama.
- Pada GL752VW, mode kipas CPU hanya menerima otomatis atau maksimum melalui
  autentikasi PolicyKit. Aplikasi memanggil executable sistem dengan argumen
  terpisah untuk atribut sysfs pengontrol kipas yang ditemukan pada driver ASUS.
- System package cache dan system journal tidak dihapus.
- Session termination tetap tunduk pada policy systemd-logind.

## Process protection

Process berikut dilindungi secara default:

- systemd user manager
- GNOME Shell dan session components
- D-Bus
- PipeWire dan WirePlumber
- Xorg/Xwayland
- NetworkManager
- display manager
- xdg-desktop-portal
- Monitor Sistem dan proses pembaca sensornya

Protection juga diterapkan untuk cgroup `init.scope` dan `session.slice`.

## Safe signaling

Linux dapat memakai ulang PID setelah sebuah process berakhir. Mengirim signal
hanya berdasarkan angka PID dapat mengenai process baru. Aplikasi
mencoba membuka PID file descriptor dan memakai `pidfd_send_signal`.

Sinyal ditolak bila pidfd tidak tersedia, PID, waktu mulai, atau cgroup berubah, atau
proses milik UID lain. Untuk pilihan per proyek, direktori kerja diverifikasi
ulang sebelum sinyal dikirim.

## Cleaner boundaries

- Target ditentukan oleh ID internal, bukan path bebas.
- Root target harus berada pada allowlist.
- Tiap komponen direktori dibuka tanpa mengikuti symlink.
- Pemindaian dan penghapusan membatasi berkas pada berkas reguler milik pengguna.
- Pemindaian `/tmp` bersifat opsional. Pembersihan hanya mencakup berkas
  reguler milik pengguna di tingkat teratas dengan umur minimal tujuh hari.
- Tempat Sampah dikelola melalui aplikasi berkas.
- Browser profile, cookie, login, bookmark, project source, Documents,
  Downloads, SSH keys dan dot-config tidak menjadi target.

## Data

- Tidak ada telemetry network.
- Tidak ada analytics.
- Tidak ada account atau cloud sync.
- Snapshot live berada dalam memory.
- QSettings hanya menyimpan preference aplikasi.

## Browser Companion

Permission:

- `nativeMessaging`
- `tabs`

Companion tidak meminta `history`, `cookies`, `storage`, `<all_urls>`,
`webRequest`, atau page-content permission.

Native host memakai perintah lokal berusia maksimal sepuluh detik.
Companion mempertahankan tab yang disematkan. Aksi penghentian program dan
keluar dari sesi tidak mengirim perintah tab.

## Reporting

Pengujian lokal:

1. Jalankan `./scripts/test-core.sh`.
2. Jalankan `vivoguard-probe`.
3. Pastikan nama sensor dan GPU sesuai.
4. Uji Close Applications dengan dokumen dummy.
5. Uji Fresh Session setelah seluruh pekerjaan disimpan.
