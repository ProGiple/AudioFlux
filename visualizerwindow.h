#pragma once
#include <QOpenGLWindow>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QVector2D>
#include <QPixmap>
#include <QTimer>
#include <QScreen>
#include <vector>
#include <array>
#include <memory>
#include "AudioEngine.h"
#include "ThemeWatcher.h"

struct Circle {
    QVector2D pos;
    QVector2D velocity;
    float baseRadius;
    float maxAlpha;
    int bandIndex;
    int offsetR, offsetG, offsetB;
    float traveledDistance;
};

class VisualizerWindow : public QOpenGLWindow, protected QOpenGLFunctions {
    Q_OBJECT
public:
    VisualizerWindow(std::shared_ptr<AudioEngine> audio, QScreen* screen);
    ~VisualizerWindow() override = default;

    QScreen* targetScreen() const { return m_targetScreen; }
    void setPlaybackActive(bool active);

protected:
    void initializeGL() override;
    void paintGL() override;
    bool event(QEvent* event) override;

private:
    void initParticles();
    void resetParticle(Circle& c, bool randomProgress);
    void updateGlowSprite();
    void setupLayerShell();

    std::shared_ptr<AudioEngine> m_audio;
    QScreen* m_targetScreen;
    QColor m_accentColor;
    ThemeWatcher* m_theme;
    QTimer* m_frameTimer;
    std::vector<Circle> m_circles;
    std::array<float, AudioEngine::NUM_BANDS> m_smoothBands;

    bool m_layerConfigured = false;
    bool m_isActive = false;
    float m_globalFade = 0.0f;
    QPixmap m_glowSprite;
};