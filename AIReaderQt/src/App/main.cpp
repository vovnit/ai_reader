#include "App/SmokeScript.hpp"
#include "Common/Navigator.hpp"
#include "Library/LibraryView.hpp"
#include "Services/ChatApi.hpp"
#include "Services/Database.hpp"
#include "Services/Env.hpp"
#include "Services/Migrations.hpp"
#include "Services/Paths.hpp"
#include "Support/Files.hpp"

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>

#include <cstdio>

/// Settings the shared code reads from the environment, filled in from where
/// this copy of the app finds itself: installed, or unpacked from an AppImage
/// on whichever distribution.
static void locateFiles() {
    // The bundled dictionary sits in share/aireader beside bin/.
    QString data = QDir(QApplication::applicationDirPath()).filePath("../share/aireader");
    if (qEnvironmentVariableIsEmpty("AIREADER_DATA_DIR") && QFileInfo::exists(data + "/dictionary.sqlite3")) {
        qputenv("AIREADER_DATA_DIR", QDir::cleanPath(data).toLocal8Bit());
    }
    // The decoders for a book's pictures are gdk-pixbuf plugins. A copy of
    // the app that brings its own (an AppImage) lists them in a template with
    // the folder left open, since the folder is only known now.
    QString loaders = QDir::cleanPath(QDir(QApplication::applicationDirPath()).filePath("../lib/gdk-pixbuf-2.0/2.10.0"));
    QFile cache(loaders + "/loaders.cache.in");
    if (qEnvironmentVariableIsEmpty("GDK_PIXBUF_MODULE_FILE") && cache.open(QIODevice::ReadOnly)) {
        QByteArray listing = cache.readAll().replace("@LOADERS@", (loaders + "/loaders").toLocal8Bit());
        QString written = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/pixbuf-loaders.cache";
        QDir().mkpath(QFileInfo(written).path());
        QFile out(written);
        if (out.open(QIODevice::WriteOnly | QIODevice::Truncate) && out.write(listing) == listing.size()) {
            out.close();
            qputenv("GDK_PIXBUF_MODULE_FILE", written.toLocal8Bit());
        }
    }
    // Where this distribution keeps its certificates.
    if (qEnvironmentVariableIsEmpty("CURL_CA_BUNDLE")) {
        for (const char* path : {"/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt",
                                 "/etc/ssl/ca-bundle.pem", "/etc/ssl/cert.pem"}) {
            if (!QFileInfo::exists(path)) continue;
            qputenv("CURL_CA_BUNDLE", path);
            break;
        }
    }
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("AIReader");
    QApplication::setDesktopFileName("aireader");

    // Work done on threads comes back through GLib's main loop, which is the
    // one Qt runs on Linux. Elsewhere the answers would never arrive.
    if (!QAbstractEventDispatcher::instance()->inherits("QEventDispatcherGlib")) {
        std::fprintf(stderr, "aireader: Qt is not running a GLib event loop (is QT_NO_GLIB set?)\n");
        return 1;
    }

    locateFiles();
    ChatApi::initialize();
    Paths::prepare();
    Database database(Paths::database());
    if (!Migrations::migrate(database)) {
        std::fprintf(stderr, "aireader: could not open %s: %s\n", Paths::database().c_str(), database.lastError().c_str());
        return 1;
    }
    Env env(database, Paths::settings());
    // Written with its defaults straight away, so it can be edited by hand
    // before anything has been changed here.
    if (!Files::exists(Paths::settings())) {
        env.settings.saveAi(env.settings.ai());
        env.settings.saveStyle(env.settings.style());
    }

    // `--endpoint` and `--model` override the saved settings for this run
    // only, which is how a test run is pointed at `mock://ai`.
    QStringList arguments = QApplication::arguments();
    auto argument = [&](const QString& name) {
        int index = arguments.indexOf(name);
        return index >= 0 && index + 1 < arguments.size() ? arguments[index + 1].toStdString() : std::string();
    };
    env.settings.overrideForRun(argument("--endpoint"), argument("--model"));

    Navigator navigator;
    navigator.setWindowTitle("AIReader");
    navigator.resize(900, 1000);
    navigator.push(new LibraryView(env, navigator));
    navigator.show();
    SmokeScript::start(&navigator);
    return app.exec();
}
