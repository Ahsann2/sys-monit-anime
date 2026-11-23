#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

#include <array>
#include <deque>
#include <optional>

class QPainter;

namespace vg::ui {

class TimelineChart final : public QWidget {
public:
    explicit TimelineChart(QWidget* parent = nullptr);

    void append(std::optional<double> cpu, std::optional<double> memory,
                std::optional<double> gpu);
    void setSampleInterval(int milliseconds);
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    struct Sample {
        qint64 time;
        std::array<std::optional<double>, 3> values;
    };

    [[nodiscard]] QRect plotArea() const;
    void drawSeries(QPainter& painter, const QRectF& plot, qint64 end,
                    std::size_t series, const QColor& color) const;

    QElapsedTimer clock_;
    QTimer animation_;
    std::deque<Sample> samples_;
    int sampleInterval_{1000};
};

} // namespace vg::ui
