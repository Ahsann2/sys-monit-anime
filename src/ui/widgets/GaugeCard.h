#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include <vector>

class QPainter;

namespace vg::ui {

class GaugeCard final : public QWidget {
public:
    enum class Icon {
        Cpu,
        Memory,
        Gpu,
        Disk,
        Battery,
    };

    explicit GaugeCard(
        QString title,
        QColor accent,
        Icon icon,
        QWidget* parent = nullptr);

    void setReading(
        double progress,
        QString value,
        QString unit,
        QString subtitle);
    void setStatus(QString status);
    void setTitle(QString title);
    void setStale(bool stale);
    void setCompactSizingEnabled(bool enabled);
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void drawIcon(QPainter& painter, const QRectF& bounds) const;
    void drawSparkline(QPainter& painter, const QRectF& bounds) const;

    QString title_;
    QString value_{"--"};
    QString unit_;
    QString subtitle_{"Waiting for sensor"};
    QString status_{"LIVE"};
    QColor accent_;
    Icon icon_;
    double progress_{0.0};
    bool readingAvailable_{false};
    bool stale_{false};
    bool compactSizingEnabled_{false};
    std::vector<double> history_;
};

} // namespace vg::ui
