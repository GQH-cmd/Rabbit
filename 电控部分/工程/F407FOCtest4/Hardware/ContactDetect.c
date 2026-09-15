#include "ContactDetect.h"

static float ContactDetect_Abs(float value)
{
	return (value < 0.0f) ? -value : value;
}

void ContactDetect_Init(ContactDetector *det, float delta_threshold, uint16_t hit_count_required)
{
	if (det == 0)
	{
		return;
	}

	det->baseline_current = 0.0f;
	det->delta_threshold = delta_threshold;
	det->baseline_samples = 0U;
	det->hit_count = 0U;
	det->hit_count_required = (hit_count_required == 0U) ? 1U : hit_count_required;
	det->touched = 0U;
}

void ContactDetect_Reset(ContactDetector *det)
{
	if (det == 0)
	{
		return;
	}

	det->baseline_current = 0.0f;
	det->baseline_samples = 0U;
	det->hit_count = 0U;
	det->touched = 0U;
}

uint8_t ContactDetect_Update(ContactDetector *det, float iq_feedback, uint8_t enable)
{
	float current;
	float limit;

	if (det == 0)
	{
		return 0U;
	}

	if (enable == 0U)
	{
		det->hit_count = 0U;
		return det->touched;
	}

	current = ContactDetect_Abs(iq_feedback);

	if (det->baseline_samples == 0U)
	{
		det->baseline_current = current;
	}
	else if (det->touched == 0U)
	{
		det->baseline_current += CONTACT_DETECT_BASELINE_ALPHA * (current - det->baseline_current);
	}

	if (det->baseline_samples < CONTACT_DETECT_BASELINE_MIN_SAMPLES)
	{
		det->baseline_samples++;
		det->hit_count = 0U;
		return det->touched;
	}

	limit = det->baseline_current + det->delta_threshold;
	if (current > limit)
	{
		if (det->hit_count < det->hit_count_required)
		{
			det->hit_count++;
		}

		if (det->hit_count >= det->hit_count_required)
		{
			det->touched = 1U;
		}
	}
	else
	{
		det->hit_count = 0U;
	}

	return det->touched;
}

uint8_t ContactDetect_IsTouched(const ContactDetector *det)
{
	if (det == 0)
	{
		return 0U;
	}

	return det->touched;
}
