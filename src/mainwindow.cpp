#include "mainwindow.h"

#include "appinfo.h"
#include "constellationwidget.h"
#include "iqreader.h"
#include "qtpath.h"
#include "spectrumview.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>

namespace {

bool isSupportedRecording(const QString& path) {
    sdapp::IqFormat f;
    return sdapp::formatForPath(toFsPath(path), f);
}

QString humanSize(qint64 bytes) {
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    return mb >= 1024.0 ? QStringLiteral("%1 GB").arg(mb / 1024.0, 0, 'f', 2) : QStringLiteral("%1 MB").arg(mb, 0, 'f', 1);
}

QLabel* valueLabel() {
    auto* l = new QLabel(QStringLiteral("—"));
    l->setObjectName(QStringLiteral("Value"));
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

QLabel* hint(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName(QStringLiteral("Muted"));
    l->setWordWrap(true);
    return l;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("%1 %2").arg(QString::fromUtf8(appinfo::kProductName), QString::fromUtf8(appinfo::kVersion)));
    setWindowIcon(QIcon(QStringLiteral(":/icon.png")));
    setAcceptDrops(true);
    resize(1240, 860);
    setMinimumSize(1200, 820);
    buildUi();

    worker_ = new SdWorker;
    worker_->moveToThread(&thread_);
    connect(this, &MainWindow::startFileRequested, worker_, &SdWorker::runFile);
    connect(worker_, &SdWorker::inputInfo, this, [this](const QString& text, double) { inputInfo_->setText(text); });
    connect(worker_, &SdWorker::logLine, this, &MainWindow::onLog);
    connect(worker_, &SdWorker::progress, this, &MainWindow::onProgress);
    connect(worker_, &SdWorker::statsChanged, this, &MainWindow::onStats);
    connect(worker_, &SdWorker::constellation, this,
            [this](const std::vector<std::complex<float>>& points) { constellation_->setPoints(points); });
    connect(worker_, &SdWorker::spectrum, this,
            [this](const std::vector<float>& db, double rate) { spectrum_->setSpectrum(db, rate); });
    connect(worker_, &SdWorker::livePreview, this, &MainWindow::onLivePreview);
    connect(worker_, &SdWorker::observationDone, this, &MainWindow::onObservationDone);
    connect(worker_, &SdWorker::finished, this, &MainWindow::onFinished);
    thread_.start();

    QSettings settings;
    outputEdit_->setText(settings.value(QStringLiteral("outputDir"), outputRoot()).toString());
    rateSpin_->setValue(settings.value(QStringLiteral("manualRateMsps"), 6.0).toDouble());
    softBox_->setChecked(settings.value(QStringLiteral("softDecisions"), true).toBool());
    statusBar()->showMessage(QStringLiteral("就绪：选择 1676 MHz 的基带录音。"));
}

MainWindow::~MainWindow() {
    if (worker_) worker_->requestStop();
    thread_.quit();
    thread_.wait();
    delete worker_;   // its thread has finished, so deleting it from here is safe
}

void MainWindow::buildUi() {
    auto* central = new QWidget;
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(12, 12, 12, 8);
    root->setSpacing(12);

    // ---- left: the recording ----------------------------------------------------------------------------------
    auto* left = new QWidget;
    left->setFixedWidth(344);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    auto* title = new QLabel(QStringLiteral("GOES SD 基带回放解码"));
    title->setObjectName(QStringLiteral("AppTitle"));
    leftLayout->addWidget(title);
    leftLayout->addWidget(hint(QStringLiteral("GOES-N/O/P 1676 MHz 传感器数据（Sensor Data）录音 → 成像仪可见光、红外和探测器数据")));

    auto* inputBox = new QGroupBox(QStringLiteral("1  基带录音"));
    auto* fileLayout = new QVBoxLayout(inputBox);
    auto* inputRow = new QHBoxLayout;
    inputEdit_ = new QLineEdit;
    inputEdit_->setPlaceholderText(QStringLiteral("拖入 .cs8 / .cu8 / .cs16 / .cf32 / .wav"));
    auto* browseIn = new QPushButton(QStringLiteral("浏览…"));
    inputRow->addWidget(inputEdit_, 1);
    inputRow->addWidget(browseIn);
    fileLayout->addLayout(inputRow);
    inputInfo_ = hint(QStringLiteral("尚未选择文件。"));
    fileLayout->addWidget(inputInfo_);
    auto* rateRow = new QHBoxLayout;
    manualRate_ = new QCheckBox(QStringLiteral("手动指定采样率"));
    rateSpin_ = new QDoubleSpinBox;
    rateSpin_->setDecimals(6);
    rateSpin_->setRange(0.5, 100.0);
    rateSpin_->setValue(6.0);
    rateSpin_->setSuffix(QStringLiteral(" MS/s"));
    rateSpin_->setEnabled(false);
    rateRow->addWidget(manualRate_);
    rateRow->addWidget(rateSpin_, 1);
    fileLayout->addLayout(rateRow);
    fileLayout->addWidget(hint(QStringLiteral("cs8 / cu8 / cs16 / cf32 文件本身不带采样率：先看同名 .txt 里的 rate、再看文件名里的“…SPS”，都没有时用这里的值。WAV 用文件头里的采样率。采样率须不低于 3.41 MS/s，建议 6 MS/s，中心频率 1676.0 MHz。")));
    softBox_ = new QCheckBox(QStringLiteral("软判决成像（弱信号推荐）"));
    softBox_->setChecked(true);
    fileLayout->addWidget(softBox_);
    leftLayout->addWidget(inputBox);

    auto* outBox = new QGroupBox(QStringLiteral("2  输出文件夹"));
    auto* outLayout = new QVBoxLayout(outBox);
    auto* outRow = new QHBoxLayout;
    outputEdit_ = new QLineEdit;
    auto* browseOut = new QPushButton(QStringLiteral("浏览…"));
    outRow->addWidget(outputEdit_, 1);
    outRow->addWidget(browseOut);
    outLayout->addLayout(outRow);
    openOutBtn_ = new QPushButton(QStringLiteral("打开输出文件夹"));
    outLayout->addWidget(openOutBtn_);
    outLayout->addWidget(hint(QStringLiteral("每段图像写入 <输出文件夹>\\GOES SD\\<UTC 时间>\\。多段可用 tools\\stitch_segments.py 拼成整盘。")));
    leftLayout->addWidget(outBox);

    leftLayout->addStretch(1);
    startBtn_ = new QPushButton(QStringLiteral("开始解码"));
    startBtn_->setObjectName(QStringLiteral("Primary"));
    startBtn_->setMinimumHeight(38);
    progress_ = new QProgressBar;
    progress_->setRange(0, 1000);
    progress_->setValue(0);
    progress_->setTextVisible(false);
    progress_->setFixedHeight(8);
    leftLayout->addWidget(startBtn_);
    leftLayout->addWidget(progress_);

    // ---- right: what is happening and what came out ----------------------------------------------------------
    auto* right = new QWidget;
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    auto* topRow = new QHBoxLayout;
    topRow->setSpacing(10);
    auto* constBox = new QGroupBox(QStringLiteral("星座图"));
    auto* constLayout = new QVBoxLayout(constBox);
    constellation_ = new ConstellationWidget;
    constellation_->setAnnotation(QStringLiteral("BPSK"), QStringLiteral("成像仪 2.6208 Mbit/s · NRZ-M"));
    constellation_->setFixedSize(288, 288);
    constLayout->addWidget(constellation_);
    topRow->addWidget(constBox);

    auto* previewBox = new QGroupBox(QStringLiteral("实时预览"));
    auto* previewLayout = new QVBoxLayout(previewBox);
    previewLabel_ = new QLabel(QStringLiteral("等待图像数据…"));
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setFixedSize(288, 288);
    previewLabel_->setObjectName(QStringLiteral("Preview"));
    previewCaption_ = new QLabel;
    previewCaption_->setObjectName(QStringLiteral("Muted"));
    previewLayout->addWidget(previewLabel_);
    previewLayout->addWidget(previewCaption_);
    topRow->addWidget(previewBox);

    auto* statBox = new QGroupBox(QStringLiteral("链路状态"));
    auto* grid = new QGridLayout(statBox);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(8);
    const auto addStat = [&](int r, const QString& name, QLabel*& value) {
        auto* n = new QLabel(name);
        n->setObjectName(QStringLiteral("Muted"));
        value = valueLabel();
        grid->addWidget(n, r, 0);
        grid->addWidget(value, r, 1);
    };
    addStat(0, QStringLiteral("成像仪同步"), lockValue_);
    addStat(1, QStringLiteral("频偏"), offsetValue_);
    addStat(2, QStringLiteral("信噪比（Es/N0 估计）"), snrValue_);
    addStat(3, QStringLiteral("同步位误码率"), berValue_);
    addStat(4, QStringLiteral("成像仪块"), blockValue_);
    addStat(5, QStringLiteral("扫描条数 / 宽度"), scanValue_);
    addStat(6, QStringLiteral("扫描方向翻转"), flipValue_);
    addStat(7, QStringLiteral("探测器帧（奇偶校验）"), sounderValue_);
    grid->setRowStretch(8, 1);
    grid->setColumnStretch(1, 1);
    topRow->addWidget(statBox, 1);
    rightLayout->addLayout(topRow);

    auto* specBox = new QGroupBox(QStringLiteral("频谱"));
    auto* specLayout = new QVBoxLayout(specBox);
    spectrum_ = new SpectrumView;
    spectrum_->setFixedHeight(140);
    spectrum_->setSignalWidthMHz(5.0);
    specLayout->addWidget(spectrum_);
    rightLayout->addWidget(specBox);

    auto* productBox = new QGroupBox(QStringLiteral("已输出的图像（双击打开文件夹）"));
    auto* productLayout = new QVBoxLayout(productBox);
    products_ = new QListWidget;
    productLayout->addWidget(products_);
    rightLayout->addWidget(productBox, 1);

    auto* logBox = new QGroupBox(QStringLiteral("日志"));
    auto* logLayout = new QVBoxLayout(logBox);
    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(2000);
    log_->setFixedHeight(100);
    logLayout->addWidget(log_);
    rightLayout->addWidget(logBox);

    root->addWidget(left);
    root->addWidget(right, 1);
    setCentralWidget(central);

    auto* credit = new QLabel(QStringLiteral("开发者：%1 · v%2").arg(QString::fromUtf8(appinfo::kDeveloper), QString::fromUtf8(appinfo::kVersion)));
    credit->setObjectName(QStringLiteral("Credit"));
    auto* agreementBtn = new QPushButton(QStringLiteral("用户协议与免责声明"));
    agreementBtn->setObjectName(QStringLiteral("Link"));
    agreementBtn->setFlat(true);
    agreementBtn->setCursor(Qt::PointingHandCursor);
    auto* aboutBtn = new QPushButton(QStringLiteral("关于"));
    aboutBtn->setObjectName(QStringLiteral("Link"));
    aboutBtn->setFlat(true);
    aboutBtn->setCursor(Qt::PointingHandCursor);
    statusBar()->addPermanentWidget(credit);
    statusBar()->addPermanentWidget(agreementBtn);
    statusBar()->addPermanentWidget(aboutBtn);
    statusBar()->setSizeGripEnabled(false);

    connect(browseIn, &QPushButton::clicked, this, &MainWindow::browseInput);
    connect(browseOut, &QPushButton::clicked, this, &MainWindow::browseOutput);
    connect(openOutBtn_, &QPushButton::clicked, this, &MainWindow::openOutputFolder);
    connect(startBtn_, &QPushButton::clicked, this, &MainWindow::toggleRun);
    connect(agreementBtn, &QPushButton::clicked, this, &MainWindow::showAgreement);
    connect(aboutBtn, &QPushButton::clicked, this, &MainWindow::showAbout);
    connect(inputEdit_, &QLineEdit::editingFinished, this, &MainWindow::inspectInput);
    connect(manualRate_, &QCheckBox::toggled, this, [this](bool on) {
        rateSpin_->setEnabled(on);
        inspectInput();
    });
    connect(rateSpin_, &QDoubleSpinBox::editingFinished, this, &MainWindow::inspectInput);
    connect(products_, &QListWidget::itemActivated, this, [](QListWidgetItem* item) {
        const QString dir = item->data(Qt::UserRole).toString();
        if (!dir.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
}

QString MainWindow::outputRoot() const {
    const QString typed = outputEdit_ ? outputEdit_->text().trimmed() : QString();
    if (!typed.isEmpty()) return typed;
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("GOES-N_SD_Decoder"));
}

void MainWindow::browseInput() {
    QSettings settings;
    const QString start = settings.value(QStringLiteral("inputDir"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择基带录音"), start,
                                                      QStringLiteral("基带录音 (*.cs8 *.cu8 *.u8 *.iq *.cs16 *.cf32 *.fc32 *.cfile *.wav);;所有文件 (*)"));
    if (path.isEmpty()) return;
    settings.setValue(QStringLiteral("inputDir"), QFileInfo(path).absolutePath());
    setInputPath(path);
}

void MainWindow::browseOutput() {
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出文件夹"), outputRoot());
    if (!dir.isEmpty()) outputEdit_->setText(QDir::toNativeSeparators(dir));
}

void MainWindow::openOutputFolder() {
    const QString dir = outputRoot();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void MainWindow::setInputPath(const QString& path) {
    inputEdit_->setText(QDir::toNativeSeparators(path));
    inspectInput();
}

// Read just the header of the chosen file and say what it is, so mistakes show before the long run.
void MainWindow::inspectInput() {
    if (running_) return;
    const QString path = inputEdit_->text().trimmed();
    inputValid_ = false;
    if (path.isEmpty()) {
        inputInfo_->setText(QStringLiteral("尚未选择文件。"));
        return;
    }
    if (!QFileInfo(path).isFile()) {
        inputInfo_->setText(QStringLiteral("找不到这个文件。"));
        return;
    }
    sdapp::IqReader reader;
    std::string error;
    const bool preferManual = manualRate_->isChecked();
    if (!reader.open(toFsPath(path), rateSpin_->value() * 1e6, error, preferManual)) {
        inputInfo_->setText(QString::fromUtf8(error.c_str()));
        return;
    }
    const sdapp::IqInfo info = reader.info();
    const bool isWav = (info.format == sdapp::IqFormat::Wav);
    manualRate_->setEnabled(!isWav);
    if (isWav && manualRate_->isChecked()) {
        QSignalBlocker block(manualRate_);
        manualRate_->setChecked(false);
        rateSpin_->setEnabled(false);
    }
    const bool rateUnknown = (!isWav && info.rateSource == "手动设置" && !preferManual);
    if (rateUnknown && !manualRate_->isChecked()) {
        QSignalBlocker block(manualRate_);
        manualRate_->setChecked(true);
        rateSpin_->setEnabled(true);
    } else if (!rateUnknown && !preferManual) {
        QSignalBlocker block(rateSpin_);
        rateSpin_->setValue(info.sampleRate / 1e6);   // show what was detected
    }
    QString text = QStringLiteral("%1 · %2 MS/s（%3）· %4 秒 · %5")
                       .arg(QString::fromUtf8(sdapp::formatName(info.format)))
                       .arg(info.sampleRate / 1e6, 0, 'f', 3)
                       .arg(QString::fromUtf8(info.rateSource.c_str()))
                       .arg(static_cast<double>(info.frames) / info.sampleRate, 0, 'f', 1)
                       .arg(humanSize(QFileInfo(path).size()));
    if (rateUnknown) text += QStringLiteral("\n文件没有标明采样率，暂用上面的值，请核对后再开始。");
    if (info.sampleRate < SdWorker::kMinSampleRate)
        text += QStringLiteral("\n采样率太低：GOES SD 至少需要 3.41 MS/s。");
    inputInfo_->setText(text);
    inputValid_ = info.sampleRate >= SdWorker::kMinSampleRate;
}

void MainWindow::saveSettings() {
    QSettings settings;
    settings.setValue(QStringLiteral("outputDir"), outputEdit_->text());
    settings.setValue(QStringLiteral("manualRateMsps"), rateSpin_->value());
    settings.setValue(QStringLiteral("softDecisions"), softBox_->isChecked());
}

void MainWindow::toggleRun() {
    if (running_) {
        if (stopping_) return;
        stopping_ = true;
        startBtn_->setEnabled(false);
        startBtn_->setText(QStringLiteral("正在停止…"));
        onLog(QStringLiteral("已请求停止，正在保存已收到的内容…"));
        worker_->requestStop();
        return;
    }
    const QString path = inputEdit_->text().trimmed();
    if (path.isEmpty() || !QFileInfo(path).isFile() || !isSupportedRecording(path)) {
        QMessageBox::information(this, QStringLiteral("还没有选好录音"),
                                 QStringLiteral("请先选择一个 .cs8、.cu8、.cs16、.cf32 或 .wav 录音文件。"));
        return;
    }
    inspectInput();
    if (!inputValid_) {
        QMessageBox::warning(this, QStringLiteral("录音不能用"), inputInfo_->text());
        return;
    }
    const QString out = outputRoot();
    outputEdit_->setText(QDir::toNativeSeparators(out));
    saveSettings();

    log_->clear();
    products_->clear();
    previewLabel_->setPixmap(QPixmap());
    previewLabel_->setText(QStringLiteral("等待图像数据…"));
    previewCaption_->clear();
    lastPreview_ = QImage();
    progress_->setValue(0);
    onStats(SdStats{});
    spectrum_->clear();
    setRunning(true);
    emit startFileRequested(path, rateSpin_->value() * 1e6, manualRate_->isChecked(), out, softBox_->isChecked());
}

void MainWindow::setRunning(bool running) {
    running_ = running;
    stopping_ = false;
    startBtn_->setEnabled(true);
    startBtn_->setText(running ? QStringLiteral("停止") : QStringLiteral("开始解码"));
    inputEdit_->setEnabled(!running);
    outputEdit_->setEnabled(!running);
    manualRate_->setEnabled(!running);
    rateSpin_->setEnabled(!running && manualRate_->isChecked());
    softBox_->setEnabled(!running);
    statusBar()->showMessage(running ? QStringLiteral("解码中…") : QStringLiteral("就绪"));
}

void MainWindow::onLog(const QString& text) {
    log_->appendPlainText(QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), text));
}

void MainWindow::onProgress(int permille) {
    progress_->setValue(permille);
    if (running_) statusBar()->showMessage(QStringLiteral("解码中… %1%").arg(permille / 10.0, 0, 'f', 1));
}

void MainWindow::onStats(const SdStats& s) {
    lockValue_->setText(s.imagerLocked ? QStringLiteral("已同步") : QStringLiteral("搜索中"));
    lockValue_->setStyleSheet(s.imagerLocked ? QStringLiteral("color:#1b7f3b;") : QStringLiteral("color:#a86b00;"));
    offsetValue_->setText(QStringLiteral("%1 Hz").arg(s.carrierHz, 0, 'f', 0));
    snrValue_->setText(QStringLiteral("%1 dB").arg(s.snrDb, 0, 'f', 1));
    berValue_->setText(s.blocks ? QStringLiteral("%1%").arg(s.syncBitErrorPct, 0, 'f', 1) : QStringLiteral("—"));
    blockValue_->setText(QString::number(s.blocks));
    scanValue_->setText(s.scanWidth > 0 ? QStringLiteral("%1 / %2 样点").arg(s.scans).arg(s.scanWidth) : QString::number(s.scans));
    flipValue_->setText(QString::number(s.directionFixes));
    sounderValue_->setText(s.sounderFrames ? QStringLiteral("%1（%2%）").arg(s.sounderFrames).arg(s.sounderParityPct, 0, 'f', 0)
                                           : QStringLiteral("0"));
}

void MainWindow::onLivePreview(const QImage& image, const QString& stamp) {
    if (image.isNull()) return;
    lastPreview_ = image;
    previewCaption_->setText(QStringLiteral("观测 %1").arg(stamp));
    updatePreview();
}

void MainWindow::updatePreview() {
    if (lastPreview_.isNull()) return;
    const QSize box = previewLabel_->size() - QSize(8, 8);
    if (box.width() < 20 || box.height() < 20) return;
    previewLabel_->setPixmap(QPixmap::fromImage(lastPreview_.scaled(box, Qt::KeepAspectRatio, Qt::FastTransformation)));
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    updatePreview();
}

void MainWindow::onObservationDone(const QString& stamp, const QStringList& paths) {
    if (paths.isEmpty()) return;
    const QString dir = QFileInfo(paths.first()).absolutePath();
    auto* item = new QListWidgetItem(QStringLiteral("%1  ·  %2 个文件").arg(stamp).arg(paths.size()));
    item->setData(Qt::UserRole, dir);
    item->setToolTip(QDir::toNativeSeparators(dir) + QLatin1Char('\n') + paths.join(QLatin1Char('\n')));
    products_->addItem(item);
    onLog(QStringLiteral("观测 %1 已保存：%2").arg(stamp, QDir::toNativeSeparators(dir)));
}

void MainWindow::onFinished(bool ok, const QString& summary) {
    setRunning(false);
    onLog(summary);
    statusBar()->showMessage(summary.section(QLatin1Char('\n'), 0, 0));
    if (!ok) QMessageBox::warning(this, QStringLiteral("没有开始"), summary);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (running_ || !event->mimeData()->hasUrls()) return;
    for (const QUrl& url : event->mimeData()->urls())
        if (url.isLocalFile() && isSupportedRecording(url.toLocalFile())) {
            event->acceptProposedAction();
            return;
        }
}

void MainWindow::dropEvent(QDropEvent* event) {
    for (const QUrl& url : event->mimeData()->urls())
        if (url.isLocalFile() && isSupportedRecording(url.toLocalFile())) {
            setInputPath(url.toLocalFile());
            return;
        }
}

void MainWindow::showAgreement() {
    const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("EULA.txt"));
    if (QFileInfo::exists(path) && QDesktopServices::openUrl(QUrl::fromLocalFile(path))) return;
    QMessageBox::information(this, QStringLiteral("用户协议与免责声明"),
                             QStringLiteral("找不到 EULA.txt。请到项目主页阅读完整协议。"));
}

void MainWindow::showAbout() {
    QMessageBox::about(this, QStringLiteral("关于"),
        QStringLiteral("<h3>%1</h3>"
                       "<p>版本 %2<br>开发者：%3<br>开源协议：GPL-3.0<br>%4</p>"
                       "<p>回放 GOES-N/O/P 1676 MHz 传感器数据（Sensor Data）的基带录音：BPSK 解调 → 成像仪块同步与去白化 → "
                       "软判决成像 → 扫描方向判定 → 可见光、红外图像，以及探测器（Sounder）帧。本软件只接收、只读录音，不发射。</p>"
                       "<p>本软件是业余爱好者的学习研究项目，不是气象业务系统，也与美国 NOAA / NASA 无任何隶属或授权关系。数据未经定标。</p>"
                       "<p style='color:#8a919c'>第三方：Qt 6（LGPL v3）。假彩色查找表来自 SatDump（GPL-3.0）。</p>")
            .arg(QString::fromUtf8(appinfo::kProductName), QString::fromUtf8(appinfo::kVersion), QString::fromUtf8(appinfo::kDeveloper),
                 QStringLiteral("<a href=\"%1\">%1</a>").arg(QString::fromUtf8(appinfo::kProjectUrl))));
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (running_) {
        const auto answer = QMessageBox::question(this, QStringLiteral("正在运行"),
                                                  QStringLiteral("还在解码。退出会先保存已收到的内容。确定退出吗？"));
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        worker_->requestStop();
    }
    saveSettings();
    event->accept();
}
