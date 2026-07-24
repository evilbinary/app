#ifndef __TRANQUILOS_APPS_COMMON_APP_TIME_H__
#define __TRANQUILOS_APPS_COMMON_APP_TIME_H__

#include "stdint.h"

#define APP_TIMEZONE_OFFSET_HOURS 8

typedef struct app_datetime {
	int year;
	int month;
	int day;
	int weekday;
	int hour;
	int minute;
	int second;
} app_datetime_s;

static inline void app_time_format_fixed_u32(char *out, uint32_t value, uint32_t width)
{
	if (out == NULL || width == 0U) {
		return;
	}

	out[width] = '\0';
	for (uint32_t i = 0; i < width; i++) {
		out[width - 1U - i] = (char)('0' + (value % 10U));
		value /= 10U;
	}
}

static inline int64_t app_time_days_from_civil(int year, int month, int day)
{
	int adjusted_year = year - (month <= 2 ? 1 : 0);
	int era = adjusted_year >= 0 ? adjusted_year / 400 : (adjusted_year - 399) / 400;
	uint32_t yoe = (uint32_t)(adjusted_year - era * 400);
	uint32_t m = (uint32_t)(month + (month > 2 ? -3 : 9));
	uint32_t doy = ((153U * m) + 2U) / 5U + (uint32_t)day - 1U;
	uint32_t doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;

	return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static inline void app_time_civil_from_days(int64_t days, int *year, int *month, int *day)
{
	int64_t z = days + 719468;
	int64_t era = (z >= 0 ? z : z - 146096) / 146097;
	uint32_t doe = (uint32_t)(z - era * 146097);
	uint32_t yoe = (doe - doe / 1460U + doe / 36524U - doe / 146096U) / 365U;
	int y = (int)yoe + (int)era * 400;
	uint32_t doy = doe - (365U * yoe + yoe / 4U - yoe / 100U);
	uint32_t mp = (5U * doy + 2U) / 153U;
	uint32_t d = doy - (153U * mp + 2U) / 5U + 1U;
	uint32_t m = mp + (mp < 10U ? 3U : (uint32_t)-9);

	y += m <= 2U;
	if (year != NULL) {
		*year = y;
	}
	if (month != NULL) {
		*month = (int)m;
	}
	if (day != NULL) {
		*day = (int)d;
	}
}

static inline uint8_t app_time_is_leap_year(int year)
{
	if ((year % 4) != 0) {
		return 0U;
	}
	if ((year % 100) != 0) {
		return 1U;
	}
	return (uint8_t)((year % 400) == 0);
}

static inline int app_time_days_in_year(int year)
{
	return app_time_is_leap_year(year) ? 366 : 365;
}

static inline int app_time_days_in_month(int year, int month)
{
	static const uint8_t month_days[12] = {
		31U, 28U, 31U, 30U, 31U, 30U,
		31U, 31U, 30U, 31U, 30U, 31U,
	};

	if (month < 1 || month > 12) {
		return 30;
	}
	if (month == 2 && app_time_is_leap_year(year)) {
		return 29;
	}
	return (int)month_days[month - 1];
}

static inline int app_time_weekday(int year, int month, int day)
{
	int64_t days = app_time_days_from_civil(year, month, day);
	int weekday = (int)((days + 4) % 7);

	if (weekday < 0) {
		weekday += 7;
	}
	return weekday;
}

static inline int app_time_day_of_year(int year, int month, int day)
{
	int total = 0;

	for (int m = 1; m < month; m++) {
		total += app_time_days_in_month(year, m);
	}
	return total + day;
}

static inline uint64_t app_time_normalize_timestamp_seconds(uint64_t raw_ts, uint64_t mono_ms)
{
	static uint64_t sample_raw_ts = 0;
	static uint64_t sample_mono_ms = 0;
	static uint8_t ts_in_msec = 0;
	static uint8_t unit_locked = 0;

	if (sample_mono_ms == 0U) {
		sample_raw_ts = raw_ts;
		sample_mono_ms = mono_ms;
	}

	if (mono_ms > sample_mono_ms && raw_ts >= sample_raw_ts) {
		uint64_t mono_delta_ms = mono_ms - sample_mono_ms;

		if (mono_delta_ms >= 200U) {
			uint64_t raw_delta = raw_ts - sample_raw_ts;

			if (raw_delta >= (mono_delta_ms / 2U)) {
				ts_in_msec = 1U;
				unit_locked = 1U;
			} else if (raw_delta <= ((mono_delta_ms / 200U) + 2U)) {
				ts_in_msec = 0U;
				unit_locked = 1U;
			}
			sample_raw_ts = raw_ts;
			sample_mono_ms = mono_ms;
		}
	}

	if (!unit_locked && raw_ts > 4102444800ULL) {
		ts_in_msec = 1U;
	}

	return ts_in_msec ? (raw_ts / 1000ULL) : raw_ts;
}

static inline void app_time_timestamp_to_local_datetime(uint64_t ts_sec, int timezone_offset_hours,
							app_datetime_s *out)
{
	int64_t local_sec = (int64_t)ts_sec + ((int64_t)timezone_offset_hours * 3600);
	int64_t days = local_sec / 86400;
	int64_t sec_of_day = local_sec % 86400;

	if (out == NULL) {
		return;
	}
	if (sec_of_day < 0) {
		sec_of_day += 86400;
		days--;
	}

	out->hour = (int)(sec_of_day / 3600);
	out->minute = (int)((sec_of_day / 60) % 60);
	out->second = (int)(sec_of_day % 60);
	out->weekday = (int)((days + 4) % 7);
	if (out->weekday < 0) {
		out->weekday += 7;
	}
	app_time_civil_from_days(days, &out->year, &out->month, &out->day);
}

#endif /* __TRANQUILOS_APPS_COMMON_APP_TIME_H__ */
