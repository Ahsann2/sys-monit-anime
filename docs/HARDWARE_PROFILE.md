# Profil perangkat keras

## ASUS ROG GL752VW

- Intel Core i7-6700HQ, Intel HD Graphics 530, GTX 960M, dan RAM nominal 32 GB.
- Suhu paket dan core dibaca dari `coretemp`. Sensor ASUS dan ACPI dipakai
  sebagai cadangan bila sensor CPU tidak terbaca.
- Peringatan CPU 85 °C dan kritis 95 °C. Peringatan GPU 80 °C dan kritis 86 °C.
- Kanal RPM CPU dan GPU ditampilkan terpisah. Angka di luar rentang pembacaan
  yang wajar ditandai N/A.
- Driver ASUS WMI mengatur mode otomatis atau maksimum untuk kipas CPU
  setelah autentikasi admin. GPU tetap dipantau lewat NVML dan sensor yang
  diekspos firmware, sesuai ketersediaan tiap metrik.

## ASUS VivoBook Pro 15 OLED

Profile bawaan:

| Komponen | Target |
|---|---|
| CPU | AMD Ryzen 5 5600H |
| iGPU | AMD Radeon Graphics |
| dGPU | NVIDIA GeForce RTX 3050 Laptop GPU 4 GB |
| RAM | 16 GB |
| OS | Ubuntu 24.04 LTS |
| Desktop | GNOME Wayland atau X11 |

Threshold awal:

| Metric | Warning | Critical |
|---|---:|---:|
| CPU temperature | 88 °C | 96 °C |
| GPU temperature | 80 °C | 86 °C |
| RAM used | 85% | 93% |
| Root storage used | 90% | 96% |

Memory alert juga dapat dipicu oleh PSI meskipun persentase belum melewati
threshold. CPU saturation membutuhkan load dan scheduler pressure yang
bertahan, sehingga compile spike singkat tidak langsung dianggap insiden.

## Sensor preference

Urutan CPU hwmon:

1. `k10temp`
2. `zenpower`
3. `asus_wmi_sensors`
4. `acpitz`

Jika tidak ditemukan, VivoGuard mencoba generic thermal zones. Nilai tidak
tersedia ditampilkan sebagai N/A atau EC AUTO; tidak ada sensor sintetis.

## RTX battery-aware mode

Sebelum NVML diinisialisasi, VivoGuard memeriksa PCI runtime status untuk
vendor NVIDIA. Jika perangkat `suspended`, card ditampilkan sebagai
`RTD3 SLEEP` dan NVML tidak disentuh. Ini penting pada laptop hybrid graphics
agar aplikasi monitoring tidak menjadi alasan dGPU terus hidup.
