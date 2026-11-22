#include "visualizerwindow.h"
#include <QRandomGenerator>
#include <QEvent>
#include <LayerShellQt/Window>
#include <cmath>

VisualizerWindow::VisualizerWindow(std::shared_ptr<AudioEngine> audio, QScreen* screen)
    : m_audio(audio), m_targetScreen(screen), m_accentColor(122, 162, 247)
{
    setScreen(screen);

    // Включаем альфа-канал для аппаратного контекста Wayland
    QSurfaceFormat format;
    format.setAlphaBufferSize(8);
    format.setDepthBufferSize(0);
    format.setStencilBufferSize(0);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    setFormat(format);

    m_smoothBands.fill(0.0f);

    m_theme = new ThemeWatcher(this);
    m_accentColor = m_theme->currentColor();
    updateGlowSprite();

    connect(m_theme, &ThemeWatcher::themeChanged, this, [this](const QColor& c) {
        m_accentColor = c;
        updateGlowSprite();
    });

    initParticles();

    m_frameTimer = new QTimer(this);
    connect(m_frameTimer, &QTimer::timeout, this, [this]() {
        if (m_isActive) {
            m_globalFade = std::min(1.0f, m_globalFade + 0.05f);
        } else {
            m_globalFade = std::max(0.0f, m_globalFade - 0.03f);
            if (m_globalFade <= 0.001f) {
                m_frameTimer->stop();
                update();
                return;
            }
        }

        for (int i = 0; i < AudioEngine::NUM_BANDS; ++i) {
            float target = m_isActive ? m_audio->getBand(i) : 0.0f;
            if (target > m_smoothBands[i]) {
                float attack = 0.25f + (i * 0.03f);
                m_smoothBands[i] += (target - m_smoothBands[i]) * attack;
            } else {
                float decay = 0.08f;
                m_smoothBands[i] += (target - m_smoothBands[i]) * decay;
            }
        }
        update();
    });
}

void VisualizerWindow::setPlaybackActive(bool active) {
    m_isActive = active;
    if (active && !m_frameTimer->isActive()) {
        m_frameTimer->start(16);
    }
}

void VisualizerWindow::initializeGL() {
    initializeOpenGLFunctions();
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void VisualizerWindow::setupLayerShell() {
    if (m_layerConfigured) return;

    if (auto* lWin = LayerShellQt::Window::get(this)) {
        lWin->setScreen(m_targetScreen);
        lWin->setLayer(LayerShellQt::Window::LayerBackground);

        LayerShellQt::Window::Anchors anchors;
        anchors.setFlag(LayerShellQt::Window::AnchorTop);
        anchors.setFlag(LayerShellQt::Window::AnchorBottom);
        anchors.setFlag(LayerShellQt::Window::AnchorLeft);
        anchors.setFlag(LayerShellQt::Window::AnchorRight);
        lWin->setAnchors(anchors);

        lWin->setExclusiveZone(-1);
        lWin->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        m_layerConfigured = true;
    }
}

bool VisualizerWindow::event(QEvent* e) {
    if (e->type() == QEvent::Show || e->type() == QEvent::Expose) {
        setupLayerShell();
    }
    return QOpenGLWindow::event(e);
}

void VisualizerWindow::updateGlowSprite() {
    const int size = 256;
    m_glowSprite = QPixmap(size, size);
    m_glowSprite.fill(Qt::transparent);

    QPainter p(&m_glowSprite);
    p.setRenderHint(QPainter::Antialiasing);

    QRadialGradient grad(size / 2.0, size / 2.0, size / 2.0);
    grad.setColorAt(0.0, QColor(m_accentColor.red(), m_accentColor.green(), m_accentColor.blue(), 255));
    grad.setColorAt(0.65, QColor(m_accentColor.red(), m_accentColor.green(), m_accentColor.blue(), 85));
    grad.setColorAt(1.0, QColor(m_accentColor.red(), m_accentColor.green(), m_accentColor.blue(), 0));

    p.setBrush(grad);
    p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, size, size);
}

void VisualizerWindow::initParticles() {
    m_circles.resize(45);
    for (auto& c : m_circles) {
        resetParticle(c, true);
    }
}

void VisualizerWindow::resetParticle(Circle& c, bool randomProgress) {
    float w = m_targetScreen ? m_targetScreen->geometry().width() : (width() > 0 ? width() : 1920);
    float h = m_targetScreen ? m_targetScreen->geometry().height() : (height() > 0 ? height() : 1080);

    auto* rng = QRandomGenerator::global();
    auto randRange = [rng](float minVal, float maxVal) -> float {
        return minVal + static_cast<float>(rng->generateDouble()) * (maxVal - minVal);
    };

    c.bandIndex = rng->bounded(0, AudioEngine::NUM_BANDS);

    float freqFactor = static_cast<float>(c.bandIndex) / (AudioEngine::NUM_BANDS - 1);
    float minR = 55.0f - (freqFactor * 30.0f);
    float maxR = 115.0f - (freqFactor * 50.0f);
    c.baseRadius = randRange(minR, maxR);

    c.maxAlpha = randRange(0.12f, 0.28f);

    c.offsetR = rng->bounded(-12, 13);
    c.offsetG = rng->bounded(-12, 13);
    c.offsetB = rng->bounded(-12, 13);

    QVector2D dir(w, -h);
    dir.normalize();

    float baseSpeed = 1.0f + (freqFactor * 0.8f);
    float speed = randRange(baseSpeed, baseSpeed + 1.1f);

    float angleJitter = randRange(-0.08f, 0.08f);
    float cosA = std::cos(angleJitter);
    float sinA = std::sin(angleJitter);

    c.velocity = QVector2D(
        (dir.x() * cosA - dir.y() * sinA) * speed,
        (dir.x() * sinA + dir.y() * cosA) * speed
        );

    float margin = 220.0f;
    QVector2D startPos;

    if (rng->generateDouble() < 0.5) {
        startPos = QVector2D(-margin, randRange(h * 0.3f, h + margin));
    } else {
        startPos = QVector2D(randRange(-margin, w * 0.7f), h + margin);
    }

    if (randomProgress) {
        float progress = static_cast<float>(rng->generateDouble());
        c.pos = startPos + QVector2D(w + margin * 2.0f, -(h + margin * 2.0f)) * progress;
        c.traveledDistance = 300.0f;
    } else {
        c.pos = startPos;
        c.traveledDistance = 0.0f;
    }
}

void VisualizerWindow::paintGL() {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_globalFade <= 0.001f) return;

    // В QOpenGLWindow QPainter транслирует всё сразу в GPU шейдеры
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    float w = width();
    QRectF srcRect = m_glowSprite.rect();

    for (auto& c : m_circles) {
        c.pos += c.velocity;
        c.traveledDistance += c.velocity.length();

        if (c.pos.x() > w + 240.0f || c.pos.y() < -240.0f) {
            resetParticle(c, false);
        }

        float fadeIn = std::clamp(c.traveledDistance / 160.0f, 0.0f, 1.0f);
        float bandPower = m_smoothBands[c.bandIndex];
        float currentRadius = c.baseRadius * (1.0f + bandPower * 0.75f);

        float alpha = c.maxAlpha * (1.0f + bandPower * 0.55f) * fadeIn * m_globalFade;
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        if (alpha <= 0.005f) continue;

        p.setOpacity(alpha);
        QRectF targetRect(
            c.pos.x() - currentRadius,
            c.pos.y() - currentRadius,
            currentRadius * 2.0f,
            currentRadius * 2.0f
            );
        p.drawPixmap(targetRect, m_glowSprite, srcRect);
    }
}