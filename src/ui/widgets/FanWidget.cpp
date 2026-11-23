#include "ui/widgets/FanWidget.h"
#include "ui/Theme.h"

#include <QFont>
#include <QLinearGradient>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPainter>
#include <QRadialGradient>

#include <algorithm>
#include <array>
#include <cmath>

namespace vg::ui {
namespace {

constexpr qint64 nanosecondsPerSecond = 1'000'000'000;
constexpr qint64 frameInterval = nanosecondsPerSecond / 60;
constexpr qint64 nanosecondsPerMillisecond = 1'000'000;
constexpr qreal artworkWidth = 450.0;
constexpr qreal artworkHeight = 596.0;
constexpr qreal artworkInset = 6.0;
constexpr QPointF referenceCenter{232.0, 223.4};
constexpr qreal referenceRotorRadius = 160.0;

void paintScenery(QPainter& painter, const QPixmap& scenery, const QRectF& artwork,
                  const QSize& panelSize) {
    const qreal scale = artwork.width() / artworkWidth;
    const auto sourceWidth = artworkWidth - 2.0 * artworkInset;
    const auto panelLeft = artwork.left() + artworkInset * scale;
    // When a narrow panel still has room above or below the art, extend only
    // the dark edge bands. The rotor window and the character keep their aspect.
    const std::array<qreal, 4> sourceY{artworkInset, 40.0, 551.0, artworkHeight - artworkInset};
    const std::array<qreal, 4> panelY{
        std::min(0.0, artwork.top() + sourceY.front() * scale),
        artwork.top() + sourceY[1] * scale,
        artwork.top() + sourceY[2] * scale,
        std::max<qreal>(panelSize.height(), artwork.top() + sourceY.back() * scale)
    };
    for (std::size_t y = 0; y + 1 < sourceY.size(); ++y) {
        const QRectF target(panelLeft, panelY[y], sourceWidth * scale, panelY[y + 1] - panelY[y]);
        const QRectF source(artworkInset, sourceY[y], sourceWidth, sourceY[y + 1] - sourceY[y]);
        painter.drawPixmap(target, scenery, source);
    }
}

void paintFanIcon(QPainter& painter, QPointF position, qreal scale) {
    painter.save();
    painter.translate(position);
    painter.scale(1.32 * scale, 1.32 * scale);
    painter.setPen(QPen(QColor("#FF98AB"), .88, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    const QRectF casing{-10, -10, 20, 20};
    for (int angle : {18, 138, 258}) {
        painter.drawArc(casing, angle * 16, 83 * 16);
    }
    painter.drawEllipse(QPointF{0, 0}, 2.1, 2.1);
    QPainterPath blade;
    blade.moveTo(0, -2);
    blade.cubicTo(-5, -7, 1, -10, 4, -6);
    blade.cubicTo(4, -4, 2, -3, 1, -3);
    for (int bladeIndex = 0; bladeIndex < 3; ++bladeIndex) {
        painter.drawPath(blade);
        painter.rotate(120);
    }
    painter.restore();
}

} // namespace

FanWidget::FanWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(225, 300);
    timer_.setSingleShot(true);
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &FanWidget::advanceAnimation);
}

void FanWidget::setData(const FanStats& fans, double cpuTemperature, double gpuTemperature) {
    if (rpm_ == fans.cpuRpm && mode_ == fans.cpuMode &&
        cpuTemperature_ == cpuTemperature && gpuTemperature_ == gpuTemperature) {
        return;
    }
    rpm_ = fans.cpuRpm;
    mode_ = fans.cpuMode;
    cpuTemperature_ = cpuTemperature;
    gpuTemperature_ = gpuTemperature;
    degreesPerSecond_ = rpm_ ? std::clamp(*rpm_ / 20.0, 0.0, 270.0)
                             : mode_ == FanMode::Maximum ? 160.0 : 110.0;
    updateAnimation();
    update();
}

void FanWidget::updateAnimation() {
    if (!isVisible() || degreesPerSecond_ == 0.0) {
        timer_.stop();
    } else if (!timer_.isActive()) {
        animationClock_.start();
        previousFrame_ = 0;
        nextFrame_ = frameInterval;
        scheduleFrame();
    }
}

void FanWidget::advanceAnimation() {
    const auto now = animationClock_.nsecsElapsed();
    if (now >= nextFrame_) {
        const double elapsed = static_cast<double>(now - previousFrame_) / nanosecondsPerSecond;
        rotation_ = std::fmod(rotation_ + degreesPerSecond_ * elapsed, 360.0);
        previousFrame_ = now;
        // Skip missed frames so a slow repaint never queues a burst of work.
        nextFrame_ += (1 + (now - nextFrame_) / frameInterval) * frameInterval;
        update(rotorBounds());
    }
    scheduleFrame();
}

void FanWidget::scheduleFrame() {
    const auto remaining = std::max<qint64>(1, nextFrame_ - animationClock_.nsecsElapsed());
    timer_.start(static_cast<int>((remaining + nanosecondsPerMillisecond - 1)
                                   / nanosecondsPerMillisecond));
}

void FanWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    updateAnimation();
}

void FanWidget::hideEvent(QHideEvent* event) {
    timer_.stop();
    QWidget::hideEvent(event);
}

QSize FanWidget::sizeHint() const {
    return {450, 596};
}

QRectF FanWidget::artworkBounds() const {
    const auto coverWidth = width() / (artworkWidth - 2.0 * artworkInset);
    const auto coverHeight = height() / (artworkHeight - 2.0 * artworkInset);
    const auto scale = std::clamp(coverHeight, coverWidth, coverWidth * 1.10);
    const QSizeF size{artworkWidth * scale, artworkHeight * scale};
    const auto visibleHeight = size.height() - 2.0 * artworkInset * scale;
    const auto verticalSpace = height() - visibleHeight;
    // Keep a little room over the ring on the dashboard. On shallow cards,
    // center the crop lower so the character still fits above the readings.
    const auto shallow = std::clamp((-verticalSpace / height() - .20) / .40, 0.0, 1.0);
    const auto cropBias = .17 + .23 * shallow;
    const auto top = verticalSpace * (verticalSpace < 0.0 ? cropBias : .43) - artworkInset * scale;
    return {(width() - size.width()) / 2.0, top, size.width(), size.height()};
}

QRect FanWidget::rotorBounds() const {
    const auto artwork = artworkBounds();
    const auto scale = artwork.width() / artworkWidth;
    const auto center = artwork.topLeft() + referenceCenter * scale;
    const auto radius = referenceRotorRadius * scale;
    return QRectF(center - QPointF(radius, radius), QSizeF(2 * radius, 2 * radius))
        .toAlignedRect();
}

void FanWidget::prepareArtwork() {
    const qreal ratio = devicePixelRatioF();
    if (artworkSize_ == size() && pixelRatio_ == ratio) {
        return;
    }
    artworkSize_ = size();
    pixelRatio_ = ratio;

    const auto artwork = artworkBounds();
    const auto scale = artwork.width() / artworkWidth;
    const auto center = artwork.topLeft() + referenceCenter * scale;
    const auto outer = QRectF(rect()).adjusted(.5, .5, -.5, -.5);
    const auto outline = theme::cutFrame(outer);
    const QSize pixels{static_cast<int>(std::ceil(width() * ratio)),
                      static_cast<int>(std::ceil(height() * ratio))};
    const auto layer = [&pixels, ratio] {
        QPixmap result(pixels);
        result.setDevicePixelRatio(ratio);
        result.fill(Qt::transparent);
        return result;
    };
    static const QPixmap referenceScenery(":/reference-fan-scene.png");
    static const QPixmap referenceRotor(":/reference-fan-rotor.png");

    panel_ = layer();
    {
        QPainter paint(&panel_);
        paint.setRenderHint(QPainter::Antialiasing);
        QLinearGradient background(outer.topLeft(), outer.bottomRight());
        background.setColorAt(0.0, QColor("#291720"));
        background.setColorAt(.60, QColor("#1B121A"));
        background.setColorAt(1.0, QColor("#261826"));
        paint.fillPath(outline, background);
        paint.setClipPath(outline);
        const auto radius = (referenceRotorRadius + 1.0) * scale;
        QRadialGradient disc(center, radius, center - QPointF(0.0, 45.0 * scale));
        disc.setColorAt(0.0, QColor("#191117"));
        disc.setColorAt(.65, QColor("#21111A"));
        disc.setColorAt(1.0, QColor("#271722"));
        paint.setPen(Qt::NoPen);
        paint.setBrush(disc);
        paint.drawEllipse(center, radius, radius);
        if (referenceScenery.isNull()) {
            paint.setBrush(Qt::NoBrush);
            paint.setPen(QPen(theme::border, 1.4));
            paint.drawEllipse(center, radius * 1.04, radius * 1.04);
            paint.setPen(QPen(theme::primary.darker(160), 1.4));
            paint.drawEllipse(center, radius * .96, radius * .96);
        }
    }

    scenery_ = layer();
    {
        QPainter paint(&scenery_);
        paint.setRenderHint(QPainter::Antialiasing);
        paint.setRenderHint(QPainter::SmoothPixmapTransform);
        paint.setClipPath(outline);
        if (!referenceScenery.isNull()) {
            paintScenery(paint, referenceScenery, artwork, size());
        }
        if (!referenceScenery.isNull() && artwork.bottom() - height() > 36.0 * scale) {
            QLinearGradient footerShade(0, height() - 120, 0, height());
            footerShade.setColorAt(0.0, QColor(27, 18, 26, 0));
            footerShade.setColorAt(1.0, QColor(27, 18, 26, 196));
            paint.fillRect(QRectF(0, height() - 120, width(), 120), footerShade);
        }
        paint.setClipping(false);
        QLinearGradient edge(outer.topLeft(), outer.bottomRight());
        edge.setColorAt(0.0, QColor("#955165"));
        edge.setColorAt(.45, QColor("#4C2C43"));
        edge.setColorAt(1.0, QColor("#42303F"));
        paint.strokePath(outline, QPen(edge, 1.0));
    }
    const int diameter = std::max(1, static_cast<int>(std::ceil(2 * referenceRotorRadius * scale * ratio)));
    if (!referenceRotor.isNull()) {
        rotor_ = referenceRotor.scaled(diameter, diameter, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    } else {
        rotor_ = QPixmap(diameter, diameter);
        rotor_.fill(Qt::transparent);
        QPainter paint(&rotor_);
        paint.setRenderHint(QPainter::Antialiasing);
        paint.translate(diameter / 2.0, diameter / 2.0);
        paint.scale(diameter / 320.0, diameter / 320.0);
        QPainterPath blade;
        blade.moveTo(13, -10);
        blade.cubicTo(29, -59, 95, -128, 125, -77);
        blade.cubicTo(152, -32, 96, -7, 24, 13);
        blade.closeSubpath();
        QLinearGradient span(18, 4, 125, -83);
        span.setColorAt(0, QColor("#371725"));
        span.setColorAt(1, QColor("#A93755"));
        paint.setBrush(span);
        paint.setPen(QPen(theme::border));
        for (int index = 0; index < 7; ++index) {
            paint.drawPath(blade);
            paint.rotate(360.0 / 7.0);
        }
        paint.setBrush(QColor("#64253B"));
        paint.drawEllipse(QPointF{}, 25, 25);
    }
    rotor_.setDevicePixelRatio(ratio);
}

void FanWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    const auto bounds = rotorBounds();
    const bool rotorOnly = bounds.contains(event->rect());
    prepareArtwork();
    if (!rotorOnly) {
        painter.fillRect(rect(), theme::background);
    }
    painter.drawPixmap(QPointF{}, panel_);
    const auto artwork = artworkBounds();
    const auto scale = artwork.width() / artworkWidth;
    const auto center = artwork.topLeft() + referenceCenter * scale;
    painter.save();
    painter.setClipPath(theme::cutFrame(QRectF(rect()).adjusted(.5, .5, -.5, -.5)));
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.translate(center);
    painter.rotate(rotation_);
    const auto half = rotor_.deviceIndependentSize() / 2.0;
    painter.drawPixmap(QPointF(-half.width(), -half.height()), rotor_);
    painter.restore();
    painter.drawPixmap(QPointF{}, scenery_);
    const qreal textScale = std::clamp(width() / artworkWidth, .81, 1.14);
    const qreal footer = height() - (rpm_ ? 79.0 : 93.0) * textScale;
    // A wide crop can put the rotor beneath a live label. Repaint that label
    // along with the rotor so it does not disappear between telemetry updates.
    if (rotorOnly && event->rect().top() >= 43 && event->rect().bottom() < footer) {
        return;
    }

    auto print = [&](const QRectF& area, const QString& text, int pixels,
                     const QColor& color, QFont::Weight weight, Qt::Alignment alignment) {
        auto type = pixels >= 18 ? theme::displayFont(qRound(pixels * textScale)) : font();
        type.setPixelSize(qRound(pixels * textScale));
        type.setWeight(weight);
        painter.setFont(type);
        painter.setPen(color);
        painter.drawText(area, alignment, text);
    };
    painter.setRenderHint(QPainter::Antialiasing);
    paintFanIcon(painter, QPointF(36.0 * textScale, 30.0), textScale);
    print(QRectF(62.0 * textScale, 15.0, width() - 140.0 * textScale, 28.0),
          "KIPAS CPU", 18, theme::text, QFont::Bold, Qt::AlignLeft | Qt::AlignVCenter);
    const auto mode = mode_ == FanMode::Maximum ? QStringLiteral("MAKSIMUM")
        : mode_ == FanMode::Automatic ? QStringLiteral("OTOMATIS") : QStringLiteral("MODE N/A");
    print(QRectF(width() - 140.0 * textScale, 16.0, 115.0 * textScale, 25.0),
          mode, 14, QColor("#FF829C"), QFont::DemiBold, Qt::AlignRight | Qt::AlignVCenter);
    print(QRectF(12, footer, width() - 24, 32.0),
          rpm_ ? QString::number(*rpm_) + " RPM" : QStringLiteral("RPM N/A"),
          24, theme::text, QFont::Bold, Qt::AlignCenter);
    if (!rpm_) {
        print(QRectF(12, footer + 31.0, width() - 24, 23.0), "Animasi ilustrasi",
              14, QColor("#E0A3B9"), QFont::Medium, Qt::AlignCenter);
    }
    const auto degrees = [](double celsius) {
        return celsius > 0 ? QString::number(celsius, 'f', 0) + " °C" : QStringLiteral("N/A");
    };
    print(QRectF(12, height() - 38.0 * textScale, width() - 24, 20.0),
          QString("CPU %1    |    GPU %2").arg(degrees(cpuTemperature_), degrees(gpuTemperature_)),
          14, theme::muted, QFont::Medium, Qt::AlignCenter);
}

} // namespace vg::ui
