#include <assert.h>
#include <stdint.h>
#include "../Hardware/ContactDetect.h"

static void feed(ContactDetector *det, float value, int count)
{
	for (int i = 0; i < count; ++i)
	{
		(void)ContactDetect_Update(det, value, 1U);
	}
}

static void low_current_does_not_touch(void)
{
	ContactDetector det;
	ContactDetect_Init(&det, 0.30f, 3U);

	feed(&det, 0.12f, 80);

	assert(ContactDetect_IsTouched(&det) == 0U);
}

static void single_spike_does_not_touch(void)
{
	ContactDetector det;
	ContactDetect_Init(&det, 0.30f, 3U);

	feed(&det, 0.10f, 80);
	(void)ContactDetect_Update(&det, 0.80f, 1U);
	(void)ContactDetect_Update(&det, 0.10f, 1U);

	assert(ContactDetect_IsTouched(&det) == 0U);
}

static void sustained_current_rise_detects_touch(void)
{
	ContactDetector det;
	ContactDetect_Init(&det, 0.30f, 3U);

	feed(&det, 0.10f, 80);
	feed(&det, 0.55f, 3);

	assert(ContactDetect_IsTouched(&det) == 1U);
}

static void disabled_update_clears_pending_spikes(void)
{
	ContactDetector det;
	ContactDetect_Init(&det, 0.30f, 3U);

	feed(&det, 0.10f, 80);
	(void)ContactDetect_Update(&det, 0.55f, 1U);
	(void)ContactDetect_Update(&det, 0.55f, 0U);
	(void)ContactDetect_Update(&det, 0.55f, 1U);

	assert(ContactDetect_IsTouched(&det) == 0U);
}

int main(void)
{
	low_current_does_not_touch();
	single_spike_does_not_touch();
	sustained_current_rise_detects_touch();
	disabled_update_clears_pending_spikes();
	return 0;
}
