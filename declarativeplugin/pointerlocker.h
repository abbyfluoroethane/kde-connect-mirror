/*
    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef POINTERLOCKER_H
#define POINTERLOCKER_H

#include <QObject>
#include <QWindow>

class AbstractPointerLocker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isLocked READ isLocked WRITE setLocked NOTIFY lockedChanged)
    Q_PROPERTY(QWindow* window READ window WRITE setWindow NOTIFY windowChanged)
public:
    AbstractPointerLocker(QObject *parent = nullptr) : QObject(parent) {}

    virtual void setLocked(bool locked) = 0;
    virtual bool isLocked() const = 0;

    void setWindow(QWindow *window);
    QWindow *window() const {
        return m_window;
    }

Q_SIGNALS:
    void lockedChanged(bool isLocked);
    void windowChanged();
    void pointerMoved(const QPointF &delta);

protected:
    QWindow *m_window = nullptr;
};

class PointerLockerQt : public AbstractPointerLocker
{
    Q_OBJECT
public:
    PointerLockerQt(QObject *parent = nullptr);
    ~PointerLockerQt() override;

    void setLocked(bool locked) override;
    bool isLocked() const override;

private:
    bool eventFilter(QObject *watched, QEvent *event) override;

    QPoint m_originalPosition;
    bool m_moving = false;

};

#endif
