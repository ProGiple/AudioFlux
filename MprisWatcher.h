#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QDBusInterface>
#include <QDBusContext>
#include <QMap>

class MprisWatcher : public QObject, protected QDBusContext {
    Q_OBJECT
public:
    explicit MprisWatcher(QObject* parent = nullptr) : QObject(parent) {
        auto bus = QDBusConnection::sessionBus();

        m_serviceWatcher = new QDBusServiceWatcher(
            "org.mpris.MediaPlayer2*",
            bus,
            QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
            this
            );

        connect(m_serviceWatcher, &QDBusServiceWatcher::serviceRegistered, this, &MprisWatcher::onServiceRegistered);
        connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, &MprisWatcher::onServiceUnregistered);

        auto reply = bus.interface()->registeredServiceNames();
        if (reply.isValid()) {
            for (const QString& service : reply.value()) {
                if (service.startsWith("org.mpris.MediaPlayer2")) {
                    onServiceRegistered(service);
                }
            }
        }
        recheckPlaybackState();
    }

    bool isPlaying() const { return m_isPlaying; }

signals:
    void playbackStateChanged(bool isPlaying);

private slots:
    void onServiceRegistered(const QString& service) {
        if (!service.startsWith("org.mpris.MediaPlayer2.")) return;

        QDBusConnection::sessionBus().connect(
            service,
            "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties",
            "PropertiesChanged",
            this,
            SLOT(onPropertiesChanged(QString, QVariantMap, QStringList))
            );

        updatePlayerStatus(service);
    }

    void onServiceUnregistered(const QString& service) {
        m_playerStates.remove(service);
        recheckPlaybackState();
    }

    void onPropertiesChanged(const QString& interfaceName,
                             const QVariantMap& changedProperties,
                             const QStringList&)
    {
        if (interfaceName != "org.mpris.MediaPlayer2.Player") return;

        if (changedProperties.contains("PlaybackStatus")) {
            QString status = changedProperties["PlaybackStatus"].toString();
            QString service = message().service();
            m_playerStates[service] = (status.compare("Playing", Qt::CaseInsensitive) == 0);
            recheckPlaybackState();
        }
    }

private:
    void updatePlayerStatus(const QString& service) {
        QDBusInterface player(service, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", QDBusConnection::sessionBus());
        if (player.isValid()) {
            QVariant status = player.property("PlaybackStatus");
            if (status.isValid()) {
                m_playerStates[service] = (status.toString().compare("Playing", Qt::CaseInsensitive) == 0);
                recheckPlaybackState();
            }
        }
    }

    void recheckPlaybackState() {
        bool anyPlaying = false;
        for (bool playing : m_playerStates) {
            if (playing) {
                anyPlaying = true;
                break;
            }
        }

        if (m_isPlaying != anyPlaying) {
            m_isPlaying = anyPlaying;
            emit playbackStateChanged(m_isPlaying);
        }
    }

    QDBusServiceWatcher* m_serviceWatcher;
    QMap<QString, bool> m_playerStates;
    bool m_isPlaying = false;
};