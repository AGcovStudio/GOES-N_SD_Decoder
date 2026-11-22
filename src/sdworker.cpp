#include "sdworker.h"

#include "iqreader.h"
#include "qtpath.h"
#include "recordingtime.h"
#include "runtime_logger.h"

#include <QDateTime>
#include <QDir>
#include <algorithm>

namespace {
constexpr std::size_t kSpectrumBins = 2048;
qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }
}  // namespace

SdWorker::SdWorker(QObject* parent) : QObject(parent) {}
SdWorker::~SdWorker() = default;

void SdWorker::processBlock(const std::vector<std::complex<float>>& iq, double sampleRate) {
    if (iq.empty() || !engine_) return;
    const auto d = engine_->process(iq);
    if (!d.symbols.empty()) {
        lastSymbols_.insert(lastSymbols_.end(), d.symbols.begin(), d.symbols.end());
        if (lastSymbols_.size() > 2500) lastSymbols_.erase(lastSymbols_.begin(), lastSymbols_.end() - 2500);
    }
    const qint64 now = nowMs();
    if (now - lastSpectrumMs_ >= 80 && iq.size() >= kSpectrumBins) {
        lastSpectrumMs_ = now;
        const auto sp = nadirstation::dsp::spectrum(iq, static_cast<float>(sampleRate), kSpectrumBins);
        if (!sp.db.empty()) emit spectrum(sp.db, sampleRate);
    }
    tick(false);
}

void SdWorker::publishResults() {
    auto live = engine_->takeLive();
    if (!live.live.isNull()) emit livePreview(live.live, live.stamp);
    if (!live.message.isEmpty()) emit logLine(live.message);
    for (auto& done : engine_->takeCompleted()) {
        if (!done.message.isEmpty()) emit logLine(done.message);
        if (!done.paths.isEmpty()) {
            ++observations_;
            emit observationDone(done.stamp, done.paths);
        }
    }
}

void SdWorker::tick(bool force) {
    const qint64 now = nowMs();
    if (!force && lastTickMs_ != 0 && now - lastTickMs_ < 150) return;
    lastTickMs_ = now;

    SdStats s;
    s.syncs = engine_->syncs();
    s.blocks = engine_->blocks();
    s.scans = engine_->scans();
    s.directionFixes = engine_->directionFixes();
    s.sounderFrames = engine_->sounderFrames();
    s.widthOutliers = engine_->widthOutliers();
    s.scanWidth = engine_->scanWidth();
    s.snrDb = engine_->snrDb();
    s.syncBitErrorPct = engine_->syncBitErrorPct();
    s.sounderParityPct = engine_->sounderParityPct();
    s.lockQuality = std::clamp(engine_->lockMeasure(), 0.f, 1.f);
    s.carrierHz = engine_->carrierOffsetHz();
    if (s.syncs != lastSyncs_) { lastSyncs_ = s.syncs; lastSyncMs_ = now; }
    s.imagerLocked = engine_->imagerLocked();
    s.sounderLocked = engine_->sounderLocked();
    emit statsChanged(s);
    if (!lastSymbols_.empty()) emit constellation(lastSymbols_);
    publishResults();

    if (lastLogMs_ == 0 || now - lastLogMs_ >= 5000) {
        lastLogMs_ = now;
        const QString m = QStringLiteral("成像仪块 %1 · 同步位误码 %2% · 扫描 %3 · 方向翻转 %4 · 探测器帧 %5（奇偶校验 %6%）· 信噪比 %7 dB")
                              .arg(s.blocks).arg(s.syncBitErrorPct, 0, 'f', 1).arg(s.scans).arg(s.directionFixes)
                              .arg(s.sounderFrames).arg(s.sounderParityPct, 0, 'f', 0).arg(s.snrDb, 0, 'f', 1);
        emit logLine(m);
        nadirstation::log::write(m);
    }
}

void SdWorker::runFile(const QString& inputPath, double manualRateHz, bool preferManual, const QString& outputDir, bool softDecisions) {
    stop_.store(false);
    sdapp::IqReader reader;
    std::string error;
    if (!reader.open(toFsPath(inputPath), manualRateHz, error, preferManual)) {
        emit finished(false, QString::fromUtf8(error.c_str()));
        return;
    }
    const sdapp::IqInfo info = reader.info();
    const double seconds = static_cast<double>(info.frames) / info.sampleRate;
    emit inputInfo(QStringLiteral("%1 · %2 MS/s（%3）· %4 秒")
                       .arg(QString::fromUtf8(sdapp::formatName(info.format)))
                       .arg(info.sampleRate / 1e6, 0, 'f', 3)
                       .arg(QString::fromUtf8(info.rateSource.c_str()))
                       .arg(seconds, 0, 'f', 1),
                   info.sampleRate);
    if (info.sampleRate < kMinSampleRate) {
        emit finished(false, QStringLiteral("采样率 %1 MS/s 太低：GOES SD 成像仪（2.6208 Mbit/s）至少需要 %2 MS/s，建议 6 MS/s。")
                                 .arg(info.sampleRate / 1e6, 0, 'f', 3).arg(kMinSampleRate / 1e6, 0, 'f', 2));
        return;
    }
    if (!QDir().mkpath(outputDir)) {
        emit finished(false, QStringLiteral("无法创建输出文件夹：%1").arg(outputDir));
        return;
    }
    engine_ = std::make_unique<nadirstation::goessd::Engine>();
    engine_->setOutputRoot(outputDir);
    engine_->setSoftDecisions(softDecisions);
    const QDateTime start = recordingStartFromName(inputPath);
    engine_->setRecordingStart(start);
    emit logLine(start.isValid() ? QStringLiteral("录音开始时间（取自文件名，按 UTC）：%1").arg(start.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
                                 : QStringLiteral("文件名里没有时间，输出文件夹按解码时的 UTC 时间命名。"));
    if (!engine_->configure(info.sampleRate)) {
        emit finished(false, QStringLiteral("解调器无法用 %1 MS/s 的采样率工作。").arg(info.sampleRate / 1e6, 0, 'f', 3));
        return;
    }
    engine_->reset();
    lastSymbols_.clear();
    lastTickMs_ = lastSpectrumMs_ = lastLogMs_ = lastSyncMs_ = 0;
    lastSyncs_ = 0;
    observations_ = 0;
    emit logLine(QStringLiteral("开始解码：%1 MS/s，约 %2 秒的数据。").arg(info.sampleRate / 1e6, 0, 'f', 3).arg(seconds, 0, 'f', 1));

    std::vector<std::complex<float>> block;
    constexpr std::size_t kBlockFrames = 1 << 19;
    bool stopped = false;
    int lastPermille = -1;
    while (reader.read(block, kBlockFrames) > 0) {
        if (stop_.load()) { stopped = true; break; }
        processBlock(block, info.sampleRate);
        const int permille = static_cast<int>(1000.0 * static_cast<double>(reader.position()) / static_cast<double>(info.frames));
        if (permille != lastPermille) { lastPermille = permille; emit progress(permille); }
    }
    emit logLine(QStringLiteral("正在保存图像…"));
    engine_->finish();
    publishResults();
    tick(true);
    QString summary = QStringLiteral("%1：成像仪块 %2 · 扫描 %3 · 探测器帧 %4 · 输出 %5 幅图")
                          .arg(stopped ? QStringLiteral("已停止") : QStringLiteral("解码完成"))
                          .arg(engine_->blocks()).arg(engine_->scans()).arg(engine_->sounderFrames()).arg(observations_);
    if (engine_->blocks() == 0)
        summary += QStringLiteral("\n没有找到成像仪同步：请确认录音中心频率（1676.0 MHz）、采样率，以及信号是否足够强。");
    else if (observations_ == 0)
        summary += QStringLiteral("\n收到的扫描还不够成图（至少需要 4 条扫描，约 5 秒）。");
    nadirstation::log::write(summary);
    emit progress(stopped ? std::max(lastPermille, 0) : 1000);
    emit finished(true, summary);
}
