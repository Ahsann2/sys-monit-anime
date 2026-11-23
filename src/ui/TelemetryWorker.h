#pragma once

#include "core/Cleaner.h"
#include "core/Types.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <functional>

class QProcess;

namespace vg::ui {

enum class Work { Sample, SampleProcesses, ScanCache, CleanCache };

struct WorkResult {
    QString error;
    SystemSnapshot snapshot;
    std::vector<CleanupTarget> targets;
    CleanupResult cleanup;
};

class TelemetryWorker final : public QObject {
public:
    using Completion = std::function<void(WorkResult)>;

    explicit TelemetryWorker(QObject* parent = nullptr);
    ~TelemetryWorker() override;

    bool submit(Work work, const QStringList& ids, Completion completion);
    [[nodiscard]] bool busy() const;

    static int runHelper();

private:
    void receive();
    void fail(const QString& error);
    void complete(WorkResult result);

    QProcess* process_;
    QTimer deadline_;
    QByteArray request_;
    QByteArray response_;
    Work work_{Work::Sample};
    Completion completion_;
    bool reaping_{false};
};

} // namespace vg::ui
