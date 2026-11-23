#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "ui/TelemetryWorker.h"

#include <QApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QStyleFactory>
#include <QSystemTrayIcon>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--telemetry-worker") {
        return vg::ui::TelemetryWorker::runHelper();
    }
    QApplication application(argc, argv);
    QFontDatabase::addApplicationFont(":/fonts/Oxanium-SemiBold.ttf");
    QApplication::setApplicationName(vg::ui::theme::appName);
    QApplication::setApplicationDisplayName(vg::ui::theme::appName);
    QApplication::setDesktopFileName("vivoguard");
    QApplication::setApplicationVersion(VIVOGUARD_VERSION);
    QApplication::setWindowIcon(QIcon(":/vivoguard.png"));
    QApplication::setQuitOnLastWindowClosed(
        !QSystemTrayIcon::isSystemTrayAvailable());

    if (auto* fusion = QStyleFactory::create("Fusion")) {
        QApplication::setStyle(fusion);
    }

    QPalette darkPalette;
    const auto& darkBg = vg::ui::theme::background;
    const auto& panelBg = vg::ui::theme::panel;
    const auto& panelRaised = vg::ui::theme::panelRaised;
    const auto& textLight = vg::ui::theme::text;
    const auto& textMuted = vg::ui::theme::muted;

    darkPalette.setColor(QPalette::Window, darkBg);
    darkPalette.setColor(QPalette::WindowText, textLight);
    darkPalette.setColor(QPalette::Base, panelBg);
    darkPalette.setColor(QPalette::AlternateBase, panelRaised);
    darkPalette.setColor(QPalette::ToolTipBase, panelBg);
    darkPalette.setColor(QPalette::ToolTipText, textLight);
    darkPalette.setColor(QPalette::Text, textLight);
    darkPalette.setColor(QPalette::Button, panelBg);
    darkPalette.setColor(QPalette::ButtonText, textLight);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, vg::ui::theme::primary);
    darkPalette.setColor(QPalette::Highlight, QColor("#7F2135"));
    darkPalette.setColor(QPalette::HighlightedText, textLight);
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, textMuted);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, textMuted);
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, textMuted);
    QApplication::setPalette(darkPalette);

    application.setStyleSheet(vg::ui::theme::styleSheet());

    vg::ui::MainWindow window;
    const bool trayMode =
        application.arguments().contains("--tray");
    if (!trayMode || !QSystemTrayIcon::isSystemTrayAvailable()) {
        window.show();
    }
    return application.exec();
}
