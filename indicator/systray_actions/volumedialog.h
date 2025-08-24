/*
 * SPDX-FileCopyrightText: 2025 Dhanajayan Krishnamoorthi <dhanajayan99@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef VOLUMEDIALOG_H
#define VOLUMEDIALOG_H

#include <QDialog>
#include <QMap>
#include <QVBoxLayout>

// Forward declarations
class MobileVolumeDbusInterface;
class QSlider;
class QLabel;

class VolumeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VolumeDialog(const QString &deviceId, QWidget *parent = nullptr);
    ~VolumeDialog();

    void requestStreamInfo();

public Q_SLOTS:
    void updateSlider(int stream, int value);
    void createVolumeControls(const QString &streamsJson);

private:
    QString getStreamName(int stream);

private:
    MobileVolumeDbusInterface *m_dbusInterface;
    QVBoxLayout *m_layout;
    QLabel *m_loadingLabel;
    QMap<int, QSlider *> m_sliders;
    QMap<int, QLabel *> m_labels;
    QString m_deviceId;
};

#endif // VOLUMEDIALOG_H