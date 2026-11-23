#pragma once

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QRegularExpression>
#include <QString>
#include <QSvgRenderer>

#include <algorithm>
#include <array>
#include <cmath>

namespace vg::ui::theme {

inline const QString appName{"Monitor Sistem"};
inline const QColor background{"#160D14"};
inline const QColor panel{"#24151F"};
inline const QColor panelRaised{"#31202B"};
inline const QColor border{"#654052"};
inline const QColor text{"#FFF6F9"};
inline const QColor muted{"#D8C3CE"};
inline const QColor primary{"#F26686"};
inline const QColor coral{"#FF9186"};
inline const QColor ember{"#FFC27E"};
inline const QColor pink{"#C886C5"};
inline const QColor green{"#6CECA2"};
inline const QColor amber{"#FFD093"};
inline const QColor red{"#FF3E50"};

struct Preset {
    const char* id;
    const char* name;
    int accentHue;
    QRgb accent;
    QRgb secondary;
    QRgb highlight;
    const char* emblem;
};

inline constexpr std::array presets{
    Preset{"default", "Bawaan", 346, 0xF26686, 0xC886C5, 0xFFC682, ""},
    Preset{"sung-jinwoo", "Sung Jinwoo", 241, 0x818EFF, 0xBB93FF, 0xB5DCFF,
        "M18 6 25 15 22 21 18 26 14 21 11 15Z M18 11V21 M9 11l-2 6 3 6 5 4 M27 11l2 6-3 6-5 4"},
    Preset{"sasuke", "Sasuke Uchiha", 277, 0xC08CFF, 0x8B99FF, 0xFFD2FA,
        "M7 13 12 8 18 10 24 8 29 13 27 22 22 27H14L9 22Z M9 13l6 4h6l6-4 M15 17l3 4 3-4 M13 23h10 M15 23v3 M21 23v3"},
    Preset{"tanjiro", "Tanjiro Kamado", 17, 0xFF845B, 0x79DDB6, 0xFFD18B,
        "M25 12a9 9 0 1 1-9-4c-1 5 7 4 6 9-1 2-3 4-5 3 M18 5V7 M28 8l-2 2 M31 18h-3 M26 26l2 2 M18 28v3 M10 26l-2 2 M8 18H5 M8 8l2 2"},
    Preset{"giyu", "Giyu Tomioka", 215, 0x74ADFF, 0xA7C7FF, 0x6FE4EE,
        "M6 24c3 0 4-2 6-6 2-4 6-7 10-7 5 0 7 4 6 8-1-3-4-5-7-4-4 1-4 6 1 7 2 0 4-1 7-2 M7 27c4 0 6-2 8-4m4-7 2-5"},
    Preset{"gojo", "Satoru Gojo", 224, 0x6BA7FF, 0xDA91FF, 0xA9D9FF,
        "M18 18c-4-5-6-6-8-5-5 1-4 9 1 10 3 0 5-2 7-5s5-5 8-5c5 1 5 8 1 10-3 1-6-1-9-5Z M18 7l2 3-2 3-2-3Z M18 24l2 3-2 3-2-3Z"},
    Preset{"sanemi", "Sanemi Shinazugawa", 150, 0x6FDEAD, 0x89E3D8, 0xCCF09A,
        "M6 15c5-5 11-6 17-4 3 1 4-3 2-4 M7 20c7-6 14-4 19-2 2 1 4-1 4-3 M8 25c4-3 10-3 14-1 2 1 2 4-1 5"},
    Preset{"muichiro", "Muichiro Tokito", 177, 0x71DED4, 0xA9BCEB, 0xA7E7FF,
        "M8 12h10l4-3h6 M6 17h8l4 3h13 M9 25h7l4-3m2 3h5 M13 29h11"},
    Preset{"itachi", "Itachi Uchiha", 358, 0xFF747B, 0xEA96BA, 0xFFB58B,
        "M5 18c4-6 8-8 13-8s9 2 13 8c-4 6-8 8-13 8s-9-2-13-8Z M18 14a4 4 0 1 0 0 8 4 4 0 0 0 0-8 M18 10v2 M11 22l2-1 M25 22l-2-1"},
    Preset{"killua", "Killua Zoldyck", 199, 0x83DEFF, 0x91A7FF, 0xE2F5FF,
        "M20 5 9 21h8l-2 10 12-16h-8l1-10Z M7 9l3 4 M26 24l3 4 M26 8l-3 3 M8 27l3-3"},
    Preset{"naruto", "Naruto Uzumaki · Kurama", 38, 0xFFC34D, 0xFF884A, 0x9CD9FF,
        "M6 7l8 6h8l8-6-2 12-4 5-6 6-6-6-4-5Z M10 17l5 3 M26 17l-5 3 M16 24h4 M18 24v3"},
};

namespace detail {
inline const Preset* activePreset = nullptr;
}

inline const Preset& preset() {
    return detail::activePreset ? *detail::activePreset : presets.front();
}

inline bool isDefaultPreset() {
    return &preset() == &presets.front();
}

inline QString currentPreset() {
    return QString::fromLatin1(preset().id);
}

inline QColor accent() {
    return QColor::fromRgb(preset().accent);
}

inline QColor secondaryAccent() {
    return QColor::fromRgb(preset().secondary);
}

inline std::array<QColor, 3> seriesColors() {
    return isDefaultPreset()
        ? std::array<QColor, 3>{QColor(0xF7607A), QColor(0xFFC682), QColor(0xC684CD)}
        : std::array<QColor, 3>{accent(), QColor::fromRgb(preset().highlight), secondaryAccent()};
}

inline QColor tint(const QColor& base) {
    if (isDefaultPreset() || !base.isValid()) {
        return base;
    }
    int hue = 0;
    int saturation = 0;
    int lightness = 0;
    base.getHsl(&hue, &saturation, &lightness);
    if (hue < 0 || lightness == 0 || lightness == 255) {
        return base;
    }
    const int variation = (hue - presets.front().accentHue + 540) % 360 - 180;
    const int themedHue = (preset().accentHue + variation / 2 + 360) % 360;
    const auto rotated = QColor::fromHsl(themedHue, saturation, lightness);
    const auto linear = [](double channel) {
        return channel <= .04045 ? channel / 12.92 : std::pow((channel + .055) / 1.055, 2.4);
    };
    const auto luminance = [](double red, double green, double blue) {
        return .2126 * red + .7152 * green + .0722 * blue;
    };
    const auto target = luminance(linear(base.redF()), linear(base.greenF()), linear(base.blueF()));
    const auto r = linear(rotated.redF());
    const auto g = linear(rotated.greenF());
    const auto b = linear(rotated.blueF());
    const auto actual = luminance(r, g, b);
    // Correct the hue shift in linear light so text and backgrounds retain their contrast.
    const auto adjust = [target, actual](double channel) {
        const auto level = target < actual ? channel * (target / actual)
            : channel + (1.0 - channel) * ((target - actual) / (1.0 - actual));
        const auto encoded = level <= .0031308 ? 12.92 * level : 1.055 * std::pow(level, 1.0 / 2.4) - .055;
        return static_cast<float>(std::clamp(encoded, 0.0, 1.0));
    };
    return QColor::fromRgbF(adjust(r), adjust(g), adjust(b), base.alphaF());
}

inline QPainterPath cutFrame(const QRectF& rect, qreal cut = 19) {
    QPainterPath path(rect.topLeft());
    path.lineTo(rect.right() - cut, rect.top());
    path.lineTo(rect.right(), rect.top() + cut);
    path.lineTo(rect.bottomRight());
    path.lineTo(rect.left() + cut, rect.bottom());
    path.lineTo(rect.left(), rect.bottom() - cut);
    path.closeSubpath();
    return path;
}

inline QFont displayFont(int pixels) {
    QFont font("Oxanium");
    font.setPixelSize(pixels);
    font.setWeight(QFont::DemiBold);
    return font;
}

inline QFont numericFont(int pixels) {
    QFont font("Ubuntu Mono");
    font.setPixelSize(pixels);
    return font;
}

namespace detail {
inline constexpr std::array iconSizes{32, 96, 192};

inline QIcon rasterIcon(QSvgRenderer& svg) {
    QIcon icon;
    if (svg.isValid()) {
        for (int size : iconSizes) {
            QPixmap raster(size, size);
            raster.fill(Qt::transparent);
            QPainter painter(&raster);
            svg.render(&painter);
            painter.end();
            icon.addPixmap(raster);
        }
    }
    return icon;
}
}

inline QIcon svgIcon(const QString& path) {
    static QHash<QString, QIcon> cache;
    const auto found = cache.constFind(path);
    if (found != cache.constEnd()) {
        return *found;
    }
    QSvgRenderer svg(path);
    const auto icon = detail::rasterIcon(svg);
    if (cache.size() >= 32) {
        cache.clear();
    }
    cache.insert(path, icon);
    return icon;
}

inline QIcon icon(const QString& name) {
    const auto path = ":/reference-icons/" + name + ".svg";
    if (isDefaultPreset()) {
        return svgIcon(path);
    }
    static const Preset* cachedPreset = nullptr;
    static QHash<QString, QIcon> cache;
    if (cachedPreset != &preset()) {
        cachedPreset = &preset();
        cache.clear();
    }
    const auto found = cache.constFind(name);
    if (found != cache.constEnd()) {
        return *found;
    }
    const auto original = svgIcon(path);
    if (original.isNull()) {
        return original;
    }
    QIcon colored;
    for (const int size : detail::iconSizes) {
        auto raster = original.pixmap(size, size);
        QPainter painter(&raster);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        QLinearGradient ink(0, 0, size, size);
        ink.setColorAt(0, accent().lighter(122));
        ink.setColorAt(1, secondaryAccent().lighter(110));
        painter.fillRect(raster.rect(), ink);
        painter.end();
        colored.addPixmap(raster);
    }
    if (cache.size() >= 24) {
        cache.clear();
    }
    cache.insert(name, colored);
    return colored;
}

inline QIcon brandIcon() {
    if (isDefaultPreset()) {
        return svgIcon(":/vivoguard.svg");
    }
    static const Preset* cachedPreset = nullptr;
    static QIcon emblem;
    if (cachedPreset != &preset()) {
        const auto mark = QStringLiteral(R"SVG(
            <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 36 36">
              <defs><linearGradient id="ink" x2="1" y2="1">
                <stop stop-color="%1"/><stop offset="1" stop-color="%2"/>
              </linearGradient></defs>
              <path d="M3 8 8 3h20l5 5v20l-5 5H8l-5-5Z" fill="%3"
                    stroke="url(#ink)" stroke-opacity=".6"/>
              <path d="M5 11V8l3-3h3 M25 31h3l3-3v-3" fill="none"
                    stroke="url(#ink)" stroke-width=".75"/>
              <path d="%4" fill="none" stroke="url(#ink)" stroke-width="1.5"
                    stroke-linecap="round" stroke-linejoin="round"/>
            </svg>)SVG").arg(accent().lighter(120).name(), secondaryAccent().name(),
                             tint(panel).name(), QString::fromLatin1(preset().emblem));
        QSvgRenderer svg(mark.toUtf8());
        emblem = detail::rasterIcon(svg);
        cachedPreset = &preset();
    }
    return emblem;
}

inline QBrush texture() {
    static const QPixmap tile(":/panel-texture.png");
    if (isDefaultPreset() || tile.isNull()) {
        return QBrush(tile);
    }
    static const Preset* cachedPreset = nullptr;
    static QPixmap colored;
    if (cachedPreset != &preset()) {
        auto image = tile.toImage().convertToFormat(QImage::Format_ARGB32);
        QHash<QRgb, QRgb> colors;
        for (int y = 0; y < image.height(); ++y) {
            auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const auto found = colors.constFind(row[x]);
                if (found == colors.constEnd()) {
                    const auto rotated = tint(QColor::fromRgba(row[x])).rgba();
                    colors.insert(row[x], rotated);
                    row[x] = rotated;
                } else {
                    row[x] = *found;
                }
            }
        }
        colored = QPixmap::fromImage(image);
        cachedPreset = &preset();
    }
    return QBrush(colored);
}

inline QLinearGradient surfaceGradient(const QRectF& bounds) {
    QLinearGradient base(bounds.topLeft(), bounds.bottomRight());
    base.setColorAt(0, tint(QColor("#211318")));
    base.setColorAt(.62, tint(panel));
    base.setColorAt(1, tint(QColor("#110F12")));
    return base;
}

inline QLinearGradient frameGradient(const QRectF& bounds) {
    QLinearGradient trim(bounds.topLeft(), bounds.bottomRight());
    trim.setColorAt(0, tint(QColor("#AA536B")));
    trim.setColorAt(.4, tint(border));
    trim.setColorAt(1, tint(QColor("#492E3D")));
    return trim;
}

inline QString styleSheet() {
    const auto base = QStringLiteral(R"QSS(
        QWidget {
            color: #FFF6F9;
            font-family: "Manrope", "Ubuntu", sans-serif;
            font-size: 14px;
        }
        QMainWindow, QWidget#root, QStackedWidget, QDialog, QMessageBox,
        QScrollArea, QScrollArea > QWidget > QWidget {
            background: #160D14;
        }
        QLabel { background: transparent; }
        QWidget#sidebar {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #1D121B,stop:0.65 #22131D,stop:1 #271722);
            border-right: 1px solid #3F2835;
        }
        QLabel#brand, QLabel#deviceTitle, QLabel#pageTitle, QLabel#sectionTitle,
        QLabel#healthStatus, QLabel#clock, QLabel#telemetryValue {
            font-family: "Oxanium";
            font-weight: 600;
        }
        QLabel#brand { font-size: 17px; }
        QLabel#deviceTitle { font-size: 16px; }
        QLabel#deviceSpecs { color: #DDCAD4; font-size: 13px; }
        QLabel#brandAccent { color: #FFB1BF; font-size: 11px; font-weight: 750; letter-spacing: 1px; }
        QLabel#pageTitle { font-size: 34px; letter-spacing: .5px; }
        QLabel#healthStatus { background: transparent; font-size: 17px; letter-spacing: 1.7px; }
        QLabel#pageSubtitle, QLabel#muted { color: #DDCAD4; }
        QLabel#sectionTitle { font-size: 18px; }
        QLabel#telemetryValue { font-size: 21px; }
        QLabel#telemetryRate { font-family: "Ubuntu Mono", monospace; font-size: 18px; font-weight: 700; }
        QLabel#telemetryDetail { color: #D8C3CE; font-size: 13px; }
        QLabel#clock { font-size: 21px; }
        QFrame#panelDivider { border: none; background: #543440; }
        QToolButton#fullscreenButton {
            background: #2A1B26;
            border: 1px solid #654052;
            border-radius: 2px;
        }
        QToolButton#fullscreenButton:hover, QToolButton#fullscreenButton:focus {
            background: #3E2434;
            border-color: #BD667D;
        }
        QToolButton#fullscreenButton:pressed { background: #572A3F; }
        QPushButton#nav {
            background: transparent;
            border: 1px solid transparent;
            color: #FFF4F9;
            min-height: 47px;
            padding: 0 12px;
            text-align: left;
            font-size: 15px;
            font-weight: 600;
        }
        QPushButton#primary, QPushButton#secondary, QPushButton#danger {
            border-radius: 2px;
            min-height: 38px;
            padding: 0 13px;
            font-weight: 800;
        }
        QPushButton#primary {
            background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #A3475B,stop:.57 #7D293F,stop:1 #67243A);
            color: #FFFFFF;
            border: 1px solid #D37387;
        }
        QPushButton#primary:hover { background: #A8485E; border-color: #EC9AAC; }
        QPushButton#secondary {
            background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #4B2C3B,stop:1 #30202B);
            color: #F7EDEF;
            border: 1px solid #955A72;
        }
        QPushButton#secondary:hover { background: #3B202A; border-color: #A44757; }
        QPushButton#primary[dashboardAction="true"], QPushButton#secondary[dashboardAction="true"] {
            min-height: 39px;
            font-size: 15px;
            font-weight: 700;
        }
        QPushButton#danger { background: #30151A; color: #FFA0AA; border: 1px solid #8C3847; }
        QPushButton#danger:hover { background: #541E29; }
        QPushButton#primary:disabled, QPushButton#secondary:disabled, QPushButton#danger:disabled {
            background: #1D171A; color: #9E838C; border-color: #3C2931;
        }
        QPushButton:focus, QLineEdit:focus, QComboBox:focus { border: 1px solid #FF7A8B; }
        QTableView, QTreeWidget {
            background: #181215;
            alternate-background-color: #21171B;
            border: 1px solid #513039;
            gridline-color: transparent;
            selection-background-color: #5B2331;
            selection-color: #FFFFFF;
        }
        QTableView::item { padding: 7px 7px; border-bottom: 1px solid #302028; }
        QTreeWidget::item { padding: 5px 7px; }
        QHeaderView::section {
            background: #2A1B22;
            color: #DEC3C9;
            border: 0;
            border-bottom: 1px solid #67404D;
            padding: 9px 8px;
            font-weight: 800;
        }
        QLineEdit, QComboBox {
            background: #21171B;
            border: 1px solid #573740;
            border-radius: 3px;
            padding: 0 10px;
            min-height: 36px;
            selection-background-color: #982B42;
            selection-color: #FFFFFF;
        }
        QProgressBar { background: #331E26; border: 0; min-height: 7px; max-height: 7px; }
        QProgressBar::chunk {
            background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #A71932,stop:1 #FF5365);
        }
        QCheckBox { spacing: 9px; }
        QCheckBox::indicator, QTreeView::indicator {
            width: 16px; height: 16px;
            border: 1px solid #A76E7C;
            border-radius: 2px;
            background: #180F14;
        }
        QCheckBox::indicator:checked, QTreeView::indicator:checked {
            background: #E7374E; border-color: #FF92A0;
        }
        QTreeView::indicator:indeterminate { background: #873446; border-color: #FF92A0; }
        QScrollBar:vertical { background: #1E1418; width: 10px; margin: 2px; }
        QScrollBar::handle:vertical { background: #744351; border-radius: 3px; min-height: 28px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QToolTip { background: #371E28; color: #FFF3F3; border: 1px solid #885260; padding: 6px; }
    )QSS");

    if (isDefaultPreset()) {
        return base;
    }
    static const QRegularExpression literalColor(QStringLiteral("#[A-Fa-f0-9]{6}\\b"));
    // Reserve the danger controls' colors for destructive actions in every theme.
    constexpr std::array dangerColors{u"#30151A", u"#FFA0AA", u"#8C3847", u"#541E29"};
    auto sheet = base;
    auto matches = literalColor.globalMatch(base);
    while (matches.hasNext()) {
        const auto match = matches.next();
        const auto color = match.capturedView();
        const bool reserved = std::ranges::any_of(dangerColors,
            [color](const auto* reservedColor) { return color == QStringView(reservedColor); });
        if (!reserved) {
            sheet.replace(match.capturedStart(), match.capturedLength(),
                tint(QColor(color.toString())).name());
        }
    }
    return sheet;
}

inline bool applyPreset(const QString& id) {
    const auto found = std::ranges::find_if(presets,
        [&id](const auto& preset) { return id == QLatin1StringView(preset.id); });
    const auto* selected = found == presets.end() ? &presets.front() : &*found;
    if (detail::activePreset == selected) {
        return false;
    }
    detail::activePreset = selected;

    QPalette palette;
    const auto backgroundColor = tint(background);
    const auto panelColor = tint(panel);
    const auto textColor = tint(text);
    const auto disabledText = tint(muted);
    palette.setColor(QPalette::Window, backgroundColor);
    palette.setColor(QPalette::WindowText, textColor);
    palette.setColor(QPalette::Base, panelColor);
    palette.setColor(QPalette::AlternateBase, tint(panelRaised));
    palette.setColor(QPalette::ToolTipBase, panelColor);
    palette.setColor(QPalette::ToolTipText, textColor);
    palette.setColor(QPalette::Text, textColor);
    palette.setColor(QPalette::Button, panelColor);
    palette.setColor(QPalette::ButtonText, textColor);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, accent());
    palette.setColor(QPalette::Highlight, tint(QColor("#7F2135")));
    palette.setColor(QPalette::HighlightedText, textColor);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    QApplication::setPalette(palette);
    qApp->setStyleSheet(styleSheet());
    return true;
}

} // namespace vg::ui::theme
