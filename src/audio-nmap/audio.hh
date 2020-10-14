#ifndef AUDIO_HH
#define AUDIO_HH

#include <memory>
#include <string>

#include "helpers.hh"

#include <alsa/asoundlib.h>

class SoundCard {
private:
    class Device {
	std::string device_name_, annotation_;
	snd_pcm_t * pcm_;
	snd_pcm_uframes_t buffer_size_, period_size_;

    public:
	Device( const std::string & name, const std::string & annotation, const snd_pcm_stream_t stream );
	~Device();
	void read_params();
	std::string name() const;

	operator snd_pcm_t * () { return pcm_; }

	unsigned int buffer_size() const { return buffer_size_; }
	unsigned int period_size() const { return period_size_; }

	/* can't copy or assign */
	Device( const Device & other ) = delete;
	Device & operator=( const Device & other ) = delete;
    };

    Device microphone_, speaker_;
    bool linked_;

    static void set_params( Device & pcm );

    void check_state( const snd_pcm_state_t state );

public:
    SoundCard( const std::string & microphone_name, const std::string & speaker_name );

    void start();
    void stop();

    void play_and_record( const RealSignal & out, RealSignal & in );

    unsigned int period_size() const { return speaker_.period_size(); }
};

#endif /* AUDIO_HH */
