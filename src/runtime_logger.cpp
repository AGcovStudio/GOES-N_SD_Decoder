#include "runtime_logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>
#include <mutex>

namespace nadirstation::log {
namespace {
std::mutex gMutex;
QString gPath;

QString defaultPath() {
    const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                            .filePath(QStringLiteral("GOES-N_SD_Decoder"));
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("runtime.log"));
}

void append(const QString& line) {
    if (gPath.isEmpty()) gPath = defaultPath();
    // keep the file small: start over once it passes 4 MB
    if (QFileInfo(gPath).size() > (4 << 20)) QFile::remove(gPath);
    QFile f(gPath);
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream s(&f);
    s.setEncoding(QStringConverter::Utf8);
    s << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")) << "  " << line << '\n';
}
}  // namespace

void init() {
    std::lock_guard<std::mutex> l(gMutex);
    if (gPath.isEmpty()) gPath = defaultPath();
    append(QStringLiteral("---- session start ----"));
}

void write(const QString& message) {
    std::lock_guard<std::mutex> l(gMutex);
    append(message);
}

QString path() {
    std::lock_guard<std::mutex> l(gMutex);
    if (gPath.isEmpty()) gPath = defaultPath();
    return gPath;
}
}  // namespace nadirstation::log
