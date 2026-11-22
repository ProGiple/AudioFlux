#include "visualizerwindow.h"
#include "AudioEngine.h"
#include "MprisWatcher.h"

#include <QGuiApplication>
#include <QScreen>
#include <vector>
#include <memory>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");

    QGuiApplication a(argc, argv);
    a.setApplicationName("AudioFlux");

    auto audio = std::make_shared<AudioEngine>();
    auto mpris = std::make_unique<MprisWatcher>();

    std::vector<std::unique_ptr<VisualizerWindow>> windows;

    auto createForScreen = [&](QScreen* screen) {
        auto win = std::make_unique<VisualizerWindow>(audio, screen);
        win->setPlaybackActive(mpris->isPlaying());
        win->show();
        windows.push_back(std::move(win));
    };

    for (QScreen* screen : QGuiApplication::screens()) {
        createForScreen(screen);
    }

    QObject::connect(&a, &QGuiApplication::screenAdded, [&](QScreen* screen) {
        createForScreen(screen);
    });

    QObject::connect(&a, &QGuiApplication::screenRemoved, [&](QScreen* screen) {
        std::erase_if(windows, [screen](const auto& w) {
            return w->targetScreen() == screen;
        });
    });

    auto updatePlaybackState = [&](bool isPlaying) {
        audio->setActive(isPlaying);
        for (auto& win : windows) {
            win->setPlaybackActive(isPlaying);
        }
    };

    QObject::connect(mpris.get(), &MprisWatcher::playbackStateChanged, updatePlaybackState);
    updatePlaybackState(mpris->isPlaying());

    return a.exec();
}