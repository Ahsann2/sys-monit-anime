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
    QApplication::setDesktopFileName("sys-monit-anime");
    QApplication::setApplicationVersion(VIVOGUARD_VERSION);
    QApplication::setWindowIcon(QIcon(":/vivoguard.png"));
    QApplication::setQuitOnLastWindowClosed(
        !QSystemTrayIcon::isSystemTrayAvailable());

    if (auto* fusion = QStyleFactory::create("Fusion")) {
        QApplication::setStyle(fusion);
    }

    vg::ui::MainWindow window;
    const bool trayMode =
        application.arguments().contains("--tray");
    if (!trayMode || !QSystemTrayIcon::isSystemTrayAvailable()) {
        window.show();
    }
    return application.exec();
}
