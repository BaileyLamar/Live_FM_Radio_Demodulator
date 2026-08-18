#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

struct wav_header
{
  char riff[4];           /* "RIFF"                                  */
  int32_t flength;        /* file length in bytes                    */
  char wave[4];           /* "WAVE"                                  */
  char fmt[4];            /* "fmt "                                  */
  int32_t chunk_size;     /* size of FMT chunk in bytes (usually 16) */
  int16_t format_tag;     /* 1=PCM, 257=Mu-Law, 258=A-Law, 259=ADPCM */
  int16_t num_chans;      /* 1=mono, 2=stereo                        */
  int32_t srate;          /* Sampling rate in samples per second     */
  int32_t bytes_per_sec;  /* bytes per second = srate*bytes_per_samp */
  int16_t bytes_per_samp; /* 2=16-bit mono, 4=16-bit stereo          */
  int16_t bits_per_samp;  /* Number of bits per sample               */
  char data[4];           /* "data"                                  */
  int32_t dlength;        /* data length in bytes (filelength - 44)  */
};

void write_wav_file(int sample_rate, char *fname, short *audio, int audio_length, int duration){
    struct wav_header wavh;
    int header_length = sizeof(struct wav_header);
    strncpy(wavh.riff, "RIFF", 4);
    strncpy(wavh.wave, "WAVE", 4);
    strncpy(wavh.fmt, "fmt ", 4);
    strncpy(wavh.data, "data", 4);

    wavh.chunk_size = 16;
    wavh.format_tag = 1;
    wavh.num_chans = 1;
    wavh.srate = sample_rate;
    wavh.bytes_per_sec = sample_rate * 2;
    wavh.bytes_per_samp = 2;
    wavh.bits_per_samp = wavh.bytes_per_samp * 8;
    wavh.dlength = audio_length * wavh.bytes_per_samp;
    wavh.flength = wavh.dlength + header_length - 8;

    FILE *f = fopen(fname, "wb");
    fwrite(&wavh, 1, header_length, f);
    fwrite(audio, 2, audio_length, f);
    fclose(f);
}
uint8_t *get_samples(int seconds, int sample_rate, int num_samples, char *file_name){
    uint8_t *samples = malloc(num_samples);
    if (!samples) {
        perror("Malloc of samples failed");
        return NULL;
    }
    FILE *f = fopen(file_name, "rb");
    if (!f) {
        perror("Failed to open file");
        free(samples);
        return NULL;
    }
    fread(samples, sizeof(uint8_t), num_samples, f);
    fclose(f);
    return samples;
}
float *demodulation(float *samples, int num_samples, int sample_rate){
    float *signal = malloc(num_samples / 2 * sizeof(float));
    int j = 0;
    for (int i = 3; i < num_samples; i += 2){
        float I0 = samples[i - 3] - 127.5;
        float Q0 = samples[i - 2] - 127.5;
        float I1 = samples[i - 1] - 127.5;
        float Q1 = samples[i] - 127.5;
        // Use conjugate of previous to get delta phi before applying atan function
        signal[j] = atan2((-I0 * Q1 + I1 * Q0), (I0 * I1 + Q1 * Q0)) * sample_rate / (2 * 3.1415); // End of this converts from radians / sample to Hz
        j++;
    }
    return signal;
}
float *pre_demod_filter(uint8_t *signal, int signal_length, float *taps, int filter_length, int decimation){
    float *filtered_signal = malloc(signal_length / decimation * sizeof(float)); // Factor of 2 for I and Q samples
    int index_of_filtered_signal = 0;
    decimation *= 2;
    for (int i = 0; i < signal_length; i += decimation){
        float sampleI = 0;
        float sampleQ = 0;
        for (int j = 0; j < filter_length; j++){
            if (i+1 - 2*j >= 0){
                sampleQ += taps[j] * signal[i + 1 - 2 * j];
            } else {
                break;
            }
            if ((i - 2 * j) >= 0){
                sampleI += taps[j] * signal[i - 2 * j]; 
            } else {
                break;
            }
        }
        filtered_signal[index_of_filtered_signal] = sampleI;
        filtered_signal[index_of_filtered_signal+1] = sampleQ;
        index_of_filtered_signal+=2;
    }
    return filtered_signal;
}
float *post_demod_filter(float *signal, int signal_length, float *taps, int filter_length, int decimation){
    float *filtered_signal = malloc(signal_length / decimation * sizeof(float));
    int index_of_filtered_signal = 0;
    for (int i = 0; i < signal_length; i += decimation){
        float sample = 0;
        for (int j = 0; j < filter_length; j++){
            if ((i - j) >= 0){
                sample += taps[j] * signal[i - j]; 
            } else {
                break;
            }
        }
        filtered_signal[index_of_filtered_signal] = sample;
        index_of_filtered_signal++;
    }
    return filtered_signal;
}

int main(void){
    int seconds = 360;
    int sample_rate = 2048000;  // 2.048 MSPS
    int num_samples = seconds * sample_rate * 2; // Doubled for IQ samples
    char *file_name = "s1.bin";
    int taps_length = 105;
    float taps[] = {0.00015769581854408333, 0.00030416845058021643, 0.0005645396551002686, 0.0009323893311120908, 0.0014127448293827362, 0.001996335507363505, 0.0026562552997163283, 0.0033461372266224676, 0.004000926057954679, 0.004540792337650156, 0.00487828318620048, 0.004928136090512238, 0.004619502464917118, 0.0039086636569665705, 0.0027906306324976113, 0.0013080365394150262, -0.0004446132136637423, -0.002323672294597792, -0.00414524130262263, -0.005701976923580897, -0.006785416530174552, -0.007212468008371441, -0.006853047900064047, -0.005654750430507404, -0.003661714251401963, -0.0010232214910764453, 0.0020096335137726913, 0.005101204261006466, 0.00786171225018415, 0.009889139853281536, 0.010818186173383409, 0.010371570831913793, 0.008407103961282142, 0.0049544958951074705, 0.00023601776893431782, -0.005333134464506076, -0.011167146635604024, -0.01655396505121126, -0.020719063747097807, -0.022902405836779983, -0.02244161663580391, -0.018851694554617902, -0.011892654362075906, -0.00161621817599725, 0.011614536785395717, 0.027136763913230208, 0.04403069726426082, 0.0611910505646848, 0.07742023780487391, 0.09153469929019227, 0.10247322166379923, 0.1093963135680822, 0.11176577374722303, 0.1093963135680822, 0.10247322166379923, 0.09153469929019227, 0.07742023780487391, 0.0611910505646848, 0.04403069726426082, 0.027136763913230208, 0.011614536785395717, -0.00161621817599725, -0.011892654362075906, -0.018851694554617902, -0.02244161663580391, -0.022902405836779983, -0.020719063747097807, -0.01655396505121126, -0.011167146635604024, -0.005333134464506076, 0.00023601776893431782, 0.0049544958951074705, 0.008407103961282142, 0.010371570831913793, 0.010818186173383409, 0.009889139853281536, 0.00786171225018415, 0.005101204261006466, 0.0020096335137726913, -0.0010232214910764453, -0.003661714251401963, -0.005654750430507404, -0.006853047900064047, -0.007212468008371441, -0.006785416530174552, -0.005701976923580897, -0.00414524130262263, -0.002323672294597792, -0.0004446132136637423, 0.0013080365394150262, 0.0027906306324976113, 0.0039086636569665705, 0.004619502464917118, 0.004928136090512238, 0.00487828318620048, 0.004540792337650156, 0.004000926057954679, 0.0033461372266224676, 0.0026562552997163283, 0.001996335507363505, 0.0014127448293827362, 0.0009323893311120908, 0.0005645396551002686, 0.00030416845058021643, 0.00015769581854408333};
    int pre_demod_filter_decimation = 8;
    int post_demod_filter_decimation = 5;
    char *output_filename = "audio.wav";

    uint8_t *samples = get_samples(seconds, sample_rate, num_samples, file_name);
    if (samples == NULL){
        return 1;
    }
    // Low pass FIR filter to remove other stations. 
    float *filtered_samples = pre_demod_filter(samples, num_samples, taps, taps_length, pre_demod_filter_decimation);
    int filtered_samples_length = num_samples / pre_demod_filter_decimation;
    int filtered_sample_rate = sample_rate / pre_demod_filter_decimation;

    // Demodulate signal. Signal length halved since we now have the phase
    // differences instead of the IQ Samples. 
    float *signal = demodulation(filtered_samples, filtered_samples_length, filtered_sample_rate);
    int signal_length = filtered_samples_length / 2; // Since we are done with IQ samples and now have phases

    // Apply same low pass FIR filter to dramatically reduce noise 
    float *post_demod_filtered = post_demod_filter(signal, signal_length, taps, taps_length, post_demod_filter_decimation);
    int post_demod_signal_length = signal_length / post_demod_filter_decimation;

    short *audio = malloc(post_demod_signal_length * sizeof(short));
    int audio_length = post_demod_signal_length;

    // Find maximum value in order to normalize audio signal into .WAV file data
    float max = 0.0;
    for (int i = 0; i < post_demod_signal_length; i++){
        if (fabs(post_demod_filtered[i]) > max){
            max = fabs(post_demod_filtered[i]);
        }
    }
    // Normalize audio into range [-32767, 32767]
    for (int i = 0; i < post_demod_signal_length; i++){
        audio[i] = (short) (post_demod_filtered[i] / max * 32767);
    }
    // Write audio into .WAV file
    int audio_sample_rate = 51200;
    printf("%d", audio_length);
    write_wav_file(audio_sample_rate, output_filename, audio, audio_length, seconds);
    // Free the heap
    free(samples);
    free(filtered_samples);
    free(signal);
    free(post_demod_filtered);
    free(audio);
    return 0;
}