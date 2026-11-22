// recordingtime.h - the UTC start time of a recording from its file name, e.g. SatDump / SDR++ style
// "2026-11-22_02-52-07_6000000SPS_1676000000Hz.cs16". Invalid QDateTime when the name carries no time.
#pragma once
#include <QDateTime>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimeZone>

inline QDateTime recordingStartFromName(const QString& path) {
    static const QRegularExpression re(QStringLiteral("(\\d{4})-(\\d{2})-(\\d{2})[_T ](\\d{2})[-:.](\\d{2})[-:.](\\d{2})"));
    const auto m = re.match(QFileInfo(path).fileName());
    if (!m.hasMatch()) return {};
    const QDate d(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
    const QTime t(m.captured(4).toInt(), m.captured(5).toInt(), m.captured(6).toInt());
    if (!d.isValid() || !t.isValid()) return {};
    return QDateTime(d, t, QTimeZone::UTC);
}
