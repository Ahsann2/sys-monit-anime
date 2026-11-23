#include "ui/widgets/TimelineChart.h"
#include "ui/Theme.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace vg::ui {
namespace {

constexpr qint64 historyMs = 120000;
constexpr qreal chartIconWidth = 28;
constexpr qreal chartIconHeight = 22;
const std::array traceColors{QColor("#F7607A"), QColor("#FFC682"), QColor("#C684CD")};

void drawChartIcon(QPainter& painter, const QPointF& position, qreal scale) {
    painter.save();
    painter.translate(position);
    painter.scale(scale, scale);
    painter.setPen(QPen(QColor("#FF97AA"), 1.1, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    constexpr std::array heights{9, 15, 22, 12};
    for (std::size_t index = 0; index < heights.size(); ++index) {
        painter.drawRoundedRect(QRectF(index * 8.0, chartIconHeight - heights[index],
                                       4.0, heights[index]), 1, 1);
    }
    painter.restore();
}

qreal heightScale(int height) {
    return std::clamp(height / 229.0, .63, 1.0);
}

qreal axisWidthScale(int width) {
    return std::clamp(width / 680.0, .64, 1.0);
}

std::optional<double> normalized(std::optional<double> value) {
    return value && std::isfinite(*value)
        ? std::optional<double>{std::clamp(*value, 0.0, 100.0)} : std::nullopt;
}

} // namespace

TimelineChart::TimelineChart(QWidget* parent) : QWidget(parent) {
    setMinimumSize(minimumSizeHint());
    clock_.start();
    animation_.setInterval(33);
    animation_.setTimerType(Qt::PreciseTimer);
    connect(&animation_, &QTimer::timeout, this, [this] { update(plotArea()); });
}

void TimelineChart::append(std::optional<double> cpu, std::optional<double> memory,
                           std::optional<double> gpu) {
    const auto now = clock_.elapsed();
    samples_.push_back({now, {normalized(cpu), normalized(memory), normalized(gpu)}});
    while (samples_.size() > 2 && samples_[1].time < now - historyMs - 16000) {
        samples_.pop_front();
    }
    if (isVisible() && !animation_.isActive()) {
        animation_.start();
    }
    update(plotArea());
}

void TimelineChart::setSampleInterval(int milliseconds) {
    sampleInterval_ = milliseconds;
}

void TimelineChart::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!samples_.empty()) {
        animation_.start();
    }
}

void TimelineChart::hideEvent(QHideEvent* event) {
    animation_.stop();
    QWidget::hideEvent(event);
}

QSize TimelineChart::sizeHint() const {
    return {842, 229};
}

QSize TimelineChart::minimumSizeHint() const {
    return {390, 145};
}

QRect TimelineChart::plotArea() const {
    const auto xScale = axisWidthScale(width());
    const auto yScale = heightScale(height());
    return rect().adjusted(qRound(60 * xScale), qRound(53 * yScale),
                           -qRound(24 * xScale), -qRound(39 * yScale));
}

void TimelineChart::drawSeries(QPainter& painter, const QRectF& plot, qint64 end,
                               std::size_t series, const QColor& color) const {
    QPainterPath line;
    QPainterPath fill;
    std::optional<QPointF> previous;
    qint64 previousTime = 0;
    const auto finishSegment = [&] {
        if (!previous) {
            return;
        }
        if (series == 0) {
            fill.lineTo(previous->x(), plot.bottom());
            fill.closeSubpath();
        }
        previous.reset();
    };

    for (const auto& sample : samples_) {
        if (!sample.values[series]) {
            finishSegment();
            continue;
        }

        if (previous && sample.time - previousTime > std::max(2500, sampleInterval_ * 3)) {
            finishSegment();
        }
        const qreal x = plot.right() +
            static_cast<qreal>(sample.time - end) / historyMs * plot.width();
        const qreal y = plot.bottom() - *sample.values[series] / 100.0 * plot.height();
        const QPointF point(x, y);
        if (previous) {
            const qreal middle = (previous->x() + x) * .5;
            line.cubicTo(QPointF(middle, previous->y()), QPointF(middle, y), point);
            if (series == 0) {
                fill.cubicTo(QPointF(middle, previous->y()), QPointF(middle, y), point);
            }
        } else {
            line.moveTo(point);
            if (series == 0) {
                fill.moveTo(x, plot.bottom());
                fill.lineTo(point);
            }
        }
        previous = point;
        previousTime = sample.time;
    }
    finishSegment();

    if (series == 0) {
        QLinearGradient wash(plot.topLeft(), plot.bottomLeft());
        auto top = color;
        top.setAlpha(64);
        wash.setColorAt(0, top);
        top.setAlpha(8);
        wash.setColorAt(1, top);
        painter.fillPath(fill, wash);
    }

    painter.setBrush(Qt::NoBrush);
    if (series == 0) {
        auto glow = color;
        glow.setAlpha(29);
        painter.setPen(QPen(glow, 3.3, Qt::SolidLine, Qt::RoundCap));
        painter.drawPath(line);
    }
    painter.setPen(QPen(color, series == 0 ? 1.85 : 1.65, Qt::SolidLine, Qt::RoundCap));
    painter.drawPath(line);
}

void TimelineChart::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), theme::background);
    const auto panel = QRectF(rect()).adjusted(.5, .5, -.5, -.5);
    const auto outline = theme::cutFrame(panel);
    painter.fillPath(outline, theme::surfaceGradient(panel));
    painter.save();
    painter.setOpacity(.23);
    painter.fillPath(outline, theme::texture());
    painter.restore();
    QLinearGradient border(panel.topLeft(), panel.bottomRight());
    border.setColorAt(0, QColor("#B45066"));
    border.setColorAt(.52, QColor("#6E3849"));
    border.setColorAt(1, QColor("#46313B"));
    painter.strokePath(outline, QPen(QBrush(border), 1));

    auto plot = QRectF(plotArea());
    if (plot.width() < 1 || plot.height() < 1) {
        return;
    }
    const auto yScale = heightScale(height());
    const auto xScale = axisWidthScale(width());
    const qreal headerScale = std::max(.78, yScale);
    const qreal headerHeight = 27 * headerScale;
    const qreal headerTop = 11 * yScale;
    const qreal headerMidpoint = headerTop + headerHeight * .5;
    const auto title = theme::displayFont(qRound(18 * headerScale));
    painter.setFont(title);
    painter.setPen(theme::text);
    const qreal iconX = 29 * xScale;
    drawChartIcon(painter, QPointF(iconX, headerMidpoint - chartIconHeight * headerScale * .5),
                  headerScale);
    const qreal legendExpansion = std::clamp((width() - 420.0) / 130.0, 0.0, 1.0);
    const qreal legendWidth = 149 + 41 * legendExpansion;
    const qreal titleX = std::max(69 * xScale, iconX + (chartIconWidth + 11) * headerScale);
    const QRectF titleRect(titleX, headerTop,
        std::max(0.0, width() - legendWidth - titleX - 11), headerHeight);
    painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
        painter.fontMetrics().elidedText(QStringLiteral("Beban sistem"),
            Qt::ElideRight, static_cast<int>(titleRect.width())));

    const std::array labels{"CPU", "RAM", "GPU"};
    auto type = font();
    type.setPixelSize(std::max(10, qRound(12 * headerScale)));
    painter.setFont(type);
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const qreal x = width() - legendWidth +
            static_cast<qreal>(index) * (47 + 20 * legendExpansion);
        painter.setPen(Qt::NoPen);
        painter.setBrush(traceColors[index]);
        painter.drawEllipse(QPointF(x, headerMidpoint), 4.5 * headerScale, 4.5 * headerScale);
        painter.setPen(theme::text);
        painter.drawText(QRectF(x + 10, headerTop, 33, headerHeight),
            Qt::AlignLeft | Qt::AlignVCenter, QString::fromLatin1(labels[index]));
    }

    type.setPixelSize(std::max(10, qRound(13 * headerScale)));
    painter.setFont(type);
    QPen grid(QColor("#674654"), 1);
    grid.setDashPattern({1.4, 3.5});
    for (int mark : {0, 25, 50, 75, 100}) {
        const qreal y = plot.bottom() - mark / 100.0 * plot.height();
        painter.setPen(mark == 0 ? QPen(QColor("#C296A6"), 1) : grid);
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.setPen(theme::text);
        painter.drawText(QRectF(5, y - 8, plot.left() - 5 - 12 * xScale, 16),
            Qt::AlignRight | Qt::AlignVCenter, QString::number(mark));
    }

    const auto end = clock_.elapsed() - sampleInterval_ - 150;
    painter.save();
    painter.setClipRect(plot.adjusted(0, -1, 0, 1));
    for (std::size_t index = 0; index < traceColors.size(); ++index) {
        drawSeries(painter, plot, end, index, traceColors[index]);
    }
    painter.restore();

    const QRectF times(plot.left(), plot.bottom() + 9 * yScale, plot.width(), 16);
    painter.setPen(theme::text);
    painter.drawText(times, Qt::AlignLeft | Qt::AlignVCenter, "−2 m");
    painter.drawText(times, Qt::AlignHCenter | Qt::AlignVCenter, "−1 m");
    painter.drawText(times, Qt::AlignRight | Qt::AlignVCenter, "Sekarang");
}

} // namespace vg::ui
