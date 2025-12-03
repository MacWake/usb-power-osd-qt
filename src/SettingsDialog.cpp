// ReSharper disable CppDFAMemoryLeak
#include "SettingsDialog.h"
#include "CurrentGraph.h"
#include "MainWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QColorDialog>

struct ColorButtonInfo {
    QString name;
    QString displayName;
    QColor *settingsColor;
    QPushButton **buttonPtr;
};

SettingsDialog::SettingsDialog(OsdSettings *settings, QWidget *parent) // NOLINT(*-pro-type-member-init)
    : QDialog(parent), m_settings(settings) {
    this->m_mainwindow = dynamic_cast<MainWindow *>(parent);
    setupUI();
    onColorChanged();
}

// ReSharper disable once CppDFAMemoryLeak
void SettingsDialog::setupUI() // ReSharper disable once CppDFAMemoryLeak
{
    setWindowTitle("Settings");
    setModal(true);
    resize(400, 300);

    auto *layout = new QVBoxLayout(this);

    // OSD Settings

    auto *osdGroup = new QGroupBox("OSD Display");
    auto *osdLayout = new QFormLayout(osdGroup);

    m_priFontButton = new QPushButton(QString::asprintf(
                                          "%s %dpt", this->m_settings->primary_font_name.toStdString().c_str(),
                                          this->m_settings->primary_font_size), this);
    connect(m_priFontButton, &QPushButton::clicked, [this] {
        bool ok;
        QFont currentFont = QFont(this->m_settings->primary_font_name, this->m_settings->primary_font_size);
        QFont font =
                QFontDialog::getFont(&ok, currentFont, this, "Select font",
                                     QFontDialog::MonospacedFonts | QFontDialog::DontUseNativeDialog);
        if (ok) {
            this->m_settings->primary_font_name = font.family();
            this->m_settings->primary_font_size = font.pointSize();
            m_priFontButton->setText(QString::asprintf(
                "%s %dpt", this->m_settings->primary_font_name.toStdString().c_str(),
                this->m_settings->primary_font_size));
            this->m_mainwindow->onPrimaryFontChanged(font);
        }
    });
    osdLayout->addRow("Primary Font", m_priFontButton);

    QFont currentFont = QFont(this->m_settings->secondary_font_name, this->m_settings->secondary_font_size);
    m_secFontButton = new QPushButton(
        QString::asprintf("%s %dpt", currentFont.family().toStdString().c_str(), currentFont.pointSize()), this);
    connect(m_secFontButton, &QPushButton::clicked, [this] {
        bool ok;
        QFont currentFont = QFont(this->m_settings->secondary_font_name, this->m_settings->secondary_font_size);
        QFont font =
                QFontDialog::getFont(&ok, currentFont, this, "Select font",
                                     QFontDialog::MonospacedFonts | QFontDialog::DontUseNativeDialog);
        if (ok) {
            this->m_settings->secondary_font_name = font.family();
            this->m_settings->secondary_font_size = font.pointSize();
            m_secFontButton->setText(QString::asprintf(
                "%s %dpt", this->m_settings->secondary_font_name.toStdString().c_str(),
                this->m_settings->secondary_font_size));
            this->m_mainwindow->onSecondaryFontChanged(font);
        }
    });
    osdLayout->addRow("Secondary Font", m_secFontButton);

    m_minCurrent = new QSpinBox();
    m_minCurrent->setRange(1, 100);
    m_minCurrent->setSuffix(" mA");
    m_minCurrent->setValue(static_cast<int>(this->m_settings->min_current * 1000.0f));
    osdLayout->addRow("Min. historic current:", m_minCurrent);
    connect(m_minCurrent, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        this->m_settings->min_current = static_cast<float>(value) / 1000.0f;
    });

    layout->addWidget(osdGroup);

    auto *colorGroup = new QGroupBox("Colors");
    auto *colorLayout = new QFormLayout(colorGroup);

    std::vector<ColorButtonInfo> colorButtons = {
        {"background", "Background", &m_settings->color_bg, &m_BackgroundButton},
        {"text", "Text Color", &m_settings->color_text, &m_TextButton},
        {"5v", "5V Color", &m_settings->color_5v, &m_5VButton},
        {"9v", "9V Color", &m_settings->color_9v, &m_9VButton},
        {"15v", "15V Color", &m_settings->color_15v, &m_15VButton},
        {"20v", "20V Color", &m_settings->color_20v, &m_20VButton},
        {"28v", "28V Color", &m_settings->color_28v, &m_28VButton},
        {"36v", "36V Color", &m_settings->color_36v, &m_36VButton},
        {"48v", "48V Color", &m_settings->color_48v, &m_48VButton}
    };

    for (const auto &info: colorButtons) {
        *info.buttonPtr = createColorButton(info.name, info.displayName, info.settingsColor);
        colorLayout->addRow(info.displayName, *info.buttonPtr);
    }

    layout->addWidget(colorGroup);

    // Dialog buttons
    auto *buttonBox =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this,
            &SettingsDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this,
            &SettingsDialog::onRejected);
    layout->addWidget(buttonBox);
}

QPushButton *SettingsDialog::createColorButton(const QString &name, const QString &displayName, QColor *settingsColor) {
    auto *button = new QPushButton(displayName, this);
    connect(button, &QPushButton::clicked, [this, settingsColor, displayName] {
        QColor selected = QColorDialog::getColor(*settingsColor, this, displayName + " Color");
        if (selected.isValid()) {
            *settingsColor = selected;
            this->onColorChanged();
            this->m_mainwindow->onColorChanged();
        }
    });
    return button;
}

void SettingsDialog::onColorChanged() {
    this->m_BackgroundButton->setStyleSheet(
        "background-color: " + m_settings->color_text.name() + ";color: " + this->m_settings->color_bg.name() + ";");
    this->m_TextButton->setStyleSheet(
        "background-color: " + m_settings->color_bg.name() + ";color: " + this->m_settings->color_text.name() + ";");
    this->m_5VButton->setStyleSheet(
        "background-color: " + m_settings->color_bg.name() + ";color: " + this->m_settings->color_5v.name() + ";");
    this->m_9VButton->setStyleSheet(
        "background-color: " + m_settings->color_bg.name() + ";color: " + this->m_settings->color_9v.name() + ";");
    this->m_15VButton->setStyleSheet(
        "background-color: " + m_settings->color_bg.name() + ";color: " + this->m_settings->color_15v.name() + ";");
    this->m_20VButton->setStyleSheet(
        "background-color: " + m_settings->color_bg.name() + ";color: " + this->m_settings->color_20v.name() + ";");
    this->m_28VButton->setStyleSheet(
        "background-color: " + m_settings->color_bg.name() + ";color: " + this->m_settings->color_28v.name() + ";");
    this->m_36VButton->setStyleSheet(
        "background-color: " + m_settings->color_bg.name() + ";color: " + this->m_settings->color_36v.name() + ";");
    this->m_48VButton->setStyleSheet(
        "background-color: " + m_settings->color_bg.name() + ";color: " + this->m_settings->color_48v.name() + ";");
}

void SettingsDialog::onAccepted() {
    m_settings->saveSettings();
    accept();
}

void SettingsDialog::onRejected() {
    m_settings->loadSettings(); // Restore previous values
    onColorChanged();
    m_mainwindow->onColorChanged();
    reject();
}
