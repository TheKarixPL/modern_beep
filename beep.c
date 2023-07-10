#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <soundio/soundio.h>

struct tone_t
{
    int freq;
    int length;
    int repeats;
    int delay;
};

const float PI = 3.1415926535f;
static float freq = 440;
static float seconds_offset = 0;
static int mute = 0;

void usage();
void version();
void out_of_memory();
void delay(int milis);
int is_number(char* str);

void tone_default(struct tone_t* tone);
void tone_play(struct tone_t* tone);
void tone_write_callback(struct SoundIoOutStream* outstream, int frame_count_min, int frame_count_max);

int main(int argc, char** argv)
{
    for(int i = 1; i < argc; i++)
    {
        char* arg = argv[i];
        if(strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)
        {
            usage();
            return 1;
        }
        else if(strcmp(arg, "-v") == 0 || strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0)
        {
            version();
            return 1;
        }
    }

    int tone_count = 1;
    for(int i = 1; i < argc; i++)
    {
        char* arg = argv[i];
        if(strcmp(arg, "-n") == 0 || strcmp(arg, "--new") == 0)
            tone_count++;
    }

    struct tone_t* tones = (struct tone_t*)malloc(sizeof(struct tone_t) * tone_count);
    if(tones == NULL)
        out_of_memory();
    for(int i = 0; i < tone_count; i++)
        tone_default(tones + i);

    for(int i = 1, tone_number = 0; i < argc; i++)
    {
        char* arg = argv[i];
        if(strcmp(arg, "-n") == 0 || strcmp(arg, "--new") == 0)
        {
            tone_number++;
            continue;
        }

        if(arg[0] == '-' && arg[2] == '\0' && i + 1 < argc && is_number(argv[i + 1]))
        {
            int value = atoi(argv[i + 1]);
            struct tone_t* tone = tones + tone_number;

            switch(arg[1])
            {
                case 'f':
                    if(value <= 0)
                    {
                        fprintf(stderr, "frequency must be higher than 0");
                        return 1;
                    }
                    else if(value >= 20000)
                    {
                        fprintf(stderr, "frequency must be lesser than 20000");
                        return 1;
                    }

                    tone->freq = value;
                    break;
                case 'l':
                    if(value <= 0)
                    {
                        fprintf(stderr, "length must be higher than 0");
                        return 1;
                    }

                    tone->length = value;
                    break;
                case 'r':
                    if(value <= 0)
                    {
                        fprintf(stderr, "repeat count must be higher than 0");
                        return 1;
                    }

                    tone->repeats = value;
                    break;
                case 'd':
                case 'D':
                    tone->delay = value;
                    break;
                default:
                    usage();
                    return 1;
            }
            i++;
        }
        else
        {
            usage();
            return 1;
        }
    }

    int err;
    struct SoundIo* soundio = soundio_create();
    if(!soundio)
        out_of_memory();

    if((err = soundio_connect(soundio)))
    {
        fprintf(stderr, "soundio: error connecting: %s\n", soundio_strerror(err));
        return 1;
    }

    soundio_flush_events(soundio);

    int default_out_device_index = soundio_default_output_device_index(soundio);
    if(default_out_device_index < 0)
    {
        fprintf(stderr, "soundio: no output device found\n");
        return 1;
    }

    struct SoundIoDevice* device = soundio_get_output_device(soundio, default_out_device_index);
    if(!device)
        out_of_memory();

    struct SoundIoOutStream* outstream = soundio_outstream_create(device);
    if(!outstream)
        out_of_memory();

    outstream->format = SoundIoFormatFloat32NE;
    outstream->write_callback = tone_write_callback;

    if((err = soundio_outstream_open(outstream)))
    {
        fprintf(stderr, "soundio: unable to open device: %s\n", soundio_strerror(err));
        return 1;
    }

    if(outstream->layout_error)
        fprintf(stderr, "soundio: unable to set channel layout: %s\n", soundio_strerror(outstream->layout_error));

    if((err = soundio_outstream_start(outstream)))
    {
        fprintf(stderr, "soundio: unable to start device: %s\n", soundio_strerror(err));
        return 1;
    }
    soundio_flush_events(soundio);
    mute = 1;

    for(int i = 0; i < tone_count; i++)
        tone_play(tones + i);

    soundio_outstream_destroy(outstream);
    soundio_device_unref(device);
    soundio_destroy(soundio);
    free(tones);
    return 0;
}

void usage()
{
    fprintf(stderr, "usage: beep [-f FREQ] [-l LEN] [-r REPEATS] [<-d|-D> DELAY]\n"
                    "beep <TONE_OPTIONS> [-n|--new] <TONE_OPTIONS>\n"
                    "beep <-h|--help>\n"
                    "beep <-v|-V|--version>\n");
}

void version()
{
    fprintf(stderr, "modern_beep 0.1\n");
}

void out_of_memory()
{
    fprintf(stderr, "out of memory\n");
    exit(1);
}

void delay(int milis)
{
    if(milis <= 0)
        return;

    struct timespec ts;
    ts.tv_sec = milis / 1000;
    ts.tv_nsec = (milis % 1000) * 1000000;
    nanosleep(&ts, &ts);
}

int is_number(char* str)
{
    for(; *str != '\0'; str++)
        if(*str != '.' && (*str < '0' || *str > '9'))
            return 0;
    return 1;
}

void tone_default(struct tone_t* tone)
{
    tone->freq = 440;
    tone->length = 1000;
    tone->repeats = 1;
    tone->delay = 50;
}

void tone_play(struct tone_t* tone)
{
    printf("%d %d %d %d\n", tone->freq, tone->length, tone->repeats, tone->delay);
    freq = tone->freq;
    int err;

    for(int i = 0; i < tone->repeats; i++)
    {
        mute = 0;
        delay(tone->length);
        mute = 1;
        delay(tone->delay);
    }
}

void tone_write_callback(struct SoundIoOutStream* outstream, int frame_count_min, int frame_count_max)
{
    const struct SoundIoChannelLayout* layout = &outstream->layout;
    float float_sample_rate = outstream->sample_rate;
    float seconds_per_frame = 1.0f / float_sample_rate;
    struct SoundIoChannelArea* areas;
    int frames_left = frame_count_max;
    int err;

    while(frames_left > 0)
    {
        int frame_count = frames_left;

        if((err = soundio_outstream_begin_write(outstream, &areas, &frame_count)))
        {
            fprintf(stderr, "soundio: %s\n", soundio_strerror(err));
            exit(1);
        }

        if(!frame_count)
            break;

        float radians_per_second = freq * 2.0f * PI;
        for(int frame = 0; frame < frame_count; frame++)
        {
            float sample = sin((seconds_offset + frame * seconds_per_frame) * radians_per_second);
            if(mute)
                sample = 0.0f;
            else if(sample > 0.0f)
                sample = 1.0f;
            else if(sample < 0.0f)
                sample = -1.0f;
            for(int channel = 0; channel < layout->channel_count; channel++)
            {
                float* ptr = (float*)(areas[channel].ptr + areas[channel].step * frame);
                *ptr = sample;
            }
        }

        seconds_offset = fmod(seconds_offset + seconds_per_frame * frame_count, 1.0);

        if((err = soundio_outstream_end_write(outstream)))
        {
            fprintf(stderr, "soundio: %s\n", soundio_strerror(err));
            exit(1);
        }

        frames_left -= frame_count;
    }
}
