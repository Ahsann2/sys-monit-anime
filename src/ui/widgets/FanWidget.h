#pragma once

#include "core/Types.h"

#include <QElapsedTimer>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

namespace vg::ui {

class FanWidget final : public QWidget {
public:
    explicit FanWidget(bool sharedFan, QWidget* parent = nullptr);

    void setData(const FanStats& fans, double cpuTemperature, double gpuTemperature);
    void setCharacterArtwork(const QPixmap& portrait, const QPixmap& scene);
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void updateAnimation();
    void advanceAnimation();
    void scheduleFrame();
    [[nodiscard]] QRectF artworkBounds() const;
    [[nodiscard]] QRect rotorBounds() const;
    void prepareArtwork();

    QTimer timer_;
    QElapsedTimer animationClock_;
    qint64 nextFrame_{0};
    qint64 previousFrame_{0};
    QPixmap panel_;
    QPixmap scenery_;
    QPixmap rotor_;
    QPixmap characterPortrait_;
    QPixmap characterScene_;
    QSize artworkSize_;
    qreal pixelRatio_{0.0};
    double rotation_{0.0};
    double degreesPerSecond_{110.0};
    std::optional<int> rpm_;
    const bool sharedFan_;
    bool usingGpuChannel_{false};
    FanMode mode_{FanMode::Unknown};
    double cpuTemperature_{0.0};
    double gpuTemperature_{0.0};
};

} // namespace vg::ui
