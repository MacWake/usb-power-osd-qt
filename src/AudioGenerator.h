#ifndef AUDIOGENERATOR_H
#define AUDIOGENERATOR_H

#include <QIODevice>
#include <QAudioFormat>
#include <cmath>
#include <atomic>

class AudioGenerator : public QIODevice {
    Q_OBJECT

public:
    explicit AudioGenerator(const QAudioFormat &format, QObject *parent = nullptr)
        : QIODevice(parent), m_format(format) {
    }

    void start() {
        open(QIODevice::ReadOnly);
    }

    void stop() {
        close();
    }

    void setFrequency(double frequency) {
        m_frequency.store(frequency);
    }

    void setAmplitude(double amplitude) {
        m_amplitude.store(amplitude);
    }

    qint64 readData(char *data, qint64 len) override {
        const int sampleSize = m_format.sampleFormat() == QAudioFormat::Int16 ? 2 : 4;
        const int channelCount = m_format.channelCount();
        const int sampleRate = m_format.sampleRate();
        qint64 samplesToRead = len / (sampleSize * channelCount);
        
        if (m_format.sampleFormat() == QAudioFormat::Int16) {
            auto *ptr = reinterpret_cast<std::int16_t *>(data);
            for (qint64 i = 0; i < samplesToRead; ++i) {
                double freq = m_frequency.load();
                double amp = m_amplitude.load();
                double val = std::sin(m_phase) * amp * 32767.0;
                std::int16_t sample = static_cast<std::int16_t>(std::clamp(val, -32768.0, 32767.0));
                for (int j = 0; j < channelCount; ++j) {
                    *ptr++ = sample;
                }
                m_phase += 2.0 * M_PI * freq / sampleRate;
                if (m_phase > 2.0 * M_PI) m_phase -= 2.0 * M_PI;
            }
        } else if (m_format.sampleFormat() == QAudioFormat::Float) {
            auto *ptr = reinterpret_cast<float *>(data);
            for (qint64 i = 0; i < samplesToRead; ++i) {
                double freq = m_frequency.load();
                double amp = m_amplitude.load();
                auto sample = static_cast<float>(std::sin(m_phase) * amp);
                for (int j = 0; j < channelCount; ++j) {
                    *ptr++ = sample;
                }
                m_phase += 2.0 * M_PI * freq / sampleRate;
                if (m_phase > 2.0 * M_PI) m_phase -= 2.0 * M_PI;
            }
        }

        return samplesToRead * sampleSize * channelCount;
    }

    qint64 writeData(const char *data, qint64 len) override {
        Q_UNUSED(data);
        Q_UNUSED(len);
        return 0;
    }

    [[nodiscard]] qint64 bytesAvailable() const override {
        return std::numeric_limits<qint64>::max();
    }

private:
    QAudioFormat m_format;
    double m_phase = 0.0;
    std::atomic<double> m_frequency{440.0};
    std::atomic<double> m_amplitude{0.0};
};

#endif // AUDIOGENERATOR_H
