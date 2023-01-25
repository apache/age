/*
 * conversion functions between pg_wchar and multibyte streams.
 * Tatsuo Ishii
 * src/backend/utils/mb/wchar.c
 *
 */
/* can be used in either frontend or backend */
#include "pool_parser.h"
#include "utils/elog.h"
#include <stdio.h>
#include <string.h>
#include "pg_wchar.h"

#ifndef WIN32
#define DEF_ENC2NAME(name, codepage) { #name, PG_##name }
#else
#define DEF_ENC2NAME(name, codepage) { #name, PG_##name, codepage }
#endif

const pg_enc2name pg_enc2name_tbl[] =
{
	DEF_ENC2NAME(SQL_ASCII, 0),
	DEF_ENC2NAME(EUC_JP, 20932),
	DEF_ENC2NAME(EUC_CN, 20936),
	DEF_ENC2NAME(EUC_KR, 51949),
	DEF_ENC2NAME(EUC_TW, 0),
	DEF_ENC2NAME(EUC_JIS_2004, 20932),
	DEF_ENC2NAME(UTF8, 65001),
	DEF_ENC2NAME(MULE_INTERNAL, 0),
	DEF_ENC2NAME(LATIN1, 28591),
	DEF_ENC2NAME(LATIN2, 28592),
	DEF_ENC2NAME(LATIN3, 28593),
	DEF_ENC2NAME(LATIN4, 28594),
	DEF_ENC2NAME(LATIN5, 28599),
	DEF_ENC2NAME(LATIN6, 0),
	DEF_ENC2NAME(LATIN7, 0),
	DEF_ENC2NAME(LATIN8, 0),
	DEF_ENC2NAME(LATIN9, 28605),
	DEF_ENC2NAME(LATIN10, 0),
	DEF_ENC2NAME(WIN1256, 1256),
	DEF_ENC2NAME(WIN1258, 1258),
	DEF_ENC2NAME(WIN866, 866),
	DEF_ENC2NAME(WIN874, 874),
	DEF_ENC2NAME(KOI8R, 20866),
	DEF_ENC2NAME(WIN1251, 1251),
	DEF_ENC2NAME(WIN1252, 1252),
	DEF_ENC2NAME(ISO_8859_5, 28595),
	DEF_ENC2NAME(ISO_8859_6, 28596),
	DEF_ENC2NAME(ISO_8859_7, 28597),
	DEF_ENC2NAME(ISO_8859_8, 28598),
	DEF_ENC2NAME(WIN1250, 1250),
	DEF_ENC2NAME(WIN1253, 1253),
	DEF_ENC2NAME(WIN1254, 1254),
	DEF_ENC2NAME(WIN1255, 1255),
	DEF_ENC2NAME(WIN1257, 1257),
	DEF_ENC2NAME(KOI8U, 21866),
	DEF_ENC2NAME(SJIS, 932),
	DEF_ENC2NAME(BIG5, 950),
	DEF_ENC2NAME(GBK, 936),
	DEF_ENC2NAME(UHC, 0),
	DEF_ENC2NAME(GB18030, 54936),
	DEF_ENC2NAME(JOHAB, 0),
	DEF_ENC2NAME(SHIFT_JIS_2004, 932)
};

/* ----------
 * These are encoding names for gettext.
 *
 * This covers all encodings except MULE_INTERNAL, which is alien to gettext.
 * ----------
 */
const pg_enc2gettext pg_enc2gettext_tbl[] =
{
	{PG_SQL_ASCII, "US-ASCII"},
	{PG_UTF8, "UTF-8"},
	{PG_LATIN1, "LATIN1"},
	{PG_LATIN2, "LATIN2"},
	{PG_LATIN3, "LATIN3"},
	{PG_LATIN4, "LATIN4"},
	{PG_ISO_8859_5, "ISO-8859-5"},
	{PG_ISO_8859_6, "ISO_8859-6"},
	{PG_ISO_8859_7, "ISO-8859-7"},
	{PG_ISO_8859_8, "ISO-8859-8"},
	{PG_LATIN5, "LATIN5"},
	{PG_LATIN6, "LATIN6"},
	{PG_LATIN7, "LATIN7"},
	{PG_LATIN8, "LATIN8"},
	{PG_LATIN9, "LATIN-9"},
	{PG_LATIN10, "LATIN10"},
	{PG_KOI8R, "KOI8-R"},
	{PG_KOI8U, "KOI8-U"},
	{PG_WIN1250, "CP1250"},
	{PG_WIN1251, "CP1251"},
	{PG_WIN1252, "CP1252"},
	{PG_WIN1253, "CP1253"},
	{PG_WIN1254, "CP1254"},
	{PG_WIN1255, "CP1255"},
	{PG_WIN1256, "CP1256"},
	{PG_WIN1257, "CP1257"},
	{PG_WIN1258, "CP1258"},
	{PG_WIN866, "CP866"},
	{PG_WIN874, "CP874"},
	{PG_EUC_CN, "EUC-CN"},
	{PG_EUC_JP, "EUC-JP"},
	{PG_EUC_KR, "EUC-KR"},
	{PG_EUC_TW, "EUC-TW"},
	{PG_EUC_JIS_2004, "EUC-JP"},
	{PG_SJIS, "SHIFT-JIS"},
	{PG_BIG5, "BIG5"},
	{PG_GBK, "GBK"},
	{PG_UHC, "UHC"},
	{PG_GB18030, "GB18030"},
	{PG_JOHAB, "JOHAB"},
	{PG_SHIFT_JIS_2004, "SHIFT_JISX0213"},
	{0, NULL}
};


/*
 * Operations on multi-byte encodings are driven by a table of helper
 * functions.
 *
 * To add an encoding support, define mblen(), dsplen(), verifychar() and
 * verifystr() for the encoding.  For server-encodings, also define mb2wchar()
 * and wchar2mb() conversion functions.
 *
 * These functions generally assume that their input is validly formed.
 * The "verifier" functions, further down in the file, have to be more
 * paranoid.
 *
 * We expect that mblen() does not need to examine more than the first byte
 * of the character to discover the correct length.  GB18030 is an exception
 * to that rule, though, as it also looks at second byte.  But even that
 * behaves in a predictable way, if you only pass the first byte: it will
 * treat 4-byte encoded characters as two 2-byte encoded characters, which is
 * good enough for all current uses.
 *
 * Note: for the display output of psql to work properly, the return values
 * of the dsplen functions must conform to the Unicode standard. In particular
 * the NUL character is zero width and control characters are generally
 * width -1. It is recommended that non-ASCII encodings refer their ASCII
 * subset to the ASCII routines to ensure consistency.
 */

/*
 * SQL/ASCII
 */
static int
pg_ascii2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
	int			cnt = 0;

	while (len > 0 && *from)
	{
		*to++ = *from++;
		len--;
		cnt++;
	}
	*to = 0;
	return cnt;
}

static int
pg_ascii_mblen(const unsigned char *s)
{
	return 1;
}

static int
pg_ascii_dsplen(const unsigned char *s)
{
	if (*s == '\0')
		return 0;
	if (*s < 0x20 || *s == 0x7f)
		return -1;

	return 1;
}

/*
 * EUC
 */
static int
pg_euc2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
	int			cnt = 0;

	while (len > 0 && *from)
	{
		if (*from == SS2 && len >= 2)	/* JIS X 0201 (so called "1 byte
										 * KANA") */
		{
			from++;
			*to = (SS2 << 8) | *from++;
			len -= 2;
		}
		else if (*from == SS3 && len >= 3)	/* JIS X 0212 KANJI */
		{
			from++;
			*to = (SS3 << 16) | (*from++ << 8);
			*to |= *from++;
			len -= 3;
		}
		else if (IS_HIGHBIT_SET(*from) && len >= 2) /* JIS X 0208 KANJI */
		{
			*to = *from++ << 8;
			*to |= *from++;
			len -= 2;
		}
		else					/* must be ASCII */
		{
			*to = *from++;
			len--;
		}
		to++;
		cnt++;
	}
	*to = 0;
	return cnt;
}

static inline int
pg_euc_mblen(const unsigned char *s)
{
	int			len;

	if (*s == SS2)
		len = 2;
	else if (*s == SS3)
		len = 3;
	else if (IS_HIGHBIT_SET(*s))
		len = 2;
	else
		len = 1;
	return len;
}

static inline int
pg_euc_dsplen(const unsigned char *s)
{
	int			len;

	if (*s == SS2)
		len = 2;
	else if (*s == SS3)
		len = 2;
	else if (IS_HIGHBIT_SET(*s))
		len = 2;
	else
		len = pg_ascii_dsplen(s);
	return len;
}

/*
 * EUC_JP
 */
static int
pg_eucjp2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
	return pg_euc2wchar_with_len(from, to, len);
}

static int
pg_eucjp_mblen(const unsigned char *s)
{
	return pg_euc_mblen(s);
}

static int
pg_eucjp_dsplen(const unsigned char *s)
{
	int			len;

	if (*s == SS2)
		len = 1;
	else if (*s == SS3)
		len = 2;
	else if (IS_HIGHBIT_SET(*s))
		len = 2;
	else
		len = pg_ascii_dsplen(s);
	return len;
}

/*
 * EUC_KR
 */
static int
pg_euckr2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
	return pg_euc2wchar_with_len(from, to, len);
}

static int
pg_euckr_mblen(const unsigned char *s)
{
	return pg_euc_mblen(s);
}

static int
pg_euckr_dsplen(const unsigned char *s)
{
	return pg_euc_dsplen(s);
}

/*
 * EUC_CN
 *
 */
static int
pg_euccn2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
	int			cnt = 0;

	while (len > 0 && *from)
	{
		if (*from == SS2 && len >= 3)	/* code set 2 (unused?) */
		{
			from++;
			*to = (SS2 << 16) | (*from++ << 8);
			*to |= *from++;
			len -= 3;
		}
		else if (*from == SS3 && len >= 3)	/* code set 3 (unused ?) */
		{
			from++;
			*to = (SS3 << 16) | (*from++ << 8);
			*to |= *from++;
			len -= 3;
		}
		else if (IS_HIGHBIT_SET(*from) && len >= 2) /* code set 1 */
		{
			*to = *from++ << 8;
			*to |= *from++;
			len -= 2;
		}
		else
		{
			*to = *from++;
			len--;
		}
		to++;
		cnt++;
	}
	*to = 0;
	return cnt;
}

static int
pg_euccn_mblen(const unsigned char *s)
{
	int			len;

	if (IS_HIGHBIT_SET(*s))
		len = 2;
	else
		len = 1;
	return len;
}

static int
pg_euccn_dsplen(const unsigned char *s)
{
	int			len;

	if (IS_HIGHBIT_SET(*s))
		len = 2;
	else
		len = pg_ascii_dsplen(s);
	return len;
}

/*
 * EUC_TW
 *
 */
static int
pg_euctw2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
	int			cnt = 0;

	while (len > 0 && *from)
	{
		if (*from == SS2 && len >= 4)	/* code set 2 */
		{
			from++;
			*to = (((uint32) SS2) << 24) | (*from++ << 16);
			*to |= *from++ << 8;
			*to |= *from++;
			len -= 4;
		}
		else if (*from == SS3 && len >= 3)	/* code set 3 (unused?) */
		{
			from++;
			*to = (SS3 << 16) | (*from++ << 8);
			*to |= *from++;
			len -= 3;
		}
		else if (IS_HIGHBIT_SET(*from) && len >= 2) /* code set 2 */
		{
			*to = *from++ << 8;
			*to |= *from++;
			len -= 2;
		}
		else
		{
			*to = *from++;
			len--;
		}
		to++;
		cnt++;
	}
	*to = 0;
	return cnt;
}

static int
pg_euctw_mblen(const unsigned char *s)
{
	int			len;

	if (*s == SS2)
		len = 4;
	else if (*s == SS3)
		len = 3;
	else if (IS_HIGHBIT_SET(*s))
		len = 2;
	else
		len = 1;
	return len;
}

static int
pg_euctw_dsplen(const unsigned char *s)
{
	int			len;

	if (*s == SS2)
		len = 2;
	else if (*s == SS3)
		len = 2;
	else if (IS_HIGHBIT_SET(*s))
		len = 2;
	else
		len = pg_ascii_dsplen(s);
	return len;
}

/*
 * Convert pg_wchar to EUC_* encoding.
 * caller must allocate enough space for "to", including a trailing zero!
 * len: length of from.
 * "from" not necessarily null terminated.
 */
static int
pg_wchar2euc_with_len(const pg_wchar *from, unsigned char *to, int len)
{
	int			cnt = 0;

	while (len > 0 && *from)
	{
		unsigned char c;

		if ((c = (*from >> 24)))
		{
			*to++ = c;
			*to++ = (*from >> 16) & 0xff;
			*to++ = (*from >> 8) & 0xff;
			*to++ = *from & 0xff;
			cnt += 4;
		}
		else if ((c = (*from >> 16)))
		{
			*to++ = c;
			*to++ = (*from >> 8) & 0xff;
			*to++ = *from & 0xff;
			cnt += 3;
		}
		else if ((c = (*from >> 8)))
		{
			*to++ = c;
			*to++ = *from & 0xff;
			cnt += 2;
		}
		else
		{
			*to++ = *from;
			cnt++;
		}
		from++;
		len--;
	}
	*to = 0;
	return cnt;
}


/*
 * JOHAB
 */
static int
pg_johab_mblen(const unsigned char *s)
{
	return pg_euc_mblen(s);
}

static int
pg_johab_dsplen(const unsigned char *s)
{
	return pg_euc_dsplen(s);
}

/*
 * convert UTF8 string to pg_wchar (UCS-4)
 * caller must allocate enough space for "to", including a trailing zero!
 * len: length of from.
 * "from" not necessarily null terminated.
 */
static int
pg_utf2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
	int			cnt = 0;
	uint32		c1,
				c2,
				c3,
				c4;

	while (len > 0 && *from)
	{
		if ((*from & 0x80) == 0)
		{
			*to = *from++;
			len--;
		}
		else if ((*from & 0xe0) == 0xc0)
		{
			if (len < 2)
				break;			/* drop trailing incomplete char */
			c1 = *from++ & 0x1f;
			c2 = *from++ & 0x3f;
			*to = (c1 << 6) | c2;
			len -= 2;
		}
		else if ((*from & 0xf0) == 0xe0)
		{
			if (len < 3)
				break;			/* drop trailing incomplete char */
			c1 = *from++ & 0x0f;
			c2 = *from++ & 0x3f;
			c3 = *from++ & 0x3f;
			*to = (c1 << 12) | (c2 << 6) | c3;
			len -= 3;
		}
		else if ((*from & 0xf8) == 0xf0)
		{
			if (len < 4)
				break;			/* drop trailing incomplete char */
			c1 = *from++ & 0x07;
			c2 = *from++ & 0x3f;
			c3 = *from++ & 0x3f;
			c4 = *from++ & 0x3f;
			*to = (c1 << 18) | (c2 << 12) | (c3 << 6) | c4;
			len -= 4;
		}
		else
		{
			/* treat a bogus char as length 1; not ours to raise error */
			*to = *from++;
			len--;
		}
		to++;
		cnt++;
	}
	*to = 0;
	return cnt;
}


/*
 * Map a Unicode code point to UTF-8.  utf8string must have 4 bytes of
 * space allocated.
 */
unsigned char *
unicode_to_utf8(pg_wchar c, unsigned char *utf8string)
{
	if (c <= 0x7F)
	{
		utf8string[0] = c;
	}
	else if (c <= 0x7FF)
	{
		utf8string[0] = 0xC0 | ((c >> 6) & 0x1F);
		utf8string[1] = 0x80 | (c & 0x3F);
	}
	else if (c <= 0xFFFF)
	{
		utf8string[0] = 0xE0 | ((c >> 12) & 0x0F);
		utf8string[1] = 0x80 | ((c >> 6) & 0x3F);
		utf8string[2] = 0x80 | (c & 0x3F);
	}
	else
	{
		utf8string[0] = 0xF0 | ((c >> 18) & 0x07);
		utf8string[1] = 0x80 | ((c >> 12) & 0x3F);
		utf8string[2] = 0x80 | ((c >> 6) & 0x3F);
		utf8string[3] = 0x80 | (c & 0x3F);
	}

	return utf8string;
}

/*
 * Trivial conversion from pg_wchar to UTF-8.
 * caller should allocate enough space for "to"
 * len: length of from.
 * "from" not necessarily null terminated.
 */
static int
pg_wchar2utf_with_len(const pg_wchar *from, unsigned char *to, int len)
{
	int			cnt = 0;

	while (len > 0 && *from)
	{
		int			char_len;

		unicode_to_utf8(*from, to);
		char_len = pg_utf_mblen(to);
		cnt += char_len;
		to += char_len;
		from++;
		len--;
	}
	*to = 0;
	return cnt;
}

/*
 * Return the byte length of a UTF8 character pointed to by s
 *
 * Note: in the current implementation we do not support UTF8 sequences
 * of more than 4 bytes; hence do NOT return a value larger than 4.
 * We return "1" for any leading byte that is either flat-out illegal or
 * indicates a length larger than we support.
 *
 * pg_utf2wchar_with_len(), utf8_to_unicode(), pg_utf8_islegal(), and perhaps
 * other places would need to be fixed to change this.
 */
int
pg_utf_mblen(const unsigned char *s)
{
	int			len;

	if ((*s & 0x80) == 0)
		len = 1;
	else if ((*s & 0xe0) == 0xc0)
		len = 2;
	else if ((*s & 0xf0) == 0xe0)
		len = 3;
	else if ((*s & 0xf8) == 0xf0)
		len = 4;
#ifdef NOT_USED
	else if ((*s & 0xfc) == 0xf8)
		len = 5;
	else if ((*s & 0xfe) == 0xfc)
		len = 6;
#endif
	else
		len = 1;
	return len;
}

/*
 * This is an implementation of wcwidth() and wcswidth() as defined in
 * "The Single UNIX Specification, Version 2, The Open Group, 1997"
 * <http://www.UNIX-systems.org/online.html>
 *
 * Markus Kuhn -- 2001-09-08 -- public domain
 *
 * customised for PostgreSQL
 *
 * original available at : http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
 */

struct mbinterval
{
	unsigned int first;
	unsigned int last;
};

/* auxiliary function for binary search in interval table */
static int
mbbisearch(pg_wchar ucs, const struct mbinterval *table, int max)
{
	int			min = 0;
	int			mid;

	if (ucs < table[0].first || ucs > table[max].last)
		return 0;
	while (max >= min)
	{
		mid = (min + max) / 2;
		if (ucs > table[mid].last)
			min = mid + 1;
		else if (ucs < table[mid].first)
			max = mid - 1;
		else
			return 1;
	}

	return 0;
}


/* The following functions define the column width of an ISO 10646
 * character as follows:
 *
 *	  - The null character (U+0000) has a column width of 0.
 *
 *	  - Other C0/C1 control characters and DEL will lead to a return
 *		value of -1.
 *
 *	  - Non-spacing and enclosing combining characters (general
 *		category code Mn or Me in the Unicode database) have a
 *		column width of 0.
 *
 *	  - Spacing characters in the East Asian Wide (W) or East Asian
 *		FullWidth (F) category as defined in Unicode Technical
 *		Report #11 have a column width of 2.
 *
 *	  - All remaining characters (including all printable
 *		ISO 8859-1 and WGL4 characters, Unicode control characters,
 *		etc.) have a column width of 1.
 *
 * This implementation assumes that wchar_t characters are encoded
 * in ISO 10646.
 */

static int
ucs_wcwidth(pg_wchar ucs)
{
	/* sorted list of non-overlapping intervals of non-spacing characters */
	static const struct mbinterval combining[] = {
		{0x0300, 0x036F},
		{0x0483, 0x0489},
		{0x0591, 0x05BD},
		{0x05BF, 0x05BF},
		{0x05C1, 0x05C2},
		{0x05C4, 0x05C5},
		{0x05C7, 0x05C7},
		{0x0610, 0x061A},
		{0x064B, 0x065F},
		{0x0670, 0x0670},
		{0x06D6, 0x06DC},
		{0x06DF, 0x06E4},
		{0x06E7, 0x06E8},
		{0x06EA, 0x06ED},
		{0x0711, 0x0711},
		{0x0730, 0x074A},
		{0x07A6, 0x07B0},
		{0x07EB, 0x07F3},
		{0x07FD, 0x07FD},
		{0x0816, 0x0819},
		{0x081B, 0x0823},
		{0x0825, 0x0827},
		{0x0829, 0x082D},
		{0x0859, 0x085B},
		{0x0898, 0x089F},
		{0x08CA, 0x08E1},
		{0x08E3, 0x0902},
		{0x093A, 0x093A},
		{0x093C, 0x093C},
		{0x0941, 0x0948},
		{0x094D, 0x094D},
		{0x0951, 0x0957},
		{0x0962, 0x0963},
		{0x0981, 0x0981},
		{0x09BC, 0x09BC},
		{0x09C1, 0x09C4},
		{0x09CD, 0x09CD},
		{0x09E2, 0x09E3},
		{0x09FE, 0x0A02},
		{0x0A3C, 0x0A3C},
		{0x0A41, 0x0A51},
		{0x0A70, 0x0A71},
		{0x0A75, 0x0A75},
		{0x0A81, 0x0A82},
		{0x0ABC, 0x0ABC},
		{0x0AC1, 0x0AC8},
		{0x0ACD, 0x0ACD},
		{0x0AE2, 0x0AE3},
		{0x0AFA, 0x0B01},
		{0x0B3C, 0x0B3C},
		{0x0B3F, 0x0B3F},
		{0x0B41, 0x0B44},
		{0x0B4D, 0x0B56},
		{0x0B62, 0x0B63},
		{0x0B82, 0x0B82},
		{0x0BC0, 0x0BC0},
		{0x0BCD, 0x0BCD},
		{0x0C00, 0x0C00},
		{0x0C04, 0x0C04},
		{0x0C3C, 0x0C3C},
		{0x0C3E, 0x0C40},
		{0x0C46, 0x0C56},
		{0x0C62, 0x0C63},
		{0x0C81, 0x0C81},
		{0x0CBC, 0x0CBC},
		{0x0CBF, 0x0CBF},
		{0x0CC6, 0x0CC6},
		{0x0CCC, 0x0CCD},
		{0x0CE2, 0x0CE3},
		{0x0D00, 0x0D01},
		{0x0D3B, 0x0D3C},
		{0x0D41, 0x0D44},
		{0x0D4D, 0x0D4D},
		{0x0D62, 0x0D63},
		{0x0D81, 0x0D81},
		{0x0DCA, 0x0DCA},
		{0x0DD2, 0x0DD6},
		{0x0E31, 0x0E31},
		{0x0E34, 0x0E3A},
		{0x0E47, 0x0E4E},
		{0x0EB1, 0x0EB1},
		{0x0EB4, 0x0EBC},
		{0x0EC8, 0x0ECD},
		{0x0F18, 0x0F19},
		{0x0F35, 0x0F35},
		{0x0F37, 0x0F37},
		{0x0F39, 0x0F39},
		{0x0F71, 0x0F7E},
		{0x0F80, 0x0F84},
		{0x0F86, 0x0F87},
		{0x0F8D, 0x0FBC},
		{0x0FC6, 0x0FC6},
		{0x102D, 0x1030},
		{0x1032, 0x1037},
		{0x1039, 0x103A},
		{0x103D, 0x103E},
		{0x1058, 0x1059},
		{0x105E, 0x1060},
		{0x1071, 0x1074},
		{0x1082, 0x1082},
		{0x1085, 0x1086},
		{0x108D, 0x108D},
		{0x109D, 0x109D},
		{0x135D, 0x135F},
		{0x1712, 0x1714},
		{0x1732, 0x1733},
		{0x1752, 0x1753},
		{0x1772, 0x1773},
		{0x17B4, 0x17B5},
		{0x17B7, 0x17BD},
		{0x17C6, 0x17C6},
		{0x17C9, 0x17D3},
		{0x17DD, 0x17DD},
		{0x180B, 0x180D},
		{0x180F, 0x180F},
		{0x1885, 0x1886},
		{0x18A9, 0x18A9},
		{0x1920, 0x1922},
		{0x1927, 0x1928},
		{0x1932, 0x1932},
		{0x1939, 0x193B},
		{0x1A17, 0x1A18},
		{0x1A1B, 0x1A1B},
		{0x1A56, 0x1A56},
		{0x1A58, 0x1A60},
		{0x1A62, 0x1A62},
		{0x1A65, 0x1A6C},
		{0x1A73, 0x1A7F},
		{0x1AB0, 0x1B03},
		{0x1B34, 0x1B34},
		{0x1B36, 0x1B3A},
		{0x1B3C, 0x1B3C},
		{0x1B42, 0x1B42},
		{0x1B6B, 0x1B73},
		{0x1B80, 0x1B81},
		{0x1BA2, 0x1BA5},
		{0x1BA8, 0x1BA9},
		{0x1BAB, 0x1BAD},
		{0x1BE6, 0x1BE6},
		{0x1BE8, 0x1BE9},
		{0x1BED, 0x1BED},
		{0x1BEF, 0x1BF1},
		{0x1C2C, 0x1C33},
		{0x1C36, 0x1C37},
		{0x1CD0, 0x1CD2},
		{0x1CD4, 0x1CE0},
		{0x1CE2, 0x1CE8},
		{0x1CED, 0x1CED},
		{0x1CF4, 0x1CF4},
		{0x1CF8, 0x1CF9},
		{0x1DC0, 0x1DFF},
		{0x20D0, 0x20F0},
		{0x2CEF, 0x2CF1},
		{0x2D7F, 0x2D7F},
		{0x2DE0, 0x2DFF},
		{0x302A, 0x302D},
		{0x3099, 0x309A},
		{0xA66F, 0xA672},
		{0xA674, 0xA67D},
		{0xA69E, 0xA69F},
		{0xA6F0, 0xA6F1},
		{0xA802, 0xA802},
		{0xA806, 0xA806},
		{0xA80B, 0xA80B},
		{0xA825, 0xA826},
		{0xA82C, 0xA82C},
		{0xA8C4, 0xA8C5},
		{0xA8E0, 0xA8F1},
		{0xA8FF, 0xA8FF},
		{0xA926, 0xA92D},
		{0xA947, 0xA951},
		{0xA980, 0xA982},
		{0xA9B3, 0xA9B3},
		{0xA9B6, 0xA9B9},
		{0xA9BC, 0xA9BD},
		{0xA9E5, 0xA9E5},
		{0xAA29, 0xAA2E},
		{0xAA31, 0xAA32},
		{0xAA35, 0xAA36},
		{0xAA43, 0xAA43},
		{0xAA4C, 0xAA4C},
		{0xAA7C, 0xAA7C},
		{0xAAB0, 0xAAB0},
		{0xAAB2, 0xAAB4},
		{0xAAB7, 0xAAB8},
		{0xAABE, 0xAABF},
		{0xAAC1, 0xAAC1},
		{0xAAEC, 0xAAED},
		{0xAAF6, 0xAAF6},
		{0xABE5, 0xABE5},
		{0xABE8, 0xABE8},
		{0xABED, 0xABED},
		{0xFB1E, 0xFB1E},
		{0xFE00, 0xFE0F},
		{0xFE20, 0xFE2F},
		{0x101FD, 0x101FD},
		{0x102E0, 0x102E0},
		{0x10376, 0x1037A},
		{0x10A01, 0x10A0F},
		{0x10A38, 0x10A3F},
		{0x10AE5, 0x10AE6},
		{0x10D24, 0x10D27},
		{0x10EAB, 0x10EAC},
		{0x10F46, 0x10F50},
		{0x10F82, 0x10F85},
		{0x11001, 0x11001},
		{0x11038, 0x11046},
		{0x11070, 0x11070},
		{0x11073, 0x11074},
		{0x1107F, 0x11081},
		{0x110B3, 0x110B6},
		{0x110B9, 0x110BA},
		{0x110C2, 0x110C2},
		{0x11100, 0x11102},
		{0x11127, 0x1112B},
		{0x1112D, 0x11134},
		{0x11173, 0x11173},
		{0x11180, 0x11181},
		{0x111B6, 0x111BE},
		{0x111C9, 0x111CC},
		{0x111CF, 0x111CF},
		{0x1122F, 0x11231},
		{0x11234, 0x11234},
		{0x11236, 0x11237},
		{0x1123E, 0x1123E},
		{0x112DF, 0x112DF},
		{0x112E3, 0x112EA},
		{0x11300, 0x11301},
		{0x1133B, 0x1133C},
		{0x11340, 0x11340},
		{0x11366, 0x11374},
		{0x11438, 0x1143F},
		{0x11442, 0x11444},
		{0x11446, 0x11446},
		{0x1145E, 0x1145E},
		{0x114B3, 0x114B8},
		{0x114BA, 0x114BA},
		{0x114BF, 0x114C0},
		{0x114C2, 0x114C3},
		{0x115B2, 0x115B5},
		{0x115BC, 0x115BD},
		{0x115BF, 0x115C0},
		{0x115DC, 0x115DD},
		{0x11633, 0x1163A},
		{0x1163D, 0x1163D},
		{0x1163F, 0x11640},
		{0x116AB, 0x116AB},
		{0x116AD, 0x116AD},
		{0x116B0, 0x116B5},
		{0x116B7, 0x116B7},
		{0x1171D, 0x1171F},
		{0x11722, 0x11725},
		{0x11727, 0x1172B},
		{0x1182F, 0x11837},
		{0x11839, 0x1183A},
		{0x1193B, 0x1193C},
		{0x1193E, 0x1193E},
		{0x11943, 0x11943},
		{0x119D4, 0x119DB},
		{0x119E0, 0x119E0},
		{0x11A01, 0x11A0A},
		{0x11A33, 0x11A38},
		{0x11A3B, 0x11A3E},
		{0x11A47, 0x11A47},
		{0x11A51, 0x11A56},
		{0x11A59, 0x11A5B},
		{0x11A8A, 0x11A96},
		{0x11A98, 0x11A99},
		{0x11C30, 0x11C3D},
		{0x11C3F, 0x11C3F},
		{0x11C92, 0x11CA7},
		{0x11CAA, 0x11CB0},
		{0x11CB2, 0x11CB3},
		{0x11CB5, 0x11CB6},
		{0x11D31, 0x11D45},
		{0x11D47, 0x11D47},
		{0x11D90, 0x11D91},
		{0x11D95, 0x11D95},
		{0x11D97, 0x11D97},
		{0x11EF3, 0x11EF4},
		{0x16AF0, 0x16AF4},
		{0x16B30, 0x16B36},
		{0x16F4F, 0x16F4F},
		{0x16F8F, 0x16F92},
		{0x16FE4, 0x16FE4},
		{0x1BC9D, 0x1BC9E},
		{0x1CF00, 0x1CF46},
		{0x1D167, 0x1D169},
		{0x1D17B, 0x1D182},
		{0x1D185, 0x1D18B},
		{0x1D1AA, 0x1D1AD},
		{0x1D242, 0x1D244},
		{0x1DA00, 0x1DA36},
		{0x1DA3B, 0x1DA6C},
		{0x1DA75, 0x1DA75},
		{0x1DA84, 0x1DA84},
		{0x1DA9B, 0x1DAAF},
		{0x1E000, 0x1E02A},
		{0x1E130, 0x1E136},
		{0x1E2AE, 0x1E2AE},
		{0x1E2EC, 0x1E2EF},
		{0x1E8D0, 0x1E8D6},
		{0x1E944, 0x1E94A},
		{0xE0100, 0xE01EF},
	};

	static const struct mbinterval east_asian_fw[] = {
		{0x1100, 0x115F},
		{0x231A, 0x231B},
		{0x2329, 0x232A},
		{0x23E9, 0x23EC},
		{0x23F0, 0x23F0},
		{0x23F3, 0x23F3},
		{0x25FD, 0x25FE},
		{0x2614, 0x2615},
		{0x2648, 0x2653},
		{0x267F, 0x267F},
		{0x2693, 0x2693},
		{0x26A1, 0x26A1},
		{0x26AA, 0x26AB},
		{0x26BD, 0x26BE},
		{0x26C4, 0x26C5},
		{0x26CE, 0x26CE},
		{0x26D4, 0x26D4},
		{0x26EA, 0x26EA},
		{0x26F2, 0x26F3},
		{0x26F5, 0x26F5},
		{0x26FA, 0x26FA},
		{0x26FD, 0x26FD},
		{0x2705, 0x2705},
		{0x270A, 0x270B},
		{0x2728, 0x2728},
		{0x274C, 0x274C},
		{0x274E, 0x274E},
		{0x2753, 0x2755},
		{0x2757, 0x2757},
		{0x2795, 0x2797},
		{0x27B0, 0x27B0},
		{0x27BF, 0x27BF},
		{0x2B1B, 0x2B1C},
		{0x2B50, 0x2B50},
		{0x2B55, 0x2B55},
		{0x2E80, 0x2E99},
		{0x2E9B, 0x2EF3},
		{0x2F00, 0x2FD5},
		{0x2FF0, 0x2FFB},
		{0x3000, 0x303E},
		{0x3041, 0x3096},
		{0x3099, 0x30FF},
		{0x3105, 0x312F},
		{0x3131, 0x318E},
		{0x3190, 0x31E3},
		{0x31F0, 0x321E},
		{0x3220, 0x3247},
		{0x3250, 0x4DBF},
		{0x4E00, 0xA48C},
		{0xA490, 0xA4C6},
		{0xA960, 0xA97C},
		{0xAC00, 0xD7A3},
		{0xF900, 0xFAFF},
		{0xFE10, 0xFE19},
		{0xFE30, 0xFE52},
		{0xFE54, 0xFE66},
		{0xFE68, 0xFE6B},
		{0xFF01, 0xFF60},
		{0xFFE0, 0xFFE6},
		{0x16FE0, 0x16FE4},
		{0x16FF0, 0x16FF1},
		{0x17000, 0x187F7},
		{0x18800, 0x18CD5},
		{0x18D00, 0x18D08},
		{0x1AFF0, 0x1AFF3},
		{0x1AFF5, 0x1AFFB},
		{0x1AFFD, 0x1AFFE},
		{0x1B000, 0x1B122},
		{0x1B150, 0x1B152},
		{0x1B164, 0x1B167},
		{0x1B170, 0x1B2FB},
		{0x1F004, 0x1F004},
		{0x1F0CF, 0x1F0CF},
		{0x1F18E, 0x1F18E},
		{0x1F191, 0x1F19A},
		{0x1F200, 0x1F202},
		{0x1F210, 0x1F23B},
		{0x1F240, 0x1F248},
		{0x1F250, 0x1F251},
		{0x1F260, 0x1F265},
		{0x1F300, 0x1F320},
		{0x1F32D, 0x1F335},
		{0x1F337, 0x1F37C},
		{0x1F37E, 0x1F393},
		{0x1F3A0, 0x1F3CA},
		{0x1F3CF, 0x1F3D3},
		{0x1F3E0, 0x1F3F0},
		{0x1F3F4, 0x1F3F4},
		{0x1F3F8, 0x1F43E},
		{0x1F440, 0x1F440},
		{0x1F442, 0x1F4FC},
		{0x1F4FF, 0x1F53D},
		{0x1F54B, 0x1F54E},
		{0x1F550, 0x1F567},
		{0x1F57A, 0x1F57A},
		{0x1F595, 0x1F596},
		{0x1F5A4, 0x1F5A4},
		{0x1F5FB, 0x1F64F},
		{0x1F680, 0x1F6C5},
		{0x1F6CC, 0x1F6CC},
		{0x1F6D0, 0x1F6D2},
		{0x1F6D5, 0x1F6D7},
		{0x1F6DD, 0x1F6DF},
		{0x1F6EB, 0x1F6EC},
		{0x1F6F4, 0x1F6FC},
		{0x1F7E0, 0x1F7EB},
		{0x1F7F0, 0x1F7F0},
		{0x1F90C, 0x1F93A},
		{0x1F93C, 0x1F945},
		{0x1F947, 0x1F9FF},
		{0x1FA70, 0x1FA74},
		{0x1FA78, 0x1FA7C},
		{0x1FA80, 0x1FA86},
		{0x1FA90, 0x1FAAC},
		{0x1FAB0, 0x1FABA},
		{0x1FAC0, 0x1FAC5},
		{0x1FAD0, 0x1FAD9},
		{0x1FAE0, 0x1FAE7},
		{0x1FAF0, 0x1FAF6},
		{0x20000, 0x2FFFD},
		{0x30000, 0x3FFFD},
	};

	/* test for 8-bit control characters */
	if (ucs == 0)
		return 0;

	if (ucs < 0x20 || (ucs >= 0x7f && ucs < 0xa0) || ucs > 0x0010ffff)
		return -1;

	/*
	 * binary search in table of non-spacing characters
	 *
	 * XXX: In the official Unicode sources, it is possible for a character to
	 * be described as both non-spacing and wide at the same time. As of
	 * Unicode 13.0, treating the non-spacing property as the determining
	 * factor for display width leads to the correct behavior, so do that
	 * search first.
	 */
	if (mbbisearch(ucs, combining,
				   sizeof(combining) / sizeof(struct mbinterval) - 1))
		return 0;

	/* binary search in table of wide characters */
	if (mbbisearch(ucs, east_asian_fw,
				   sizeof(east_asian_fw) / sizeof(struct mbinterval) - 1))
		return 2;

	return 1;
}

/*
 * Convert a UTF-8 character to a Unicode code point.
 * This is a one-character version of pg_utf2wchar_with_len.
 *
 * No error checks here, c must point to a long-enough string.
 */
pg_wchar
utf8_to_unicode(const unsigned char *c)
{
	if ((*c & 0x80) == 0)
		return (pg_wchar) c[0];
	else if ((*c & 0xe0) == 0xc0)
		return (pg_wchar) (((c[0] & 0x1f) << 6) |
						   (c[1] & 0x3f));
	else if ((*c & 0xf0) == 0xe0)
		return (pg_wchar) (((c[0] & 0x0f) << 12) |
						   ((c[1] & 0x3f) << 6) |
						   (c[2] & 0x3f));
	else if ((*c & 0xf8) == 0xf0)
		return (pg_wchar) (((c[0] & 0x07) << 18) |
						   ((c[1] & 0x3f) << 12) |
						   ((c[2] & 0x3f) << 6) |
						   (c[3] & 0x3f));
	else
		/* that is an invalid code on purpose */
		return 0xffffffff;
}

static int
pg_utf_dsplen(const unsigned char *s)
{
	return ucs_wcwidth(utf8_to_unicode(s));
}

/*
 * convert mule internal code to pg_wchar
 * caller should allocate enough space for "to"
 * len: length of from.
 * "from" not necessarily null terminated.
 */
static int
pg_mule2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
	int			cnt = 0;

	while (len > 0 && *from)
	{
		if (IS_LC1(*from) && len >= 2)
		{
			*to = *from++ << 16;
			*to |= *from++;
			len -= 2;
		}
		else if (IS_LCPRV1(*from) && len >= 3)
		{
			from++;
			*to = *from++ << 16;
			*to |= *from++;
			len -= 3;
		}
		else if (IS_LC2(*from) && len >= 3)
		{
			*to = *from++ << 16;
			*to |= *from++ << 8;
			*to |= *from++;
			len -= 3;
		}
		else if (IS_LCPRV2(*from) && len >= 4)
		{
			from++;
			*to = *from++ << 16;
			*to |= *from++ << 8;
			*to |= *from++;
			len -= 4;
		}
		else
		{						/* assume ASCII */
			*to = (unsigned char) *from++;
			len--;
		}
		to++;
		cnt++;
	}
	*to = 0;
	return cnt;
}

/*
 * convert pg_wchar to mule internal code
 * caller should allocate enough space for "to"
 * len: length of from.
 * "from" not necessarily null terminated.
 */
static int
pg_wchar2mule_with_len(const pg_wchar *from, unsigned char *to, int len)
{
	int			cnt = 0;

	while (len > 0 && *from)
	{
		unsigned char lb;

		lb = (*from >> 16) & 0xff;
		if (IS_LC1(lb))
		{
			*to++ = lb;
			*to++ = *from & 0xff;
			cnt += 2;
		}
		else if (IS_LC2(lb))
		{
			*to++ = lb;
			*to++ = (*from >> 8) & 0xff;
			*to++ = *from & 0xff;
			cnt += 3;
		}
		else if (IS_LCPRV1_A_RANGE(lb))
		{
			*to++ = LCPRV1_A;
			*to++ = lb;
			*to++ = *from & 0xff;
			cnt += 3;
		}
		else if (IS_LCPRV1_B_RANGE(lb))
		{
			*to++ = LCPRV1_B;
			*to++ = lb;
			*to++ = *from & 0xff;
			cnt += 3;
		}
		else if (IS_LCPRV2_A_RANGE(lb))
		{
			*to++ = LCPRV2_A;
			*to++ = lb;
			*to++ = (*from >> 8) & 0xff;
			*to++ = *from & 0xff;
			cnt += 4;
		}
		else if (IS_LCPRV2_B_RANGE(lb))
		{
			*to++ = LCPRV2_B;
			*to++ = lb;
			*to++ = (*from >> 8) & 0xff;
			*to++ = *from & 0xff;
			cnt += 4;
		}
		else
		{
			*to++ = *from & 0xff;
			cnt += 1;
		}
		from++;
		len--;
	}
	*to = 0;
	return cnt;
}

int
pg_mule_mblen(const unsigned char *s)
{
	int			len;

	if (IS_LC1(*s))
		len = 2;
	else if (IS_LCPRV1(*s))
		len = 3;
	else if (IS_LC2(*s))
		len = 3;
	else if (IS_LCPRV2(*s))
		len = 4;
	else
		len = 1;				/* assume ASCII */
	return len;
}

static int
pg_mule_dsplen(const unsigned char *s)
{
	int			len;

	/*
	 * Note: it's not really appropriate to assume that all multibyte charsets
	 * are double-wide on screen.  But this seems an okay approximation for
	 * the MULE charsets we currently support.
	 */

	if (IS_LC1(*s))
		len = 1;
	else if (IS_LCPRV1(*s))
		len = 1;
	else if (IS_LC2(*s))
		len = 2;
	else if (IS_LCPRV2(*s))
		len = 2;
	else
		len = 1;				/* assume ASCII */

	return len;
}

/*
 * ISO8859-1
 */
static int
pg_latin12wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
	int			cnt = 0;

	while (len > 0 && *from)
	{
		*to++ = *from++;
		len--;
		cnt++;
	}
	*to = 0;
	return cnt;
}

/*
 * Trivial conversion from pg_wchar to single byte encoding. Just ignores
 * high bits.
 * caller should allocate enough space for "to"
 * len: length of from.
 * "from" not necessarily null terminated.
 */
static int
pg_wchar2single_with_len(const pg_wchar *from, unsigned char *to, int len)
{
	int			cnt = 0;

	while (len > 0 && *from)
	{
		*to++ = *from++;
		len--;
		cnt++;
	}
	*to = 0;
	return cnt;
}

static int
pg_latin1_mblen(const unsigned char *s)
{
	return 1;
}

static int
pg_latin1_dsplen(const unsigned char *s)
{
	return pg_ascii_dsplen(s);
}

/*
 * SJIS
 */
static int
pg_sjis_mblen(const unsigned char *s)
{
	int			len;

	if (*s >= 0xa1 && *s <= 0xdf)
		len = 1;				/* 1 byte kana? */
	else if (IS_HIGHBIT_SET(*s))
		len = 2;				/* kanji? */
	else
		len = 1;				/* should be ASCII */
	return len;
}

static int
pg_sjis_dsplen(const unsigned char *s)
{
	int			len;

	if (*s >= 0xa1 && *s <= 0xdf)
		len = 1;				/* 1 byte kana? */
	else if (IS_HIGHBIT_SET(*s))
		len = 2;				/* kanji? */
	else
		len = pg_ascii_dsplen(s);	/* should be ASCII */
	return len;
}

/*
 * Big5
 */
static int
pg_big5_mblen(const unsigned char *s)
{
	int			len;

	if (IS_HIGHBIT_SET(*s))
		len = 2;				/* kanji? */
	else
		len = 1;				/* should be ASCII */
	return len;
}

static int
pg_big5_dsplen(const unsigned char *s)
{
	int			len;

	if (IS_HIGHBIT_SET(*s))
		len = 2;				/* kanji? */
	else
		len = pg_ascii_dsplen(s);	/* should be ASCII */
	return len;
}

/*
 * GBK
 */
static int
pg_gbk_mblen(const unsigned char *s)
{
	int			len;

	if (IS_HIGHBIT_SET(*s))
		len = 2;				/* kanji? */
	else
		len = 1;				/* should be ASCII */
	return len;
}

static int
pg_gbk_dsplen(const unsigned char *s)
{
	int			len;

	if (IS_HIGHBIT_SET(*s))
		len = 2;				/* kanji? */
	else
		len = pg_ascii_dsplen(s);	/* should be ASCII */
	return len;
}

/*
 * UHC
 */
static int
pg_uhc_mblen(const unsigned char *s)
{
	int			len;

	if (IS_HIGHBIT_SET(*s))
		len = 2;				/* 2byte? */
	else
		len = 1;				/* should be ASCII */
	return len;
}

static int
pg_uhc_dsplen(const unsigned char *s)
{
	int			len;

	if (IS_HIGHBIT_SET(*s))
		len = 2;				/* 2byte? */
	else
		len = pg_ascii_dsplen(s);	/* should be ASCII */
	return len;
}

/*
 * GB18030
 *	Added by Bill Huang <bhuang@redhat.com>,<bill_huanghb@ybb.ne.jp>
 */

/*
 * Unlike all other mblen() functions, this also looks at the second byte of
 * the input.  However, if you only pass the first byte of a multi-byte
 * string, and \0 as the second byte, this still works in a predictable way:
 * a 4-byte character will be reported as two 2-byte characters.  That's
 * enough for all current uses, as a client-only encoding.  It works that
 * way, because in any valid 4-byte GB18030-encoded character, the third and
 * fourth byte look like a 2-byte encoded character, when looked at
 * separately.
 */
static int
pg_gb18030_mblen(const unsigned char *s)
{
	int			len;

	if (!IS_HIGHBIT_SET(*s))
		len = 1;				/* ASCII */
	else if (*(s + 1) >= 0x30 && *(s + 1) <= 0x39)
		len = 4;
	else
		len = 2;
	return len;
}

static int
pg_gb18030_dsplen(const unsigned char *s)
{
	int			len;

	if (IS_HIGHBIT_SET(*s))
		len = 2;
	else
		len = pg_ascii_dsplen(s);	/* ASCII */
	return len;
}

/*
 *-------------------------------------------------------------------
 * multibyte sequence validators
 *
 * The verifychar functions accept "s", a pointer to the first byte of a
 * string, and "len", the remaining length of the string.  If there is a
 * validly encoded character beginning at *s, return its length in bytes;
 * else return -1.
 *
 * The verifystr functions also accept "s", a pointer to a string and "len",
 * the length of the string.  They verify the whole string, and return the
 * number of input bytes (<= len) that are valid.  In other words, if the
 * whole string is valid, verifystr returns "len", otherwise it returns the
 * byte offset of the first invalid character.  The verifystr functions must
 * test for and reject zeroes in the input.
 *
 * The verifychar functions can assume that len > 0 and that *s != '\0', but
 * they must test for and reject zeroes in any additional bytes of a
 * multibyte character.  Note that this definition allows the function for a
 * single-byte encoding to be just "return 1".
 *-------------------------------------------------------------------
 */
static int
pg_ascii_verifychar(const unsigned char *s, int len)
{
	return 1;
}

static int
pg_ascii_verifystr(const unsigned char *s, int len)
{
	const unsigned char *nullpos = memchr(s, 0, len);

	if (nullpos == NULL)
		return len;
	else
		return nullpos - s;
}

#define IS_EUC_RANGE_VALID(c)	((c) >= 0xa1 && (c) <= 0xfe)

static int
pg_eucjp_verifychar(const unsigned char *s, int len)
{
	int			l;
	unsigned char c1,
				c2;

	c1 = *s++;

	switch (c1)
	{
		case SS2:				/* JIS X 0201 */
			l = 2;
			if (l > len)
				return -1;
			c2 = *s++;
			if (c2 < 0xa1 || c2 > 0xdf)
				return -1;
			break;

		case SS3:				/* JIS X 0212 */
			l = 3;
			if (l > len)
				return -1;
			c2 = *s++;
			if (!IS_EUC_RANGE_VALID(c2))
				return -1;
			c2 = *s++;
			if (!IS_EUC_RANGE_VALID(c2))
				return -1;
			break;

		default:
			if (IS_HIGHBIT_SET(c1)) /* JIS X 0208? */
			{
				l = 2;
				if (l > len)
					return -1;
				if (!IS_EUC_RANGE_VALID(c1))
					return -1;
				c2 = *s++;
				if (!IS_EUC_RANGE_VALID(c2))
					return -1;
			}
			else
				/* must be ASCII */
			{
				l = 1;
			}
			break;
	}

	return l;
}

static int
pg_eucjp_verifystr(const unsigned char *s, int len)
{
	const unsigned char *start = s;

	while (len > 0)
	{
		int			l;

		/* fast path for ASCII-subset characters */
		if (!IS_HIGHBIT_SET(*s))
		{
			if (*s == '\0')
				break;
			l = 1;
		}
		else
		{
			l = pg_eucjp_verifychar(s, len);
			if (l == -1)
				break;
		}
		s += l;
		len -= l;
	}

	return s - start;
}

static int
pg_euckr_verifychar(const unsigned char *s, int len)
{
	int			l;
	unsigned char c1,
				c2;

	c1 = *s++;

	if (IS_HIGHBIT_SET(c1))
	{
		l = 2;
		if (l > len)
			return -1;
		if (!IS_EUC_RANGE_VALID(c1))
			return -1;
		c2 = *s++;
		if (!IS_EUC_RANGE_VALID(c2))
			return -1;
	}
	else
		/* must be ASCII */
	{
		l = 1;
	}

	return l;
}

static int
pg_euckr_verifystr(const unsigned char *s, int len)
{
	const unsigned char *start = s;

	while (len > 0)
	{
		int			l;

		/* fast path for ASCII-subset characters */
		if (!IS_HIGHBIT_SET(*s))
		{
			if (*s == '\0')
				break;
			l = 1;
		}
		else
		{
			l = pg_euckr_verifychar(s, len);
			if (l == -1)
				break;
		}
		s += l;
		len -= l;
	}

	return s - start;
}

/* EUC-CN byte sequences are exactly same as EUC-KR */
#define pg_euccn_verifychar	pg_euckr_verifychar
#define pg_euccn_verifystr	pg_euckr_verifystr

static int
pg_euctw_verifychar(const unsigned char *s, int len)
{
	int			l;
	unsigned char c1,
				c2;

	c1 = *s++;

	switch (c1)
	{
		case SS2:				/* CNS 11643 Plane 1-7 */
			l = 4;
			if (l > len)
				return -1;
			c2 = *s++;
			if (c2 < 0xa1 || c2 > 0xa7)
				return -1;
			c2 = *s++;
			if (!IS_EUC_RANGE_VALID(c2))
				return -1;
			c2 = *s++;
			if (!IS_EUC_RANGE_VALID(c2))
				return -1;
			break;

		case SS3:				/* unused */
			return -1;

		default:
			if (IS_HIGHBIT_SET(c1)) /* CNS 11643 Plane 1 */
			{
				l = 2;
				if (l > len)
					return -1;
				/* no further range check on c1? */
				c2 = *s++;
				if (!IS_EUC_RANGE_VALID(c2))
					return -1;
			}
			else
				/* must be ASCII */
			{
				l = 1;
			}
			break;
	}
	return l;
}

static int
pg_euctw_verifystr(const unsigned char *s, int len)
{
	const unsigned char *start = s;

	while (len > 0)
	{
		int			l;

		/* fast path for ASCII-subset characters */
		if (!IS_HIGHBIT_SET(*s))
		{
			if (*s == '\0')
				break;
			l = 1;
		}
		else
		{
			l = pg_euctw_verifychar(s, len);
			if (l == -1)
				break;
		}
		s += l;
		len -= l;
	}

	return s - start;
}

static int
pg_johab_verifychar(const unsigned char *s, int len)
{
	int			l,
				mbl;
	unsigned char c;

	l = mbl = pg_johab_mblen(s);

	if (len < l)
		return -1;

	if (!IS_HIGHBIT_SET(*s))
		return mbl;

	while (--l > 0)
	{
		c = *++s;
		if (!IS_EUC_RANGE_VALID(c))
			return -1;
	}
	return mbl;
}

static int
pg_johab_verifystr(const unsigned char *s, int len)
{
	const unsigned char *start = s;

	while (len > 0)
	{
		int			l;

		/* fast path for ASCII-subset characters */
		if (!IS_HIGHBIT_SET(*s))
		{
			if (*s == '\0')
				break;
			l = 1;
		}
		else
		{
			l = pg_johab_verifychar(s, len);
			if (l == -1)
				break;
		}
		s += l;
		len -= l;
	}

	return s - start;
}

static int
pg_mule_verifychar(const unsigned char *s, int len)
{
	int			l,
				mbl;
	unsigned char c;

	l = mbl = pg_mule_mblen(s);

	if (len < l)
		return -1;

	while (--l > 0)
	{
		c = *++s;
		if (!IS_HIGHBIT_SET(c))
			return -1;
	}
	return mbl;
}

static int
pg_mule_verifystr(const unsigned char *s, int len)
{
	const unsigned char *start = s;

	while (len > 0)
	{
		int			l;

		/* fast path for ASCII-subset characters */
		if (!IS_HIGHBIT_SET(*s))
		{
			if (*s == '\0')
				break;
			l = 1;
		}
		else
		{
			l = pg_mule_verifychar(s, len);
			if (l == -1)
				break;
		}
		s += l;
		len -= l;
	}

	return s - start;
}

static int
pg_latin1_verifychar(const unsigned char *s, int len)
{
	return 1;
}

static int
pg_latin1_verifystr(const unsigned char *s, int len)
{
	const unsigned char *nullpos = memchr(s, 0, len);

	if (nullpos == NULL)
		return len;
	else
		return nullpos - s;
}

static int
pg_sjis_verifychar(const unsigned char *s, int len)
{
	int			l,
				mbl;
	unsigned char c1,
				c2;

	l = mbl = pg_sjis_mblen(s);

	if (len < l)
		return -1;

	if (l == 1)					/* pg_sjis_mblen already verified it */
		return mbl;

	c1 = *s++;
	c2 = *s;
	if (!ISSJISHEAD(c1) || !ISSJISTAIL(c2))
		return -1;
	return mbl;
}

static int
pg_sjis_verifystr(const unsigned char *s, int len)
{
	const unsigned char *start = s;

	while (len > 0)
	{
		int			l;

		/* fast path for ASCII-subset characters */
		if (!IS_HIGHBIT_SET(*s))
		{
			if (*s == '\0')
				break;
			l = 1;
		}
		else
		{
			l = pg_sjis_verifychar(s, len);
			if (l == -1)
				break;
		}
		s += l;
		len -= l;
	}

	return s - start;
}

static int
pg_big5_verifychar(const unsigned char *s, int len)
{
	int			l,
				mbl;

	l = mbl = pg_big5_mblen(s);

	if (len < l)
		return -1;

	while (--l > 0)
	{
		if (*++s == '\0')
			return -1;
	}

	return mbl;
}

static int
pg_big5_verifystr(const unsigned char *s, int len)
{
	const unsigned char *start = s;

	while (len > 0)
	{
		int			l;

		/* fast path for ASCII-subset characters */
		if (!IS_HIGHBIT_SET(*s))
		{
			if (*s == '\0')
				break;
			l = 1;
		}
		else
		{
			l = pg_big5_verifychar(s, len);
			if (l == -1)
				break;
		}
		s += l;
		len -= l;
	}

	return s - start;
}

static int
pg_gbk_verifychar(const unsigned char *s, int len)
{
	int			l,
				mbl;

	l = mbl = pg_gbk_mblen(s);

	if (len < l)
		return -1;

	while (--l > 0)
	{
		if (*++s == '\0')
			return -1;
	}

	return mbl;
}

static int
pg_gbk_verifystr(const unsigned char *s, int len)
{
	const unsigned char *start = s;

	while (len > 0)
	{
		int			l;

		/* fast path for ASCII-subset characters */
		if (!IS_HIGHBIT_SET(*s))
		{
			if (*s == '\0')
				break;
			l = 1;
		}
		else
		{
			l = pg_gbk_verifychar(s, len);
			if (l == -1)
				break;
		}
		s += l;
		len -= l;
	}

	return s - start;
}

static int
pg_uhc_verifychar(const unsigned char *s, int len)
{
	int			l,
				mbl;

	l = mbl = pg_uhc_mblen(s);

	if (len < l)
		return -1;

	while (--l > 0)
	{
		if (*++s == '\0')
			return -1;
	}

	return mbl;
}

static int
pg_uhc_verifystr(const unsigned char *s, int len)
{
	const unsigned char *start = s;

	while (len > 0)
	{
		int			l;

		/* fast path for ASCII-subset characters */
		if (!IS_HIGHBIT_SET(*s))
		{
			if (*s == '\0')
				break;
			l = 1;
		}
		else
		{
			l = pg_uhc_verifychar(s, len);
			if (l == -1)
				break;
		}
		s += l;
		len -= l;
	}

	return s - start;
}

static int
pg_gb18030_verifychar(const unsigned char *s, int len)
{
	int			l;

	if (!IS_HIGHBIT_SET(*s))
		l = 1;					/* ASCII */
	else if (len >= 4 && *(s + 1) >= 0x30 && *(s + 1) <= 0x39)
	{
		/* Should be 4-byte, validate remaining bytes */
		if (*s >= 0x81 && *s <= 0xfe &&
			*(s + 2) >= 0x81 && *(s + 2) <= 0xfe &&
			*(s + 3) >= 0x30 && *(s + 3) <= 0x39)
			l = 4;
		else
			l = -1;
	}
	else if (len >= 2 && *s >= 0x81 && *s <= 0xfe)
	{
		/* Should be 2-byte, validate */
		if ((*(s + 1) >= 0x40 && *(s + 1) <= 0x7e) ||
			(*(s + 1) >= 0x80 && *(s + 1) <= 0xfe))
			l = 2;
		else
			l = -1;
	}
	else
		l = -1;
	return l;
}

static int
pg_gb18030_verifystr(const unsigned char *s, int len)
{
	const unsigned char *start = s;

	while (len > 0)
	{
		int			l;

		/* fast path for ASCII-subset characters */
		if (!IS_HIGHBIT_SET(*s))
		{
			if (*s == '\0')
				break;
			l = 1;
		}
		else
		{
			l = pg_gb18030_verifychar(s, len);
			if (l == -1)
				break;
		}
		s += l;
		len -= l;
	}

	return s - start;
}

static int
pg_utf8_verifychar(const unsigned char *s, int len)
{
	int			l;

	if ((*s & 0x80) == 0)
	{
		if (*s == '\0')
			return -1;
		return 1;
	}
	else if ((*s & 0xe0) == 0xc0)
		l = 2;
	else if ((*s & 0xf0) == 0xe0)
		l = 3;
	else if ((*s & 0xf8) == 0xf0)
		l = 4;
	else
		l = 1;

	if (l > len)
		return -1;

	if (!pg_utf8_islegal(s, l))
		return -1;

	return l;
}

/*
 * The fast path of the UTF-8 verifier uses a deterministic finite automaton
 * (DFA) for multibyte characters. In a traditional table-driven DFA, the
 * input byte and current state are used to compute an index into an array of
 * state transitions. Since the address of the next transition is dependent
 * on this computation, there is latency in executing the load instruction,
 * and the CPU is not kept busy.
 *
 * Instead, we use a "shift-based" DFA as described by Per Vognsen:
 *
 * https://gist.github.com/pervognsen/218ea17743e1442e59bb60d29b1aa725
 *
 * In a shift-based DFA, the input byte is an index into array of integers
 * whose bit pattern encodes the state transitions. To compute the next
 * state, we simply right-shift the integer by the current state and apply a
 * mask. In this scheme, the address of the transition only depends on the
 * input byte, so there is better pipelining.
 *
 * The naming convention for states and transitions was adopted from a UTF-8
 * to UTF-16/32 transcoder, whose table is reproduced below:
 *
 * https://github.com/BobSteagall/utf_utils/blob/6b7a465265de2f5fa6133d653df0c9bdd73bbcf8/src/utf_utils.cpp
 *
 * ILL  ASC  CR1  CR2  CR3  L2A  L3A  L3B  L3C  L4A  L4B  L4C CLASS / STATE
 * ==========================================================================
 * err, END, err, err, err, CS1, P3A, CS2, P3B, P4A, CS3, P4B,      | BGN/END
 * err, err, err, err, err, err, err, err, err, err, err, err,      | ERR
 *                                                                  |
 * err, err, END, END, END, err, err, err, err, err, err, err,      | CS1
 * err, err, CS1, CS1, CS1, err, err, err, err, err, err, err,      | CS2
 * err, err, CS2, CS2, CS2, err, err, err, err, err, err, err,      | CS3
 *                                                                  |
 * err, err, err, err, CS1, err, err, err, err, err, err, err,      | P3A
 * err, err, CS1, CS1, err, err, err, err, err, err, err, err,      | P3B
 *                                                                  |
 * err, err, err, CS2, CS2, err, err, err, err, err, err, err,      | P4A
 * err, err, CS2, err, err, err, err, err, err, err, err, err,      | P4B
 *
 * In the most straightforward implementation, a shift-based DFA for UTF-8
 * requires 64-bit integers to encode the transitions, but with an SMT solver
 * it's possible to find state numbers such that the transitions fit within
 * 32-bit integers, as Dougall Johnson demonstrated:
 *
 * https://gist.github.com/dougallj/166e326de6ad4cf2c94be97a204c025f
 *
 * This packed representation is the reason for the seemingly odd choice of
 * state values below.
 */

/* Error */
#define	ERR  0
/* Begin */
#define	BGN 11
/* Continuation states, expect 1/2/3 continuation bytes */
#define	CS1 16
#define	CS2  1
#define	CS3  5
/* Partial states, where the first continuation byte has a restricted range */
#define	P3A  6					/* Lead was E0, check for 3-byte overlong */
#define	P3B 20					/* Lead was ED, check for surrogate */
#define	P4A 25					/* Lead was F0, check for 4-byte overlong */
#define	P4B 30					/* Lead was F4, check for too-large */
/* Begin and End are the same state */
#define	END BGN

/* the encoded state transitions for the lookup table */

/* ASCII */
#define ASC (END << BGN)
/* 2-byte lead */
#define L2A (CS1 << BGN)
/* 3-byte lead */
#define L3A (P3A << BGN)
#define L3B (CS2 << BGN)
#define L3C (P3B << BGN)
/* 4-byte lead */
#define L4A (P4A << BGN)
#define L4B (CS3 << BGN)
#define L4C (P4B << BGN)
/* continuation byte */
#define CR1 (END << CS1) | (CS1 << CS2) | (CS2 << CS3) | (CS1 << P3B) | (CS2 << P4B)
#define CR2 (END << CS1) | (CS1 << CS2) | (CS2 << CS3) | (CS1 << P3B) | (CS2 << P4A)
#define CR3 (END << CS1) | (CS1 << CS2) | (CS2 << CS3) | (CS1 << P3A) | (CS2 << P4A)
/* invalid byte */
#define ILL ERR

static int
pg_utf8_verifystr(const unsigned char *s, int len)
{
	const unsigned char *start = s;

	while (len > 0)
	{
		int			l;

		/* fast path for ASCII-subset characters */
		if (!IS_HIGHBIT_SET(*s))
		{
			if (*s == '\0')
				break;
			l = 1;
		}
		else
		{
			l = pg_utf8_verifychar(s, len);
			if (l == -1)
				break;
		}
		s += l;
		len -= l;
	}

	return s - start;
}

/*
 * Check for validity of a single UTF-8 encoded character
 *
 * This directly implements the rules in RFC3629.  The bizarre-looking
 * restrictions on the second byte are meant to ensure that there isn't
 * more than one encoding of a given Unicode character point; that is,
 * you may not use a longer-than-necessary byte sequence with high order
 * zero bits to represent a character that would fit in fewer bytes.
 * To do otherwise is to create security hazards (eg, create an apparent
 * non-ASCII character that decodes to plain ASCII).
 *
 * length is assumed to have been obtained by pg_utf_mblen(), and the
 * caller must have checked that that many bytes are present in the buffer.
 */
bool
pg_utf8_islegal(const unsigned char *source, int length)
{
	unsigned char a;

	switch (length)
	{
		default:
			/* reject lengths 5 and 6 for now */
			return false;
		case 4:
			a = source[3];
			if (a < 0x80 || a > 0xBF)
				return false;
			/* FALL THRU */
		case 3:
			a = source[2];
			if (a < 0x80 || a > 0xBF)
				return false;
			/* FALL THRU */
		case 2:
			a = source[1];
			switch (*source)
			{
				case 0xE0:
					if (a < 0xA0 || a > 0xBF)
						return false;
					break;
				case 0xED:
					if (a < 0x80 || a > 0x9F)
						return false;
					break;
				case 0xF0:
					if (a < 0x90 || a > 0xBF)
						return false;
					break;
				case 0xF4:
					if (a < 0x80 || a > 0x8F)
						return false;
					break;
				default:
					if (a < 0x80 || a > 0xBF)
						return false;
					break;
			}
			/* FALL THRU */
		case 1:
			a = *source;
			if (a >= 0x80 && a < 0xC2)
				return false;
			if (a > 0xF4)
				return false;
			break;
	}
	return true;
}

#ifndef FRONTEND

/*
 * Generic character incrementer function.
 *
 * Not knowing anything about the properties of the encoding in use, we just
 * keep incrementing the last byte until we get a validly-encoded result,
 * or we run out of values to try.  We don't bother to try incrementing
 * higher-order bytes, so there's no growth in runtime for wider characters.
 * (If we did try to do that, we'd need to consider the likelihood that 255
 * is not a valid final byte in the encoding.)
 */
static bool
pg_generic_charinc(unsigned char *charptr, int len)
{
	unsigned char *lastbyte = charptr + len - 1;
	mbchar_verifier	mbverify;

	/* We can just invoke the character verifier directly. */
	mbverify = pg_wchar_table[GetDatabaseEncoding()].mbverifychar;

	while (*lastbyte < (unsigned char) 255)
	{
		(*lastbyte)++;
		if ((*mbverify) (charptr, len) == len)
			return true;
	}

	return false;
}

/*
 * UTF-8 character incrementer function.
 *
 * For a one-byte character less than 0x7F, we just increment the byte.
 *
 * For a multibyte character, every byte but the first must fall between 0x80
 * and 0xBF; and the first byte must be between 0xC0 and 0xF4.  We increment
 * the last byte that's not already at its maximum value.  If we can't find a
 * byte that's less than the maximum allowable value, we simply fail.  We also
 * need some special-case logic to skip regions used for surrogate pair
 * handling, as those should not occur in valid UTF-8.
 *
 * Note that we don't reset lower-order bytes back to their minimums, since
 * we can't afford to make an exhaustive search (see make_greater_string).
 */
static bool
pg_utf8_increment(unsigned char *charptr, int length)
{
	unsigned char a;
	unsigned char limit;

	switch (length)
	{
		default:
			/* reject lengths 5 and 6 for now */
			return false;
		case 4:
			a = charptr[3];
			if (a < 0xBF)
			{
				charptr[3]++;
				break;
			}
			/* FALL THRU */
		case 3:
			a = charptr[2];
			if (a < 0xBF)
			{
				charptr[2]++;
				break;
			}
			/* FALL THRU */
		case 2:
			a = charptr[1];
			switch (*charptr)
			{
				case 0xED:
					limit = 0x9F;
					break;
				case 0xF4:
					limit = 0x8F;
					break;
				default:
					limit = 0xBF;
					break;
			}
			if (a < limit)
			{
				charptr[1]++;
				break;
			}
			/* FALL THRU */
		case 1:
			a = *charptr;
			if (a == 0x7F || a == 0xDF || a == 0xEF || a == 0xF4)
				return false;
			charptr[0]++;
			break;
	}

	return true;
}

/*
 * EUC-JP character incrementer function.
 *
 * If the sequence starts with SS2 (0x8e), it must be a two-byte sequence
 * representing JIS X 0201 characters with the second byte ranging between
 * 0xa1 and 0xdf.  We just increment the last byte if it's less than 0xdf,
 * and otherwise rewrite the whole sequence to 0xa1 0xa1.
 *
 * If the sequence starts with SS3 (0x8f), it must be a three-byte sequence
 * in which the last two bytes range between 0xa1 and 0xfe.  The last byte
 * is incremented if possible, otherwise the second-to-last byte.
 *
 * If the sequence starts with a value other than the above and its MSB
 * is set, it must be a two-byte sequence representing JIS X 0208 characters
 * with both bytes ranging between 0xa1 and 0xfe.  The last byte is
 * incremented if possible, otherwise the second-to-last byte.
 *
 * Otherwise, the sequence is a single-byte ASCII character. It is
 * incremented up to 0x7f.
 */
static bool
pg_eucjp_increment(unsigned char *charptr, int length)
{
	unsigned char c1,
				c2;
	int			i;

	c1 = *charptr;

	switch (c1)
	{
		case SS2:				/* JIS X 0201 */
			if (length != 2)
				return false;

			c2 = charptr[1];

			if (c2 >= 0xdf)
				charptr[0] = charptr[1] = 0xa1;
			else if (c2 < 0xa1)
				charptr[1] = 0xa1;
			else
				charptr[1]++;
			break;

		case SS3:				/* JIS X 0212 */
			if (length != 3)
				return false;

			for (i = 2; i > 0; i--)
			{
				c2 = charptr[i];
				if (c2 < 0xa1)
				{
					charptr[i] = 0xa1;
					return true;
				}
				else if (c2 < 0xfe)
				{
					charptr[i]++;
					return true;
				}
			}

			/* Out of 3-byte code region */
			return false;

		default:
			if (IS_HIGHBIT_SET(c1)) /* JIS X 0208? */
			{
				if (length != 2)
					return false;

				for (i = 1; i >= 0; i--)
				{
					c2 = charptr[i];
					if (c2 < 0xa1)
					{
						charptr[i] = 0xa1;
						return true;
					}
					else if (c2 < 0xfe)
					{
						charptr[i]++;
						return true;
					}
				}

				/* Out of 2 byte code region */
				return false;
			}
			else
			{					/* ASCII, single byte */
				if (c1 > 0x7e)
					return false;
				(*charptr)++;
			}
			break;
	}

	return true;
}
#endif							/* !FRONTEND */


/*
 *-------------------------------------------------------------------
 * encoding info table
 * XXX must be sorted by the same order as enum pg_enc (in mb/pg_wchar.h)
 *-------------------------------------------------------------------
 */
const pg_wchar_tbl pg_wchar_table[] = {
	{pg_ascii2wchar_with_len, pg_wchar2single_with_len, pg_ascii_mblen, pg_ascii_dsplen, pg_ascii_verifychar, pg_ascii_verifystr, 1},	/* PG_SQL_ASCII */
	{pg_eucjp2wchar_with_len, pg_wchar2euc_with_len, pg_eucjp_mblen, pg_eucjp_dsplen, pg_eucjp_verifychar, pg_eucjp_verifystr, 3},	/* PG_EUC_JP */
	{pg_euccn2wchar_with_len, pg_wchar2euc_with_len, pg_euccn_mblen, pg_euccn_dsplen, pg_euccn_verifychar, pg_euccn_verifystr, 2},	/* PG_EUC_CN */
	{pg_euckr2wchar_with_len, pg_wchar2euc_with_len, pg_euckr_mblen, pg_euckr_dsplen, pg_euckr_verifychar, pg_euckr_verifystr, 3},	/* PG_EUC_KR */
	{pg_euctw2wchar_with_len, pg_wchar2euc_with_len, pg_euctw_mblen, pg_euctw_dsplen, pg_euctw_verifychar, pg_euctw_verifystr, 4},	/* PG_EUC_TW */
	{pg_eucjp2wchar_with_len, pg_wchar2euc_with_len, pg_eucjp_mblen, pg_eucjp_dsplen, pg_eucjp_verifychar, pg_eucjp_verifystr, 3},	/* PG_EUC_JIS_2004 */
	{pg_utf2wchar_with_len, pg_wchar2utf_with_len, pg_utf_mblen, pg_utf_dsplen, pg_utf8_verifychar, pg_utf8_verifystr, 4},	/* PG_UTF8 */
	{pg_mule2wchar_with_len, pg_wchar2mule_with_len, pg_mule_mblen, pg_mule_dsplen, pg_mule_verifychar, pg_mule_verifystr, 4},	/* PG_MULE_INTERNAL */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_LATIN1 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_LATIN2 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_LATIN3 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_LATIN4 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_LATIN5 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_LATIN6 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_LATIN7 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_LATIN8 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_LATIN9 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_LATIN10 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_WIN1256 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_WIN1258 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_WIN866 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_WIN874 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_KOI8R */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_WIN1251 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_WIN1252 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* ISO-8859-5 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* ISO-8859-6 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* ISO-8859-7 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* ISO-8859-8 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_WIN1250 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_WIN1253 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_WIN1254 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_WIN1255 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_WIN1257 */
	{pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifychar, pg_latin1_verifystr, 1},	/* PG_KOI8U */
	{0, 0, pg_sjis_mblen, pg_sjis_dsplen, pg_sjis_verifychar, pg_sjis_verifystr, 2},	/* PG_SJIS */
	{0, 0, pg_big5_mblen, pg_big5_dsplen, pg_big5_verifychar, pg_big5_verifystr, 2},	/* PG_BIG5 */
	{0, 0, pg_gbk_mblen, pg_gbk_dsplen, pg_gbk_verifychar, pg_gbk_verifystr, 2},	/* PG_GBK */
	{0, 0, pg_uhc_mblen, pg_uhc_dsplen, pg_uhc_verifychar, pg_uhc_verifystr, 2},	/* PG_UHC */
	{0, 0, pg_gb18030_mblen, pg_gb18030_dsplen, pg_gb18030_verifychar, pg_gb18030_verifystr, 4},	/* PG_GB18030 */
	{0, 0, pg_johab_mblen, pg_johab_dsplen, pg_johab_verifychar, pg_johab_verifystr, 3},	/* PG_JOHAB */
	{0, 0, pg_sjis_mblen, pg_sjis_dsplen, pg_sjis_verifychar, pg_sjis_verifystr, 2} /* PG_SHIFT_JIS_2004 */
};

/*
 * Returns the byte length of a multibyte character.
 *
 * Caution: when dealing with text that is not certainly valid in the
 * specified encoding, the result may exceed the actual remaining
 * string length.  Callers that are not prepared to deal with that
 * should use pg_encoding_mblen_bounded() instead.
 */
int
pg_encoding_mblen(int encoding, const char *mbstr)
{
	return (PG_VALID_ENCODING(encoding) ?
			pg_wchar_table[encoding].mblen((const unsigned char *) mbstr) :
			pg_wchar_table[PG_SQL_ASCII].mblen((const unsigned char *) mbstr));
}

/*
 * Returns the byte length of a multibyte character; but not more than
 * the distance to end of string.
 */
int
pg_encoding_mblen_bounded(int encoding, const char *mbstr)
{
	return strnlen(mbstr, pg_encoding_mblen(encoding, mbstr));
}

/*
 * Returns the display length of a multibyte character.
 */
int
pg_encoding_dsplen(int encoding, const char *mbstr)
{
	return (PG_VALID_ENCODING(encoding) ?
			pg_wchar_table[encoding].dsplen((const unsigned char *) mbstr) :
			pg_wchar_table[PG_SQL_ASCII].dsplen((const unsigned char *) mbstr));
}

/*
 * Verify the first multibyte character of the given string.
 * Return its byte length if good, -1 if bad.  (See comments above for
 * full details of the mbverifychar API.)
 */
int
pg_encoding_verifymbchar(int encoding, const char *mbstr, int len)
{
	return (PG_VALID_ENCODING(encoding) ?
			pg_wchar_table[encoding].mbverifychar((const unsigned char *) mbstr, len) :
			pg_wchar_table[PG_SQL_ASCII].mbverifychar((const unsigned char *) mbstr, len));
}

/*
 * Verify that a string is valid for the given encoding.
 * Returns the number of input bytes (<= len) that form a valid string.
 * (See comments above for full details of the mbverifystr API.)
 */
int
pg_encoding_verifymbstr(int encoding, const char *mbstr, int len)
{
	return (PG_VALID_ENCODING(encoding) ?
			pg_wchar_table[encoding].mbverifystr((const unsigned char *) mbstr, len) :
			pg_wchar_table[PG_SQL_ASCII].mbverifystr((const unsigned char *) mbstr, len));
}

/*
 * fetch maximum length of a given encoding
 */
int
pg_encoding_max_length(int encoding)
{
	Assert(PG_VALID_ENCODING(encoding));

	return pg_wchar_table[encoding].maxmblen;
}

#ifndef FRONTEND

/*
 * fetch maximum length of the encoding for the current database
 */
int
pg_database_encoding_max_length(void)
{
	return pg_wchar_table[GetDatabaseEncoding()].maxmblen;
}

/*
 * get the character incrementer for the encoding for the current database
 */
mbcharacter_incrementer
pg_database_encoding_character_incrementer(void)
{
	/*
	 * Eventually it might be best to add a field to pg_wchar_table[], but for
	 * now we just use a switch.
	 */
	switch (GetDatabaseEncoding())
	{
		case PG_UTF8:
			return pg_utf8_increment;

		case PG_EUC_JP:
			return pg_eucjp_increment;

		default:
			return pg_generic_charinc;
	}
}

/*
 * Verify mbstr to make sure that it is validly encoded in the current
 * database encoding.  Otherwise same as pg_verify_mbstr().
 */
bool
pg_verifymbstr(const char *mbstr, int len, bool noError)
{
	return
		pg_verify_mbstr_len(GetDatabaseEncoding(), mbstr, len, noError) >= 0;
}

/*
 * Verify mbstr to make sure that it is validly encoded in the specified
 * encoding.
 */
bool
pg_verify_mbstr(int encoding, const char *mbstr, int len, bool noError)
{
	return pg_verify_mbstr_len(encoding, mbstr, len, noError) >= 0;
}

/*
 * Convert a single Unicode code point into a string in the server encoding.
 *
 * The code point given by "c" is converted and stored at *s, which must
 * have at least MAX_UNICODE_EQUIVALENT_STRING+1 bytes available.
 * The output will have a trailing '\0'.  Throws error if the conversion
 * cannot be performed.
 *
 * Note that this relies on having previously looked up any required
 * conversion function.  That's partly for speed but mostly because the parser
 * may call this outside any transaction, or in an aborted transaction.
 */
void
pg_unicode_to_server(pg_wchar c, unsigned char *s)
{
#ifdef NOT_USED
	unsigned char c_as_utf8[MAX_MULTIBYTE_CHAR_LEN + 1];
	int			c_as_utf8_len;
	int			server_encoding;

	/*
	 * Complain if invalid Unicode code point.  The choice of errcode here is
	 * debatable, but really our caller should have checked this anyway.
	 */
	if (!is_valid_unicode_codepoint(c))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("invalid Unicode code point")));

	/* Otherwise, if it's in ASCII range, conversion is trivial */
	if (c <= 0x7F)
	{
		s[0] = (unsigned char) c;
		s[1] = '\0';
		return;
	}

	/* If the server encoding is UTF-8, we just need to reformat the code */
	server_encoding = GetDatabaseEncoding();
	if (server_encoding == PG_UTF8)
	{
		unicode_to_utf8(c, s);
		s[pg_utf_mblen(s)] = '\0';
		return;
	}

	/* For all other cases, we must have a conversion function available */
	if (Utf8ToServerConvProc == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("conversion between %s and %s is not supported",
						pg_enc2name_tbl[PG_UTF8].name,
						GetDatabaseEncodingName())));

	/* Construct UTF-8 source string */
	unicode_to_utf8(c, c_as_utf8);
	c_as_utf8_len = pg_utf_mblen(c_as_utf8);
	c_as_utf8[c_as_utf8_len] = '\0';

	/* Convert, or throw error if we can't */
	FunctionCall5(Utf8ToServerConvProc,
				  Int32GetDatum(PG_UTF8),
				  Int32GetDatum(server_encoding),
				  CStringGetDatum(c_as_utf8),
				  CStringGetDatum(s),
				  Int32GetDatum(c_as_utf8_len));
#endif
}

/*
 * Verify mbstr to make sure that it is validly encoded in the specified
 * encoding.
 *
 * mbstr is not necessarily zero terminated; length of mbstr is
 * specified by len.
 *
 * If OK, return length of string in the encoding.
 * If a problem is found, return -1 when noError is
 * true; when noError is false, ereport() a descriptive message.
 */
int
pg_verify_mbstr_len(int encoding, const char *mbstr, int len, bool noError)
{
	mbchar_verifier	mbverify;
	int			mb_len;

	Assert(PG_VALID_ENCODING(encoding));

	/*
	 * In single-byte encodings, we need only reject nulls (\0).
	 */
	if (pg_encoding_max_length(encoding) <= 1)
	{
		const char *nullpos = memchr(mbstr, 0, len);

		if (nullpos == NULL)
			return len;
		if (noError)
			return -1;
		report_invalid_encoding(encoding, nullpos, 1);
	}

	/* fetch function pointer just once */
	mbverify = pg_wchar_table[encoding].mbverifychar;

	mb_len = 0;

	while (len > 0)
	{
		int			l;

		/* fast path for ASCII-subset characters */
		if (!IS_HIGHBIT_SET(*mbstr))
		{
			if (*mbstr != '\0')
			{
				mb_len++;
				mbstr++;
				len--;
				continue;
			}
			if (noError)
				return -1;
			report_invalid_encoding(encoding, mbstr, len);
		}

		l = (*mbverify) ((const unsigned char *) mbstr, len);

		if (l < 0)
		{
			if (noError)
				return -1;
			report_invalid_encoding(encoding, mbstr, len);
		}

		mbstr += l;
		len -= l;
		mb_len++;
	}
	return mb_len;
}

/*
 * check_encoding_conversion_args: check arguments of a conversion function
 *
 * "expected" arguments can be either an encoding ID or -1 to indicate that
 * the caller will check whether it accepts the ID.
 *
 * Note: the errors here are not really user-facing, so elog instead of
 * ereport seems sufficient.  Also, we trust that the "expected" encoding
 * arguments are valid encoding IDs, but we don't trust the actuals.
 */
void
check_encoding_conversion_args(int src_encoding,
							   int dest_encoding,
							   int len,
							   int expected_src_encoding,
							   int expected_dest_encoding)
{
	if (!PG_VALID_ENCODING(src_encoding))
		elog(ERROR, "invalid source encoding ID: %d", src_encoding);
	if (src_encoding != expected_src_encoding && expected_src_encoding >= 0)
		elog(ERROR, "expected source encoding \"%s\", but got \"%s\"",
			 pg_enc2name_tbl[expected_src_encoding].name,
			 pg_enc2name_tbl[src_encoding].name);
	if (!PG_VALID_ENCODING(dest_encoding))
		elog(ERROR, "invalid destination encoding ID: %d", dest_encoding);
	if (dest_encoding != expected_dest_encoding && expected_dest_encoding >= 0)
		elog(ERROR, "expected destination encoding \"%s\", but got \"%s\"",
			 pg_enc2name_tbl[expected_dest_encoding].name,
			 pg_enc2name_tbl[dest_encoding].name);
	if (len < 0)
		elog(ERROR, "encoding conversion length must not be negative");
}

/*
 * report_invalid_encoding: complain about invalid multibyte character
 *
 * note: len is remaining length of string, not length of character;
 * len must be greater than zero, as we always examine the first byte.
 */
void
report_invalid_encoding(int encoding, const char *mbstr, int len)
{
	int			l = pg_encoding_mblen(encoding, mbstr);
	char		buf[8 * 5 + 1];
	char	   *p = buf;
	int			j,
				jlimit;

	jlimit = Min(l, len);
	jlimit = Min(jlimit, 8);	/* prevent buffer overrun */

	for (j = 0; j < jlimit; j++)
	{
		p += sprintf(p, "0x%02x", (unsigned char) mbstr[j]);
		if (j < jlimit - 1)
			p += sprintf(p, " ");
	}

	ereport(ERROR,
			(errcode(ERRCODE_CHARACTER_NOT_IN_REPERTOIRE),
			 errmsg("invalid byte sequence for encoding \"%s\": %s",
					pg_enc2name_tbl[encoding].name,
					buf)));
}

/*
 * report_untranslatable_char: complain about untranslatable character
 *
 * note: len is remaining length of string, not length of character;
 * len must be greater than zero, as we always examine the first byte.
 */
void
report_untranslatable_char(int src_encoding, int dest_encoding,
						   const char *mbstr, int len)
{
	int			l = pg_encoding_mblen(src_encoding, mbstr);
	char		buf[8 * 5 + 1];
	char	   *p = buf;
	int			j,
				jlimit;

	jlimit = Min(l, len);
	jlimit = Min(jlimit, 8);	/* prevent buffer overrun */

	for (j = 0; j < jlimit; j++)
	{
		p += sprintf(p, "0x%02x", (unsigned char) mbstr[j]);
		if (j < jlimit - 1)
			p += sprintf(p, " ");
	}

	ereport(ERROR,
			(errcode(ERRCODE_UNTRANSLATABLE_CHARACTER),
			 errmsg("character with byte sequence %s in encoding \"%s\" has no equivalent in encoding \"%s\"",
					buf,
					pg_enc2name_tbl[src_encoding].name,
					pg_enc2name_tbl[dest_encoding].name)));
}

#endif							/* !FRONTEND */
