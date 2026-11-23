#include "ui/widgets/GaugeCard.h"

#include "ui/Theme.h"

#include <QConicalGradient>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>
#include <utility>

namespace vg::ui {
namespace {

QColor withAlpha(QColor color, int alpha) {
    color.setAlpha(alpha);
    return color;
}

QString displayedStatus(const QString& status, bool stale) {
    if (stale) {
        return QStringLiteral("TERTUNDA");
    }
    if (status == "NVML LIVE") {
        return QStringLiteral("LIVE");
    }
    return status;
}

QColor gaugeAccent(GaugeCard::Icon icon, const QColor& fallback) {
    if (theme::isDefaultPreset()) {
        return fallback;
    }
    const auto colors = theme::seriesColors();
    switch (icon) {
    case GaugeCard::Icon::Cpu: return colors[0];
    case GaugeCard::Icon::Memory: return colors[1];
    case GaugeCard::Icon::Gpu: return colors[2];
    case GaugeCard::Icon::Disk:
    case GaugeCard::Icon::Battery: return colors[0].lighter(130);
    }
    return fallback;
}

} // namespace

GaugeCard::GaugeCard(
    QString title,
    QColor accent,
    Icon icon,
    QWidget* parent)
    : QWidget(parent),
      title_(std::move(title)),
      accent_(std::move(accent)),
      icon_(icon) {
    setMinimumHeight(151);
    setToolTip(title_);
}

void GaugeCard::setReading(
    double progress,
    QString value,
    QString unit,
    QString subtitle) {
    readingAvailable_ =
        std::isfinite(progress) && value != "N/A" && value != "OFF" && value != "--";
    progress_ = readingAvailable_ ? std::clamp(progress, 0.0, 100.0) : 0;
    value_ = std::move(value);
    unit_ = std::move(unit);
    subtitle_ = std::move(subtitle);
    if (compactSizingEnabled_) {
        const auto details = QStringLiteral("<qt>%1<br>%2</qt>")
                                 .arg(title_.toHtmlEscaped(), subtitle_.toHtmlEscaped());
        if (toolTip() != details) {
            setToolTip(details);
        }
    }
    if (readingAvailable_) {
        history_.push_back(progress_);
        if (history_.size() > 54) {
            history_.erase(history_.begin());
        }
    } else {
        history_.clear();
    }
    update();
}

void GaugeCard::setStatus(QString status) {
    if (status_ == status) {
        return;
    }
    status_ = std::move(status);
    update();
}

void GaugeCard::setTitle(QString title) {
    if (title_ != title) {
        title_ = std::move(title);
        update();
    }
}

void GaugeCard::setStale(bool stale) {
    if (stale_ != stale) {
        stale_ = stale;
        if (stale) {
            history_.clear();
        }
        update();
    }
}

void GaugeCard::setCompactSizingEnabled(bool enabled) {
    if (compactSizingEnabled_ != enabled) {
        compactSizingEnabled_ = enabled;
        setMinimumHeight(minimumSizeHint().height());
        if (!enabled) {
            setToolTip(title_);
        }
        updateGeometry();
        update();
    }
}

QSize GaugeCard::sizeHint() const {
    return {411, 174};
}

QSize GaugeCard::minimumSizeHint() const {
    return compactSizingEnabled_ ? QSize(208, 107) : QSize(210, 151);
}

void GaugeCard::drawIcon(QPainter& painter, const QRectF& bounds) const {
    painter.save();
    painter.translate(bounds.topLeft());
    painter.scale(bounds.width() / 26.0, bounds.height() / 26.0);
    const qreal lineWidth = std::clamp(bounds.width() * .04, .95, 1.2);
    painter.setPen(QPen(gaugeAccent(icon_, accent_).lighter(126), lineWidth * 26.0 / bounds.width(),
                       Qt::SolidLine, Qt::RoundCap,
                       Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    switch (icon_) {
    case Icon::Cpu:
        painter.drawRoundedRect(QRectF(6, 6, 14, 14), 2.5, 2.5);
        painter.drawRoundedRect(QRectF(10, 10, 6, 6), 1, 1);
        for (const int pin : {8, 13, 18}) {
            painter.drawLine(QPointF(pin, 2), QPointF(pin, 6));
            painter.drawLine(QPointF(pin, 20), QPointF(pin, 24));
            painter.drawLine(QPointF(2, pin), QPointF(6, pin));
            painter.drawLine(QPointF(20, pin), QPointF(24, pin));
        }
        break;
    case Icon::Memory:
        painter.drawRoundedRect(QRectF(1, 5, 24, 14), 2.5, 2.5);
        for (const int pin : {6, 11, 16, 21}) {
            painter.drawLine(QPointF(pin, 19), QPointF(pin, 22));
        }
        for (const int chip : {6, 12, 18}) {
            painter.drawRoundedRect(QRectF(chip, 10, 2, 4), .5, .5);
        }
        break;
    case Icon::Gpu:
        painter.drawRoundedRect(QRectF(1, 6, 24, 14), 2.5, 2.5);
        painter.drawEllipse(QRectF(6, 9, 7, 7));
        painter.drawEllipse(QRectF(16, 10, 5, 5));
        painter.drawLine(QPointF(3, 3), QPointF(3, 6));
        painter.drawLine(QPointF(6, 20), QPointF(6, 23));
        painter.drawLine(QPointF(14, 20), QPointF(14, 22));
        break;
    case Icon::Disk: {
        QPainterPath caseOutline;
        caseOutline.moveTo(7, 3);
        caseOutline.lineTo(19, 3);
        caseOutline.quadTo(20, 3, 21, 5);
        caseOutline.lineTo(24, 19);
        caseOutline.quadTo(25, 23, 21, 23);
        caseOutline.lineTo(5, 23);
        caseOutline.quadTo(1, 23, 2, 19);
        caseOutline.lineTo(5, 5);
        caseOutline.quadTo(6, 3, 7, 3);
        painter.drawPath(caseOutline);
        painter.drawLine(QPointF(10, 13), QPointF(16, 13));
        painter.drawLine(QPointF(7, 19), QPointF(19, 19));
        break;
    }
    case Icon::Battery:
        painter.drawRoundedRect(QRectF(2, 6, 19, 14), 2.5, 2.5);
        painter.drawLine(QPointF(24, 10), QPointF(24, 16));
        painter.drawRoundedRect(QRectF(6, 10, 5, 6), 1, 1);
        break;
    }
    painter.restore();
}

void GaugeCard::drawSparkline(QPainter& painter, const QRectF& bounds) const {
    if (history_.size() < 2 || bounds.width() <= 0) {
        return;
    }

    const auto [minimum, maximum] = std::minmax_element(history_.begin(), history_.end());
    const double range = std::max(16.0, *maximum - *minimum);
    const double middle = (*minimum + *maximum) * .5;
    const double step = bounds.width() / static_cast<double>(history_.size() - 1);
    const auto pointAt = [&](std::size_t index) {
        const double value = .5 + (history_[index] - middle) / range * .84;
        return QPointF(bounds.left() + static_cast<double>(index) * step,
                       bounds.bottom() - value * bounds.height());
    };

    QPointF previous = pointAt(0);
    QPainterPath line(previous);
    for (std::size_t index = 1; index < history_.size(); ++index) {
        const auto next = pointAt(index);
        const qreal x = (previous.x() + next.x()) * .5;
        line.cubicTo(QPointF(x, previous.y()), QPointF(x, next.y()), next);
        previous = next;
    }

    QPainterPath fill = line;
    fill.lineTo(bounds.bottomRight());
    fill.lineTo(bounds.bottomLeft());
    fill.closeSubpath();

    const auto accent = gaugeAccent(icon_, accent_);
    QLinearGradient vertical(bounds.topLeft(), bounds.bottomLeft());
    vertical.setColorAt(0.0, withAlpha(accent, 37));
    vertical.setColorAt(1.0, withAlpha(accent, 3));
    painter.fillPath(fill, vertical);

    QLinearGradient horizontal(bounds.topLeft(), bounds.topRight());
    horizontal.setColorAt(0.0, withAlpha(accent, 0));
    horizontal.setColorAt(.2, withAlpha(accent, 35));
    horizontal.setColorAt(.62, withAlpha(accent, 69));
    horizontal.setColorAt(1.0, withAlpha(accent, 1));
    painter.strokePath(line, QPen(QBrush(horizontal), 1.2));
}

void GaugeCard::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), theme::tint(theme::background));
    const auto accent = gaugeAccent(icon_, accent_);

    const QRectF card = QRectF(rect()).adjusted(.5, .5, -.5, -.5);
    const qreal scale = compactSizingEnabled_
        ? std::clamp(height() / 174.0, 107.0 / 174.0, 1.0) : 1.0;
    const auto metric = [scale](qreal full, qreal minimum) {
        return std::max(full * scale, minimum);
    };
    const qreal inset = std::clamp(card.width() * .061, 17.0 * scale, 25.0);
    const qreal thermalShift = !compactSizingEnabled_ && height() < 165 ? 5.0 : 0.0;
    const auto outline = theme::cutFrame(card, metric(19, 12));
    painter.fillPath(outline, theme::surfaceGradient(card));
    painter.save();
    painter.setOpacity(.23);
    painter.fillPath(outline, theme::texture());
    painter.restore();
    painter.save();
    painter.setClipPath(outline);
    const auto ambient = theme::tint(QColor("#C1475C"));
    QRadialGradient wash(QPointF(inset, 0), card.width() * .75);
    wash.setColorAt(0, withAlpha(ambient, 35));
    wash.setColorAt(.54, withAlpha(ambient, 10));
    wash.setColorAt(1, withAlpha(ambient, 0));
    painter.fillRect(card, wash);
    drawSparkline(painter, QRectF(inset + card.width() * .15, height() - 104 * scale,
        card.width() * .62, 67 * scale));
    painter.restore();
    QLinearGradient trim(card.topLeft(), card.bottomRight());
    trim.setColorAt(0.0, theme::tint(QColor("#B55870")));
    trim.setColorAt(.38, theme::tint(QColor("#783C51")));
    trim.setColorAt(.7, theme::tint(QColor("#51323E")));
    trim.setColorAt(1.0, theme::tint(QColor("#422D38")));
    painter.strokePath(outline, QPen(QBrush(trim), 1.0));

    const qreal iconSize = metric(32, 21);
    const qreal headerTop = 13 * scale;
    const qreal headerHeight = metric(27, 19);
    const qreal iconTop = headerTop + (headerHeight - iconSize) * .5;
    drawIcon(painter, QRectF(inset, iconTop, iconSize, iconSize));

    QFont statusFont = font();
    statusFont.setPixelSize(qRound(metric(11, 9)));
    statusFont.setWeight(QFont::Bold);
    painter.setFont(statusFont);
    const auto status = displayedStatus(status_, stale_);
    const qreal badgeWidth = std::max(44 * scale,
        painter.fontMetrics().horizontalAdvance(status) + metric(16, 10));
    const auto badgeRect = QRectF(width() - badgeWidth - (inset - 3),
                                  15 * scale, badgeWidth, metric(20, 14));
    auto statusColor = accent.lighter(119);
    if (stale_ || (theme::isDefaultPreset() && icon_ == Icon::Disk)) {
        statusColor = theme::amber;
    } else if (theme::isDefaultPreset() && icon_ != Icon::Gpu) {
        statusColor = QColor("#FF8A9B");
    }
    painter.setPen(QPen(withAlpha(statusColor, 137), 1));
    painter.setBrush(withAlpha(statusColor, 21));
    painter.drawRoundedRect(badgeRect, 6, 6);
    painter.setPen(statusColor);
    painter.drawText(badgeRect, Qt::AlignCenter, status);

    QFont titleFont = font();
    titleFont.setPixelSize(qRound(metric(15, 11)));
    titleFont.setWeight(QFont::DemiBold);
    painter.setFont(titleFont);
    painter.setPen(theme::text);
    const qreal titleX = inset + iconSize + metric(10, 7);
    const int titleWidth = std::max(0, static_cast<int>(badgeRect.left() - titleX - 9 * scale));
    painter.drawText(QRectF(titleX, headerTop, titleWidth, headerHeight),
        Qt::AlignVCenter | Qt::AlignLeft,
        painter.fontMetrics().elidedText(title_, Qt::ElideRight, titleWidth));

    const qreal diameter = std::clamp(width() * .189, 55 * scale, 77 * scale);
    const QRectF gauge(width() - inset - diameter - 7 * scale,
                       53 * scale - thermalShift,
                       diameter, diameter);
    QConicalGradient ring(gauge.center(), 270);
    const auto ringShadow = theme::tint(QColor("#37242F"));
    ring.setColorAt(0.0, ringShadow);
    ring.setColorAt(.40, withAlpha(accent, 122));
    ring.setColorAt(.65, withAlpha(accent, 111));
    ring.setColorAt(1.0, ringShadow);
    painter.setBrush(Qt::NoBrush);
    const qreal stroke = metric(6.4, 4);
    painter.setPen(QPen(QBrush(ring), stroke, Qt::SolidLine, Qt::FlatCap));
    painter.drawArc(gauge, 225 * 16, -270 * 16);
    if (readingAvailable_ && progress_ > 0) {
        QLinearGradient signal(gauge.bottomLeft(), gauge.topRight());
        signal.setColorAt(0, accent);
        signal.setColorAt(.72, accent.lighter(130));
        signal.setColorAt(1, accent.lighter(147));
        painter.setPen(QPen(QBrush(signal), stroke, Qt::SolidLine, Qt::FlatCap));
        painter.drawArc(gauge, 225 * 16,
            static_cast<int>(-270.0 * 16.0 * progress_ / 100.0));
    }
    auto gaugeFont = theme::displayFont(qRound(metric(18, 12)));
    painter.setFont(gaugeFont);
    painter.setPen(theme::text);
    const QString gaugeLabel = !readingAvailable_ ? value_
        : unit_.startsWith("°C") ? value_ + "°"
        : QString::number(qRound(progress_)) + "%";
    painter.drawText(gauge, Qt::AlignCenter, gaugeLabel);

    auto valueFont = theme::displayFont(qRound(metric(35, 22)));
    const bool percentUnit = readingAvailable_ && unit_.startsWith('%');
    const qreal valueWidth = std::max(0.0, gauge.left() - inset - 13 * scale);
    const auto valueRect = QRectF(inset, 48 * scale - thermalShift, valueWidth, 46 * scale);
    const int textWidth = std::max(0, static_cast<int>(valueWidth - (percentUnit ? 34 * scale : 0)));
    const int idealWidth = QFontMetrics(valueFont).horizontalAdvance(value_);
    if (compactSizingEnabled_ && idealWidth > textWidth && textWidth > 0) {
        valueFont.setPixelSize(std::max(18, valueFont.pixelSize() * textWidth / idealWidth));
    }
    painter.setFont(valueFont);
    painter.setPen(theme::text);
    const auto value = painter.fontMetrics().elidedText(value_, Qt::ElideRight,
                                                       textWidth);
    painter.drawText(valueRect, Qt::AlignLeft | Qt::AlignVCenter, value);
    if (percentUnit) {
        const qreal suffixX = inset + painter.fontMetrics().horizontalAdvance(value) + 10 * scale;
        valueFont.setPixelSize(qRound(metric(22, 14)));
        painter.setFont(valueFont);
        const QRectF suffix(suffixX, valueRect.top(), metric(23, 15), valueRect.height());
        painter.drawText(suffix, Qt::AlignVCenter | Qt::AlignLeft, "%");
    }

    QFont unitFont = font();
    unitFont.setPixelSize(qRound(metric(12, 9)));
    unitFont.setWeight(QFont::DemiBold);
    unitFont.setLetterSpacing(QFont::AbsoluteSpacing, .45);
    painter.setFont(unitFont);
    painter.setPen(accent.lighter(122));
    const auto caption = percentUnit ? unit_.sliced(1).trimmed() : unit_;
    painter.drawText(QRectF(inset, 95 * scale - thermalShift, valueWidth, metric(19, 13)),
        Qt::AlignVCenter | Qt::AlignLeft,
        painter.fontMetrics().elidedText(caption, Qt::ElideRight, static_cast<int>(valueWidth)));

    QFont subtitleFont = font();
    subtitleFont.setPixelSize(qRound(metric(13, 10)));
    painter.setFont(subtitleFont);
    painter.setPen(theme::text);
    const int subtitleWidth = std::max(0, static_cast<int>(width() - 2 * inset));
    painter.drawText(QRectF(inset, height() - 43 * scale, subtitleWidth, metric(21, 14)),
        Qt::AlignLeft | Qt::AlignVCenter,
        painter.fontMetrics().elidedText(subtitle_, Qt::ElideRight, subtitleWidth));

    const QRectF bar(inset, height() - 16 * scale, subtitleWidth, metric(4, 3));
    painter.setPen(Qt::NoPen);
    painter.setBrush(withAlpha(accent, 100));
    painter.drawRoundedRect(bar, 2, 2);
    if (readingAvailable_ && progress_ > 0) {
        const QRectF fill(bar.topLeft(), QSizeF(bar.width() * progress_ * .01, bar.height()));
        QLinearGradient sheen(fill.topLeft(), fill.topRight());
        sheen.setColorAt(0, accent.lighter(121));
        sheen.setColorAt(1, accent);
        painter.setBrush(sheen);
        painter.drawRoundedRect(fill, 2, 2);
    }
}

} // namespace vg::ui
