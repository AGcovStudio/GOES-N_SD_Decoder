// sdworker.h - plays a baseband recording through the GOES SD receive chain on its own thread and reports to the window.
#pragma once
#include "goessd.h"

#include <QImage>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>
#include <complex>
#include <memory>
#include <vector>

struct SdStats {
    bool imagerLocked = false, sounderLocked = false;
    float lockQuality = 0.f;       // carrier measure, 0..1
    float carrierHz = 0.f;
    double snrDb = 0.0;
    double syncBitErrorPct = 0.0, sounderParityPct = 0.0;
    qulonglong syncs = 0, blocks = 0, scans = 0, directionFixes = 0, sounderFrames = 0, widthOutliers = 0;
    int scanWidth = 0;
};
Q_DECLARE_METATYPE(SdStats)
Q_DECLARE_METATYPE(std::vector<std::complex<float>>)
Q_DECLARE_METATYPE(std::vector<float>)

class SdWorker : public QObject {
    Q_OBJECT
public:
    static constexpr double kMinSampleRate = 1.3 * nadirstation::goessd::kImagerRate;   // about 3.41 MS/s

    explicit SdWorker(QObject* parent = nullptr);
    ~SdWorker() override;
    void requestStop() { stop_.store(true); }   // thread-safe

public slots:
    void runFile(const QString& inputPath, double manualRateHz, bool preferManual, const QString& outputDir, bool softDecisions);

signals:
    void inputInfo(const QString& description, double sampleRate);
    void logLine(const QString& text);
    void progress(int permille);
    void statsChanged(const SdStats& stats);
    void constellation(const std::vector<std::complex<float>>& points);
    void spectrum(const std::vector<float>& db, double sampleRate);
    void livePreview(const QImage& image, const QString& stamp);
    void observationDone(const QString& stamp, const QStringList& paths);
    void finished(bool ok, const QString& summary);

private:
    void processBlock(const std::vector<std::complex<float>>& iq, double sampleRate);
    void tick(bool force);
    void publishResults();

    std::unique_ptr<nadirstation::goessd::Engine> engine_;
    std::atomic<bool> stop_{false};
    std::vector<std::complex<float>> lastSymbols_;
    qint64 lastTickMs_ = 0, lastSpectrumMs_ = 0, lastLogMs_ = 0, lastSyncMs_ = 0;
    uint64_t lastSyncs_ = 0;
    int observations_ = 0;
};
