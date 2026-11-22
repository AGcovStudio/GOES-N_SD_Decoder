// runtime_logger.h - a small log file next to the user's documents, so a problem report can include what happened.
#pragma once
#include <QString>

namespace nadirstation::log {
void init();                       // choose the file and start a new session line (safe to call twice)
void write(const QString& message);
QString path();
}  // namespace nadirstation::log
