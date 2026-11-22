// mainwindow.h - the window of the GOES SD decoder: choose a baseband recording, press start, watch it lock,
// open the pictures.
#pragma once
#include "sdworker.h"

#include <QImage>
#include <QMainWindow>
#include <QThread>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class ConstellationWidget;
class SpectrumView;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

signals:
    void startFileRequested(const QString& inputPath, double manualRateHz, bool preferManual, const QString& outputDir, bool softDecisions);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void browseInput();
    void browseOutput();
    void openOutputFolder();
    void toggleRun();
    void inspectInput();
    void showAgreement();
    void showAbout();
    void onLog(const QString& text);
    void onProgress(int permille);
    void onStats(const SdStats& stats);
    void onLivePreview(const QImage& image, const QString& stamp);
    void onObservationDone(const QString& stamp, const QStringList& paths);
    void onFinished(bool ok, const QString& summary);

private:
    void buildUi();
    void setInputPath(const QString& path);
    void setRunning(bool running);
    void updatePreview();
    void saveSettings();
    QString outputRoot() const;

    QLineEdit* inputEdit_ = nullptr;
    QLabel* inputInfo_ = nullptr;
    QCheckBox* manualRate_ = nullptr;
    QDoubleSpinBox* rateSpin_ = nullptr;
    QCheckBox* softBox_ = nullptr;
    QLineEdit* outputEdit_ = nullptr;
    QPushButton* startBtn_ = nullptr;
    QPushButton* openOutBtn_ = nullptr;
    QProgressBar* progress_ = nullptr;
    ConstellationWidget* constellation_ = nullptr;
    SpectrumView* spectrum_ = nullptr;
    QLabel* lockValue_ = nullptr;
    QLabel* offsetValue_ = nullptr;
    QLabel* snrValue_ = nullptr;
    QLabel* berValue_ = nullptr;
    QLabel* blockValue_ = nullptr;
    QLabel* scanValue_ = nullptr;
    QLabel* flipValue_ = nullptr;
    QLabel* sounderValue_ = nullptr;
    QLabel* previewLabel_ = nullptr;
    QLabel* previewCaption_ = nullptr;
    QListWidget* products_ = nullptr;
    QPlainTextEdit* log_ = nullptr;

    QThread thread_;
    SdWorker* worker_ = nullptr;
    QImage lastPreview_;
    bool running_ = false;
    bool stopping_ = false;
    bool inputValid_ = false;
};
