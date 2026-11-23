#include <QCoreApplication>
#include <QDataStream>
#include <QElapsedTimer>
#include <QProcess>
#include <QtEndian>
#include <iostream>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().size() != 2) {
        return 1;
    }
    QProcess helper;
    helper.start(app.arguments().at(1), {"--telemetry-worker"});
    if (!helper.waitForStarted(5000)) {
        std::cerr << helper.errorString().toStdString() << '\n';
        return 2;
    }
    bool valid = true;
    for (const auto* command : {"0\n", "1\n"}) {
        if (helper.write(command) != 2 || !helper.waitForBytesWritten(2500)) {
            valid = false;
            break;
        }
        QByteArray response;
        QElapsedTimer deadline;
        deadline.start();
        quint32 payload = 0;
        while (deadline.elapsed() < 6500) {
            const auto remaining = static_cast<int>(6500 - deadline.elapsed());
            if (remaining <= 0 || !helper.waitForReadyRead(remaining)) {
                break;
            }
            response += helper.readAllStandardOutput();
            if (response.size() >= 4) {
                payload = qFromBigEndian<quint32>(response.constData());
                if (payload == 0 || payload > 16 * 1024 * 1024 || response.size() >= 4 + payload) {
                    break;
                }
            }
        }
        if (payload == 0 || response.size() != 4 + payload) {
            valid = false;
            break;
        }
        QDataStream stream(response.sliced(4));
        stream.setVersion(QDataStream::Qt_6_0);
        QString error;
        stream >> error;
        if (stream.status() != QDataStream::Ok || !error.isEmpty() || stream.atEnd()) {
            std::cerr << error.toStdString() << '\n';
            valid = false;
            break;
        }
    }
    helper.closeWriteChannel();
    if (!helper.waitForFinished(3500)) {
        helper.kill();
        helper.waitForFinished();
        valid = false;
    }
    if (!valid || helper.exitStatus() != QProcess::NormalExit || helper.exitCode() != 0) {
        std::cerr << "Telemetry worker binary protocol failed: "
                  << helper.readAllStandardError().constData() << '\n';
        return 3;
    }
    return 0;
}
