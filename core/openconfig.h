#include <QObject>

#include "kdeconnectcore_export.h"

class KDECONNECTCORE_EXPORT OpenConfig : public QObject
{
    Q_OBJECT
public:
    void setXdgActivationToken(const QString &token);
    Q_INVOKABLE void openConfiguration(const QString &deviceId = QString(), const QString &pluginName = QString());

private:
    QString m_currentToken;
};
