#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QSpinBox;
class QComboBox;
class QSlider;
class QLabel;
QT_END_NAMESPACE

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    
    // Settings accessors
    bool autoStartOSD() const;
    int osdUpdateInterval() const;
    double osdOpacity() const;
    QString connectionPreference() const;
    bool enableNotifications() const;

private slots:
    void onAccepted();
    void onRejected();
    void onOpacityChanged(int value);

private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    
    QSettings *m_settings;
    
    QCheckBox *m_autoStartOSDCheck;
    QSpinBox *m_updateIntervalSpin;
    QSlider *m_opacitySlider;
    QLabel *m_opacityLabel;
    QComboBox *m_connectionCombo;
    QCheckBox *m_notificationsCheck;
};

#endif // SETTINGS_H
