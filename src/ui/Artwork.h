#pragma once

#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>

#include <algorithm>

namespace vg::ui::artwork {

inline void cover(QPainter& painter, const QRectF& target, const QPixmap& image,
                  qreal verticalFocus = .5) {
    if (target.isEmpty() || image.isNull()) {
        return;
    }
    const auto scale = std::max(target.width() / image.width(), target.height() / image.height());
    const qreal width = target.width() / scale;
    const qreal height = target.height() / scale;
    const QRectF crop((image.width() - width) / 2, (image.height() - height) * verticalFocus,
                     width, height);
    painter.drawPixmap(target, image, crop);
}

inline void fade(QPainter& painter, const QRectF& bounds, Qt::Orientation direction,
                 const QGradientStops& stops) {
    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    QLinearGradient mask(bounds.topLeft(),
        direction == Qt::Horizontal ? bounds.topRight() : bounds.bottomLeft());
    mask.setStops(stops);
    painter.fillRect(bounds, mask);
    painter.restore();
}

} // namespace vg::ui::artwork
