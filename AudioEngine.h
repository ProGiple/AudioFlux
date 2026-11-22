#pragma once
#include <pulse/simple.h>
#include <pulse/error.h>
#include <fftw3.h>
#include <vector>
#include <cmath>
#include <atomic>
#include <thread>
#include <array>
#include <algorithm>

class AudioEngine {
public:
    static constexpr int SAMPLES = 1024;
    static constexpr int NUM_BANDS = 8;

    AudioEngine() : m_running(true), m_active(false) {
        for (int i = 0; i < NUM_BANDS; ++i) {
            m_bands[i].store(0.0f);
        }

        for (int i = 0; i < SAMPLES; ++i) {
            m_hannTable[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (SAMPLES - 1)));
        }

        m_worker = std::thread(&AudioEngine::processAudio, this);
    }

    ~AudioEngine() {
        m_running = false;
        if (m_worker.joinable()) {
            m_worker.join();
        }
    }

    void setActive(bool active) {
        m_active.store(active, std::memory_order_relaxed);
        if (!active) {
            for (int i = 0; i < NUM_BANDS; ++i) {
                m_bands[i].store(0.0f, std::memory_order_relaxed);
            }
        }
    }

    float getBand(int index) const {
        if (index < 0 || index >= NUM_BANDS) return 0.0f;
        return m_bands[index].load(std::memory_order_relaxed);
    }

private:
    void processAudio() {
        pa_sample_spec ss;
        ss.format = PA_SAMPLE_S16LE;
        ss.channels = 2;
        ss.rate = 44100;

        pa_buffer_attr attr;
        attr.maxlength = (uint32_t)-1;
        attr.tlength   = (uint32_t)-1;
        attr.prebuf    = (uint32_t)-1;
        attr.minreq    = (uint32_t)-1;
        attr.fragsize  = SAMPLES * sizeof(int16_t) * 2;

        int error = 0;
        pa_simple* pa = pa_simple_new(
            nullptr, "AudioFlux", PA_STREAM_RECORD,
            "@DEFAULT_MONITOR@", "Desktop Audio Stream", &ss, nullptr, &attr, &error
            );

        if (!pa) {
            pa = pa_simple_new(
                nullptr, "AudioFlux", PA_STREAM_RECORD,
                nullptr, "Desktop Audio Fallback", &ss, nullptr, &attr, &error
                );
        }

        if (!pa) return;

        double* in = (double*) fftw_malloc(sizeof(double) * SAMPLES);
        fftw_complex* out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * (SAMPLES / 2 + 1));
        fftw_plan plan = fftw_plan_dft_r2c_1d(SAMPLES, in, out, FFTW_ESTIMATE);

        std::vector<int16_t> buffer(SAMPLES * 2);

        const int bandRanges[NUM_BANDS][2] = {
            {1, 2}, {3, 5}, {6, 12}, {13, 24},
            {25, 55}, {56, 115}, {116, 230}, {231, 410}
        };

        const float bandBoost[NUM_BANDS] = {
            1.8f, 2.2f, 3.4f, 4.8f, 6.5f, 9.0f, 13.0f, 18.0f
        };

        while (m_running) {
            // Чтение блокирует поток ровно на 23.2 мс без лишней нагрузки на CPU
            if (pa_simple_read(pa, buffer.data(), buffer.size() * sizeof(int16_t), &error) < 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Если трек не играет, не тратим ресурсы на FFTW
            if (!m_active.load(std::memory_order_relaxed)) {
                continue;
            }

            for (int i = 0; i < SAMPLES; ++i) {
                double mono = (buffer[i * 2] + buffer[i * 2 + 1]) / 65536.0;
                in[i] = mono * m_hannTable[i];
            }

            fftw_execute(plan);

            for (int b = 0; b < NUM_BANDS; ++b) {
                int start = bandRanges[b][0];
                int end = bandRanges[b][1];
                double energy = 0.0;

                for (int i = start; i <= end; ++i) {
                    double r = out[i][0];
                    double im = out[i][1];
                    energy += std::sqrt(r * r + im * im);
                }
                energy /= (end - start + 1);

                constexpr double NOISE_FLOOR = 0.035;
                if (energy < NOISE_FLOOR) {
                    energy = 0.0;
                } else {
                    energy = (energy - NOISE_FLOOR) / (1.0 - NOISE_FLOOR);
                }

                float val = std::clamp(static_cast<float>(energy * bandBoost[b]), 0.0f, 1.0f);
                m_bands[b].store(val, std::memory_order_relaxed);
            }
        }

        pa_simple_free(pa);
        fftw_destroy_plan(plan);
        fftw_free(in);
        fftw_free(out);
    }

    std::atomic<bool> m_running;
    std::atomic<bool> m_active;
    std::array<double, SAMPLES> m_hannTable;
    std::array<std::atomic<float>, NUM_BANDS> m_bands;
    std::thread m_worker;
};