#include "ui/widgets/FanWidget.h"
#include "ui/Artwork.h"
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
#include <string_view>

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

struct RotorDesign {
    int blades;
    qreal shoulder;
    qreal tip;
    qreal sweep;
    bool centeredEmblem{false};
};

RotorDesign rotorDesign() {
    struct Shape {
        std::string_view id;
        RotorDesign rotor;
    };
    static constexpr std::array shapes{
        Shape{"sung-jinwoo", {6, 74, 138, 62}},
        Shape{"sasuke", {5, 37, 127, 86}},
        Shape{"tanjiro", {6, 99, 134, 63}},
        Shape{"giyu", {5, 110, 122, 56}},
        Shape{"gojo", {3, 110, 116, 98}},
        Shape{"sanemi", {7, 48, 147, 39}},
        Shape{"muichiro", {4, 92, 123, 74}},
        Shape{"itachi", {3, 80, 108, 106}},
        Shape{"killua", {6, 30, 146, 43}},
        Shape{"naruto", {9, 48, 136, 63}},
        Shape{"shinobu", {4, 112, 109, 101}},
        Shape{"spider-man", {8, 43, 131, 67, true}},
        Shape{"doma", {6, 91, 107, 99}},
        Shape{"akaza", {6, 42, 138, 51}},
    };
    const std::string_view id(theme::preset().id);
    const auto found = std::ranges::find_if(shapes, [id](const auto& shape) {
        return id == shape.id;
    });
    return found != shapes.end() ? found->rotor : RotorDesign{7, 95, 125, 77};
}

void paintShrineRotor(QPainter& painter) {
    QPainterPath blade;
    blade.moveTo(13, 5);
    blade.cubicTo(32, -9, 42, -53, 84, -75);
    blade.lineTo(129, -76);
    blade.lineTo(99, -55);
    blade.lineTo(109, -51);
    blade.cubicTo(83, -21, 51, -11, 23, 9);
    blade.closeSubpath();

    QPainterPath edge;
    edge.moveTo(27, 0);
    edge.cubicTo(53, -25, 62, -55, 115, -72);
    QLinearGradient lacquer(14, 5, 126, -75);
    lacquer.setColorAt(0, QColor("#211018"));
    lacquer.setColorAt(.54, QColor("#851327"));
    lacquer.setColorAt(1, QColor("#EC3C56"));
    const QColor rim(255, 90, 113, 164);
    const QRectF ring(-145, -145, 290, 290);

    for (int index = 0; index < 4; ++index) {
        painter.setBrush(lacquer);
        painter.setPen(QPen(QColor("#D54B62"), 1.2));
        painter.drawPath(blade);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor("#FF9A90"), 2, Qt::SolidLine, Qt::RoundCap));
        painter.drawPath(edge);
        painter.setPen(QPen(rim, 1.2));
        painter.drawArc(ring, 13 * 16, 51 * 16);
        painter.drawLine(QPointF(145, 0), QPointF(136, -8));
        painter.drawLine(QPointF(139, 6), QPointF(130, -2));
        painter.rotate(90);
    }

    painter.setPen(QPen(QColor("#F26370"), 1.6));
    painter.setBrush(QColor("#200C13"));
    painter.drawPolygon(QPolygonF{QPointF(0, -25), QPointF(25, 0),
        QPointF(0, 25), QPointF(-25, 0)});
    painter.setPen(QPen(QColor("#FFAB89"), 1.6, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(-10, 7), QPointF(7, -10));
    painter.drawLine(QPointF(-5, 12), QPointF(12, -5));
}

void paintStandardRotor(QPainter& painter, const RotorDesign& design) {
    QPainterPath blade;
    blade.moveTo(13, -10);
    blade.cubicTo(29, -59, design.shoulder, -128, design.tip, -design.sweep);
    blade.cubicTo(151, -32, design.shoulder, -7, 24, 13);
    blade.closeSubpath();
    QLinearGradient span(18, 4, 125, -83);
    auto root = theme::accent().darker(330);
    auto edge = theme::secondaryAccent().darker(140);
    root.setAlpha(194);
    edge.setAlpha(209);
    span.setColorAt(0, root);
    span.setColorAt(1, edge);
    painter.setBrush(span);
    painter.setPen(QPen(theme::tint(theme::border)));
    for (int index = 0; index < design.blades; ++index) {
        painter.drawPath(blade);
        painter.rotate(360.0 / design.blades);
    }
    painter.setBrush(theme::accent().darker(210));
    painter.drawEllipse(QPointF{}, 23, 23);
    painter.setPen(QPen(theme::secondaryAccent(), 1.2));
    painter.drawEllipse(QPointF{}, 12, 12);
}

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
    painter.setPen(QPen(theme::tint(QColor("#FF98AB")), .88, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
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

void paintPortrait(QPainter& painter, const QPixmap& portrait, const QSize& panelSize) {
    const auto fit = QSizeF(portrait.size()).scaled(
        QSizeF(panelSize.width() * .96, panelSize.height() * .77), Qt::KeepAspectRatio);
    const QRectF target(QPointF(panelSize.width() - fit.width() + 7.0,
                              panelSize.height() - fit.height() + 3.0), fit);
    painter.drawPixmap(target, portrait, portrait.rect());
    artwork::fade(painter, target, Qt::Horizontal,
        {{0, Qt::transparent}, {.085, Qt::white}, {1, Qt::white}});
    artwork::fade(painter, target, Qt::Vertical,
        {{0, Qt::transparent}, {.055, Qt::white}, {1, Qt::white}});
}

} // namespace

FanWidget::FanWidget(bool sharedFan, QWidget* parent)
    : QWidget(parent), sharedFan_(sharedFan) {
    setMinimumSize(225, 300);
    timer_.setSingleShot(true);
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &FanWidget::advanceAnimation);
}

void FanWidget::setData(const FanStats& fans, double cpuTemperature, double gpuTemperature) {
    const bool useGpuChannel = !fans.cpuRpm && fans.gpuRpm.has_value();
    const auto rpm = useGpuChannel ? fans.gpuRpm : fans.cpuRpm;
    const auto mode = useGpuChannel && !sharedFan_ ? FanMode::Unknown : fans.cpuMode;
    if (rpm_ == rpm && usingGpuChannel_ == useGpuChannel && mode_ == mode &&
        cpuTemperature_ == cpuTemperature && gpuTemperature_ == gpuTemperature) {
        return;
    }
    rpm_ = rpm;
    usingGpuChannel_ = useGpuChannel;
    mode_ = mode;
    setToolTip(sharedFan_ && useGpuChannel
        ? QStringLiteral("RPM kipas sistem dari kanal kedua driver ASUS (fan2_input).")
        : QString());
    cpuTemperature_ = cpuTemperature;
    gpuTemperature_ = gpuTemperature;
    degreesPerSecond_ = rpm_ ? std::clamp(*rpm_ / 20.0, 0.0, 270.0)
                             : mode_ == FanMode::Maximum ? 160.0 : 110.0;
    updateAnimation();
    update();
}

void FanWidget::setCharacterArtwork(const QPixmap& portrait, const QPixmap& scene) {
    characterPortrait_ = portrait;
    characterScene_ = scene;
    artworkSize_ = {};
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
    const bool useReferenceScene = theme::isDefaultPreset() && !referenceScenery.isNull();
    const auto color = [](const char* hex) { return theme::tint(QColor(hex)); };
    const auto design = rotorDesign();

    panel_ = layer();
    {
        QPainter paint(&panel_);
        paint.setRenderHint(QPainter::Antialiasing);
        QLinearGradient background(outer.topLeft(), outer.bottomRight());
        background.setColorAt(0.0, color("#291720"));
        background.setColorAt(.60, color("#1B121A"));
        background.setColorAt(1.0, color("#261826"));
        paint.fillPath(outline, background);
        paint.setClipPath(outline);
        if (!characterScene_.isNull()) {
            paint.setRenderHint(QPainter::SmoothPixmapTransform);
            paint.setOpacity(.68);
            artwork::cover(paint, outer, characterScene_, .49);
            paint.setOpacity(1);
            QLinearGradient shadow(outer.topLeft(), outer.bottomLeft());
            auto ink = theme::tint(theme::background);
            ink.setAlpha(221);
            shadow.setColorAt(0, ink);
            ink.setAlpha(56);
            shadow.setColorAt(.37, ink);
            ink.setAlpha(150);
            shadow.setColorAt(1, ink);
            paint.fillRect(outer, shadow);
        }
        const auto radius = (referenceRotorRadius + 1.0) * scale;
        QRadialGradient disc(center, radius, center - QPointF(0.0, 45.0 * scale));
        auto inner = color("#191117");
        auto middle = color("#21111A");
        auto rim = color("#271722");
        if (!characterScene_.isNull()) {
            inner.setAlpha(113);
            middle.setAlpha(94);
            rim.setAlpha(155);
        }
        disc.setColorAt(0.0, inner);
        disc.setColorAt(.65, middle);
        disc.setColorAt(1.0, rim);
        paint.setPen(Qt::NoPen);
        paint.setBrush(disc);
        paint.drawEllipse(center, radius, radius);
        if (!useReferenceScene) {
            paint.setBrush(Qt::NoBrush);
            paint.setPen(QPen(theme::tint(theme::border), 1.4));
            paint.drawEllipse(center, radius * 1.04, radius * 1.04);
            paint.setPen(QPen(theme::accent().darker(155), 1.4));
            paint.drawEllipse(center, radius * .96, radius * .96);
            paint.save();
            paint.translate(center);
            for (int tick = 0; tick < 36; ++tick) {
                paint.setPen(QPen(tick % 3 == 0 ? theme::secondaryAccent()
                                               : theme::accent().darker(175)));
                paint.drawLine(QPointF(0, -radius * 1.025), QPointF(0, -radius * .975));
                paint.rotate(10);
            }
            const QRectF arc(-radius * 1.085, -radius * 1.085,
                             radius * 2.17, radius * 2.17);
            paint.setPen(QPen(theme::secondaryAccent(), 1.2));
            paint.drawArc(arc, 32 * 16, (13 + design.blades * 6) * 16);
            paint.setPen(QPen(theme::accent(), 1.2));
            paint.drawArc(arc, 211 * 16, (8 + design.blades * 5) * 16);
            paint.restore();
        }
    }

    scenery_ = layer();
    {
        QPainter paint(&scenery_);
        paint.setRenderHint(QPainter::Antialiasing);
        paint.setRenderHint(QPainter::SmoothPixmapTransform);
        paint.setClipPath(outline);
        if (useReferenceScene) {
            paintScenery(paint, referenceScenery, artwork, size());
        } else if (design.centeredEmblem && !characterPortrait_.isNull()) {
            const qreal span = referenceRotorRadius * 1.5 * scale;
            const auto fit = QSizeF(characterPortrait_.size()).scaled(span, span, Qt::KeepAspectRatio);
            const QPointF origin = center - QPointF(fit.width() / 2, fit.height() / 2);
            paint.drawPixmap(QRectF(origin, fit), characterPortrait_, characterPortrait_.rect());
        } else if (!characterPortrait_.isNull()) {
            paintPortrait(paint, characterPortrait_, size());
        }
        if (!characterPortrait_.isNull() ||
            (useReferenceScene && artwork.bottom() - height() > 36.0 * scale)) {
            QLinearGradient footerShade(0, height() - 120, 0, height());
            auto shade = color("#1B121A");
            shade.setAlpha(0);
            footerShade.setColorAt(0.0, shade);
            shade.setAlpha(196);
            footerShade.setColorAt(1.0, shade);
            paint.fillRect(QRectF(0, height() - 120, width(), 120), footerShade);
        }
        paint.setClipping(false);
        QLinearGradient edge(outer.topLeft(), outer.bottomRight());
        edge.setColorAt(0.0, color("#955165"));
        edge.setColorAt(.45, color("#4C2C43"));
        edge.setColorAt(1.0, color("#42303F"));
        paint.strokePath(outline, QPen(edge, 1.0));
    }
    const int diameter = std::max(1, static_cast<int>(std::ceil(2 * referenceRotorRadius * scale * ratio)));
    if (useReferenceScene && !referenceRotor.isNull()) {
        rotor_ = referenceRotor.scaled(diameter, diameter, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    } else {
        rotor_ = QPixmap(diameter, diameter);
        rotor_.fill(Qt::transparent);
        QPainter paint(&rotor_);
        paint.setRenderHint(QPainter::Antialiasing);
        paint.translate(diameter / 2.0, diameter / 2.0);
        paint.scale(diameter / 320.0, diameter / 320.0);
        if (theme::currentPreset() == "sukuna") {
            paintShrineRotor(paint);
        } else {
            paintStandardRotor(paint, design);
        }
    }
    rotor_.setDevicePixelRatio(ratio);
}

void FanWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    const auto bounds = rotorBounds();
    const bool rotorOnly = bounds.contains(event->rect());
    prepareArtwork();
    if (!rotorOnly) {
        painter.fillRect(rect(), theme::tint(theme::background));
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
    const qreal footer = height() - 79.0 * textScale;
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
    const auto title = sharedFan_ ? QStringLiteral("KIPAS SISTEM")
        : usingGpuChannel_ ? QStringLiteral("KIPAS GPU") : QStringLiteral("KIPAS CPU");
    print(QRectF(62.0 * textScale, 15.0, width() - 140.0 * textScale, 28.0),
          title, 18, theme::text, QFont::Bold, Qt::AlignLeft | Qt::AlignVCenter);
    const auto mode = mode_ == FanMode::Maximum ? QStringLiteral("MAKSIMUM")
        : mode_ == FanMode::Automatic ? QStringLiteral("OTOMATIS") : QString();
    print(QRectF(width() - 140.0 * textScale, 16.0, 115.0 * textScale, 25.0),
          mode, 14, theme::tint(QColor("#FF829C")), QFont::DemiBold, Qt::AlignRight | Qt::AlignVCenter);
    print(QRectF(12, footer, width() - 24, 32.0),
          rpm_ ? QString::number(*rpm_) + " RPM" : QStringLiteral("RPM tak terbaca"),
          24, theme::text, QFont::Bold, Qt::AlignCenter);
    const auto degrees = [](double celsius) {
        return celsius > 0 ? QString::number(celsius, 'f', 0) + " °C" : QStringLiteral("N/A");
    };
    print(QRectF(12, height() - 38.0 * textScale, width() - 24, 20.0),
          QString("CPU %1    |    GPU %2").arg(degrees(cpuTemperature_), degrees(gpuTemperature_)),
          14, theme::muted, QFont::Medium, Qt::AlignCenter);
}

} // namespace vg::ui
