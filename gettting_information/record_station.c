#include <stdio.h>
#include <stdlib.h>
#include <rtl-sdr.h>

int main(void){
    rtlsdr_dev_t *dev;
    if (rtlsdr_open(&dev, 0) < 0) {
        printf("Failed to open SDR\n");
        return 1;
    } else {
         printf("Devices found: %u\n", rtlsdr_get_device_count());
         printf("%s\n", rtlsdr_get_device_name(0));
    }
    int center_freq = 91900000; // 91.9 MHz
    int sample_rate = 2048000;  // 2.048 MSPS
    rtlsdr_set_center_freq(dev, center_freq);
    rtlsdr_set_sample_rate(dev, sample_rate);  
    rtlsdr_set_tuner_gain_mode(dev, 0);
    rtlsdr_reset_buffer(dev);
    int samples_in_one_second = 4096000;
    int seconds = 360;
    uint8_t *samples = malloc(samples_in_one_second * seconds * sizeof(uint8_t));
    if (!samples) {
        perror("malloc");
        return 1;
    }
    int n_read;
    rtlsdr_read_sync(dev, samples, samples_in_one_second * seconds, &n_read);
    FILE *f = fopen("s.bin", "ab");
    if (!f) {
        perror("fopen");
        free(samples);
        return 1;
    }
    fwrite(samples, sizeof(uint8_t), n_read, f);
    fclose(f);
    return 0;
}
