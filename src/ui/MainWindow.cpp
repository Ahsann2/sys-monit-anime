#include "ui/MainWindow.h"

#include "core/ProcessController.h"
#include "ui/Theme.h"
#include "ui/widgets/FanWidget.h"
#include "ui/widgets/GaugeCard.h"
#include "ui/widgets/TimelineChart.h"

#include <QAbstractItemView>
#include <QAbstractTableModel>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QEvent>
#if defined(__linux__)
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#endif
#include <QDialogButtonBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QProgressBar>
#include <QProcess>
#include <QRegularExpression>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QSettings>
#include <QSaveFile>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleOption>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numeric>

namespace vg::ui {
namespace {

#if defined(__linux__)
constexpr bool linuxActions = true;
#else
constexpr bool linuxActions = false;
#endif

QLabel* makeLabel(
    const QString& text,
    const QString& objectName = {}) {
    auto* label = new QLabel(text);
    label->setTextFormat(Qt::PlainText);
    if (!objectName.isEmpty()) {
        label->setObjectName(objectName);
    }
    return label;
}

QLabel* makeValueLabel(const QString& value = "--") {
    auto* label = new QLabel(value);
    QFont font = label->font();
    font.setPixelSize(21);
    font.setWeight(QFont::Bold);
    label->setFont(font);
    return label;
}

QLabel* glyphLabel(const QString& glyph, int size) {
    auto* icon = new QLabel;
    icon->setFixedSize(size, size);
    icon->setPixmap(theme::icon(glyph).pixmap(size, size));
    return icon;
}

QFrame* divider(Qt::Orientation direction = Qt::Horizontal) {
    auto* line = new QFrame;
    line->setObjectName("panelDivider");
    if (direction == Qt::Horizontal) {
        line->setFixedHeight(1);
    } else {
        line->setFixedWidth(1);
    }
    return line;
}

QWidget* panelHeading(const QString& icon, const QString& title) {
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    layout->addWidget(glyphLabel(icon, 27));
    layout->addWidget(makeLabel(title, "sectionTitle"), 1);
    return row;
}

QPixmap softArtwork(const QString& name, int horizontalFade, int verticalFade) {
    const QPixmap source(name);
    if (source.isNull()) {
        return source;
    }
    QPixmap image(source.size());
    image.fill(Qt::transparent);
    QPainter mask(&image);
    mask.drawPixmap(0, 0, source);
    mask.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    const auto fade = [&](int span, int margin, Qt::Orientation direction) {
        if (margin == 0) {
            return;
        }
        QLinearGradient edges(0, 0, direction == Qt::Horizontal ? span : 0,
                                    direction == Qt::Vertical ? span : 0);
        edges.setColorAt(0, Qt::transparent);
        edges.setColorAt(static_cast<double>(margin) / span, Qt::white);
        edges.setColorAt(1.0 - static_cast<double>(margin) / span, Qt::white);
        edges.setColorAt(1, Qt::transparent);
        mask.fillRect(image.rect(), edges);
    };
    fade(image.width(), horizontalFade, Qt::Horizontal);
    fade(image.height(), verticalFade, Qt::Vertical);
    return image;
}

class ArtworkPanel final : public QWidget {
public:
    enum class Artwork { Header, Sidebar, Sensor };
    explicit ArtworkPanel(Artwork artwork) : artwork_(artwork) {}

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        if (artwork_ == Artwork::Sidebar) {
            QStyleOption surface;
            surface.initFrom(this);
            style()->drawPrimitive(QStyle::PE_Widget, &surface, &painter, this);
        }
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto bounds = QRectF(rect()).adjusted(.5, .5, -.5, -.5);
        const auto outline = theme::cutFrame(bounds);
        if (artwork_ != Artwork::Sidebar) {
            painter.fillPath(outline, theme::surfaceGradient(bounds));
            painter.save();
            painter.setClipPath(outline);
        }
        if (artwork_ == Artwork::Header) {
            static const auto flowers = softArtwork(":/reference-header.png", 30, 5);
            const qreal start = width() * .42;
            const qreal span = std::clamp(width() - start - 348, 0.0, 408.0);
            painter.drawPixmap(QRectF(start, 0, span, span * 94 / 408), flowers, flowers.rect());
        } else if (artwork_ == Artwork::Sidebar) {
            static const auto scenery = softArtwork(":/reference-sidebar.png", 0, 16);
            const qreal scale = (width() - 1) / 254.0;
            painter.drawPixmap(QRectF(0, height() - 610 * scale, width() - 1, 420 * scale),
                               scenery, scenery.rect());
        } else {
            static const auto wave = softArtwork(":/reference-status.png", 13, 5);
            const qreal span = width() - 3;
            const qreal waveHeight = span * 89 / 386;
            painter.drawPixmap(QRectF(1, height() - waveHeight - 2, span, waveHeight),
                               wave, wave.rect());
        }
        if (artwork_ != Artwork::Sidebar) {
            painter.restore();
            painter.strokePath(outline, QPen(theme::frameGradient(bounds), 1));
        }
    }

private:
    Artwork artwork_;
};

class PanelCard final : public QFrame {
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto bounds = QRectF(rect()).adjusted(.5, .5, -.5, -.5);
        const auto outline = theme::cutFrame(bounds);
        painter.fillPath(outline, theme::surfaceGradient(bounds));
        painter.strokePath(outline, QPen(theme::frameGradient(bounds), 1));
    }
};

class NavigationButton final : public QPushButton {
public:
    NavigationButton(const QString& text, const QString& icon)
        : QPushButton(text), glyph_(theme::icon(icon)) {}

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        if (isChecked() || underMouse() || hasFocus()) {
            painter.setRenderHint(QPainter::Antialiasing);
            const auto bounds = QRectF(rect()).adjusted(.5, .5, -.5, -.5);
            const auto outline = theme::cutFrame(bounds, 11);
            QLinearGradient surface(bounds.topLeft(), bounds.bottomRight());
            surface.setColorAt(0, isChecked() ? QColor("#733043") : QColor("#392332"));
            surface.setColorAt(1, QColor("#301A27"));
            painter.fillPath(outline, surface);
            painter.strokePath(outline, QPen(hasFocus() ? theme::primary : theme::border));
        }
        glyph_.paint(&painter, QRect(17, (height() - 28) / 2, 28, 28));
        painter.setFont(font());
        painter.setPen(theme::text);
        painter.drawText(QRect(63, 0, width() - 68, height()),
                         Qt::AlignVCenter | Qt::AlignLeft, text().replace("&&", "&"));
    }

private:
    QIcon glyph_;
};

QString processDisplayName(const ProcessInfo& process) {
    if (process.name == "sys-monit-anime" || process.name == "sys-monit-anime.exe") {
        return theme::appName;
    }
    if (!process.name.empty()) {
        return QString::fromStdString(process.name);
    }
    return QStringLiteral("Unknown");
}

QString reading(double value, const QString& unit, int decimals = 0) {
    return std::isfinite(value) && value >= 0.0
        ? QString::number(value, 'f', decimals) + unit
        : QStringLiteral("N/A");
}

QSettings monitorSettings() {
    // Retain existing user preferences across the display-name change.
    return QSettings("VivoGuard Project", "VivoGuard");
}

QString temperature(double value) {
    return value > 0.0 ? reading(value, " °C") : QStringLiteral("N/A");
}

QString fanModeName(FanMode mode) {
    switch (mode) {
    case FanMode::Automatic: return "Otomatis";
    case FanMode::Maximum: return "Maksimum";
    case FanMode::Unknown: return "N/A";
    }
    return "N/A";
}

QString formatBytes(std::uint64_t bytes, int precision = 1) {
    static const std::array<const char*, 5> units{"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < units.size()) {
        value /= 1024.0;
        ++unit;
    }
    return QString("%1 %2").arg(value, 0, 'f', unit == 0 ? 0 : precision).arg(units[unit]);
}

} // namespace

class ProcessTableModel final : public QAbstractTableModel {
public:
    using QAbstractTableModel::QAbstractTableModel;

    int rowCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : static_cast<int>(rows_.size());
    }

    int columnCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : static_cast<int>(headings_.size());
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (orientation == Qt::Horizontal && section == 2 && role == Qt::ToolTipRole) {
            return QStringLiteral("100% CPU setara satu thread logis.");
        }
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole &&
            section >= 0 && section < columnCount()) {
            return QString::fromLatin1(headings_[static_cast<std::size_t>(section)]);
        }
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        const auto* process = processAt(index.row());
        if (!index.isValid() || process == nullptr) {
            return {};
        }
        const int column = index.column();
        if (role == Qt::DisplayRole) {
            switch (column) {
            case 0: return process->pid;
            case 1: return processDisplayName(*process);
            case 2: return reading(process->cpuPercent, "%", 1);
            case 3: return process->residentAvailable ? formatBytes(process->residentBytes) : QStringLiteral("N/A");
            case 4: return process->ioAvailable ? formatBytes(process->readBytes) : QStringLiteral("N/A");
            case 5: return process->ioAvailable ? formatBytes(process->writeBytes) : QStringLiteral("N/A");
            case 6: return !linuxActions ? QStringLiteral("PANTAU SAJA")
                : process->protectedProcess ? QStringLiteral("DILINDUNGI")
                                            : QStringLiteral("APLIKASI USER");
            }
        }
        if (role == Qt::TextAlignmentRole && column != 1) {
            return column == 0 || column == 6
                ? int(Qt::AlignCenter) : int(Qt::AlignRight | Qt::AlignVCenter);
        }
        if (role == Qt::ToolTipRole && column == 1) {
            return QStringLiteral("<qt>%1</qt>")
                .arg(QString::fromStdString(process->command).toHtmlEscaped());
        }
        if (role == Qt::ForegroundRole && column == 6) {
            return process->protectedProcess ? theme::green : theme::muted;
        }
        return {};
    }

    const ProcessInfo* processAt(int row) const {
        return row >= 0 && row < rowCount()
            ? &processes_[rows_[static_cast<std::size_t>(row)]] : nullptr;
    }

    int rowFor(int pid, std::uint64_t startTicks) const {
        for (int row = 0; row < rowCount(); ++row) {
            const auto* process = processAt(row);
            if (process->pid == pid && process->startTicks == startTicks) {
                return row;
            }
        }
        return -1;
    }

    void setProcesses(const std::vector<ProcessInfo>& processes) {
        beginResetModel();
        processes_ = processes;
        rebuildRows();
        endResetModel();
    }

    void setFilter(QString filter) {
        filter = filter.trimmed();
        if (filter_ == filter) {
            return;
        }
        beginResetModel();
        filter_ = std::move(filter);
        rebuildRows();
        endResetModel();
    }

    void sort(int column, Qt::SortOrder order) override {
        if (column < 0 || column >= columnCount() ||
            (sortColumn_ == column && sortOrder_ == order)) {
            return;
        }
        beginResetModel();
        sortColumn_ = column;
        sortOrder_ = order;
        sortRows();
        endResetModel();
    }

private:
    void rebuildRows() {
        rows_.clear();
        rows_.reserve(processes_.size());
        for (std::size_t index = 0; index < processes_.size(); ++index) {
            const auto& process = processes_[index];
            if (filter_.isEmpty() ||
                processDisplayName(process).contains(filter_, Qt::CaseInsensitive) ||
                QString::fromStdString(process.command).contains(filter_, Qt::CaseInsensitive) ||
                QString::number(process.pid).contains(filter_) ||
                QString::fromStdString(process.project).contains(filter_, Qt::CaseInsensitive)) {
                rows_.push_back(index);
            }
        }
        sortRows();
    }

    void sortRows() {
        std::sort(rows_.begin(), rows_.end(), [this](std::size_t first, std::size_t second) {
            if (sortOrder_ == Qt::DescendingOrder) {
                std::swap(first, second);
            }
            const auto& a = processes_[first];
            const auto& b = processes_[second];
            switch (sortColumn_) {
            case 1: if (a.name != b.name) return a.name < b.name; break;
            case 2: if (a.cpuPercent != b.cpuPercent) return a.cpuPercent < b.cpuPercent; break;
            case 3: if (a.residentBytes != b.residentBytes) return a.residentBytes < b.residentBytes; break;
            case 4: if (a.readBytes != b.readBytes) return a.readBytes < b.readBytes; break;
            case 5: if (a.writeBytes != b.writeBytes) return a.writeBytes < b.writeBytes; break;
            case 6: if (a.protectedProcess != b.protectedProcess) return a.protectedProcess < b.protectedProcess; break;
            }
            return a.pid < b.pid;
        });
    }

    static constexpr std::array headings_{"PID", "APLIKASI", "CPU", "MEMORI", "BACA", "TULIS", "KEAMANAN"};
    std::vector<ProcessInfo> processes_;
    std::vector<std::size_t> rows_;
    QString filter_;
    int sortColumn_{0};
    Qt::SortOrder sortOrder_{Qt::DescendingOrder};
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      profile_(HardwareProfile::detect()),
      alertEngine_(profile_) {
    setWindowTitle(theme::appName + " · " + QString::fromStdString(profile_.displayName));
    setWindowIcon(QIcon(":/vivoguard.png"));
    const QSize minimumWindow(1120, 720);
    auto windowSize = QSize(1460, 900);
    if (const auto* screen = QGuiApplication::primaryScreen()) {
        const auto available = screen->availableGeometry().size();
        windowSize = windowSize.boundedTo(available - QSize(40, 40));
    }
    setMinimumSize(minimumWindow);
    resize(windowSize.expandedTo(minimumWindow));

    auto* root = new QWidget;
    root->setObjectName("root");
    auto* shell = new QHBoxLayout(root);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);
    shell->addWidget(buildSidebar());

    auto* content = new QWidget;
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(18, 0, 16, 16);
    contentLayout->setSpacing(12);
    contentLayout->addWidget(buildHeader());

    pages_ = new QStackedWidget;
    pages_->addWidget(buildOverviewPage());
    pages_->addWidget(buildProcessesPage());
    pages_->addWidget(buildThermalsPage());
    pages_->addWidget(buildCleanerPage());
    pages_->addWidget(buildSettingsPage());
    contentLayout->addWidget(pages_, 1);
    shell->addWidget(content, 1);
    setCentralWidget(root);

    auto* fullScreenShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    fullScreenShortcut->setAutoRepeat(false);
    connect(fullScreenShortcut, &QShortcut::activated, this, &MainWindow::toggleFullScreen);
    exitFullScreenShortcut_ = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    exitFullScreenShortcut_->setAutoRepeat(false);
    connect(exitFullScreenShortcut_, &QShortcut::activated, this, &MainWindow::toggleFullScreen);
    updateFullScreenAction();

    setupTray();

    auto settings = monitorSettings();
    const int interval =
        settings.value("monitor/sampleIntervalMs", 1000).toInt();
    sampleTimer_.setInterval(std::clamp(interval, 500, 5000));
    resourceChart_->setSampleInterval(sampleTimer_.interval());
    connect(&sampleTimer_, &QTimer::timeout, this, [this] {
        sampleNow();
    });
    sampleTimer_.start();

    switchPage(0);
    sampleNow();
}

MainWindow::~MainWindow() = default;

void MainWindow::toggleFullScreen() {
    // Preserve the underlying maximized state through the compositor transition.
    setWindowState(windowState() ^ Qt::WindowFullScreen);
}

void MainWindow::updateFullScreenAction() {
    const bool fullScreen = isFullScreen();
    const auto label = fullScreen
        ? QStringLiteral("Keluar layar penuh (Esc atau F11)")
        : QStringLiteral("Layar penuh (F11)");
    fullscreenButton_->setIcon(theme::icon(fullScreen ? "fullscreen-exit" : "fullscreen"));
    fullscreenButton_->setToolTip(label);
    fullscreenButton_->setAccessibleName(label);
    exitFullScreenShortcut_->setEnabled(fullScreen);
}

void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && exitFullScreenShortcut_ != nullptr) {
        updateFullScreenAction();
    }
}

QFrame* MainWindow::createCard(const QString& objectName) const {
    auto* card = new PanelCard;
    card->setObjectName(objectName.isEmpty() ? QStringLiteral("card") : objectName);
    card->setFrameShape(QFrame::NoFrame);
    return card;
}

QPushButton* MainWindow::createNavButton(
    const QString& label,
    const QString& glyph,
    int pageIndex) {
    auto* button = new NavigationButton(label, glyph);
    button->setObjectName("nav");
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    connect(button, &QPushButton::clicked, this,
        [this, pageIndex] {
            switchPage(pageIndex);
        });
    navButtons_.push_back(button);
    return button;
}

QWidget* MainWindow::buildSidebar() {
    auto* sidebar = new ArtworkPanel(ArtworkPanel::Artwork::Sidebar);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(233);

    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(16, 22, 16, 22);
    layout->setSpacing(5);

    auto* brandRow = new QHBoxLayout;
    brandRow->setContentsMargins(3, 0, 0, 0);
    brandRow->setSpacing(12);
    auto* icon = new QLabel;
    icon->setFixedSize(55, 55);
    icon->setPixmap(theme::svgIcon(":/vivoguard.svg").pixmap(55, 55));
    brandRow->addWidget(icon);
    auto* brandText = new QVBoxLayout;
    brandText->setSpacing(0);
    brandText->addWidget(makeLabel(theme::appName, "brand"));
    brandText->addWidget(makeLabel("System Monitor", "muted"));
    brandRow->addLayout(brandText, 1);
    layout->addLayout(brandRow);
    layout->addSpacing(27);

    layout->addWidget(
        createNavButton("Dashboard", "home", 0));
    layout->addWidget(
        createNavButton("Proses", "process", 1));
    layout->addWidget(
        createNavButton("Suhu && Kipas", "fan", 2));
    auto* cleanerNav = createNavButton("Pembersihan", "brush", 3);
    cleanerNav->setVisible(linuxActions);
    layout->addWidget(cleanerNav);
    layout->addWidget(
        createNavButton("Pengaturan", "settings", 4));
    layout->addStretch();

    auto* deviceCard = createCard();
    deviceCard->setMinimumHeight(146);
    auto* deviceLayout = new QVBoxLayout(deviceCard);
    deviceLayout->setContentsMargins(21, 15, 15, 16);
    deviceLayout->setSpacing(8);
    auto* deviceHeading = new QHBoxLayout;
    deviceHeading->setSpacing(16);
    deviceHeading->addWidget(glyphLabel("laptop", 30));
    deviceHeading->addWidget(makeLabel("PERANGKAT", "brandAccent"), 1);
    deviceLayout->addLayout(deviceHeading);
    auto* deviceName = makeLabel(QString::fromStdString(profile_.displayName), "deviceTitle");
    QFont deviceFont = deviceName->font();
    deviceFont.setWeight(QFont::DemiBold);
    deviceName->setFont(deviceFont);
    deviceName->setWordWrap(true);
    deviceLayout->addWidget(deviceName);
    auto* deviceSpecs = makeLabel(
        QString::fromStdString(profile_.expectedCpu + "\n" +
                               profile_.expectedDiscreteGpu), "deviceSpecs");
    deviceSpecs->setWordWrap(true);
    deviceLayout->addWidget(deviceSpecs);
    layout->addWidget(deviceCard);

    return sidebar;
}

QWidget* MainWindow::buildHeader() {
    auto* header = new ArtworkPanel(ArtworkPanel::Artwork::Header);
    header->setObjectName("systemHeader");
    header->setFixedHeight(96);
    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(9, 10, 19, 11);
    layout->setSpacing(14);
    layout->addWidget(glyphLabel("slash", 40));

    auto* heading = new QVBoxLayout;
    heading->setSpacing(1);
    pageTitleLabel_ = makeLabel("Dashboard", "pageTitle");
    headerSubtitle_ = makeLabel(
        QString::fromStdString(profile_.displayName) + " · memulai sensor",
        "pageSubtitle");
    heading->addWidget(pageTitleLabel_);
    heading->addWidget(headerSubtitle_);
    layout->addLayout(heading);
    layout->addStretch();

    healthLabel_ = makeLabel("MENGINISIALISASI", "healthStatus");
    healthLabel_->setAlignment(Qt::AlignCenter);
    healthLabel_->setMinimumWidth(155);
    healthLabel_->setFixedHeight(43);
    layout->addWidget(healthLabel_, 0, Qt::AlignVCenter);
    layout->addSpacing(12);
    auto* separator = divider(Qt::Vertical);
    separator->setFixedHeight(22);
    layout->addWidget(separator, 0, Qt::AlignVCenter);
    layout->addSpacing(10);

    clockLabel_ = makeLabel("--:--", "clock");
    clockLabel_->setMinimumWidth(90);
    clockLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(clockLabel_);
    fullscreenButton_ = new QToolButton;
    fullscreenButton_->setObjectName("fullscreenButton");
    fullscreenButton_->setIconSize(QSize(26, 26));
    fullscreenButton_->setFixedSize(40, 40);
    fullscreenButton_->setCursor(Qt::PointingHandCursor);
    connect(fullscreenButton_, &QToolButton::clicked, this, &MainWindow::toggleFullScreen);
    layout->addWidget(fullscreenButton_, 0, Qt::AlignVCenter);
    return header;
}

QWidget* MainWindow::buildOverviewPage() {
    auto* canvas = new QWidget;
    canvas->setObjectName("dashboard");
    auto* layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* metricsPanel = new QWidget;
    metricsPanel->setObjectName("dashboardMetrics");
    metricsPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* metrics = new QGridLayout(metricsPanel);
    metrics->setContentsMargins(0, 0, 0, 0);
    metrics->setHorizontalSpacing(12);
    metrics->setVerticalSpacing(10);
    cpuCard_ = new GaugeCard(
        QString::fromStdString(profile_.expectedCpu), theme::primary, GaugeCard::Icon::Cpu);
    memoryCard_ = new GaugeCard(
        "MEMORI SISTEM", theme::coral, GaugeCard::Icon::Memory);
    gpuCard_ = new GaugeCard(
        QString::fromStdString(profile_.expectedDiscreteGpu).remove("NVIDIA "),
        theme::pink, GaugeCard::Icon::Gpu);
    diskCard_ = new GaugeCard(
        "PENYIMPANAN SISTEM", theme::amber, GaugeCard::Icon::Disk);
    for (auto* card : {cpuCard_, memoryCard_, gpuCard_, diskCard_}) {
        card->setCompactSizingEnabled(true);
    }
    metrics->addWidget(cpuCard_, 0, 0);
    metrics->addWidget(memoryCard_, 0, 1);
    metrics->addWidget(gpuCard_, 1, 0);
    metrics->addWidget(diskCard_, 1, 1);
    resourceChart_ = new TimelineChart;
    overviewFan_ = new FanWidget(profile_.gl752vwFanControl);
    metrics->addWidget(resourceChart_, 2, 0, 1, 2);
    metrics->addWidget(overviewFan_, 0, 2, 3, 1);
    metrics->setColumnStretch(0, 41);
    metrics->setColumnStretch(1, 42);
    metrics->setColumnStretch(2, 45);
    metrics->setRowStretch(0, 174);
    metrics->setRowStretch(1, 169);
    metrics->setRowStretch(2, 229);
    layout->addWidget(metricsPanel, 1);

    auto* footer = new QWidget;
    footer->setObjectName("dashboardFooter");
    footer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* lower = new QHBoxLayout(footer);
    lower->setContentsMargins(0, 0, 0, 0);
    lower->setSpacing(12);

    auto* telemetry = createCard();
    auto* telemetryLayout = new QGridLayout(telemetry);
    telemetryLayout->setContentsMargins(16, 10, 16, 10);
    telemetryLayout->setHorizontalSpacing(11);
    telemetryLayout->setVerticalSpacing(5);
    telemetryLayout->addWidget(panelHeading("bolt", "Daya & perangkat"), 0, 0, 1, 3);
    telemetryLayout->addWidget(divider(), 1, 0, 1, 3);
    telemetryLayout->addWidget(divider(Qt::Vertical), 2, 1, 3, 1);
    telemetryLayout->setColumnStretch(0, 1);
    telemetryLayout->setColumnStretch(2, 1);
    const auto addMetric = [&](int row, int column, const QString& icon, const QString& label,
                               QLabel*& value, QLabel** detail) {
        auto* item = new QHBoxLayout;
        item->setSpacing(8);
        item->addWidget(glyphLabel(icon, 23), 0, Qt::AlignTop);
        auto* box = new QVBoxLayout;
        box->setSpacing(0);
        box->addWidget(makeLabel(label, "brandAccent"));
        value = makeLabel("—", "telemetryValue");
        box->addWidget(value);
        if (detail != nullptr) {
            *detail = makeLabel("—", "telemetryDetail");
            box->addWidget(*detail);
        }
        box->addStretch();
        item->addLayout(box, 1);
        telemetryLayout->addLayout(item, row, column);
    };
    addMetric(2, 0, "network", "JARINGAN", networkValue_, &networkDetail_);
    networkValue_->setObjectName("telemetryRate");
    addMetric(2, 2, "battery", "BATERAI", batteryValue_, &batteryDetail_);
    telemetryLayout->addWidget(divider(), 3, 0);
    telemetryLayout->addWidget(divider(), 3, 2);
    addMetric(4, 0, "cpu", "iGPU", integratedGpuValue_, &integratedGpuDetail_);
    addMetric(4, 2, "clock", "WAKTU AKTIF", uptimeValue_, nullptr);
    telemetryLayout->setRowStretch(5, 1);
    lower->addWidget(telemetry, 55);

    auto* alertCard = new ArtworkPanel(ArtworkPanel::Artwork::Sensor);
    alertCard->setObjectName("card");
    alertListLayout_ = new QVBoxLayout(alertCard);
    alertListLayout_->setContentsMargins(16, 10, 16, 10);
    alertListLayout_->setSpacing(7);
    alertListLayout_->addWidget(panelHeading("activity", "Status sensor"));
    alertListLayout_->addWidget(divider());
    alertListLayout_->addWidget(makeLabel(
        "Mengumpulkan data sensor pertama…", "muted"));
    alertListLayout_->addStretch();
    lower->addWidget(alertCard, 39);

    auto* quick = createCard();
    auto* quickLayout = new QVBoxLayout(quick);
    quickLayout->setContentsMargins(16, 10, 16, 10);
    quickLayout->setSpacing(8);
    quickLayout->addWidget(panelHeading("layers", "TINDAKAN CEPAT"));
    quickLayout->addWidget(divider());
    auto* freeMemory = new QPushButton(linuxActions ? "Pilih proses RAM" : "Lihat proses");
    freeMemory->setObjectName("primary");
    freeMemory->setProperty("dashboardAction", true);
    freeMemory->setIcon(theme::icon("rocket"));
    freeMemory->setIconSize(QSize(28, 28));
    connect(freeMemory, &QPushButton::clicked, this, [this] {
        if (linuxActions) {
            freeMemoryNow();
        } else {
            switchPage(1);
        }
    });
    quickLayout->addWidget(freeMemory);
    auto* openCleaner = new QPushButton(linuxActions ? "Pindai cache" : "Suhu & kipas");
    openCleaner->setObjectName("secondary");
    openCleaner->setProperty("dashboardAction", true);
    openCleaner->setIcon(theme::icon(linuxActions ? "brush" : "fan"));
    openCleaner->setIconSize(QSize(28, 28));
    connect(openCleaner, &QPushButton::clicked,
        this, [this] { switchPage(linuxActions ? 3 : 2); });
    quickLayout->addWidget(openCleaner);
    quickLayout->addStretch();
    lower->addWidget(quick, 34);

    layout->addWidget(footer);
    return canvas;
}

QWidget* MainWindow::buildProcessesPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* toolbar = new QHBoxLayout;
    processSearch_ = new QLineEdit;
    processSearch_->setPlaceholderText("Cari proses, PID, atau perintah…");
    processSearch_->setClearButtonEnabled(true);
    toolbar->addWidget(processSearch_, 1);

    auto* selectMemory = new QPushButton("Pilih aplikasi / proyek");
    selectMemory->setVisible(linuxActions);
    selectMemory->setObjectName("secondary");
    connect(selectMemory, &QPushButton::clicked,
        this, [this] { freeMemoryNow(); });
    toolbar->addWidget(selectMemory);

    auto* terminate = new QPushButton("Tutup Proses");
    terminate->setVisible(linuxActions);
    terminate->setObjectName("secondary");
    connect(terminate, &QPushButton::clicked,
        this, [this] { terminateSelectedProcess(false); });
    toolbar->addWidget(terminate);

    auto* force = new QPushButton("Hentikan Paksa");
    force->setVisible(linuxActions);
    force->setObjectName("danger");
    connect(force, &QPushButton::clicked,
        this, [this] { terminateSelectedProcess(true); });
    toolbar->addWidget(force);

    processSummary_ = makeLabel("Menunggu pemindaian proses", "muted");
    toolbar->addWidget(processSummary_);
    layout->addLayout(toolbar);

    processTable_ = new QTableView;
    processModel_ = new ProcessTableModel(processTable_);
    processTable_->setModel(processModel_);
    processTable_->verticalHeader()->setVisible(false);
    processTable_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    processTable_->verticalHeader()->setDefaultSectionSize(39);
    processTable_->setAlternatingRowColors(true);
    processTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    processTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    processTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    processTable_->setShowGrid(false);
    processTable_->setWordWrap(false);
    auto* header = processTable_->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    const std::array widths{75, 180, 75, 96, 96, 96, 132};
    for (int column = 0; column < processModel_->columnCount(); ++column) {
        if (column != 1) {
            processTable_->setColumnWidth(column, widths[static_cast<std::size_t>(column)]);
        }
    }
    header->setSortIndicator(0, Qt::DescendingOrder);
    processTable_->setSortingEnabled(true);
    connect(processSearch_, &QLineEdit::textChanged,
        processModel_, &ProcessTableModel::setFilter);
    connect(processModel_, &QAbstractItemModel::modelAboutToBeReset, this, [this] {
        const auto* process = selectedProcess();
        selectedPid_ = process ? process->pid : -1;
        selectedStartTicks_ = process ? process->startTicks : 0;
        processScrollPosition_ = processTable_->verticalScrollBar()->value();
    });
    connect(processModel_, &QAbstractItemModel::modelReset, this, [this] {
        const int row = selectedPid_ < 0 ? -1
            : processModel_->rowFor(selectedPid_, selectedStartTicks_);
        if (row >= 0) {
            processTable_->selectRow(row);
        }
        processTable_->verticalScrollBar()->setValue(processScrollPosition_);
    });
    layout->addWidget(processTable_, 1);
    return page;
}

QWidget* MainWindow::buildThermalsPage() {
    auto* canvas = new QWidget;
    auto* layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    auto* top = new QHBoxLayout;
    top->setSpacing(14);
    thermalFan_ = new FanWidget(profile_.gl752vwFanControl);
    thermalFan_->setMaximumHeight(380);
    top->addWidget(thermalFan_, 2);

    auto* thermalCards = new QGridLayout;
    thermalCards->setSpacing(13);
    cpuThermalCard_ = new GaugeCard(
        "SUHU CPU", theme::primary, GaugeCard::Icon::Cpu);
    gpuThermalCard_ = new GaugeCard(
        "SUHU GPU DISKRIT", theme::pink, GaugeCard::Icon::Gpu);
    batteryThermalCard_ = new GaugeCard(
        "KESEHATAN BATERAI", theme::amber, GaugeCard::Icon::Battery);
    thermalCards->addWidget(cpuThermalCard_, 0, 0);
    thermalCards->addWidget(gpuThermalCard_, 0, 1);
    thermalCards->addWidget(batteryThermalCard_, 1, 0, 1, 2);
    top->addLayout(thermalCards, 5);
    layout->addLayout(top);

    auto* fanCard = createCard();
    auto* fanLayout = new QHBoxLayout(fanCard);
    fanLayout->setContentsMargins(20, 12, 20, 12);
    fanStatus_ = makeLabel("Menunggu data kipas", "muted");
    fanStatus_->setWordWrap(true);
    fanLayout->addWidget(fanStatus_, 1);
    fanAutomatic_ = new QPushButton("Otomatis");
    fanMaximum_ = new QPushButton("Maksimum");
    fanAutomatic_->setObjectName("secondary");
    fanMaximum_->setObjectName("secondary");
    connect(fanAutomatic_, &QPushButton::clicked,
        this, [this] { setCpuFanMode(FanMode::Automatic); });
    connect(fanMaximum_, &QPushButton::clicked,
        this, [this] { setCpuFanMode(FanMode::Maximum); });
    fanLayout->addWidget(fanAutomatic_);
    fanLayout->addWidget(fanMaximum_);
    fanAutomatic_->setVisible(profile_.gl752vwFanControl);
    fanMaximum_->setVisible(profile_.gl752vwFanControl);
    layout->addWidget(fanCard);

    coreTemperatureLabel_ = makeLabel("", "muted");
    coreTemperatureLabel_->setWordWrap(true);
    layout->addWidget(coreTemperatureLabel_);

    auto* coreCard = createCard();
    coreBarsLayout_ = new QVBoxLayout(coreCard);
    coreBarsLayout_->setContentsMargins(20, 16, 20, 18);
    coreBarsLayout_->setSpacing(8);
    coreBarsLayout_->addWidget(
        makeLabel("BEBAN CPU PER THREAD", "sectionTitle"));
    coreBarsLayout_->addWidget(makeLabel(
        "Utilisasi scheduler per thread, diperbarui setiap detik.",
        "muted"));
    layout->addWidget(coreCard, 1);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(canvas);
    return scroll;
}

QWidget* MainWindow::buildCleanerPage() {
    auto* page = new QWidget;
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    auto* scannerCard = createCard();
    auto* scannerLayout = new QVBoxLayout(scannerCard);
    scannerLayout->setContentsMargins(20, 18, 20, 18);
    scannerLayout->setSpacing(11);
    scannerLayout->addWidget(
        makeLabel("INVENTARIS PEMBERSIHAN AMAN", "sectionTitle"));
    scannerLayout->addWidget(makeLabel(
        "Hanya lokasi cache yang dikenal yang dipindai. Berkas pribadi, profil "
        "browser, kata sandi, dan proyek tetap terlindungi.",
        "muted"));

    auto* targetContainer = new QWidget;
    cleanupTargetsLayout_ = new QVBoxLayout(targetContainer);
    cleanupTargetsLayout_->setContentsMargins(0, 4, 0, 4);
    cleanupTargetsLayout_->setSpacing(8);
    cleanupTargetsLayout_->addWidget(makeLabel(
        "Tekan Pindai untuk menghitung ruang yang dapat dibebaskan.", "muted"));
    cleanupTargetsLayout_->addStretch();

    auto* targetScroll = new QScrollArea;
    targetScroll->setWidgetResizable(true);
    targetScroll->setFrameShape(QFrame::NoFrame);
    targetScroll->setWidget(targetContainer);
    scannerLayout->addWidget(targetScroll, 1);

    cleanupSummary_ = makeValueLabel("Belum dipindai");
    scannerLayout->addWidget(cleanupSummary_);
    cleanupLog_ = makeLabel(
        "Tidak ada berkas yang dihapus sebelum Anda mengonfirmasi.", "muted");
    cleanupLog_->setWordWrap(true);
    scannerLayout->addWidget(cleanupLog_);

    auto* scannerButtons = new QHBoxLayout;
    scanButton_ = new QPushButton("Pindai cache");
    scanButton_->setObjectName("secondary");
    connect(scanButton_, &QPushButton::clicked,
        this, [this] { scanCleanupTargets(); });
    scannerButtons->addWidget(scanButton_);
    cleanButton_ = new QPushButton("Smart Clean");
    cleanButton_->setObjectName("primary");
    cleanButton_->setEnabled(false);
    connect(cleanButton_, &QPushButton::clicked,
        this, [this] { executeCleanup(); });
    scannerButtons->addWidget(cleanButton_);
    scannerLayout->addLayout(scannerButtons);
    layout->addWidget(scannerCard, 3);

    auto* rescueColumn = new QVBoxLayout;
    rescueColumn->setSpacing(14);

    auto makeActionCard = [this](
                              const QString& title,
                              const QString& detail,
                              const QString& buttonText,
                              const QString& buttonStyle,
                              auto action) {
        auto* card = createCard();
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(18, 16, 18, 16);
        cardLayout->setSpacing(8);
        cardLayout->addWidget(makeLabel(title, "sectionTitle"));
        auto* description = makeLabel(detail, "muted");
        description->setWordWrap(true);
        cardLayout->addWidget(description);
        cardLayout->addStretch();
        auto* button = new QPushButton(buttonText);
        button->setObjectName(buttonStyle);
        connect(button, &QPushButton::clicked, this, action);
        cardLayout->addWidget(button);
        return card;
    };

    rescueColumn->addWidget(makeActionCard(
        "Bebaskan RAM",
        "Pilih aplikasi atau proyek yang akan menerima SIGTERM.",
        "Pilih proses",
        "primary",
        [this] { freeMemoryNow(); }));
    rescueColumn->addWidget(makeActionCard(
        "Tutup aplikasi desktop",
        "Kirim SIGTERM ke proses aplikasi yang terdeteksi setelah konfirmasi.",
        "Tutup Aplikasi",
        "secondary",
        [this] { closeAllApplications(); }));
    rescueColumn->addWidget(makeActionCard(
        "Keluar dari sesi",
        "Simpan pekerjaan sebelum kembali ke layar login.",
        "Keluar dari sesi",
        "danger",
        [this] { freshSession(); }));
    layout->addLayout(rescueColumn, 2);
    return page;
}

QWidget* MainWindow::buildSettingsPage() {
    auto* canvas = new QWidget;
    auto* layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    auto* profileCard = createCard();
    auto* profileLayout = new QGridLayout(profileCard);
    profileLayout->setContentsMargins(22, 20, 22, 20);
    profileLayout->setHorizontalSpacing(30);
    profileLayout->setVerticalSpacing(10);
    profileLayout->addWidget(
        makeLabel("PROFIL PERANGKAT KERAS", "sectionTitle"), 0, 0, 1, 2);
    profileLayout->addWidget(makeLabel("PERANGKAT", "brandAccent"), 1, 0);
    profileLayout->addWidget(makeLabel(
        QString::fromStdString(profile_.displayName)), 1, 1);
    profileLayout->addWidget(makeLabel("CPU", "brandAccent"), 2, 0);
    profileLayout->addWidget(makeLabel(
        QString::fromStdString(profile_.expectedCpu)), 2, 1);
    profileLayout->addWidget(makeLabel("GPU DISKRIT", "brandAccent"), 3, 0);
    profileLayout->addWidget(makeLabel(
        QString::fromStdString(profile_.expectedDiscreteGpu)), 3, 1);
    profileLayout->addWidget(makeLabel("TARGET MEMORI", "brandAccent"), 4, 0);
    profileLayout->addWidget(makeLabel(profile_.expectedRamBytes > 0
        ? formatBytes(profile_.expectedRamBytes, 0) : QStringLiteral("N/A")), 4, 1);
    profileLayout->addWidget(makeLabel("GPU TERINTEGRASI", "brandAccent"), 5, 0);
    profileLayout->addWidget(makeLabel(
        QString::fromStdString(profile_.expectedIntegratedGpu)), 5, 1);
    profileLayout->addWidget(makeLabel("ADAPTOR GRAFIS", "brandAccent"), 6, 0);
    graphicsValue_ = makeLabel("Memulai deteksi");
    graphicsValue_->setWordWrap(true);
    profileLayout->addWidget(graphicsValue_, 6, 1);
    layout->addWidget(profileCard);

    auto* behaviorCard = createCard();
    auto* behaviorLayout = new QGridLayout(behaviorCard);
    behaviorLayout->setContentsMargins(22, 20, 22, 20);
    behaviorLayout->setHorizontalSpacing(26);
    behaviorLayout->setVerticalSpacing(14);
    behaviorLayout->addWidget(
        makeLabel("PERILAKU MONITORING", "sectionTitle"), 0, 0, 1, 2);

    notificationsCheck_ = new QCheckBox(
        "Notifikasi desktop untuk peringatan dan bahaya");
    auto settings = monitorSettings();
    notificationsCheck_->setChecked(
        settings.value("monitor/notifications", true).toBool());
    connect(notificationsCheck_, &QCheckBox::toggled,
        this, [](bool enabled) {
            monitorSettings().setValue("monitor/notifications", enabled);
        });
    behaviorLayout->addWidget(notificationsCheck_, 1, 0, 1, 2);

    autostartCheck_ = new QCheckBox(
        "Jalankan Monitor Sistem secara otomatis saat login");
    autostartCheck_->setVisible(linuxActions);
    autostartCheck_->setChecked(
        QFileInfo::exists(
            QStandardPaths::writableLocation(
                QStandardPaths::ConfigLocation) +
            "/autostart/sys-monit-anime.desktop"));
    connect(autostartCheck_, &QCheckBox::toggled,
        this, [this](bool enabled) { setAutostart(enabled); });
    behaviorLayout->addWidget(autostartCheck_, 2, 0, 1, 2);

    behaviorLayout->addWidget(
        makeLabel("INTERVAL PEMINDAIAN", "brandAccent"), 3, 0);
    intervalCombo_ = new QComboBox;
    intervalCombo_->addItem("0,5 detik", 500);
    intervalCombo_->addItem("1 detik · direkomendasikan", 1000);
    intervalCombo_->addItem("2 detik", 2000);
    intervalCombo_->addItem("5 detik · konsumsi daya sangat rendah", 5000);
    const int storedInterval =
        settings.value("monitor/sampleIntervalMs", 1000).toInt();
    const int intervalIndex =
        intervalCombo_->findData(storedInterval);
    intervalCombo_->setCurrentIndex(
        intervalIndex >= 0 ? intervalIndex : 1);
    connect(intervalCombo_,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
            const int milliseconds = intervalCombo_->itemData(index).toInt();
            if (milliseconds <= 0) {
                return;
            }
            sampleTimer_.setInterval(milliseconds);
            resourceChart_->setSampleInterval(milliseconds);
            monitorSettings().setValue(
                "monitor/sampleIntervalMs", milliseconds);
        });
    behaviorLayout->addWidget(intervalCombo_, 3, 1);
    layout->addWidget(behaviorCard);

    auto* safetyCard = createCard();
    auto* safetyLayout = new QVBoxLayout(safetyCard);
    safetyLayout->setContentsMargins(22, 20, 22, 20);
    safetyLayout->setSpacing(8);
    safetyLayout->addWidget(
        makeLabel("Izin dan konfirmasi", "sectionTitle"));
    auto* safety = makeLabel(
        linuxActions
            ? "Perubahan mode kipas meminta autentikasi admin. Penghapusan cache, "
              "penghentian proses, dan keluar dari sesi meminta konfirmasi."
            : "Sensor yang tidak tersedia pada sistem operasi ini ditampilkan sebagai N/A.",
        "muted");
    safety->setWordWrap(true);
    safetyLayout->addWidget(safety);
    layout->addWidget(safetyCard);
    layout->addStretch();

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(canvas);
    return scroll;
}

void MainWindow::setupTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    tray_ = new QSystemTrayIcon(QIcon(":/vivoguard.png"), this);
    tray_->setToolTip(theme::appName + " · Memulai sensor");

    auto* menu = new QMenu(this);
    auto* open = menu->addAction("Buka Monitor Sistem");
    connect(open, &QAction::triggered, this, [this] {
        showNormal();
        raise();
        activateWindow();
    });
    auto* cleaner = menu->addAction("Pembersih Pintar");
    cleaner->setVisible(linuxActions);
    connect(cleaner, &QAction::triggered, this, [this] {
        switchPage(3);
        showNormal();
        raise();
        activateWindow();
    });
    menu->addSeparator();
    auto* quit = menu->addAction("Keluar");
    connect(quit, &QAction::triggered, this, [this] {
        allowQuit_ = true;
        QApplication::quit();
    });

    tray_->setContextMenu(menu);
    connect(tray_, &QSystemTrayIcon::activated, this,
        [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger ||
                reason == QSystemTrayIcon::DoubleClick) {
                setVisible(!isVisible());
                if (isVisible()) {
                    showNormal();
                    raise();
                    activateWindow();
                }
            }
        });
    tray_->show();
}

void MainWindow::sampleNow() {
    clockLabel_->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
    if (telemetry_.busy()) {
        return;
    }
    ++sampleCounter_;
    const bool includeProcesses =
        snapshot_.processes.empty() || sampleCounter_ % 2 == 0;
    telemetry_.submit(includeProcesses ? Work::SampleProcesses : Work::Sample, {},
        [this, includeProcesses](WorkResult result) {
            acceptSample(std::move(result), includeProcesses);
        });
}

void MainWindow::acceptSample(WorkResult result, bool includeProcesses) {
    if (!result.error.isEmpty()) {
        if (!samplingFailed_) {
            qWarning() << "Pengambilan sampel tertunda:" << result.error;
            samplingFailed_ = true;
            alertEngine_.resetContinuity();
            notifiedAlertIds_.clear();
            resourceChart_->append(std::nullopt, std::nullopt, std::nullopt);
        }
        updateHeader();
        updateThermals();
        updateAlerts();
        return;
    }
    samplingFailed_ = false;
    auto next = std::move(result.snapshot);
    if (!includeProcesses) {
        next.processes = std::move(snapshot_.processes);
    }
    snapshot_ = std::move(next);
    if (fanReadback_ && fanCommand_ == nullptr && sampleCounter_ != fanCommandSample_) {
        fanFeedback_ = snapshot_.fans.cpuMode == *fanReadback_
            ? QString() : QStringLiteral("Mode yang diminta belum terbaca dari driver.");
        fanReadback_.reset();
    }
    alerts_ = alertEngine_.evaluate(snapshot_);

    updateHeader();
    updateOverview();
    if (includeProcesses && pages_->currentIndex() == 1 && isVisible()) {
        updateProcesses();
    }
    updateThermals();
    updateAlerts();
    notifyNewAlerts();
}

void MainWindow::updateHeader() {
    const Severity severity = samplingFailed_ ? Severity::Warning
        : AlertEngine::overallSeverity(alerts_);
    const QString state = samplingFailed_ ? QStringLiteral("Sensor tertunda")
                                          : severityName(severity);
    healthLabel_->setText(state.toUpper());
    const QString color = severityColor(severity);
    for (auto* card : {cpuCard_, memoryCard_, gpuCard_, diskCard_,
                       cpuThermalCard_, gpuThermalCard_, batteryThermalCard_}) {
        card->setStale(samplingFailed_);
    }
    const auto statusStyle = QStringLiteral("color: %1;").arg(color);
    if (healthLabel_->styleSheet() != statusStyle) {
        healthLabel_->setStyleSheet(statusStyle);
    }

    headerSubtitle_->setText(samplingFailed_
        ? QStringLiteral("Sensor tertunda · menampilkan bacaan terakhir")
        :
        QString("%1 · %2 %3 · %4 thread")
            .arg(QString::fromStdString(profile_.displayName),
                 QString::fromStdString(snapshot_.systemName),
                 QString::fromStdString(snapshot_.kernel))
            .arg(snapshot_.cpu.logicalCores));
    QStringList detected;
    for (const auto& name : snapshot_.graphicsAdapters) {
        detected.append(QString::fromStdString(name));
    }
    graphicsValue_->setText(detected.isEmpty() ? QStringLiteral("N/A") : detected.join('\n'));

    if (tray_ != nullptr) {
        tray_->setToolTip(
            QString("Monitor Sistem · CPU %1% · RAM %2% · %3")
                .arg(snapshot_.cpu.usagePercent, 0, 'f', 0)
                .arg(snapshot_.memory.usedPercent(), 0, 'f', 0)
                .arg(state));
    }
}

void MainWindow::updateOverview() {
    const auto& cpu = snapshot_.cpu;
    cpuCard_->setReading(
        cpu.usagePercent,
        cpu.usageAvailable ? reading(cpu.usagePercent, "") : QStringLiteral("N/A"),
        "% UTILISASI",
        cpu.averageClockGhz > 0
            ? QString("%1 · %2 · beban %3")
                  .arg(reading(cpu.averageClockGhz, " GHz", 2), temperature(cpu.temperatureC))
                  .arg(cpu.load1, 0, 'f', 2)
            : QString("Frekuensi N/A · %1").arg(temperature(cpu.temperatureC)));

    const auto& memory = snapshot_.memory;
    const QString swap = memory.swapAvailable
        ? formatBytes(memory.swapUsedBytes) + "/" + formatBytes(memory.swapTotalBytes)
        : QStringLiteral("N/A");
    const QString pressure = memory.pressure.available
        ? reading(memory.pressure.someAvg10, "%", 1) : QStringLiteral("N/A");
    memoryCard_->setReading(
        memory.usedPercent(),
        memory.totalBytes > 0 ? formatBytes(memory.usedBytes) : QStringLiteral("N/A"),
        memory.totalBytes > 0 ? QString("DARI %1").arg(formatBytes(memory.totalBytes)) : QString(),
        QString("%1 tersedia · swap %2 · PSI %3")
            .arg(memory.totalBytes > 0 ? formatBytes(memory.availableBytes) : QStringLiteral("N/A"),
                 swap, pressure));

    const auto& gpu = snapshot_.discreteGpu.available ? snapshot_.discreteGpu : snapshot_.integratedGpu;
    const auto gpuClientScope = QStringLiteral("Beban GPU dari aplikasi yang dapat dipantau oleh akun ini.");
    const auto gpuTooltip = gpu.utilizationFromClients ? gpuClientScope : QString();
    gpuCard_->setToolTip(gpuTooltip);
    resourceChart_->setToolTip(gpuTooltip);
    if (gpu.available) {
        auto name = QString::fromStdString(gpu.name);
        if (name.startsWith("NVIDIA GeForce")) {
            name.remove(0, 7);
        }
        gpuCard_->setTitle(std::move(name));
    }
    if (gpu.sleeping) {
        gpuCard_->setStatus("RTD3 SLEEP");
        gpuCard_->setReading(0.0, "OFF", "HEMAT DAYA", "GPU sedang dalam mode hemat daya");
    } else if (!gpu.available) {
        gpuCard_->setStatus("N/A");
        gpuCard_->setReading(0.0, "N/A", "GPU", "Data GPU tidak tersedia");
    } else {
        const auto vram = gpu.utilizationAvailable && gpu.memoryTotalBytes > 0
            ? QString("%1 / %2 VRAM").arg(formatBytes(gpu.memoryUsedBytes), formatBytes(gpu.memoryTotalBytes))
            : gpu.memoryTotalBytes > 0 ? formatBytes(gpu.memoryTotalBytes) + " VRAM"
                                      : QStringLiteral("VRAM N/A");
        gpuCard_->setStatus(gpu.utilizationAvailable ? "LIVE" : "TERDETEKSI");
        gpuCard_->setReading(
            gpu.usagePercent,
            gpu.utilizationAvailable ? reading(gpu.usagePercent, "") : QStringLiteral("N/A"),
            gpu.utilizationFromClients ? "% BEBAN APLIKASI" : "% BEBAN GPU",
            vram + " · " + temperature(gpu.temperatureC));
    }

    const auto& disk = snapshot_.disk;
    diskCard_->setReading(
        disk.usedPercent(),
        disk.totalBytes > 0 ? formatBytes(disk.usedBytes) : QStringLiteral("N/A"),
        disk.totalBytes > 0 ? QString("DARI %1").arg(formatBytes(disk.totalBytes)) : QString(),
        disk.ratesAvailable
            ? QString("B %1 · T %2").arg(formatRate(disk.readBytesPerSecond),
                                         formatRate(disk.writeBytesPerSecond))
            : QStringLiteral("Laju baca dan tulis N/A"));

    resourceChart_->append(
        cpu.usageAvailable ? std::optional<double>{cpu.usagePercent} : std::nullopt,
        memory.totalBytes > 0 ? std::optional<double>{memory.usedPercent()} : std::nullopt,
        gpu.utilizationAvailable && !gpu.sleeping
            ? std::optional<double>{gpu.usagePercent} : std::nullopt);
    overviewFan_->setData(
        snapshot_.fans,
        snapshot_.cpu.temperatureC,
        gpu.temperatureC);

    const auto& network = snapshot_.network;
    networkValue_->setText(network.available
        ? "↓ " + formatRate(network.receiveBytesPerSecond, 0) : QStringLiteral("N/A"));
    networkValue_->setToolTip(network.available
        ? "Unduh " + formatRate(network.receiveBytesPerSecond) : QString());
    networkDetail_->setText(network.available
        ? "↑ " + formatRate(network.transmitBytesPerSecond) : QString());
    const auto& igpu = snapshot_.integratedGpu;
    const auto igpuClock = reading(igpu.graphicsClockMhz, " MHz");
    const auto igpuLoad = reading(igpu.usagePercent, "%", igpu.utilizationFromClients ? 1 : 0);
    const bool showClock = igpu.utilizationFromClients && igpu.graphicsClockMhz >= 0;
    integratedGpuValue_->setText(!igpu.available ? QStringLiteral("N/A")
        : igpu.utilizationAvailable && !showClock ? igpuLoad : igpuClock);
    integratedGpuDetail_->setText(showClock ? QStringLiteral("Aplikasi %1").arg(igpuLoad)
        : igpu.utilizationFromClients ? QStringLiteral("Beban aplikasi")
        : igpu.utilizationAvailable ? temperature(igpu.temperatureC)
        : igpu.available ? QStringLiteral("Beban tak terbaca") : QString());
    QString igpuTooltip = igpu.available ? QString::fromStdString(igpu.name) : QString();
    if (igpu.graphicsClockMhz >= 0) {
        igpuTooltip += "\n" + igpuClock;
    }
    if (igpu.utilizationFromClients) {
        igpuTooltip += "\n" + gpuClientScope;
    }
    integratedGpuValue_->setToolTip(igpuTooltip);
    integratedGpuDetail_->setToolTip(igpuTooltip);
    const auto& ac = snapshot_.battery.acOnline;
    const QString acText = !ac ? QStringLiteral("AC N/A")
        : *ac ? QStringLiteral("AC") : QStringLiteral("Tanpa AC");
    if (snapshot_.battery.present) {
        const QString batteryStatusText =
            snapshot_.battery.status == "Charging" ? QStringLiteral("Mengisi") :
            snapshot_.battery.status == "Discharging" ? QStringLiteral("Pakai baterai") :
            snapshot_.battery.status == "Full" ? QStringLiteral("Penuh") :
            snapshot_.battery.status == "Not charging" ? QStringLiteral("Tidak mengisi") :
            QStringLiteral("Status N/A");
        batteryValue_->setText(reading(snapshot_.battery.percent, "%"));
        batteryDetail_->setText(snapshot_.battery.status == "Discharging" && (!ac || !*ac)
            ? batteryStatusText : acText + " · " + batteryStatusText);
    } else {
        batteryValue_->setText("N/A");
        batteryDetail_->setText(acText);
    }
    uptimeValue_->setText(formatUptime(snapshot_.uptime));
}

void MainWindow::updateProcesses() {
    processModel_->setProcesses(snapshot_.processes);
    const std::uint64_t userMemory = std::accumulate(
        snapshot_.processes.begin(), snapshot_.processes.end(), 0ULL,
        [](std::uint64_t total, const ProcessInfo& process) {
            return total + process.residentBytes;
        });
    processSummary_->setText(
        QString("%1 proses · %2 RSS")
            .arg(snapshot_.processes.size())
            .arg(formatBytes(userMemory)));
}

void MainWindow::updateThermals() {
    const auto& gpu = snapshot_.discreteGpu;
    const auto& fans = snapshot_.fans;
    thermalFan_->setData(
        fans, snapshot_.cpu.temperatureC, gpu.temperatureC);

    if (fanCommand_ == nullptr) {
        const auto rpmText = [](const std::optional<int>& rpm) {
            return rpm ? QString::number(*rpm) + " RPM" : QStringLiteral("RPM tak terbaca");
        };
        QString status = QString("Kipas CPU %1 · %2\nKanal kipas GPU %3")
            .arg(fanModeName(fans.cpuMode), rpmText(fans.cpuRpm), rpmText(fans.gpuRpm));
        if (samplingFailed_) {
            status += "\nSensor tertunda · kontrol kipas dijeda.";
        } else if (fanReadback_) {
            status += "\nMemeriksa mode yang dilaporkan driver…";
        } else if (!fanFeedback_.isEmpty()) {
            status += "\n" + fanFeedback_;
        }
        fanStatus_->setText(status);
    }
    const bool canChangeFan = !samplingFailed_ && !fanReadback_ && fanCommand_ == nullptr &&
        !fans.controlPath.empty() && QFileInfo("/usr/bin/pkexec").isExecutable() &&
        QFileInfo("/usr/bin/tee").isExecutable();
    fanAutomatic_->setEnabled(canChangeFan && fans.cpuMode != FanMode::Automatic);
    fanMaximum_->setEnabled(canChangeFan && fans.cpuMode != FanMode::Maximum);

    QStringList coreTemperatures;
    for (const double value : snapshot_.cpu.coreTemperaturesC) {
        coreTemperatures << QString("Core %1 · %2")
            .arg(coreTemperatures.size()).arg(temperature(value));
    }
    if (snapshot_.cpu.packageThrottleCount) {
        coreTemperatures << QString("Throttling CPU sejak boot %1")
            .arg(*snapshot_.cpu.packageThrottleCount);
    }
    coreTemperatureLabel_->setText(coreTemperatures.join("     "));
    coreTemperatureLabel_->setVisible(!coreTemperatures.isEmpty());

    cpuThermalCard_->setReading(
        std::clamp(snapshot_.cpu.temperatureC, 0.0, 100.0),
        snapshot_.cpu.temperatureC > 0 ? QString::number(snapshot_.cpu.temperatureC, 'f', 0) : "N/A",
        "°C · CPU PACKAGE",
        QString("Peringatan %1 °C  ·  Kritis %2 °C")
            .arg(profile_.cpuWarningC, 0, 'f', 0)
            .arg(profile_.cpuCriticalC, 0, 'f', 0));
    gpuThermalCard_->setReading(
        gpu.sleeping ? 0.0 :
            std::clamp(gpu.temperatureC, 0.0, 100.0),
        gpu.sleeping ? "OFF" : gpu.temperatureC > 0
            ? QString::number(gpu.temperatureC, 'f', 0) : "N/A",
        gpu.sleeping ? "RTD3 SLEEP" : "°C · GPU",
        gpu.sleeping
            ? "GPU diskrit dalam hemat daya penuh"
            : QString("%1  ·  kipas %2  ·  %3")
                  .arg(reading(gpu.powerWatts, " W", 1),
                       reading(gpu.fanPercent, "%"),
                       reading(gpu.graphicsClockMhz, " MHz")));

    if (snapshot_.battery.present) {
        const auto& battery = snapshot_.battery;
        batteryThermalCard_->setReading(
            battery.healthPercent,
            reading(battery.healthPercent, ""),
            "% ESTIMASI KESEHATAN",
            QString("%1 · %2 · %3")
                .arg(reading(battery.powerWatts, " W", 1),
                     reading(battery.voltageVolts, " V", 2),
                     reading(battery.cycleCount, " siklus")));
    } else {
        batteryThermalCard_->setReading(
            0.0, "N/A", "SENSOR BATERAI",
            "Antarmuka baterai tidak terdeteksi");
    }

    const int coreCount =
        static_cast<int>(snapshot_.cpu.perCorePercent.size());
    if (static_cast<int>(coreBars_.size()) != coreCount) {
        while (coreBarsLayout_->count() > 2) {
            QLayoutItem* item = coreBarsLayout_->takeAt(2);
            if (item->widget() != nullptr) {
                item->widget()->deleteLater();
            }
            delete item;
        }
        coreBars_.clear();
        for (int core = 0; core < coreCount; ++core) {
            auto* row = new QWidget;
            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(10);
            auto* label = makeLabel(
                QString("THREAD %1").arg(core + 1, 2, 10, QChar('0')),
                "brandAccent");
            label->setFixedWidth(78);
            rowLayout->addWidget(label);
            auto* bar = new QProgressBar;
            bar->setRange(0, 100);
            bar->setTextVisible(false);
            rowLayout->addWidget(bar, 1);
            auto* value = makeLabel("0%");
            value->setObjectName("coreValue");
            value->setFixedWidth(44);
            value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            rowLayout->addWidget(value);
            bar->setProperty("valueLabel",
                QVariant::fromValue(static_cast<QObject*>(value)));
            coreBarsLayout_->addWidget(row);
            coreBars_.push_back(bar);
        }
        coreBarsLayout_->addStretch();
    }

    for (int core = 0; core < coreCount; ++core) {
        const int value = qRound(
            snapshot_.cpu.perCorePercent[static_cast<std::size_t>(core)]);
        coreBars_[static_cast<std::size_t>(core)]->setValue(std::max(0, value));
        QObject* labelObject =
            coreBars_[static_cast<std::size_t>(core)]
                ->property("valueLabel").value<QObject*>();
        if (auto* label = qobject_cast<QLabel*>(labelObject)) {
            label->setText(reading(value, "%"));
        }
    }
}

void MainWindow::setCpuFanMode(FanMode mode) {
    const auto& fans = snapshot_.fans;
    if (!profile_.gl752vwFanControl || samplingFailed_ || fanReadback_ || fanCommand_ != nullptr ||
        fans.controlPath.empty() || fans.cpuMode == mode ||
        mode == FanMode::Unknown) {
        return;
    }

    const QString path = QString::fromStdString(fans.controlPath);
    static const QRegularExpression fanAttribute(
        "^/sys/devices/platform/asus-nb-wmi/hwmon/hwmon[0-9]+/pwm1_enable$");
    if (!fanAttribute.match(path).hasMatch()) {
        return;
    }
    const QByteArray value = mode == FanMode::Maximum ? "0\n" : "2\n";
    auto* command = new QProcess(this);
    fanFeedback_.clear();
    fanCommand_ = command;
    fanStatus_->setText("Menunggu autentikasi admin untuk mengubah mode kipas…");
    updateThermals();
    connect(command, &QProcess::started, this, [command, value] {
        command->write(value);
        command->closeWriteChannel();
    });
    connect(command, &QProcess::errorOccurred, this,
        [this, command](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart) {
                return;
            }
            fanCommand_ = nullptr;
            updateThermals();
            QMessageBox::warning(this, "Mode kipas", command->errorString());
            command->deleteLater();
        });
    connect(command, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this, [this, command, mode](int code, QProcess::ExitStatus status) {
            fanCommand_ = nullptr;
            command->deleteLater();
            if (status == QProcess::NormalExit && code == 0) {
                fanReadback_ = mode;
                fanCommandSample_ = sampleCounter_;
            } else {
                fanFeedback_ = "Permintaan mode gagal atau autentikasi dibatalkan.";
            }
            updateThermals();
            sampleNow();
        });
    QTimer::singleShot(90000, command, [command] {
        if (command->state() != QProcess::NotRunning) {
            command->kill();
        }
    });
    command->start("/usr/bin/pkexec", {"--disable-internal-agent",
                                      "/usr/bin/tee", path});
}

void MainWindow::updateAlerts() {
    if (alertListLayout_ == nullptr) {
        return;
    }
    const auto pressure = [](const PressureStats& stats) {
        return stats.available ? reading(stats.someAvg10, "%", 1) : QStringLiteral("N/A");
    };
    const auto pressures = QString("Memori PSI %1 · I/O PSI %2").arg(
        pressure(snapshot_.memory.pressure), pressure(snapshot_.disk.pressure));
    const auto visible = samplingFailed_ ? 0U : std::min<std::size_t>(3, alerts_.size());
    QString signature = samplingFailed_ ? QStringLiteral("stale")
        : alerts_.empty() ? pressures : QString{};
    for (std::size_t index = 0; index < visible; ++index) {
        const auto& alert = alerts_[index];
        signature += severityColor(alert.severity) + QChar('\n')
            + QString::fromStdString(alert.title) + QChar('\n')
            + QString::fromStdString(alert.detail) + QChar('\n');
    }
    if (alertRenderSignature_ == signature) {
        return;
    }
    alertRenderSignature_ = std::move(signature);
    while (QLayoutItem* item = alertListLayout_->takeAt(0)) {
        if (item->widget() != nullptr) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    alertListLayout_->addWidget(panelHeading("activity", "Status sensor"));
    alertListLayout_->addWidget(divider());

    if (samplingFailed_) {
        auto* pending = makeLabel("Sensor tertunda. Status belum bisa diperbarui.", "muted");
        pending->setWordWrap(true);
        alertListLayout_->addWidget(pending);
    } else if (alerts_.empty()) {
        auto* healthy = makeLabel("Sensor dalam batas normal");
        healthy->setWordWrap(true);
        healthy->setStyleSheet("color:#FFF6F9;font-family:'Oxanium';"
                               "font-size:17px;font-weight:600;padding:4px 0;");
        alertListLayout_->addWidget(healthy);
        auto* detail = makeLabel(pressures, "muted");
        detail->setWordWrap(true);
        alertListLayout_->addWidget(detail);
    } else {
        for (std::size_t index = 0; index < visible; ++index) {
            const Alert& alert = alerts_[index];
            auto* title = makeLabel(
                QString("●  %1")
                    .arg(QString::fromStdString(alert.title)));
            title->setStyleSheet(
                QString("color:%1;font-weight:700;")
                    .arg(severityColor(alert.severity)));
            title->setWordWrap(true);
            title->setToolTip(QStringLiteral("<qt>%1</qt>")
                .arg(QString::fromStdString(alert.detail).toHtmlEscaped()));
            alertListLayout_->addWidget(title);
        }
    }
    alertListLayout_->addStretch();
}

void MainWindow::notifyNewAlerts() {
    QSet<QString> active;
    for (const auto& alert : alerts_) {
        const QString id = QString::fromStdString(alert.id);
        active.insert(id);
        if (notifiedAlertIds_.contains(id) ||
            alert.severity == Severity::Info ||
            alert.severity == Severity::Normal) {
            continue;
        }
        const bool notificationsEnabled =
            notificationsCheck_ == nullptr ||
            notificationsCheck_->isChecked();
        if (notificationsEnabled && tray_ != nullptr) {
            tray_->showMessage(
                QString::fromStdString(alert.title),
                QString("%1\n%2")
                    .arg(QString::fromStdString(alert.detail))
                    .arg(QString::fromStdString(alert.recommendation)),
                alert.severity == Severity::Critical
                    ? QSystemTrayIcon::Critical
                    : QSystemTrayIcon::Warning,
                9000);
        }
    }
    notifiedAlertIds_ = active;
}

void MainWindow::scanCleanupTargets() {
    if (maintenance_.busy()) {
        return;
    }
    cleanupLog_->setText("Memindai lokasi cache yang dikenal…");
    cleanupFresh_ = false;
    scanButton_->setEnabled(false);
    cleanButton_->setEnabled(false);
    maintenance_.submit(Work::ScanCache, {}, [this](WorkResult result) {
        scanButton_->setEnabled(true);
        if (!result.error.isEmpty()) {
            cleanupLog_->setText("Pemindaian gagal. " + result.error);
            return;
        }
        renderCleanupTargets(std::move(result.targets));
    });
}

void MainWindow::renderCleanupTargets(std::vector<CleanupTarget> targets) {
    cleanupTargets_ = std::move(targets);
    cleanupFresh_ = true;

    while (QLayoutItem* item = cleanupTargetsLayout_->takeAt(0)) {
        if (item->widget() != nullptr) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    cleanupChecks_.clear();

    std::uint64_t selectedBytes = 0;
    for (const auto& target : cleanupTargets_) {
        auto* row = createCard();
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(14, 11, 14, 11);
        rowLayout->setSpacing(12);

        auto* check = new QCheckBox;
        check->setChecked(
            target.selectedByDefault && target.sizeBytes > 0);
        check->setEnabled(target.sizeBytes > 0);
        rowLayout->addWidget(check);

        auto* text = new QVBoxLayout;
        auto* title = makeLabel(
            QString::fromStdString(target.title));
        QFont titleFont = title->font();
        titleFont.setWeight(QFont::DemiBold);
        title->setFont(titleFont);
        text->addWidget(title);
        auto* detail = makeLabel(
            QString::fromStdString(target.description), "muted");
        detail->setWordWrap(true);
        text->addWidget(detail);
        rowLayout->addLayout(text, 1);

        auto* size = makeValueLabel(formatBytes(target.sizeBytes));
        size->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rowLayout->addWidget(size);
        cleanupTargetsLayout_->addWidget(row);
        cleanupChecks_.emplace_back(target.id, check);
        if (check->isChecked()) {
            selectedBytes += target.sizeBytes;
        }
        connect(check, &QCheckBox::toggled, this, [this] {
            std::uint64_t bytes = 0;
            for (std::size_t index = 0;
                 index < cleanupChecks_.size(); ++index) {
                if (cleanupChecks_[index].second->isChecked()) {
                    bytes += cleanupTargets_[index].sizeBytes;
                }
            }
            cleanupSummary_->setText(
                QString("%1 dipilih").arg(formatBytes(bytes)));
            cleanButton_->setEnabled(bytes > 0 && cleanupFresh_ && !maintenance_.busy());
        });
    }
    cleanupTargetsLayout_->addStretch();
    cleanupSummary_->setText(
        QString("%1 dipilih").arg(formatBytes(selectedBytes)));
    cleanupLog_->setText(
        QString("%1 lokasi aman telah dipindai.")
            .arg(cleanupTargets_.size()));
    cleanButton_->setEnabled(selectedBytes > 0);
}

void MainWindow::executeCleanup() {
    if (!cleanupFresh_ || maintenance_.busy()) {
        return;
    }
    QStringList selected;
    QStringList names;
    std::uint64_t bytes = 0;
    for (std::size_t index = 0;
         index < cleanupChecks_.size(); ++index) {
        if (!cleanupChecks_[index].second->isChecked()) {
            continue;
        }
        selected.push_back(QString::fromStdString(cleanupChecks_[index].first));
        names.push_back(QString::fromStdString(cleanupTargets_[index].title));
        bytes += cleanupTargets_[index].sizeBytes;
    }
    if (selected.empty()) {
        return;
    }

    const auto answer = QMessageBox::question(
        this,
        "Konfirmasi hapus berkas",
        QString("Hapus permanen berkas yang memenuhi kriteria pada kategori berikut?\n\n"
                "%1\n\nPerkiraan ukuran berkas %2.")
            .arg(names.join('\n'), formatBytes(bytes)),
        QMessageBox::Cancel | QMessageBox::Yes,
        QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) {
        return;
    }

    if (maintenance_.busy()) {
        return;
    }
    cleanupFresh_ = false;
    scanButton_->setEnabled(false);
    cleanButton_->setEnabled(false);
    cleanupLog_->setText("Membersihkan cache yang dipilih…");
    maintenance_.submit(Work::CleanCache, selected, [this](WorkResult work) {
        scanButton_->setEnabled(true);
        if (!work.error.isEmpty()) {
            cleanupLog_->setText("Pembersihan terhenti. Sebagian berkas mungkin sudah terhapus. "
                                 "Pindai ulang sebelum melanjutkan. " + work.error);
            return;
        }
        const auto& result = work.cleanup;
        cleanupLog_->setText(
            QString("%2 berkas dihapus (%1) · %3 gagal. Pindai ulang untuk memperbarui daftar.")
                .arg(formatBytes(result.bytesFreed))
                .arg(result.filesRemoved)
                .arg(result.errors.size()));
    });
}

void MainWindow::freeMemoryNow() {
    auto candidates = ProcessController::memoryRescueCandidates(snapshot_.processes);
    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
        [](const ProcessInfo& process) { return !process.project.empty(); }),
        candidates.end());
    if (candidates.size() > 20) {
        candidates.resize(20);
    }
    for (const auto& process : snapshot_.processes) {
        if (!process.protectedProcess && !process.project.empty()) {
            candidates.push_back(process);
        }
    }
    if (candidates.empty()) {
        QMessageBox::information(
            this,
            "Penyelamatan Memori",
            "Tidak ada kandidat aplikasi besar atau proses proyek yang aktif.");
        return;
    }

    std::vector<QTreeWidgetItem*> rows;
    QDialog dialog(this);
    dialog.setWindowTitle("Pilih proses untuk membebaskan RAM");
    dialog.resize(670, 480);
    auto* layout = new QVBoxLayout(&dialog);
    auto* description = makeLabel(
        "Centang hanya aplikasi atau proyek yang boleh dihentikan. "
        "Simpan pekerjaan sebelum mengirim SIGTERM. RSS mencakup memori bersama.",
        "muted");
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* tree = new QTreeWidget;
    tree->setAlternatingRowColors(true);
    tree->setHeaderLabels({"Aplikasi atau proyek", "PID", "RAM (RSS)"});
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    layout->addWidget(tree);
    std::map<std::string, QTreeWidgetItem*> projects;
    for (const auto& process : candidates) {
        QTreeWidgetItem* row = nullptr;
        if (process.project.empty()) {
            row = new QTreeWidgetItem(tree);
        } else {
            auto*& group = projects[process.project];
            if (group == nullptr) {
                group = new QTreeWidgetItem(tree);
                group->setText(0, QFileInfo(QString::fromStdString(process.project)).fileName());
                group->setFlags(group->flags() | Qt::ItemIsAutoTristate);
                group->setCheckState(0, Qt::Unchecked);
            }
            const auto total = group->data(2, Qt::UserRole).toULongLong()
                + process.residentBytes;
            group->setData(2, Qt::UserRole, static_cast<qulonglong>(total));
            group->setText(2, formatBytes(total));
            row = new QTreeWidgetItem(group);
        }
        row->setText(0, processDisplayName(process));
        row->setText(1, QString::number(process.pid));
        row->setText(2, formatBytes(process.residentBytes));
        row->setToolTip(0, QString::fromStdString(process.command));
        row->setCheckState(0, Qt::Unchecked);
        rows.push_back(row);
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Cancel)->setText("Batal");
    buttons->button(QDialogButtonBox::Cancel)->setObjectName("secondary");
    auto* stop = buttons->addButton("Hentikan pilihan", QDialogButtonBox::AcceptRole);
    stop->setObjectName("primary");
    stop->setEnabled(false);
    connect(tree, &QTreeWidget::itemChanged, &dialog, [stop, &rows] {
        stop->setEnabled(std::any_of(rows.begin(), rows.end(),
            [](const auto* row) { return row->checkState(0) == Qt::Checked; }));
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    std::vector<const ProcessInfo*> selected;
    for (std::size_t index = 0; index < rows.size(); ++index) {
        if (rows[index]->checkState(0) == Qt::Checked) {
            selected.push_back(&candidates[index]);
        }
    }
    std::sort(selected.begin(), selected.end(), [](const auto* a, const auto* b) {
        return a->startTicks < b->startTicks;
    });
    ProcessActionResult result;
    for (const auto* process : selected) {
        std::string error;
        if (ProcessController::terminate(*process, false, error)) {
            ++result.succeeded;
        } else {
            ++result.skipped;
            result.errors.push_back(process->name + ": " + error);
        }
    }
    QTimer::singleShot(900, this, [this, result] {
        sampleNow();
        QMessageBox::information(
            this, "Penyelamatan Memori",
            QString("SIGTERM dikirim ke %1 proses. %2 dilewati.\n"
                    "Pemakaian RAM akan diperbarui di dashboard.")
                .arg(result.succeeded).arg(result.skipped));
    });
}

void MainWindow::closeAllApplications() {
    const auto candidates =
        ProcessController::applicationCandidates(snapshot_.processes);
    if (candidates.empty()) {
        QMessageBox::information(
            this,
            "Tutup Aplikasi",
            "Tidak ditemukan scope aplikasi desktop yang dapat ditutup.");
        return;
    }
    const std::uint64_t resident = std::accumulate(
        candidates.begin(), candidates.end(), 0ULL,
        [](std::uint64_t total, const ProcessInfo& process) {
            return total + process.residentBytes;
        });
    const auto answer = QMessageBox::warning(
        this,
        "Tutup Semua Aplikasi",
        QString("Kirim SIGTERM ke %1 proses aplikasi dengan total RSS %2? "
                "Simpan pekerjaan terlebih dahulu. Aplikasi bisa berhenti tanpa dialog simpan.")
            .arg(candidates.size())
            .arg(formatBytes(resident)),
        QMessageBox::Cancel | QMessageBox::Yes,
        QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) {
        return;
    }

    const ProcessActionResult result = ProcessController::closeApplications(candidates);
    QMessageBox::information(this, "Tutup Aplikasi",
        QString("SIGTERM dikirim ke %1 proses. %2 dilewati.")
            .arg(result.succeeded).arg(result.skipped));
}

void MainWindow::freshSession() {
#if defined(__linux__)
    const auto confirmed = QMessageBox::critical(
        this,
        "Reset Sesi Grafis",
        "Tindakan ini akan mengakhiri seluruh sesi grafis Ubuntu dan kembali ke "
        "layar login.\n\nSetiap aplikasi desktop, terminal, "
        "browser, dan tugas latar belakang dalam sesi ini akan ditutup. Simpan semua "
        "pekerjaan sebelum melanjutkan.",
        QMessageBox::Cancel | QMessageBox::Yes,
        QMessageBox::Cancel);
    if (confirmed != QMessageBox::Yes) {
        return;
    }
    auto message = QDBusMessage::createMethodCall(
        "org.gnome.SessionManager", "/org/gnome/SessionManager",
        "org.gnome.SessionManager", "Logout");
    message.setArguments({QVariant::fromValue(1U)});
    auto* request = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(message, 5000), this);
    connect(request, &QDBusPendingCallWatcher::finished, this,
        [this, request] {
            const auto error = request->error();
            request->deleteLater();
            if (error.type() == QDBusError::UnknownMethod ||
                error.type() == QDBusError::ServiceUnknown ||
                error.type() == QDBusError::Disconnected) {
                forceSessionEnd();
            } else if (error.isValid()) {
                QMessageBox::warning(this, "Keluar dari sesi",
                    "Status permintaan keluar belum terkonfirmasi. Periksa dialog sesi Ubuntu.");
            }
        });
#endif
}

void MainWindow::forceSessionEnd() {
#if defined(__linux__)
    const QString sessionId =
        QString::fromUtf8(qgetenv("XDG_SESSION_ID"));
    if (sessionId.isEmpty()) {
        QMessageBox::critical(
            this,
            "Reset Sesi Gagal",
            "GNOME Session Manager tidak tersedia dan XDG_SESSION_ID tidak "
            "terset. Gunakan menu sistem Ubuntu untuk keluar dengan aman.");
        return;
    }

    const auto forceConfirmed = QMessageBox::critical(
        this,
        "Penghentian Sesi Paksa",
        "GNOME tidak merespons permintaan logout normal. Hentikan paksa "
        "sesi login ini melalui systemd-logind?\n\nAplikasi tidak dapat "
        "menampilkan dialog simpan setelah tahap ini.",
        QMessageBox::Cancel | QMessageBox::Yes,
        QMessageBox::Cancel);
    if (forceConfirmed != QMessageBox::Yes) {
        return;
    }

    auto message = QDBusMessage::createMethodCall(
        "org.freedesktop.login1",
        "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager",
        "TerminateSession");
    message.setArguments({sessionId});
    auto* request = new QDBusPendingCallWatcher(
        QDBusConnection::systemBus().asyncCall(message, 5000), this);
    connect(request, &QDBusPendingCallWatcher::finished, this,
        [this, request] {
            const auto reply = request->reply();
            request->deleteLater();
            if (reply.type() == QDBusMessage::ErrorMessage) {
                QMessageBox::critical(this, "Reset Sesi Gagal", reply.errorMessage());
            }
        });
#endif
}

void MainWindow::terminateSelectedProcess(bool force) {
    const ProcessInfo* process = selectedProcess();
    if (process == nullptr) {
        QMessageBox::information(
            this,
            "Kontrol Proses",
            "Pilih baris proses terlebih dahulu.");
        return;
    }
    if (process->protectedProcess) {
        QMessageBox::warning(
            this,
            "Proses Dilindungi",
            "Proses sistem atau desktop ini dilindungi.");
        return;
    }

    const ProcessInfo selected = *process;
    const auto answer = QMessageBox::warning(
        this,
        force ? "Hentikan Paksa Proses" : "Tutup Proses",
        QString("%1 %2 (PID %3)?\n\n%4")
            .arg(force ? "Hentikan paksa" : "Kirim SIGTERM ke")
            .arg(processDisplayName(selected))
            .arg(selected.pid)
            .arg(force
                    ? "SIGKILL tidak memberi kesempatan aplikasi menyimpan data."
                    : "Simpan pekerjaan. Aplikasi bisa berhenti tanpa dialog simpan."),
        QMessageBox::Cancel | QMessageBox::Yes,
        QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) {
        return;
    }
    std::string error;
    if (!ProcessController::terminate(selected, force, error)) {
        QMessageBox::critical(
            this,
            "Gagal Mengontrol Proses",
            QString::fromStdString(error));
    }
}

void MainWindow::switchPage(int index) {
    if (pages_ == nullptr || index < 0 ||
        index >= pages_->count() || (!linuxActions && index == 3)) {
        return;
    }
    pages_->setCurrentIndex(index);
    for (int button = 0;
         button < static_cast<int>(navButtons_.size()); ++button) {
        navButtons_[static_cast<std::size_t>(button)]
            ->setChecked(button == index);
    }
    static const std::array titles{
        QStringLiteral("Dashboard"),
        linuxActions ? QStringLiteral("Kontrol Proses") : QStringLiteral("Daftar Proses"),
        QStringLiteral("Suhu & Kipas"),
        QStringLiteral("Pembersihan"),
        QStringLiteral("Pengaturan"),
    };
    pageTitleLabel_->setText(titles[static_cast<std::size_t>(index)]);
    if (index == 1) {
        updateProcesses();
    }
    if (index == 3 && cleanupTargets_.empty()) {
        scanCleanupTargets();
    }
}

void MainWindow::setAutostart(bool enabled) {
    const QString directory =
        QStandardPaths::writableLocation(
            QStandardPaths::ConfigLocation) +
        "/autostart";
    const QString path = directory + "/sys-monit-anime.desktop";
    if (!enabled) {
        if (!QFile::remove(path) && QFileInfo::exists(path)) {
            QMessageBox::warning(
                this,
                "Autostart",
                "Tidak dapat menghapus entri autostart.");
            const QSignalBlocker blocker(autostartCheck_);
            autostartCheck_->setChecked(true);
        }
        return;
    }

    QSaveFile file(path);
    const QString executable =
        QCoreApplication::applicationFilePath();
    const QByteArray desktop = QString(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Monitor Sistem\n"
        "Comment=Guardian sistem lokal\n"
        "Exec=\"%1\" --tray\n"
        "Icon=sys-monit-anime\n"
        "Terminal=false\n"
        "Categories=System;Monitor;\n"
        "X-GNOME-Autostart-enabled=true\n")
        .arg(executable)
        .toUtf8();
    if (!QDir().mkpath(directory) || !file.open(QIODevice::WriteOnly) ||
        file.write(desktop) != desktop.size() || !file.commit()) {
        QMessageBox::warning(this, "Autostart",
            "Tidak dapat menyimpan entri autostart.");
        const QSignalBlocker blocker(autostartCheck_);
        autostartCheck_->setChecked(QFileInfo::exists(path));
    }
}

const ProcessInfo* MainWindow::selectedProcess() const {
    const auto selected = processTable_->selectionModel()->selectedRows();
    return selected.empty() ? nullptr : processModel_->processAt(selected.first().row());
}

QString MainWindow::formatRate(double bytesPerSecond, int precision) {
    if (!std::isfinite(bytesPerSecond) || bytesPerSecond < 0.0) {
        return "0 B/s";
    }
    return formatBytes(
        static_cast<std::uint64_t>(bytesPerSecond), precision) + "/s";
}

QString MainWindow::formatUptime(
    std::chrono::seconds uptime) {
    const auto hours =
        std::chrono::duration_cast<std::chrono::hours>(uptime);
    const auto days = hours.count() / 24;
    const auto remainingHours = hours.count() % 24;
    if (days > 0) {
        return QString("%1h %2j").arg(days).arg(remainingHours);
    }
    const auto minutes =
        std::chrono::duration_cast<std::chrono::minutes>(
            uptime - hours);
    return QString("%1j %2m")
        .arg(hours.count())
        .arg(minutes.count());
}

QString MainWindow::severityName(Severity severity) {
    switch (severity) {
    case Severity::Info:
        return "Perhatian";
    case Severity::Warning:
        return "Peringatan";
    case Severity::Critical:
        return "Kritis";
    case Severity::Normal:
    default:
        return "Normal";
    }
}

QString MainWindow::severityColor(Severity severity) {
    switch (severity) {
    case Severity::Info:
        return theme::primary.name();
    case Severity::Warning:
        return theme::amber.name();
    case Severity::Critical:
        return theme::red.name();
    case Severity::Normal:
    default:
        return theme::text.name();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!allowQuit_ && tray_ != nullptr &&
        QSystemTrayIcon::isSystemTrayAvailable()) {
        hide();
        event->ignore();
        tray_->showMessage(
            "Monitor Sistem tetap berjalan",
            "Pemantauan berlanjut di sistem tray.",
            QSystemTrayIcon::Information,
            3500);
        return;
    }
    event->accept();
}

} // namespace vg::ui
