// goessd_cli - play a baseband recording through the GOES SD receive chain without the window.
//   goessd_cli <recording> [sample rate in Hz] [output folder] [--hard]
// Prints the link statistics and writes the pictures the same way the application does.
#include "goessd.h"
#include "iqreader.h"
#include "qtpath.h"
#include "recordingtime.h"

#include <QCoreApplication>
#include <QDir>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    bool hard = false;
    std::vector<const char*> args;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--hard") == 0) hard = true;
        else args.push_back(argv[i]);
    }
    if (args.empty()) {
        std::fprintf(stderr, "usage: goessd_cli <recording .cs8|.cu8|.cs16|.cf32|.wav> [sample rate Hz] [output folder] [--hard]\n");
        return 2;
    }
    const double manualRate = args.size() > 1 ? std::atof(args[1]) : 0.0;
    const QString outDir = args.size() > 2 ? QString::fromLocal8Bit(args[2]) : QDir::current().filePath(QStringLiteral("goessd_out"));

    sdapp::IqReader reader;
    std::string error;
    if (!reader.open(toFsPath(QString::fromLocal8Bit(args[0])), manualRate, error, manualRate > 0.0)) {
        std::fprintf(stderr, "cannot open: %s\n", error.c_str());
        return 1;
    }
    const auto info = reader.info();
    std::printf("%s, %.3f MS/s, %.1f s\n", sdapp::formatName(info.format), info.sampleRate / 1e6,
                static_cast<double>(info.frames) / info.sampleRate);

    nadirstation::goessd::Engine engine;
    QDir().mkpath(outDir);
    engine.setOutputRoot(outDir);
    engine.setSoftDecisions(!hard);
    engine.setRecordingStart(recordingStartFromName(QString::fromLocal8Bit(args[0])));
    if (!engine.configure(info.sampleRate)) {
        std::fprintf(stderr, "sample rate too low: GOES SD needs at least 3.41 MS/s (6 MS/s recommended)\n");
        return 1;
    }
    engine.reset();
    std::vector<std::complex<float>> block;
    int observations = 0, k = 0;
    auto count = [&] {
        for (auto& done : engine.takeCompleted()) {
            if (!done.message.isEmpty()) std::printf("%s\n", done.message.toLocal8Bit().constData());
            if (!done.paths.isEmpty()) ++observations;
        }
    };
    const auto t0 = std::chrono::steady_clock::now();
    while (reader.read(block, 1 << 19) > 0) {
        engine.process(block);
        count();
        if (++k % 12 == 0)
            std::printf("t=%6.1f s  locked=%d  offset=%6.0f Hz  SNR=%4.1f dB  blocks=%llu  sync BER=%4.1f%%  scans=%llu  sounder=%llu\n",
                        static_cast<double>(reader.position()) / info.sampleRate, int(engine.imagerLocked()), engine.carrierOffsetHz(),
                        engine.snrDb(), static_cast<unsigned long long>(engine.blocks()), engine.syncBitErrorPct(),
                        static_cast<unsigned long long>(engine.scans()), static_cast<unsigned long long>(engine.sounderFrames()));
    }
    engine.finish();
    count();
    const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("blocks %llu, scans %llu, direction fixes %llu, sounder frames %llu (parity %.1f%%), sync BER %.1f%%, pictures %d, %.1f s\n",
                static_cast<unsigned long long>(engine.blocks()), static_cast<unsigned long long>(engine.scans()),
                static_cast<unsigned long long>(engine.directionFixes()), static_cast<unsigned long long>(engine.sounderFrames()),
                engine.sounderParityPct(), engine.syncBitErrorPct(), observations, el);
    return engine.blocks() > 0 ? 0 : 3;
}
