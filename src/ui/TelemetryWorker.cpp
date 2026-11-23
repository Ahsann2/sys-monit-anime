#include "ui/TelemetryWorker.h"

#include "core/SystemSampler.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QIODevice>
#include <QProcess>
#include <QtEndian>

#include <cstdio>
#include <iostream>
#include <type_traits>
#include <utility>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

namespace vg::ui {
namespace {

constexpr quint32 maxFrame = 16 * 1024 * 1024;

struct Wire {
    QDataStream& stream;

    template<typename T>
    void field(T& value) {
        if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
            using Number = std::conditional_t<std::is_floating_point_v<T>, double,
                std::conditional_t<std::is_signed_v<T>, qint64, quint64>>;
            Number number = static_cast<Number>(value);
            if (writing) {
                stream << number;
            } else {
                stream >> number;
                value = static_cast<T>(number);
            }
        } else if constexpr (std::is_same_v<T, std::string>) {
            QByteArray bytes = QByteArray::fromStdString(value);
            if (writing) {
                stream << bytes;
            } else {
                stream >> bytes;
                value = bytes.toStdString();
            }
        } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
            auto path = value.string();
            field(path);
            if (!writing) {
                value = std::move(path);
            }
        } else {
            transfer(*this, value);
        }
    }

    template<typename T>
    void field(std::optional<T>& value) {
        bool present = value.has_value();
        field(present);
        if (!present) {
            value.reset();
            return;
        }
        if (!writing) {
            value.emplace();
        }
        field(*value);
    }

    template<typename T>
    void field(std::vector<T>& values) {
        quint32 count = static_cast<quint32>(values.size());
        if (writing) {
            stream << count;
        } else {
            stream >> count;
            if (count > 131072) {
                stream.setStatus(QDataStream::ReadCorruptData);
                return;
            }
            values.resize(count);
        }
        for (auto& value : values) {
            if (stream.status() != QDataStream::Ok) {
                return;
            }
            field(value);
        }
    }

    void operator()(auto&... values) {
        (field(values), ...);
    }

    bool writing;
};

void transfer(auto& wire, PressureStats& value) {
    wire(value.available, value.someAvg10, value.someAvg60, value.someAvg300,
        value.fullAvg10, value.fullAvg60, value.fullAvg300);
}
void transfer(auto& wire, CpuStats& value) {
    wire(value.usageAvailable, value.usagePercent, value.perCorePercent, value.load1, value.load5,
        value.load15, value.averageClockGhz, value.temperatureC,
        value.coreTemperaturesC, value.packageThrottleCount, value.logicalCores, value.pressure);
}
void transfer(auto& wire, FanStats& value) {
    wire(value.cpuRpm, value.gpuRpm, value.cpuMode, value.controlPath);
}
void transfer(auto& wire, MemoryStats& value) {
    wire(value.swapAvailable, value.totalBytes, value.availableBytes, value.usedBytes, value.cachedBytes,
        value.buffersBytes, value.swapTotalBytes, value.swapUsedBytes, value.pressure);
}
void transfer(auto& wire, GpuStats& value) {
    wire(value.available, value.sleeping, value.discrete, value.name,
        value.usagePercent, value.memoryUsagePercent, value.memoryUsedBytes,
        value.memoryTotalBytes, value.temperatureC, value.utilizationAvailable,
        value.powerWatts, value.fanPercent, value.graphicsClockMhz);
}
void transfer(auto& wire, DiskStats& value) {
    wire(value.ratesAvailable, value.totalBytes, value.freeBytes, value.usedBytes, value.readBytesPerSecond,
        value.writeBytesPerSecond, value.temperatureC, value.pressure);
}
void transfer(auto& wire, NetworkStats& value) {
    wire(value.available, value.receivedBytes, value.transmittedBytes, value.receiveBytesPerSecond,
        value.transmitBytesPerSecond);
}
void transfer(auto& wire, BatteryStats& value) {
    wire(value.acOnline, value.present, value.charging, value.percent, value.healthPercent,
        value.powerWatts, value.voltageVolts, value.temperatureC, value.cycleCount,
        value.estimatedSeconds, value.status);
}
void transfer(auto& wire, ProcessInfo& value) {
    wire(value.pid, value.parentPid, value.uid, value.startTicks, value.name,
        value.command, value.cgroup, value.project, value.cpuPercent, value.residentAvailable,
        value.ioAvailable, value.residentBytes,
        value.readBytes, value.writeBytes, value.threadCount, value.protectedProcess);
}
void transfer(auto& wire, SystemSnapshot& value) {
    auto uptime = value.uptime.count();
    wire(uptime, value.cpu, value.fans, value.memory, value.integratedGpu,
        value.discreteGpu, value.graphicsAdapters, value.disk, value.network, value.battery, value.processes,
        value.hostname, value.kernel, value.systemName);
    value.uptime = std::chrono::seconds{uptime};
}
void transfer(auto& wire, CleanupTarget& value) {
    wire(value.id, value.title, value.description, value.path, value.sizeBytes,
        value.selectedByDefault, value.browserRelated, value.trash, value.olderThanDays);
}
void transfer(auto& wire, CleanupResult& value) {
    wire(value.bytesFreed, value.filesRemoved, value.errors);
}

void transferResult(QDataStream& stream, Work work, WorkResult& result, bool writing) {
    stream.setVersion(QDataStream::Qt_6_0);
    if (writing) {
        stream << result.error;
    } else {
        stream >> result.error;
    }
    if (!result.error.isEmpty()) {
        return;
    }
    Wire wire{stream, writing};
    switch (work) {
    case Work::Sample:
    case Work::SampleProcesses:
        wire(result.snapshot);
        break;
    case Work::ScanCache:
        wire(result.targets);
        break;
    case Work::CleanCache:
        wire(result.cleanup);
        break;
    }
}

} // namespace

TelemetryWorker::TelemetryWorker(QObject* parent)
    : QObject(parent), process_(new QProcess) {
    deadline_.setSingleShot(true);
    connect(&deadline_, &QTimer::timeout, this, [this] {
        reaping_ = process_->state() != QProcess::NotRunning;
        process_->kill();
        fail("Pekerjaan latar melewati batas waktu.");
    });
    connect(process_, &QProcess::started, this, [this] {
        if (!request_.isEmpty()) {
            process_->write(std::exchange(request_, {}));
        }
    });
    connect(process_, &QProcess::readyReadStandardOutput, this, [this] { receive(); });
    connect(process_, &QProcess::readyReadStandardError, this, [this] {
        static_cast<void>(process_->readAllStandardError());
    });
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            fail("Pekerja latar gagal dibuka.");
        }
    });
    connect(process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this, [this] {
            reaping_ = false;
            fail("Pekerja latar berhenti.");
        });
}

TelemetryWorker::~TelemetryWorker() {
    process_->disconnect(this);
    process_->closeWriteChannel();
    if (process_->state() == QProcess::NotRunning || process_->waitForFinished(20)) {
        delete process_;
        return;
    }
    process_->kill();
    if (process_->state() == QProcess::NotRunning || process_->waitForFinished(35)) {
        delete process_;
        return;
    }
    // Leave an uninterruptible driver call for the OS to reap without blocking quit.
    connect(process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        process_, &QObject::deleteLater);
}

bool TelemetryWorker::busy() const {
    return reaping_ || static_cast<bool>(completion_) ||
        process_->state() == QProcess::Starting;
}

bool TelemetryWorker::submit(Work work, const QStringList& ids, Completion completion) {
    if (busy()) {
        return false;
    }
    work_ = work;
    completion_ = std::move(completion);
    response_.clear();
    QByteArray request = QByteArray::number(static_cast<int>(work));
    for (const auto& id : ids) {
        request += ' ';
        request += id.toUtf8();
    }
    request += '\n';
    deadline_.start(work == Work::CleanCache ? 120000 : work == Work::ScanCache ? 30000 : 4500);
    if (process_->state() == QProcess::NotRunning) {
        request_ = std::move(request);
        process_->start(QCoreApplication::applicationFilePath(), {"--telemetry-worker"});
    } else {
        process_->write(request);
    }
    return true;
}

void TelemetryWorker::receive() {
    const auto chunk = process_->readAllStandardOutput();
    if (!completion_) {
        return;
    }
    response_ += chunk;
    if (response_.size() < 4) {
        return;
    }
    const auto length = qFromBigEndian<quint32>(response_.constData());
    if (length > maxFrame || response_.size() > 4 + length) {
        reaping_ = true;
        process_->kill();
        fail("Format data pekerja latar tidak valid.");
        return;
    }
    if (response_.size() != 4 + length) {
        return;
    }
    QDataStream stream(response_.sliced(4));
    WorkResult result;
    transferResult(stream, work_, result, false);
    if (stream.status() != QDataStream::Ok || !stream.atEnd()) {
        reaping_ = true;
        process_->kill();
        fail("Data dari pekerja latar tidak lengkap.");
        return;
    }
    complete(std::move(result));
}

void TelemetryWorker::complete(WorkResult result) {
    deadline_.stop();
    request_.clear();
    response_.clear();
    auto callback = std::exchange(completion_, {});
    if (callback) {
        callback(std::move(result));
    }
}

void TelemetryWorker::fail(const QString& error) {
    WorkResult result;
    result.error = error;
    complete(std::move(result));
}

int TelemetryWorker::runHelper() {
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        return 2;
    }
#endif
    SystemSampler sampler;
    Cleaner cleaner;
    for (std::string line; std::getline(std::cin, line);) {
        if (line.size() > 4096) {
            return 2;
        }
        const auto fields = QString::fromStdString(line).split(' ', Qt::SkipEmptyParts);
        bool valid = false;
        const int code = fields.value(0).toInt(&valid);
        if (!valid || code < static_cast<int>(Work::Sample) ||
            code > static_cast<int>(Work::CleanCache)) {
            return 2;
        }
        const auto work = static_cast<Work>(code);
        WorkResult result;
        try {
            switch (work) {
            case Work::Sample:
            case Work::SampleProcesses:
                result.snapshot = sampler.sample(work == Work::SampleProcesses);
                break;
            case Work::ScanCache:
                result.targets = cleaner.scan();
                break;
            case Work::CleanCache: {
                std::vector<std::string> ids;
                for (qsizetype index = 1; index < fields.size(); ++index) {
                    ids.push_back(fields[index].toStdString());
                }
                result.cleanup = cleaner.clean(ids);
                break;
            }
            }
        } catch (const std::exception& error) {
            result.error = QString::fromLocal8Bit(error.what());
        }

        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        transferResult(stream, work, result, true);
        if (stream.status() != QDataStream::Ok || data.size() > maxFrame) {
            return 3;
        }
        const auto size = qToBigEndian<quint32>(static_cast<quint32>(data.size()));
        const auto length = static_cast<std::size_t>(data.size());
        if (std::fwrite(&size, sizeof(size), 1, stdout) != 1 ||
            std::fwrite(data.constData(), 1, length, stdout) != length ||
            std::fflush(stdout) != 0) {
            return 4;
        }
    }
    return 0;
}

} // namespace vg::ui
