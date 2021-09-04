#pragma once

#include "SoundManagerBase.hpp"
#include <pulse/pulseaudio.h>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <memory>

#include <complex.h>
#include <tgmath.h>
#include <fftw3.h>

//#include "BeatDetektor.h"

#include "libbeat/beatanalyser.h"

enum w_type
{
	WINDOW_TRIANGLE,
	WINDOW_HANNING,
	WINDOW_HAMMING,
	WINDOW_BLACKMAN,
	WINDOW_BLACKMAN_HARRIS,
	WINDOW_FLAT,
	WINDOW_WELCH,
};

class PulseAudioSoundManager : public SoundManagerBase
{
public:
	PulseAudioSoundManager();
	virtual ~PulseAudioSoundManager();

public:
	virtual void start(bool isEnabled);
	void addDevice(const pa_source_info *l, int eol);
	void checkPulse();
    bool gotDrum()
    {
        if (!new_data)
            return false;
        new_data = false;
        return m_beatanalyzer->getDrumBeat() || m_beatanalyzer->getSnareBeat();
    }


protected:
	virtual bool init();
	virtual void updateFft();

private:
	void Uninit();
	void init_buffers();
	void process_fft();
	void populatePulseaudioDeviceList();

	static void *pa_fft_thread(void *arg);
	static void context_state_cb(pa_context *c, void *userdata);
	static void stream_state_cb(pa_stream *s, void *userdata);
	static void stream_read_cb (pa_stream *p, size_t nbytes, void *userdata);
	static void stream_success_cb (pa_stream *p, int success, void *userdata)
	{
		(void)(p);
		(void)(success);
		(void)(userdata);
	}

	//QTimer m_timer;
	//QTimer m_pa_alive_timer;
	std::mutex m_mutex;
	int m_cont = 0;

	/* Pulse */
	int m_pa_ready = 0;
	pa_threaded_mainloop *m_main_loop = nullptr;
	pa_context *m_context = nullptr;
	pa_stream *m_stream = nullptr;
	char *m_server = nullptr; // TODO: add server selector?
	int m_error;
	pa_sample_spec m_ss;
	int m_buffering = 50;
	std::vector<std::pair<std::string, std::string>> m_devices; // buffer devices for index-to-name mapping purposes

	/* Buffer */
	std::vector<float> m_pa_buf;
	size_t m_pa_samples_read, m_buffer_samples;

	/* FFT */
	fftwf_complex *m_output = nullptr; //special buffer with proper SIMD alignments
	std::vector<float> m_input;
	fftwf_plan m_plan = nullptr;
	std::vector<float> m_weights;
    float timer_seconds{0};
    int bpm_temp = 0;
    bool new_data = false;

public:
    //BeatDetektor m_detektor;

    std::unique_ptr<libbeat::BeatAnalyser> m_beatanalyzer;
    std::unique_ptr<libbeat::FFT> m_FFT;
    int bpm = 0;
};
