/* Standalone regression test: compile with the host C compiler. */
#include "../src/hle/audio_pcm_surround.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    int channel;
    for (channel = 0; channel < 6; ++channel) {
        int16_t pcm[6] = {0};
        const int16_t expected[6][2] = {
            {10000, 0}, {0, 10000}, {7070, 7070},
            {0, 0}, {7070, 0}, {0, 7070}
        };
        pcm[channel] = 10000;
        assert(recomp_pcm16_surround_to_stereo((uint8_t *)pcm, sizeof pcm) == 4);
        assert(pcm[0] == expected[channel][0] && pcm[1] == expected[channel][1]);
    }
    {
        int16_t pcm[18] = {32767,32767,32767,32767,32767,32767,
            -32768,-32768,-32768,-32768,-32768,-32768,
            123,-456,0,0,0,0};
        assert(recomp_pcm16_surround_to_stereo((uint8_t *)pcm, sizeof pcm) == 12);
        assert(pcm[0] == 32767 && pcm[1] == 32767);
        assert(pcm[2] == -32768 && pcm[3] == -32768);
        assert(pcm[4] == 123 && pcm[5] == -456);
    }
    {
        uint8_t unaligned[25] = {0};
        int16_t sample = 10000, result;
        memcpy(unaligned + 1 + 4, &sample, 2);
        assert(recomp_pcm16_surround_to_stereo(unaligned + 1, 24) == 8);
        memcpy(&result, unaligned + 1, 2);
        assert(result == 7070);
        assert(unaligned[0] == 0);
    }
    puts("surround audio: channel routing, saturation, frame count and unaligned input passed");
    return 0;
}
