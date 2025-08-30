/*
 * SPDX-FileCopyrightText: 2025 Dhanajayan Krishnamoorthi <dhanajayan99@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "volumedialog.h"
#include "mobilevolumedbusinterface.h"
#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

VolumeDialog::VolumeDialog(const QString &deviceId, QWidget *parent)
    : QDialog(parent)
    , m_dbusInterface(new MobileVolumeDbusInterface(deviceId, this))
    , m_deviceId(deviceId)
{
    setWindowIcon(QIcon::fromTheme(QStringLiteral("kdeconnect")));
    setWindowTitle(i18n("Mobile Volume Control"));
    setAttribute(Qt::WA_DeleteOnClose);

    setFixedSize(400, 400);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // setMinimumSize(400, 500);
    // setMaximumSize(400, 500);

    m_layout = new QVBoxLayout(this);

    m_loadingLabel = new QLabel(i18n("Loading volume information..."), this);
    m_layout->addWidget(m_loadingLabel);

    requestStreamInfo();

    QPushButton *closeBtn = new QPushButton(i18n("Close"), this);
    m_layout->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    connect(m_dbusInterface, &MobileVolumeDbusInterface::volumeChanged, this, &VolumeDialog::updateSlider);
    connect(m_dbusInterface, &MobileVolumeDbusInterface::streamsInfoReceived, this, &VolumeDialog::createVolumeControls);
}

VolumeDialog::~VolumeDialog()
{
    // Clean UI elements
    qDeleteAll(m_sliders);
    qDeleteAll(m_labels);
}

void VolumeDialog::requestStreamInfo()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kdeconnect"),
                                                          QStringLiteral("/modules/kdeconnect/devices/") + m_deviceId + QStringLiteral("/mobilevolume"),
                                                          QStringLiteral("org.kde.kdeconnect.device.mobilevolume"),
                                                          QStringLiteral("sendVolumeRequest"));

    QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
    qDebug() << "Requested stream information from device";
}

void VolumeDialog::createVolumeControls(const QString &streamsJson)
{
    qDebug() << "Creating volume controls from JSON:" << streamsJson;

    // Parse JSON string
    QJsonDocument doc = QJsonDocument::fromJson(streamsJson.toUtf8());
    if (!doc.isArray()) {
        qWarning() << "Invalid streams JSON format";
        return;
    }

    QJsonArray streamsArray = doc.array();

    // Remove loading
    if (m_loadingLabel) {
        m_layout->removeWidget(m_loadingLabel);
        delete m_loadingLabel;
        m_loadingLabel = nullptr;
    }

    // Clear existing control
    qDeleteAll(m_sliders);
    qDeleteAll(m_labels);
    m_sliders.clear();
    m_labels.clear();

    // Create sliders and label for each stream
    for (const auto &streamValue : streamsArray) {
        QJsonObject streamObj = streamValue.toObject();
        int streamId = streamObj.value(QStringLiteral("stream")).toInt();
        QString name = streamObj.value(QStringLiteral("name")).toString();
        int current = streamObj.value(QStringLiteral("current")).toInt();
        int max = streamObj.value(QStringLiteral("max")).toInt();

        qDebug() << "Creating control for stream:" << streamId << name << current << "/" << max;

        QLabel *label = new QLabel(QStringLiteral("%1: %2/%3").arg(name).arg(current).arg(max), this);
        m_layout->insertWidget(m_layout->count() - 1, label);
        m_labels[streamId] = label;

        QSlider *slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(0, max);
        slider->setValue(current);
        m_layout->insertWidget(m_layout->count() - 1, slider);
        m_sliders[streamId] = slider;

        connect(slider, &QSlider::valueChanged, this, [this, streamId, name, max](int value) {
            qDebug() << "Slider changed - Stream:" << streamId << "Value:" << value;
            m_dbusInterface->sendVolumeChange(streamId, value);
            if (m_labels.contains(streamId)) {
                m_labels[streamId]->setText(QStringLiteral("%1: %2/%3").arg(name).arg(value).arg(max));
            }
        });
    }

    adjustSize();
    update();
}

void VolumeDialog::updateSlider(int stream, int value)
{
    if (m_sliders.contains(stream) && m_labels.contains(stream)) {
        QSlider *slider = m_sliders[stream];
        QLabel *label = m_labels[stream];

        // Block signals to prevent infinite loop
        slider->blockSignals(true);
        slider->setValue(value);
        slider->blockSignals(false);

        // Update label text
        int max = slider->maximum();
        QString name = getStreamName(stream);
        label->setText(QStringLiteral("%1: %2/%3").arg(name).arg(value).arg(max));
    }
}

QString VolumeDialog::getStreamName(int stream)
{
    static QMap<int, QString> streamNames =
        {{0, i18n("Voice Call")}, {1, i18n("System")}, {2, i18n("Ringtone")}, {3, i18n("Media")}, {4, i18n("Alarm")}, {5, i18n("Notification")}};
    // {8, i18n("DTMF")},
    // {10, i18n("Accessibility")}};
    return streamNames.value(stream, i18n("Unknown Stream"));
}