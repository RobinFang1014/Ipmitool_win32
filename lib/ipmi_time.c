/*
 * Copyright (c) 2018 Alexander Amelkin.  All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistribution of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * Redistribution in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of the copyright holder, nor the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * 
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * THE COPYRIGHT HOLDER AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * THE COPYRIGHT HOLDER OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE,
 * PROFIT OR DATA, OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL,
 * INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE
 * THEORY OF LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS
 * SOFTWARE, EVEN IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGES.
 */

#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h> /* snprintf */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <ipmitool/ipmi_time.h>

bool time_in_utc; /* Set by '-Z' command line option */

time_t
ipmi_localtime2utc(time_t local)
{
	struct tm tm;
	gmtime_r(&local, &tm);
	tm.tm_isdst = (-1);
	return mktime(&tm);
}

#ifdef _WIN32
/*
 * MSVC's strftime() expands %Z to the full Windows time zone name
 * (e.g. "Taipei Standard Time", or a localized one such as
 * "台北標準時間"), and it does so even for a struct tm that came from
 * gmtime(), so '-Z' output would be mislabeled.
 *
 * Replace %Z with a short label like Linux prints:
 *   - UTC output ('-Z' or BMC-relative stamps) -> "UTC"
 *   - local time -> initials of the English Windows time zone key name,
 *     e.g. "Taipei Standard Time" -> "TST",
 *          "Pacific Standard Time" during DST -> "PDT"
 *   - fallback if no name is available -> "UTC+8", "UTC+5:30"
 */
static bool
ipmi_win32_tz_abbrev(char *buf, size_t len, bool is_dst)
{
	DYNAMIC_TIME_ZONE_INFORMATION dtzi;
	const WCHAR *name;
	const WCHAR *word;
	size_t o = 0;
	size_t wlen;

	if (len < 2 ||
	    GetDynamicTimeZoneInformation(&dtzi) == TIME_ZONE_ID_INVALID) {
		return false;
	}
	name = dtzi.TimeZoneKeyName;

	/* Single-word key names ("UTC", "UTC+12") are already short */
	if (!wcschr(name, L' ')) {
		for (; *name && o + 1 < len; name++) {
			if (*name > 0x7F) {
				return false;
			}
			buf[o++] = (char)*name;
		}
		buf[o] = '\0';
		return o > 0;
	}

	for (word = name; *word && o + 1 < len; ) {
		while (*word == L' ') {
			word++;
		}
		if (!*word) {
			break;
		}
		wlen = wcscspn(word, L" ");
		if (is_dst && wlen == 8 && wcsncmp(word, L"Standard", 8) == 0) {
			buf[o++] = 'D'; /* Standard -> Daylight */
		} else if (*word < 0x80 && isalpha((int)*word)) {
			buf[o++] = (char)toupper((int)*word);
		}
		word += wlen;
	}
	buf[o] = '\0';
	return o > 0;
}

static void
ipmi_win32_tz_label(char *buf, size_t len, const struct tm *tm,
                    time_t stamp, bool is_utc)
{
	struct tm tmp;
	long offset;
	char sign = '+';

	if (is_utc) {
		snprintf(buf, len, "UTC");
		return;
	}

	if (ipmi_win32_tz_abbrev(buf, len, tm->tm_isdst > 0)) {
		return;
	}

	/* Fallback: numeric offset from UTC */
	tmp = *tm; /* _mkgmtime() normalizes its argument */
	offset = (long)(_mkgmtime(&tmp) - stamp);

	if (offset == 0) {
		snprintf(buf, len, "UTC");
		return;
	}
	if (offset < 0) {
		sign = '-';
		offset = -offset;
	}
	if (offset % 3600) {
		snprintf(buf, len, "UTC%c%ld:%02ld", sign,
		         offset / 3600, (offset % 3600) / 60);
	} else {
		snprintf(buf, len, "UTC%c%ld", sign, offset / 3600);
	}
}

static size_t
ipmi_win32_strftime(char *s, size_t max, const char *format,
                    const struct tm *tm, time_t stamp, bool is_utc)
{
	char fmt[IPMI_ASCTIME_SZ * 2];
	char label[16];
	size_t o = 0;
	size_t i;

	ipmi_win32_tz_label(label, sizeof(label), tm, stamp, is_utc);

	while (*format && o + 2 < sizeof(fmt)) {
		if (format[0] == '%' && format[1] == '%') {
			fmt[o++] = *format++;
			fmt[o++] = *format++;
		} else if (format[0] == '%' && format[1] == 'Z') {
			for (i = 0; label[i] && o + 1 < sizeof(fmt); i++) {
				fmt[o++] = label[i];
			}
			format += 2;
		} else {
			fmt[o++] = *format++;
		}
	}
	fmt[o] = '\0';

	return strftime(s, max, fmt, tm);
}
# define ipmi_do_strftime(buf, buflen, fmt, t, stamp, is_utc) \
	ipmi_win32_strftime(buf, buflen, fmt, t, stamp, is_utc)
#else
# define ipmi_do_strftime(buf, buflen, fmt, t, stamp, is_utc) \
	wrapstrftime(buf, buflen, fmt, t)
#endif

/**
 * @brief Convert a timestamp to a formatted string,
 *        considering the '-Z' option. Acts as if tzset() was called.
 *
 * @param[out] s          The output string buffer
 * @param[in]  max        The size of the output string buffer including the
 *                        terminating null byte
 * @param[in]  format     The format string, as in strftime(), ignored for
 *                        special timestamp values as per section 37.1 of
 *                        IPMI v2.0 specification rev 1.1.
 * @param[in]  stamp      The time stamp to convert
 *
 * @returns the number of bytes written to s or 0, see strftime()
 */
size_t
ipmi_strftime(char *s, size_t max, const char *format, time_t stamp)
{
	struct tm tm;
	/*
	 * There is a bug in gcc since 4.3.2 and still not fixed in 8.1.0.
	 * Even if __attribute__((format(strftime... is specified for a wrapper
	 * function around strftime, gcc still complains about strftime being
	 * called from the wrapper with a "non-literal" format argument.
	 *
	 * See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=39438
	 *
	 * The following macro uses an "ugly cast" from that discussion to
	 * silence the compiler. The format string is checked for the wrapper
	 * because __attribute__((format)) is specified in the header file.
	 */
	#define wrapstrftime(buf, buflen, fmt, t) \
		((size_t (*)(char *, size_t, const char *, const struct tm *))\
		 strftime)(buf, buflen, fmt, t)


	if (IPMI_TIME_UNSPECIFIED == stamp) {
		return snprintf(s, max, "Unknown");
	}
	else if (stamp <= IPMI_TIME_INIT_DONE) {
		/* Timestamp is relative to BMC start, no GMT offset */
		gmtime_r(&stamp, &tm);

		return ipmi_do_strftime(s, max, format, &tm, stamp, true);
	}

	if (time_in_utc || ipmi_timestamp_is_special(stamp)) {
		/*
		 * The user wants the time reported in UTC or the stamp represents the
		 * number of seconds since system power on. In any case, don't apply
		 * the timezone offset.
		 */
		gmtime_r(&stamp, &tm);
#ifndef _WIN32
		daylight = -1;
#endif
		return ipmi_do_strftime(s, max, format, &tm, stamp, true);
	}

	/*
	 * The user wants the time reported in local time zone.
	 */
	localtime_r(&stamp, &tm);
	return ipmi_do_strftime(s, max, format, &tm, stamp, false);
}

/**
 * @brief Convert a timestamp to string, considering the '-Z' option.
 *        Similar to asctime_r(), but takes time_t instead of struct tm,
 *        and the string is in form "Wed Jun 30 21:49:08 1993 TZD" without
 *        the new line at the end.
 *
 * @param[in]  stamp      The timestamp to convert
 * @param[out] outbuf     The buffer to write the string to.
 * @param[in]  len        The maximum length of the output buffer.
 *                        Recommended size is IPMI_ASCTIME_SZ.
 *
 * @returns outbuf
 */
char *
ipmi_asctime_r(const time_t stamp, ipmi_datebuf_t outbuf)
{
	if (ipmi_timestamp_is_special(stamp)) {
		if (stamp < SECONDS_A_DAY) {
			ipmi_strftime(outbuf, IPMI_ASCTIME_SZ, "S+%H:%M:%S", stamp);
		}
		/*
		 * IPMI_TIME_INIT_DONE is over 17 years. This should never
		 * happen normally, but we'll support this anyway.
		 */
		else {
			ipmi_strftime(outbuf, IPMI_ASCTIME_SZ, "S+%yy %jd %H:%M:%S", stamp);
		}
	}

	ipmi_strftime(outbuf, IPMI_ASCTIME_SZ, "%c %Z", stamp);
	return outbuf;
}

char *
ipmi_timestamp_fmt(uint32_t stamp, const char *fmt)
{
	/*
	 * It's assumed that supplied 'fmt' is never longer
	 * than IPMI_ASCTIME_SZ
	 */
	static ipmi_datebuf_t datebuf;
	/*
	 * There is a bug in gcc since 4.3.2 and still not fixed in 8.1.0.
	 * Even if __attribute__((format(strftime... is specified for a wrapper
	 * function around strftime, gcc still complains about strftime being
	 * called from the wrapper with a "non-literal" format argument.
	 *
	 * See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=39438
	 *
	 * The following call uses an "ugly cast" from that discussion to
	 * silence the compiler. The format string is checked for the wrapper
	 * because __attribute__((format)) is specified in the header file.
	 */
	((size_t (*)(char *, size_t, const char *, time_t))
	 ipmi_strftime)(datebuf, sizeof(datebuf), fmt, stamp);

	return datebuf;
}

char *
ipmi_timestamp_string(uint32_t stamp)
{
	if (!ipmi_timestamp_is_valid(stamp)) {
		return "Unspecified";
	}

	if (ipmi_timestamp_is_special(stamp)) {
		if (stamp < SECONDS_A_DAY) {
			return ipmi_timestamp_fmt(stamp, "S+ %H:%M:%S");
		}
		/*
		 * IPMI_TIME_INIT_DONE is over 17 years. This should never
		 * happen normally, but we'll support this anyway.
		 */
		else {
			return ipmi_timestamp_fmt(stamp, "S+ %y years %j days %H:%M:%S");
		}
	}
	return ipmi_timestamp_fmt(stamp, "%c %Z");
}

char *
ipmi_timestamp_numeric(uint32_t stamp)
{
	if (!ipmi_timestamp_is_valid(stamp)) {
		return "Unspecified";
	}

	if (ipmi_timestamp_is_special(stamp)) {
		if (stamp < SECONDS_A_DAY) {
			return ipmi_timestamp_fmt(stamp, "S+ %H:%M:%S");
		}
		/*
		 * IPMI_TIME_INIT_DONE is over 17 years. This should never
		 * happen normally, but we'll support this anyway.
		 */
		else {
			return ipmi_timestamp_fmt(stamp, "S+ %y/%j %H:%M:%S");
		}
	}
	return ipmi_timestamp_fmt(stamp, "%x %X %Z");
}

char *
ipmi_timestamp_date(uint32_t stamp)
{
	if (!ipmi_timestamp_is_valid(stamp)) {
		return "Unspecified";
	}

	if (ipmi_timestamp_is_special(stamp)) {
		return ipmi_timestamp_fmt(stamp, "S+ %y/%j");
	}
	return ipmi_timestamp_fmt(stamp, "%x");
}

char *
ipmi_timestamp_time(uint32_t stamp)
{
	if (!ipmi_timestamp_is_valid(stamp)) {
		return "Unspecified";
	}

	/* Format is the same for both normal and special timestamps */
	return ipmi_timestamp_fmt(stamp, "%X %Z");
}
