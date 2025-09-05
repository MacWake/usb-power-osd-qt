
#ifndef DEVICESELECTIONDIALOG_H
#define DEVICESELECTIONDIALOG_H

#include <QDialog>
#include <QButtonGroup>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSerialPortInfo>
#include <QVBoxLayout>

class DeviceSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    enum class ConnectionType {
        None,
        BluetoothAuto,
        SerialPort
    };

    explicit DeviceSelectionDialog(QWidget *parent = nullptr);
    ~DeviceSelectionDialog() override = default;

    ConnectionType getSelectedConnectionType() const;
    QString getSelectedSerialPort() const;
    
    // Called by MainWindow to populate with available serial ports
    void refreshSerialPorts();
    
    // Called by MainWindow to check if last used device is available
    static bool isLastDeviceAvailable();

private slots:
    void onConnectionTypeChanged();
    void onOkButtonClicked();
    void onCancelButtonClicked();
    void onRefreshSerialPorts();

private:
    void setupUI();
    void updateControlStates();
    void saveSelectedDevice();
    QString getLastUsedDevice();
    
    // UI components
    QVBoxLayout *m_mainLayout;
    QGridLayout *m_contentLayout;
    QHBoxLayout *m_buttonLayout;
    
    QLabel *m_titleLabel;
    QLabel *m_instructionLabel;
    
    QButtonGroup *m_connectionTypeGroup;
    QRadioButton *m_bluetoothRadio;
    QRadioButton *m_serialRadio;
    
    QLabel *m_serialPortLabel;
    QComboBox *m_serialPortCombo;
    QPushButton *m_refreshSerialButton;
    
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    
    ConnectionType m_selectedType;
    QString m_selectedSerialPort;
};

#endif // DEVICESELECTIONDIALOG_H
