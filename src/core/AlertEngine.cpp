#include "core/AlertEngine.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace vg {
namespace {

std::string formatPercent(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(0) << value << '%';
    return stream.str();
}

std::string formatTemperature(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(0) << value << " °C";
    return stream.str();
}

std::string formatGib(std::uint64_t bytes) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1)
           << static_cast<double>(bytes) /
                  (1024.0 * 1024.0 * 1024.0)
           << " GiB";
    return stream.str();
}

} // namespace

AlertEngine::AlertEngine(HardwareProfile profile)
    : profile_(std::move(profile)) {}

bool AlertEngine::sustained(
    const std::string& id,
    bool condition,
    std::chrono::seconds duration) {
    auto& state = states_[id];
    const auto now = std::chrono::steady_clock::now();
    if (!condition) {
        state.active = false;
        return false;
    }
    if (!state.active) {
        state.active = true;
        state.activeSince = now;
        return duration.count() == 0;
    }
    return now - state.activeSince >= duration;
}

std::string AlertEngine::topMemoryConsumer(
    const SystemSnapshot& snapshot) {
    const auto process = std::max_element(
        snapshot.processes.begin(), snapshot.processes.end(),
        [](const ProcessInfo& left, const ProcessInfo& right) {
            return left.residentBytes < right.residentBytes;
        });
    if (process == snapshot.processes.end()) {
        return "suatu proses yang tidak diketahui";
    }
    return process->name + " (" + formatGib(process->residentBytes) + ")";
}

std::vector<Alert> AlertEngine::evaluate(
    const SystemSnapshot& snapshot) {
    std::vector<Alert> alerts;

    const double memoryPercent = snapshot.memory.usedPercent();
    if (memoryPercent >= profile_.memoryCriticalPercent ||
        snapshot.memory.pressure.fullAvg10 >= 5.0) {
        alerts.push_back({
            "memory-critical",
            Severity::Critical,
            "Tekanan memori sangat kritis",
            formatPercent(memoryPercent) + " RAM sedang digunakan; " +
                topMemoryConsumer(snapshot) + " adalah yang terbesar saat ini.",
            "Simpan pekerjaan dan tutup aplikasi yang tidak diperlukan."
        });
    } else if (memoryPercent >= profile_.memoryWarningPercent ||
               snapshot.memory.pressure.someAvg10 >= 12.0) {
        alerts.push_back({
            "memory-warning",
            Severity::Warning,
            "Tekanan memori meningkat",
            formatPercent(memoryPercent) + " RAM sedang digunakan; beban memori dapat memperlambat sistem.",
            "Periksa aplikasi berat sebelum sistem menjadi tidak responsif."
        });
    }

    if (snapshot.cpu.temperatureC >= profile_.cpuCriticalC) {
        alerts.push_back({
            "cpu-temperature-critical",
            Severity::Critical,
            "Suhu CPU sangat kritis",
            "Suhu CPU mencapai " +
                formatTemperature(snapshot.cpu.temperatureC) + '.',
            "Hentikan pekerjaan berat dan periksa aliran udara laptop segera."
        });
    } else if (snapshot.cpu.temperatureC >= profile_.cpuWarningC) {
        alerts.push_back({
            "cpu-temperature-warning",
            Severity::Warning,
            "Suhu CPU cukup panas",
            "Suhu CPU mencapai " +
                formatTemperature(snapshot.cpu.temperatureC) + '.',
            "Tinggikan posisi laptop, periksa kipas, atau kurangi beban kerja."
        });
    }

    if (sustained("cpu-saturation",
            snapshot.cpu.usagePercent >= 94.0 &&
                snapshot.cpu.pressure.someAvg10 >= 10.0,
            std::chrono::seconds(10))) {
        alerts.push_back({
            "cpu-saturation",
            Severity::Warning,
            "CPU mengalami saturasi beban",
            "Beban CPU dan scheduler tinggi secara terus-menerus selama lebih dari 10 detik.",
            "Buka menu Proses untuk menemukan tugas latar belakang yang tinggi."
        });
    }

    const auto& gpu = snapshot.discreteGpu;
    if (gpu.available && !gpu.sleeping) {
        if (gpu.temperatureC >= profile_.gpuCriticalC) {
            alerts.push_back({
                "gpu-temperature-critical",
                Severity::Critical,
                "Suhu GPU sangat kritis",
                "Suhu GPU mencapai " +
                    formatTemperature(gpu.temperatureC) + '.',
                "Hentikan beban kerja GPU dan periksa aliran udara laptop."
            });
        } else if (gpu.temperatureC >= profile_.gpuWarningC) {
            alerts.push_back({
                "gpu-temperature-warning",
                Severity::Warning,
                "Suhu GPU cukup panas",
                "Suhu GPU mencapai " +
                    formatTemperature(gpu.temperatureC) + '.',
                "Pertimbangkan untuk mengurangi beban kerja GPU atau menggunakan mode Seimbang."
            });
        }
        if (gpu.memoryUsagePercent >= 94.0) {
            alerts.push_back({
                "vram-critical",
                Severity::Critical,
                "VRAM GPU hampir penuh",
                formatPercent(gpu.memoryUsagePercent) +
                    " dari " + formatGib(gpu.memoryTotalBytes) + " VRAM telah dialokasikan.",
                "Tutup aplikasi berat GPU atau kurangi ukuran tekstur/model."
            });
        }
    }

    const double diskPercent = snapshot.disk.usedPercent();
    if (diskPercent >= profile_.diskCriticalPercent) {
        alerts.push_back({
            "disk-critical",
            Severity::Critical,
            "Penyimpanan sistem hampir penuh",
            "Penyimpanan Root terisi " + formatPercent(diskPercent) + '.',
            "Jalankan Smart Clean sekarang untuk mencegah kegagalan aplikasi."
        });
    } else if (diskPercent >= profile_.diskWarningPercent) {
        alerts.push_back({
            "disk-warning",
            Severity::Warning,
            "Penyimpanan sistem mulai penuh",
            "Penyimpanan Root terisi " + formatPercent(diskPercent) + '.',
            "Periksa berkas cache dan berkas build."
        });
    }

    if (sustained("io-pressure",
            snapshot.disk.pressure.fullAvg10 >= 8.0,
            std::chrono::seconds(8))) {
        alerts.push_back({
            "io-pressure",
            Severity::Warning,
            "Aktivitas penyimpanan menghambat aplikasi",
            "Tekanan I/O tinggi dan dapat menyebabkan antarmuka tersendat.",
            "Buka menu Proses dan periksa aplikasi yang membaca/menulis media secara intensif."
        });
    }

    if (snapshot.battery.present &&
        snapshot.battery.healthPercent >= 0.0 &&
        snapshot.battery.healthPercent < 75.0) {
        alerts.push_back({
            "battery-health",
            Severity::Info,
            "Kesehatan baterai telah menurun",
            "Kapasitas penuh saat ini diperkirakan " +
                formatPercent(snapshot.battery.healthPercent) +
                " dari kapasitas desain.",
            "Pertimbangkan servis baterai jika waktu pakai sudah tidak optimal."
        });
    }

    return alerts;
}

Severity AlertEngine::overallSeverity(
    const std::vector<Alert>& alerts) {
    Severity result = Severity::Normal;
    for (const auto& alert : alerts) {
        if (static_cast<int>(alert.severity) >
            static_cast<int>(result)) {
            result = alert.severity;
        }
    }
    return result;
}

} // namespace vg
