#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include <SDL2/SDL.h>
#include "yssimplesound.h"

class RingBuffer
{
private:
	std::vector<uint8_t> buffer;
	size_t head = 0;
	size_t tail = 0;
	size_t size = 0;
	size_t capacity = 0;

public:
	void Init(size_t cap)
	{
		capacity = cap;
		buffer.resize(cap);
		head = 0;
		tail = 0;
		size = 0;
	}

	size_t Write(const uint8_t *data, size_t len)
	{
		size_t written = 0;
		while (written < len && size < capacity) {
			buffer[head] = data[written];
			head = (head + 1) % capacity;
			size++;
			written++;
		}
		return written;
	}

	size_t Read(uint8_t *data, size_t len)
	{
		size_t read = 0;
		while (read < len && size > 0) {
			data[read] = buffer[tail];
			tail = (tail + 1) % capacity;
			size--;
			read++;
		}
		return read;
	}

	size_t GetAvailableWriteSpace() const
	{
		return capacity - size;
	}

	size_t GetSize() const
	{
		return size;
	}

	void Clear()
	{
		head = 0;
		tail = 0;
		size = 0;
	}
};

static RingBuffer g_pcmRingBuffer;

static void SDLAudioCallback(void *userdata, Uint8 *stream, int len)
{
	memset(stream, 0, len);
	if (g_pcmRingBuffer.GetSize() > 0) {
		g_pcmRingBuffer.Read(stream, len);
	}
}

class YsSoundPlayer::APISpecificData
{
public:
	bool initialized = false;
	SDL_AudioSpec wantedSpec;
	SDL_AudioSpec obtainedSpec;

	APISpecificData()
	{
		CleanUp();
	}

	~APISpecificData()
	{
		CleanUp();
	}

	void CleanUp()
	{
		if (initialized) {
			SDL_CloseAudio();
			initialized = false;
		}
	}

	void Start()
	{
		if (initialized) return;

		if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
			return;
		}

		g_pcmRingBuffer.Init(128 * 1024);

		memset(&wantedSpec, 0, sizeof(wantedSpec));
		wantedSpec.freq = 44100;
		wantedSpec.format = AUDIO_S16SYS;
		wantedSpec.channels = 2;
		wantedSpec.samples = 1024;
		wantedSpec.callback = SDLAudioCallback;
		wantedSpec.userdata = nullptr;

		if (SDL_OpenAudio(&wantedSpec, &obtainedSpec) < 0) {
			return;
		}

		SDL_PauseAudio(0);
		initialized = true;
	}

	void End()
	{
		CleanUp();
	}
};

class YsSoundPlayer::SoundData::APISpecificDataPerSoundData
{
public:
	APISpecificDataPerSoundData() {}
	~APISpecificDataPerSoundData() {}
	void CleanUp() {}
};

class YsSoundPlayer::Stream::APISpecificData
{
public:
	APISpecificData() {}
	~APISpecificData() {}
};

YsSoundPlayer::APISpecificData *YsSoundPlayer::CreateAPISpecificData(void)
{
	return new APISpecificData;
}

void YsSoundPlayer::DeleteAPISpecificData(APISpecificData *ptr)
{
	delete ptr;
}

YSRESULT YsSoundPlayer::StartAPISpecific(void)
{
	if (api) {
		api->Start();
	}
	return YSOK;
}

YSRESULT YsSoundPlayer::EndAPISpecific(void)
{
	if (api) {
		api->End();
	}
	return YSOK;
}

void YsSoundPlayer::SetVolumeAPISpecific(SoundData &, float, float) {}

YSRESULT YsSoundPlayer::PlayOneShotAPISpecific(SoundData &dat)
{
	if (dat.SizeInByte() > 0 && dat.DataPointer()) {
		SDL_LockAudio();
		g_pcmRingBuffer.Write(dat.DataPointer(), dat.SizeInByte());
		SDL_UnlockAudio();
	}
	return YSOK;
}

YSRESULT YsSoundPlayer::PlayBackgroundAPISpecific(SoundData &dat)
{
	return PlayOneShotAPISpecific(dat);
}

void YsSoundPlayer::StopAPISpecific(SoundData &)
{
	SDL_LockAudio();
	g_pcmRingBuffer.Clear();
	SDL_UnlockAudio();
}

void YsSoundPlayer::PauseAPISpecific(SoundData &)
{
	SDL_PauseAudio(1);
}

void YsSoundPlayer::ResumeAPISpecific(SoundData &)
{
	SDL_PauseAudio(0);
}

void YsSoundPlayer::KeepPlayingAPISpecific(void) {}

YSBOOL YsSoundPlayer::IsPlayingAPISpecific(const SoundData &) const
{
	return (g_pcmRingBuffer.GetSize() > 0) ? YSTRUE : YSFALSE;
}

double YsSoundPlayer::GetCurrentPositionAPISpecific(const SoundData &) const
{
	return 0.0;
}

YsSoundPlayer::SoundData::APISpecificDataPerSoundData *YsSoundPlayer::SoundData::CreateAPISpecificData(void)
{
	return new APISpecificDataPerSoundData;
}

void YsSoundPlayer::SoundData::DeleteAPISpecificData(APISpecificDataPerSoundData *ptr)
{
	delete ptr;
}

bool YsSoundPlayer::SoundData::IsPrepared(YsSoundPlayer &)
{
	return true;
}

YSRESULT YsSoundPlayer::SoundData::PreparePlay(YsSoundPlayer &)
{
	return YSOK;
}

void YsSoundPlayer::SoundData::CleanUpAPISpecific(void)
{
	if (api) {
		api->CleanUp();
	}
}

YsSoundPlayer::Stream::APISpecificData *YsSoundPlayer::Stream::CreateAPISpecificData(void)
{
	return new APISpecificData;
}

void YsSoundPlayer::Stream::DeleteAPISpecificData(APISpecificData *ptr)
{
	delete ptr;
}

YSRESULT YsSoundPlayer::StartStreamingAPISpecific(Stream &, StreamingOption)
{
	return YSOK;
}

void YsSoundPlayer::StopStreamingAPISpecific(Stream &)
{
	SDL_LockAudio();
	g_pcmRingBuffer.Clear();
	SDL_UnlockAudio();
}

YSBOOL YsSoundPlayer::StreamPlayerReadyToAcceptNextNumSampleAPISpecific(const Stream &, unsigned int numSamples) const
{
	size_t neededBytes = (size_t)numSamples * 4; // stereo 16-bit = 4 bytes/sample
	return (g_pcmRingBuffer.GetAvailableWriteSpace() >= neededBytes) ? YSTRUE : YSFALSE;
}

YSRESULT YsSoundPlayer::AddNextStreamingSegmentAPISpecific(Stream &, const SoundData &dat)
{
	if (dat.SizeInByte() > 0 && dat.DataPointer()) {
		SDL_LockAudio();
		g_pcmRingBuffer.Write(dat.DataPointer(), dat.SizeInByte());
		SDL_UnlockAudio();
	}
	return YSOK;
}
