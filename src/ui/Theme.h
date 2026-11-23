#pragma once

#include <QColor>
#include <QFont>
#include <QHash>
#include <QIcon>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QString>
#include <QSvgRenderer>

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

inline QIcon svgIcon(const QString& path) {
    static QHash<QString, QIcon> cache;
    const auto found = cache.constFind(path);
    if (found != cache.constEnd()) {
        return *found;
    }
    QSvgRenderer svg(path);
    QIcon icon;
    if (svg.isValid()) {
        for (int size : {32, 96, 192}) {
            QPixmap raster(size, size);
            raster.fill(Qt::transparent);
            QPainter painter(&raster);
            svg.render(&painter);
            painter.end();
            icon.addPixmap(raster);
        }
    }
    cache.insert(path, icon);
    return icon;
}

inline QIcon icon(const QString& name) {
    return svgIcon(":/reference-icons/" + name + ".svg");
}

inline QBrush texture() {
    static const QPixmap tile(":/panel-texture.png");
    return QBrush(tile);
}

inline QLinearGradient surfaceGradient(const QRectF& bounds) {
    QLinearGradient base(bounds.topLeft(), bounds.bottomRight());
    base.setColorAt(0, QColor("#211318"));
    base.setColorAt(.62, panel);
    base.setColorAt(1, QColor("#110F12"));
    return base;
}

inline QLinearGradient frameGradient(const QRectF& bounds) {
    QLinearGradient trim(bounds.topLeft(), bounds.bottomRight());
    trim.setColorAt(0, QColor("#AA536B"));
    trim.setColorAt(.4, border);
    trim.setColorAt(1, QColor("#492E3D"));
    return trim;
}

inline QString styleSheet() {
    return QStringLiteral(R"QSS(
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
}

} // namespace vg::ui::theme
