#pragma once
#include <QObject>
#include <QFileSystemWatcher>
#include <QColor>
#include <QFile>
#include <QRegularExpression>
#include <QDir>
#include <QTimer>

class ThemeWatcher : public QObject {
    Q_OBJECT
public:
    explicit ThemeWatcher(QObject* parent = nullptr)
        : QObject(parent), m_currentColor(122, 162, 247)
    {
        m_watcher = new QFileSystemWatcher(this);
        m_debounceTimer = new QTimer(this);
        m_debounceTimer->setSingleShot(true);
        m_debounceTimer->setInterval(60);

        QString home = QDir::homePath();
        m_targetFiles = {
            home + "/.config/quickshell/wallpaper-picker/Colors.qml",
            home + "/.config/waybar/style.css"
        };

        for (const auto& path : m_targetFiles) {
            QFileInfo fi(path);
            if (fi.dir().exists()) m_watcher->addPath(fi.dir().absolutePath());
            if (fi.exists()) m_watcher->addPath(path);
        }

        connect(m_watcher, &QFileSystemWatcher::fileChanged, m_debounceTimer, qOverload<>(&QTimer::start));
        connect(m_watcher, &QFileSystemWatcher::directoryChanged, m_debounceTimer, qOverload<>(&QTimer::start));
        connect(m_debounceTimer, &QTimer::timeout, this, &ThemeWatcher::reloadTheme);

        reloadTheme();
    }

    QColor currentColor() const { return m_currentColor; }

signals:
    void themeChanged(const QColor& color);

private:
    void reloadTheme() {
        static const QRegularExpression regex(R"(primary[^\w#]*#([0-9a-fA-F]{6}))", QRegularExpression::CaseInsensitiveOption);

        for (const auto& path : m_targetFiles) {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString content = QString::fromUtf8(file.readAll());
                file.close();

                auto match = regex.match(content);
                if (match.hasMatch()) {
                    QColor newColor("#" + match.captured(1));
                    if (newColor.isValid() && newColor != m_currentColor) {
                        m_currentColor = newColor;
                        emit themeChanged(m_currentColor);
                        return;
                    }
                }
            }
        }
    }

    QFileSystemWatcher* m_watcher;
    QTimer* m_debounceTimer;
    QStringList m_targetFiles;
    QColor m_currentColor;
};