/* PCM16 5.1 (FL FR FC LFE BL BR) folded in place for stereo-only voices.
 * Match audio_speakers.c: centre and surrounds at -3 dB; omit LFE.
 * Saturate sums before storing signed PCM to prevent wraparound distortion.
 */
#ifndef RECOMP_AUDIO_PCM_SURROUND_H
#define RECOMP_AUDIO_PCM_SURROUND_H
#include <stdint.h>
#include <string.h>

static uint32_t recomp_pcm16_surround_to_stereo(uint8_t *pcm, uint32_t bytes)
{
    uint32_t frames = bytes / 12u, i;
    for (i = 0; i < frames; ++i) {
        int16_t source[6], stereo[2];
        int32_t left, right;
        memcpy(source, pcm + i * 12u, sizeof source);
        left = source[0] + ((int32_t)source[2] + source[4]) * 23170 / 32768;
        right = source[1] + ((int32_t)source[2] + source[5]) * 23170 / 32768;
        stereo[0] = (int16_t)(left > 32767 ? 32767 : left < -32768 ? -32768 : left);
        stereo[1] = (int16_t)(right > 32767 ? 32767 : right < -32768 ? -32768 : right);
        memcpy(pcm + i * 4u, stereo, sizeof stereo);
    }
    return frames * 4u;
}
#endif
