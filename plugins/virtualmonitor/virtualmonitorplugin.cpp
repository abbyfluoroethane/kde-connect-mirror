/**
 * SPDX-FileCopyrightText: 2021 Aleix Pol i Gonzalez <aleixpol@kde.org>
 * SPDX-FileCopyrightText: 2024 Fabian Arndt <fabian.arndt@root-core.net>
 * SPDX-FileCopyrightText: 2026 KDE Connect Developers
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "virtualmonitorplugin.h"

#include <KLocalizedString>
#include <KPluginFactory>

#include "plugin_virtualmonitor_debug.h"
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QScreen>
#include <QSettings>
#include <QSslCertificate>
#include <QSslKey>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>

#include <algorithm>

#include <core/device.h>
#include <core/sslhelper.h>

#if !defined(Q_OS_WIN)
#include <KApplicationTrader>
#endif

K_PLUGIN_CLASS_WITH_JSON(VirtualMonitorPlugin, "kdeconnect_virtualmonitor.json")

#define QS QLatin1String

namespace
{
constexpr int ProtocolVersion = 2;
constexpr quint16 LegacyDefaultPort = 5901;
constexpr quint16 KdeConnectMinPort = 1714;
constexpr quint16 KdeConnectMaxPort = 1764;
constexpr int ServerProbeAttempts = 50;
constexpr int ServerProbeIntervalMs = 100;
constexpr int ClientConnectionTimeoutMs = 45'000;

const auto ActionCapabilities = QStringLiteral("capabilities");
const auto ActionRequest = QStringLiteral("request");
const auto ActionConnect = QStringLiteral("connect");
const auto ActionClientConnected = QStringLiteral("clientConnected");
const auto ActionStop = QStringLiteral("stop");
const auto ActionStatus = QStringLiteral("status");

const auto ModeVirtualMonitor = QStringLiteral("virtualMonitor");
const auto ModeMonitor = QStringLiteral("monitor");
const auto ModeWorkspace = QStringLiteral("workspace");
}

class EmitActive
{
public:
    explicit EmitActive(VirtualMonitorPlugin *plugin)
        : m_plugin(plugin)
        , m_wasActive(plugin->active())
    {
        if (!s_outmost) {
            s_outmost = this;
            m_isOutmost = true;
        }
    }

    ~EmitActive()
    {
        if (m_isOutmost) {
            s_outmost = nullptr;
            if (m_plugin->active() != m_wasActive) {
                Q_EMIT m_plugin->activeChanged();
            }
        }
    }

private:
    static EmitActive *s_outmost;
    VirtualMonitorPlugin *const m_plugin;
    const bool m_wasActive;
    bool m_isOutmost = false;
};

EmitActive *EmitActive::s_outmost = nullptr;

VirtualMonitorPlugin::~VirtualMonitorPlugin()
{
    stopSession(false);
}

bool VirtualMonitorPlugin::active() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void VirtualMonitorPlugin::stop()
{
    stopSession(true);
}

void VirtualMonitorPlugin::stopSession(bool notifyRemote)
{
    EmitActive emitter(this);

    const QString sessionId = m_currentRequest.sessionId;
    const bool hadSession = m_process || m_serverReady;
    ++m_sessionGeneration;
    m_stopping = true;

    if (m_process) {
        QObject::disconnect(m_process, nullptr, this, nullptr);
        m_process->terminate();
        if (!m_process->waitForFinished(2000)) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
        m_process->deleteLater();
        m_process = nullptr;
    }

    m_stopping = false;
    m_serverReady = false;
    m_clientConnected = false;
    m_port = 0;
    m_username.clear();
    m_password.clear();
    m_certificateFingerprint.clear();
    m_tlsDirectory.reset();

    if (notifyRemote && hadSession && !sessionId.isEmpty()) {
        NetworkPacket np(PACKET_TYPE_VIRTUALMONITOR,
                         {
                             {QStringLiteral("protocolVersion"), ProtocolVersion},
                             {QStringLiteral("action"), ActionStatus},
                             {QStringLiteral("sessionId"), sessionId},
                             {QStringLiteral("status"), QStringLiteral("stopped")},
                             {QStringLiteral("active"), false},
        });
        sendPacket(np);
    }

    m_currentRequest = {};
}

void VirtualMonitorPlugin::connected()
{
    if (!m_deviceLifecycleConnected) {
        connect(device(), &Device::reachableChanged, this, [this](bool reachable) {
            if (!reachable && active()) {
                stopSession(false);
            }
        });
        connect(device(), &Device::linksChanged, this, [this]() {
            if (active() && device()->getLocalIpAddress() == QHostAddress::Null) {
                stopSession(false);
            }
        });
        m_deviceLifecycleConnected = true;
    }

    m_capabilitiesLocal.rdpClient = checkRdpClient();
    m_capabilitiesLocal.virtualMonitor = !QStandardPaths::findExecutable(QS("krdpserver")).isEmpty();
    m_capabilitiesLocal.remoteDesktop = m_capabilitiesLocal.virtualMonitor;

    qCDebug(KDECONNECT_PLUGIN_VIRTUALMONITOR) << "Local device supports RDP client:" << m_capabilitiesLocal.rdpClient;
    qCDebug(KDECONNECT_PLUGIN_VIRTUALMONITOR) << "Local device supports KRdp server:" << m_capabilitiesLocal.remoteDesktop;

    QJsonArray resolutions;
    QJsonArray monitors;
    const auto screens = QGuiApplication::screens();
    for (int index = 0; index < screens.size(); ++index) {
        const QScreen *screen = screens.at(index);
        const QSize physicalSize = screen->size() * screen->devicePixelRatio();
        const QJsonObject monitor{
            {QStringLiteral("index"), index},
            {QStringLiteral("name"), screen->name()},
            {QStringLiteral("width"), physicalSize.width()},
            {QStringLiteral("height"), physicalSize.height()},
            {QStringLiteral("scale"), screen->devicePixelRatio()},
            {QStringLiteral("primary"), screen == QGuiApplication::primaryScreen()},
        };
        monitors.append(monitor);
        resolutions.append(QJsonObject{
            {QStringLiteral("resolution"), QStringLiteral("%1x%2").arg(physicalSize.width()).arg(physicalSize.height())},
            {QStringLiteral("scale"), screen->devicePixelRatio()},
        });
    }

    NetworkPacket np(PACKET_TYPE_VIRTUALMONITOR,
                     {
                         {QStringLiteral("protocolVersion"), ProtocolVersion},
                         {QStringLiteral("action"), ActionCapabilities},
                         {QStringLiteral("resolutions"), resolutions},
                         {QStringLiteral("monitors"), monitors},
                         {QStringLiteral("supports_rdp"), m_capabilitiesLocal.rdpClient},
                         {QStringLiteral("supports_virt_mon"), m_capabilitiesLocal.virtualMonitor},
                         {QStringLiteral("supports_remote_desktop"), m_capabilitiesLocal.remoteDesktop},
                         {QStringLiteral("sessionModes"), QJsonArray{ModeMonitor, ModeWorkspace, ModeVirtualMonitor}},
                         {QStringLiteral("rdpFeatures"),
                          QJsonObject{
                              {QStringLiteral("graphicsPipeline"), true},
                              {QStringLiteral("h264"), true},
                              {QStringLiteral("progressive"), true},
                              {QStringLiteral("nla"), true},
                              {QStringLiteral("tlsFingerprint"), true},
                          }},
                     });
    sendPacket(np);
}

void VirtualMonitorPlugin::receivePacket(const NetworkPacket &received)
{
    if (received.type() == PACKET_TYPE_VIRTUALMONITOR_REQUEST) {
        const QString action = received.get<QString>(QStringLiteral("action"));
        if (action == ActionRequest) {
            handleSessionRequest(received);
        } else if (action == ActionStop) {
            const QString sessionId = received.get<QString>(QStringLiteral("sessionId"));
            if (!sessionId.isEmpty() && sessionId == m_currentRequest.sessionId) {
                stopSession(true);
            }
        } else if (action == ActionClientConnected) {
            const QString sessionId = received.get<QString>(QStringLiteral("sessionId"));
            if (sessionId == m_currentRequest.sessionId && m_serverReady) {
                m_clientConnected = true;
                sendStatus(QStringLiteral("connected"));
            }
        } else if (action == ActionConnect || received.has(QStringLiteral("password"))) {
            openRdpClient(received);
        } else {
            qCWarning(KDECONNECT_PLUGIN_VIRTUALMONITOR) << "Unsupported remote desktop request" << received.body();
        }
        return;
    }

    if (received.type() != PACKET_TYPE_VIRTUALMONITOR) {
        return;
    }

    const QString action = received.get<QString>(QStringLiteral("action"));
    if (received.has(QStringLiteral("resolutions"))) {
        const QJsonArray resolutions = received.get<QJsonArray>(QStringLiteral("resolutions"));
        if (!resolutions.isEmpty()) {
            m_remoteResolution = resolutions.first().toObject();
        }
    }

    if (received.has(QStringLiteral("supports_rdp"))) {
        m_capabilitiesRemote.rdpClient = received.get<bool>(QStringLiteral("supports_rdp"), false);
    }
    if (received.has(QStringLiteral("supports_virt_mon"))) {
        m_capabilitiesRemote.virtualMonitor = received.get<bool>(QStringLiteral("supports_virt_mon"), false);
    }
    if (received.has(QStringLiteral("supports_remote_desktop"))) {
        m_capabilitiesRemote.remoteDesktop = received.get<bool>(QStringLiteral("supports_remote_desktop"), false);
    }

    if (action == ActionStatus) {
        const QString sessionId = received.get<QString>(QStringLiteral("sessionId"));
        if (sessionId == m_currentRequest.sessionId) {
            const QString status = received.get<QString>(QStringLiteral("status"));
            if (status == QLatin1String("failed")) {
                m_lastError = received.get<QString>(QStringLiteral("message"), i18n("The remote RDP client failed to connect."));
                stopSession(false);
            } else if (status == QLatin1String("stopped")) {
                stopSession(false);
            }
        }
    } else if (received.has(QStringLiteral("failed"))) {
        m_lastError = received.get<QString>(QStringLiteral("failed"));
        stopSession(false);
    }

    qCDebug(KDECONNECT_PLUGIN_VIRTUALMONITOR) << "Remote RDP capabilities:" << m_capabilitiesRemote.rdpClient
                                             << m_capabilitiesRemote.remoteDesktop << m_remoteResolution;
}

void VirtualMonitorPlugin::handleSessionRequest(const NetworkPacket &np)
{
    const QString sessionId = np.get<QString>(QStringLiteral("sessionId"));
    if (sessionId.isEmpty() || sessionId.size() > 128) {
        sendStatusForSession(sessionId, QStringLiteral("failed"), i18n("The remote desktop request has an invalid session identifier."));
        return;
    }

    if (active() || m_serverReady) {
        if (sessionId != m_currentRequest.sessionId) {
            sendStatusForSession(sessionId, QStringLiteral("failed"), i18n("Another remote desktop session is already active."));
        } else if (m_serverReady) {
            sendConnectionInfo();
        } else {
            sendStatus(QStringLiteral("starting"));
        }
        return;
    }

    m_capabilitiesLocal.remoteDesktop = !QStandardPaths::findExecutable(QS("krdpserver")).isEmpty();
    if (!m_capabilitiesLocal.remoteDesktop) {
        sendStatusForSession(sessionId, QStringLiteral("failed"), i18n("The krdp package is required on the local device."));
        return;
    }

    if (device()->getLocalIpAddress() == QHostAddress::Null) {
        sendStatusForSession(sessionId, QStringLiteral("failed"), i18n("Remote desktop requires a direct local network connection."));
        return;
    }

    SessionRequest request;
    request.sessionId = sessionId;
    request.quality = std::clamp(np.get<int>(QStringLiteral("quality"), 70), 0, 100);

    const QString mode = np.get<QString>(QStringLiteral("mode"), ModeMonitor);
    if (mode == ModeWorkspace) {
        request.mode = SessionMode::Workspace;
    } else if (mode == ModeVirtualMonitor) {
        request.mode = SessionMode::VirtualMonitor;
        const int width = std::clamp(np.get<int>(QStringLiteral("width"), 1280), 320, 7680);
        const int height = std::clamp(np.get<int>(QStringLiteral("height"), 720), 240, 4320);
        request.resolution = QStringLiteral("%1x%2").arg(width).arg(height);
        request.scale = std::clamp(np.get<double>(QStringLiteral("scale"), 1.0), 0.5, 4.0);
    } else {
        request.mode = SessionMode::Monitor;
        const int screenCount = QGuiApplication::screens().size();
        request.monitorIndex = np.get<int>(QStringLiteral("monitorIndex"), 0);
        if (request.monitorIndex < 0 || request.monitorIndex >= screenCount) {
            sendStatusForSession(sessionId, QStringLiteral("failed"), i18n("The requested monitor does not exist."));
            return;
        }
    }

    m_lastError.clear();
    if (!startRdpSession(request)) {
        sendStatus(QStringLiteral("failed"), m_lastError);
    }
}

void VirtualMonitorPlugin::openRdpClient(const NetworkPacket &np)
{
    if (!np.has(QStringLiteral("password"))) {
        qCWarning(KDECONNECT_PLUGIN_VIRTUALMONITOR) << "RDP connection request is missing a password";
        return;
    }

    const QHostAddress address = device()->getLocalIpAddress();
    if (address == QHostAddress::Null) {
        sendStatusForSession(np.get<QString>(QStringLiteral("sessionId")),
                             QStringLiteral("failed"),
                             i18n("Remote desktop requires a direct local network connection."));
        return;
    }

    const int port = np.get<int>(QStringLiteral("port"), LegacyDefaultPort);
    if (port <= 0 || port > 65535) {
        sendStatusForSession(np.get<QString>(QStringLiteral("sessionId")),
                             QStringLiteral("failed"),
                             i18n("The remote desktop server supplied an invalid port."));
        return;
    }

    QUrl url;
    url.setScheme(QStringLiteral("rdp"));
    url.setUserName(np.get<QString>(QStringLiteral("username"), QStringLiteral("kdeconnect")));
    url.setPassword(np.get<QString>(QStringLiteral("password")));
    url.setPort(port);
    url.setHost(address.toString());

    qCDebug(KDECONNECT_PLUGIN_VIRTUALMONITOR) << "Opening RDP client for paired device" << device()->name() << "on port" << port;
    if (!QDesktopServices::openUrl(url)) {
        NetworkPacket failed(PACKET_TYPE_VIRTUALMONITOR,
                             {
                                 {QStringLiteral("protocolVersion"), ProtocolVersion},
                                 {QStringLiteral("action"), ActionStatus},
                                 {QStringLiteral("sessionId"), np.get<QString>(QStringLiteral("sessionId"))},
                                 {QStringLiteral("status"), QStringLiteral("failed")},
                                 {QStringLiteral("message"), i18n("Could not open an RDP client.")},
                             });
        sendPacket(failed);
    }
}

QString VirtualMonitorPlugin::dbusPath() const
{
    return QLatin1String("/modules/kdeconnect/devices/%1/virtualmonitor").arg(device()->id());
}

bool VirtualMonitorPlugin::requestVirtualMonitor()
{
    m_lastError.clear();
    auto addError = [this](QStringView error) {
        if (!m_lastError.isEmpty()) {
            m_lastError.append(u"\n\n");
        }
        m_lastError.append(error);
    };

    if (!hasRemoteClient()) {
        addError(i18n("Remote device does not have an RDP client installed."));
    }
    if (!isVirtualMonitorAvailable()) {
        addError(i18n("The krdp package is required on the local device."));
    }
    if (m_remoteResolution.isEmpty()) {
        addError(i18n("Cannot start a request without a resolution."));
    }
    if (!m_lastError.isEmpty()) {
        return false;
    }

    SessionRequest request;
    request.sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request.mode = SessionMode::VirtualMonitor;
    request.resolution = m_remoteResolution.value(QStringLiteral("resolution")).toString();
    request.scale = std::clamp(m_remoteResolution.value(QStringLiteral("scale")).toDouble(1.0), 0.5, 4.0);
    return startRdpSession(request);
}

bool VirtualMonitorPlugin::startRdpSession(const SessionRequest &request)
{
    EmitActive emitter(this);
    stopSession(false);

    m_currentRequest = request;
    m_username = QStringLiteral("kdeconnect");
    m_password = QUuid::createUuid().toString(QUuid::Id128);
    m_port = findAvailablePort();
    m_serverReady = false;
    m_clientConnected = false;
    m_lastError.clear();

    if (m_port == 0) {
        m_lastError = i18n("No local TCP port is available for the RDP server.");
        return false;
    }
    if (!prepareTlsCredentials()) {
        return false;
    }

    const quint64 generation = ++m_sessionGeneration;
    auto *process = new QProcess(this);
    process->setProgram(QS("krdpserver"));

    QStringList arguments;
    if (request.mode == SessionMode::VirtualMonitor) {
        arguments << QStringLiteral("--virtual-monitor") << request.resolution + u'@' + QString::number(request.scale);
    } else if (request.mode == SessionMode::Monitor) {
        arguments << QStringLiteral("--monitor") << QString::number(request.monitorIndex);
    }

    arguments << QStringLiteral("--username") << m_username << QStringLiteral("--password") << m_password << QStringLiteral("--port")
              << QString::number(m_port) << QStringLiteral("--quality") << QString::number(request.quality) << QStringLiteral("--certificate")
              << m_tlsDirectory->filePath(QStringLiteral("server.crt")) << QStringLiteral("--certificate-key")
              << m_tlsDirectory->filePath(QStringLiteral("server.key"));

    static const bool plasma = qgetenv("XDG_CURRENT_DESKTOP").compare("KDE", Qt::CaseInsensitive) == 0;
    if (plasma) {
        arguments << QStringLiteral("--plasma");
    }

    process->setArguments(arguments);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(process, &QProcess::finished, this, [this, process, generation](int exitCode, QProcess::ExitStatus exitStatus) {
        const QByteArray errors = process->readAllStandardError();
        qCWarning(KDECONNECT_PLUGIN_VIRTUALMONITOR) << "KRdp server stopped with" << exitCode << exitStatus << errors;

        if (m_process == process) {
            EmitActive emitter(this);
            m_process = nullptr;
        }
        process->deleteLater();

        if (generation != m_sessionGeneration || m_stopping) {
            return;
        }

        const QString message = errors.isEmpty() ? i18n("The RDP server stopped unexpectedly.") : QString::fromLocal8Bit(errors).trimmed();
        m_lastError = message;
        sendStatus(QStringLiteral("failed"), message);
        stopSession(false);
    });

    m_process = process;
    process->start();
    if (!process->waitForStarted(3000)) {
        m_lastError = process->errorString();
        QObject::disconnect(process, nullptr, this, nullptr);
        process->deleteLater();
        m_process = nullptr;
        m_tlsDirectory.reset();
        return false;
    }

    sendStatus(QStringLiteral("starting"));
    probeServerReady(generation, ServerProbeAttempts);
    return true;
}

bool VirtualMonitorPlugin::prepareTlsCredentials()
{
    m_tlsDirectory = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/kdeconnect-rdp-XXXXXX"));
    if (!m_tlsDirectory->isValid()) {
        m_lastError = i18n("Could not create temporary RDP credentials.");
        m_tlsDirectory.reset();
        return false;
    }

    const QSslKey key = SslHelper::generateRsaPrivateKey();
    const QSslCertificate certificate = SslHelper::generateSelfSignedCertificate(key, QStringLiteral("KDE Connect RDP"));
    if (key.isNull() || certificate.isNull()) {
        m_lastError = i18n("Could not generate temporary RDP TLS credentials.");
        m_tlsDirectory.reset();
        return false;
    }

    const QString keyPath = m_tlsDirectory->filePath(QStringLiteral("server.key"));
    const QString certificatePath = m_tlsDirectory->filePath(QStringLiteral("server.crt"));
    QFile keyFile(keyPath);
    QFile certificateFile(certificatePath);
    if (!keyFile.open(QIODevice::WriteOnly | QIODevice::Truncate) || keyFile.write(key.toPem()) <= 0 || !certificateFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || certificateFile.write(certificate.toPem()) <= 0) {
        m_lastError = i18n("Could not write temporary RDP TLS credentials.");
        m_tlsDirectory.reset();
        return false;
    }
    keyFile.close();
    certificateFile.close();
    QFile::setPermissions(keyPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QFile::setPermissions(certificatePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    m_certificateFingerprint = QString::fromLatin1(certificate.digest(QCryptographicHash::Sha256).toHex());
    return true;
}

quint16 VirtualMonitorPlugin::findAvailablePort() const
{
    for (int port = KdeConnectMaxPort; port >= KdeConnectMinPort; --port) {
        QTcpServer probe;
        if (probe.listen(QHostAddress::Any, quint16(port))) {
            probe.close();
            return quint16(port);
        }
    }
    return 0;
}

void VirtualMonitorPlugin::probeServerReady(quint64 generation, int attemptsRemaining)
{
    if (generation != m_sessionGeneration || !m_process || m_process->state() == QProcess::NotRunning) {
        return;
    }
    if (attemptsRemaining <= 0) {
        m_lastError = i18n("The RDP server did not become ready in time.");
        sendStatus(QStringLiteral("failed"), m_lastError);
        stopSession(false);
        return;
    }

    auto *socket = new QTcpSocket(this);
    auto *timeout = new QTimer(socket);
    timeout->setSingleShot(true);
    timeout->setInterval(250);

    const auto retry = [this, socket, generation, attemptsRemaining]() {
        if (socket->property("kdeconnectProbeFinished").toBool()) {
            return;
        }
        socket->setProperty("kdeconnectProbeFinished", true);
        socket->abort();
        socket->deleteLater();
        QTimer::singleShot(ServerProbeIntervalMs, this, [this, generation, attemptsRemaining]() {
            probeServerReady(generation, attemptsRemaining - 1);
        });
    };

    connect(socket, &QTcpSocket::connected, this, [this, socket, timeout, generation]() {
        if (socket->property("kdeconnectProbeFinished").toBool()) {
            return;
        }
        socket->setProperty("kdeconnectProbeFinished", true);
        timeout->stop();
        socket->abort();
        socket->deleteLater();
        if (generation != m_sessionGeneration) {
            return;
        }

        m_serverReady = true;
        sendConnectionInfo();
        QTimer::singleShot(ClientConnectionTimeoutMs, this, [this, generation]() {
            if (generation == m_sessionGeneration && m_serverReady && !m_clientConnected) {
                m_lastError = i18n("The remote RDP client did not connect in time.");
                sendStatus(QStringLiteral("failed"), m_lastError);
                stopSession(false);
            }
        });
    });
    connect(socket, &QTcpSocket::errorOccurred, this, [retry](QAbstractSocket::SocketError) {
        retry();
    });
    connect(timeout, &QTimer::timeout, this, retry);

    timeout->start();
    socket->connectToHost(QHostAddress::LocalHost, m_port);
}

void VirtualMonitorPlugin::sendConnectionInfo()
{
    NetworkPacket np(PACKET_TYPE_VIRTUALMONITOR_REQUEST,
                     {
                         {QStringLiteral("protocolVersion"), ProtocolVersion},
                         {QStringLiteral("action"), ActionConnect},
                         {QStringLiteral("sessionId"), m_currentRequest.sessionId},
                         {QStringLiteral("protocol"), QStringLiteral("rdp")},
                         {QStringLiteral("username"), m_username},
                         {QStringLiteral("password"), m_password},
                         {QStringLiteral("port"), int(m_port)},
                         {QStringLiteral("certificateFingerprint"), m_certificateFingerprint},
                         {QStringLiteral("certificateHash"), QStringLiteral("sha256")},
                         {QStringLiteral("security"), QStringLiteral("nla+tls")},
                         {QStringLiteral("status"), QStringLiteral("ready")},
                         {QStringLiteral("rdpFeatures"),
                          QJsonObject{
                              {QStringLiteral("graphicsPipeline"), true},
                              {QStringLiteral("h264"), true},
                              {QStringLiteral("progressive"), true},
                          }},
                     });
    sendPacket(np);
}

void VirtualMonitorPlugin::sendStatus(const QString &status, const QString &message)
{
    sendStatusForSession(m_currentRequest.sessionId, status, message);
}

void VirtualMonitorPlugin::sendStatusForSession(const QString &sessionId, const QString &status, const QString &message)
{
    NetworkPacket np(PACKET_TYPE_VIRTUALMONITOR,
                     {
                         {QStringLiteral("protocolVersion"), ProtocolVersion},
                         {QStringLiteral("action"), ActionStatus},
                         {QStringLiteral("sessionId"), sessionId},
                         {QStringLiteral("status"), status},
                         {QStringLiteral("active"), active()},
                     });
    if (!message.isEmpty()) {
        np.set(QStringLiteral("message"), message);
    }
    sendPacket(np);
}

bool VirtualMonitorPlugin::checkRdpClient() const
{
    if (!QStandardPaths::findExecutable(QS("krdc")).isEmpty()) {
        return true;
    }
    return checkDefaultSchemeHandler(QS("rdp"));
}

bool VirtualMonitorPlugin::checkDefaultSchemeHandler(const QString &scheme) const
{
#if defined(Q_OS_WIN)
    QSettings handler(QS("HKEY_CLASSES_ROOT\\") + scheme, QSettings::Registry64Format);
    if (handler.value(QStringLiteral("URL Protocol")).isNull()) {
        return false;
    }

    QSettings command(QS("HKEY_CLASSES_ROOT\\") + scheme + QS("\\shell\\open\\command"), QSettings::Registry64Format);
    return !command.value(QStringLiteral("Default")).isNull();
#else
    const KService::Ptr service = KApplicationTrader::preferredService(QS("x-scheme-handler/") + scheme);
    return service && !service->desktopEntryName().isEmpty();
#endif
}

#include "moc_virtualmonitorplugin.cpp"
#include "virtualmonitorplugin.moc"
