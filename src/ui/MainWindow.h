#pragma once

#include "core/AlertEngine.h"
#include "core/Cleaner.h"
#include "core/HardwareProfile.h"
#include "core/Types.h"
#include "ui/TelemetryWorker.h"

#include <QMainWindow>
#include <QSet>
#include <QTimer>

#include <memory>
#include <string>
#include <utility>
#include <vector>

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QEvent;
class QFrame;
class QLabel;
class QLineEdit;
class QProgressBar;
class QProcess;
class QPushButton;
class QShortcut;
class QStackedWidget;
class QSystemTrayIcon;
class QTableView;
class QToolButton;
class QVBoxLayout;

namespace vg::ui {

class FanWidget;
class GaugeCard;
class ProcessTableModel;
class TimelineChart;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    QWidget* buildSidebar();
    QWidget* buildHeader();
    QWidget* buildOverviewPage();
    QWidget* buildProcessesPage();
    QWidget* buildThermalsPage();
    QWidget* buildCleanerPage();
    QWidget* buildSettingsPage();
    QFrame* createCard(const QString& objectName = "card") const;
    QPushButton* createNavButton(
        const QString& label,
        const QString& glyph,
        int pageIndex);

    void setupTray();
    void toggleFullScreen();
    void updateFullScreenAction();
    void sampleNow();
    void acceptSample(WorkResult result, bool includeProcesses);
    void updateOverview();
    void updateProcesses();
    void updateThermals();
    void updateAlerts();
    void updateHeader();
    void notifyNewAlerts();
    void scanCleanupTargets();
    void renderCleanupTargets(std::vector<CleanupTarget> targets);
    void executeCleanup();
    void setCpuFanMode(FanMode mode);
    void freeMemoryNow();
    void closeAllApplications();
    void freshSession();
    void forceSessionEnd();
    void terminateSelectedProcess(bool force);
    void switchPage(int index);
    void setAutostart(bool enabled);

    [[nodiscard]] const ProcessInfo* selectedProcess() const;
    [[nodiscard]] static QString formatRate(double bytesPerSecond, int precision = 1);
    [[nodiscard]] static QString formatUptime(
        std::chrono::seconds uptime);
    [[nodiscard]] static QString severityName(Severity severity);
    [[nodiscard]] static QString severityColor(Severity severity);

    HardwareProfile profile_;
    TelemetryWorker telemetry_;
    TelemetryWorker maintenance_;
    AlertEngine alertEngine_;
    SystemSnapshot snapshot_;
    std::vector<Alert> alerts_;
    QSet<QString> notifiedAlertIds_;
    QTimer sampleTimer_;
    unsigned int sampleCounter_{0};
    bool samplingFailed_{false};
    bool allowQuit_{false};

    QStackedWidget* pages_{nullptr};
    std::vector<QPushButton*> navButtons_;
    QLabel* pageTitleLabel_{nullptr};
    QLabel* clockLabel_{nullptr};
    QLabel* healthLabel_{nullptr};
    QLabel* headerSubtitle_{nullptr};
    QLabel* graphicsValue_{nullptr};
    QToolButton* fullscreenButton_{nullptr};
    QShortcut* exitFullScreenShortcut_{nullptr};
    QSystemTrayIcon* tray_{nullptr};

    GaugeCard* cpuCard_{nullptr};
    GaugeCard* memoryCard_{nullptr};
    GaugeCard* gpuCard_{nullptr};
    GaugeCard* diskCard_{nullptr};
    TimelineChart* resourceChart_{nullptr};
    FanWidget* overviewFan_{nullptr};
    QLabel* networkValue_{nullptr};
    QLabel* networkDetail_{nullptr};
    QLabel* integratedGpuValue_{nullptr};
    QLabel* integratedGpuDetail_{nullptr};
    QLabel* batteryValue_{nullptr};
    QLabel* batteryDetail_{nullptr};
    QLabel* uptimeValue_{nullptr};
    QVBoxLayout* alertListLayout_{nullptr};
    QString alertRenderSignature_;

    QTableView* processTable_{nullptr};
    ProcessTableModel* processModel_{nullptr};
    int selectedPid_{-1};
    std::uint64_t selectedStartTicks_{0};
    int processScrollPosition_{0};
    QLineEdit* processSearch_{nullptr};
    QLabel* processSummary_{nullptr};

    FanWidget* thermalFan_{nullptr};
    GaugeCard* cpuThermalCard_{nullptr};
    GaugeCard* gpuThermalCard_{nullptr};
    GaugeCard* batteryThermalCard_{nullptr};
    QLabel* fanStatus_{nullptr};
    QLabel* coreTemperatureLabel_{nullptr};
    QPushButton* fanAutomatic_{nullptr};
    QPushButton* fanMaximum_{nullptr};
    QProcess* fanCommand_{nullptr};
    std::optional<FanMode> fanReadback_;
    unsigned int fanCommandSample_{0};
    QString fanFeedback_;
    QVBoxLayout* coreBarsLayout_{nullptr};
    std::vector<QProgressBar*> coreBars_;

    QVBoxLayout* cleanupTargetsLayout_{nullptr};
    QLabel* cleanupSummary_{nullptr};
    QLabel* cleanupLog_{nullptr};
    QPushButton* cleanButton_{nullptr};
    QPushButton* scanButton_{nullptr};
    std::vector<std::pair<std::string, QCheckBox*>> cleanupChecks_;
    std::vector<CleanupTarget> cleanupTargets_;
    bool cleanupFresh_{false};

    QCheckBox* notificationsCheck_{nullptr};
    QCheckBox* autostartCheck_{nullptr};
    QComboBox* intervalCombo_{nullptr};
};

} // namespace vg::ui
