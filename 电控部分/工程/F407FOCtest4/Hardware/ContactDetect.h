#ifndef __CONTACT_DETECT_H
#define __CONTACT_DETECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define CONTACT_DETECT_BASELINE_MIN_SAMPLES 50U
#define CONTACT_DETECT_BASELINE_ALPHA       0.02f

typedef struct
{
	float baseline_current;
	float delta_threshold;
	uint16_t baseline_samples;
	uint16_t hit_count;
	uint16_t hit_count_required;
	uint8_t touched;
} ContactDetector;

void ContactDetect_Init(ContactDetector *det, float delta_threshold, uint16_t hit_count_required);
void ContactDetect_Reset(ContactDetector *det);
uint8_t ContactDetect_Update(ContactDetector *det, float iq_feedback, uint8_t enable);
uint8_t ContactDetect_IsTouched(const ContactDetector *det);

#ifdef __cplusplus
}
#endif

#endif
