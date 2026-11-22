// spectrumview.h - a small power-spectrum plot of the recording, with the expected signal width marked.
#pragma once
#include <QWidget>
#include <vector>

class SpectrumView : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumView(QWidget* parent = nullptr);
    void setSpectrum(const std::vector<float>& db, double sampleRate);   // smoothed over time
    void setCenterMHz(double mhz) { centerMhz_ = mhz; update(); }
    void setSignalWidthMHz(double mhz) { signalMhz_ = mhz; update(); }
    void clear();
protected:
    void paintEvent(QPaintEvent*) override;
private:
    std::vector<float> avg_;
    double sampleRate_ = 0.0;
    double centerMhz_ = 0.0;
    double signalMhz_ = 5.0;
};
