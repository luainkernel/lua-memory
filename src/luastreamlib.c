/*
** $Id$
** NOTE: most of the code in here is copied from the source of Lua 5.3.1 by
**       R. Ierusalimschy, L. H. de Figueiredo, W. Celes - Lua.org, PUC-Rio.
**
** Stream support for the Lua language
** See Copyright Notice in luastreamaux.h
*/

#define luastreamlib_c

#include "luastreamaux.h"

#include <string.h>
#include <ctype.h>



#if !defined(lua_assert)
#define lua_assert(x)   ((void)0)
#endif



/*
** maximum number of captures that a pattern can do during
** pattern-matching. This limit is arbitrary.
*/
#if !defined(LUA_MAXCAPTURES)
#define LUA_MAXCAPTURES		32
#endif


/* macro to 'unsign' a character */
#define uchar(c)	((unsigned char)(c))


/*
** Some sizes are better limited to fit in 'int', but must also fit in
** 'size_t'. (We assume that 'lua_Integer' cannot be smaller than 'int'.)
*/
#define MAXSIZE  \
	(sizeof(size_t) < sizeof(int) ? (~(size_t)0) : (size_t)(INT_MAX))




static int str_len (lua_State *L) {
	size_t l;
	luastream_checkstream(L, 1, &l);
	lua_pushinteger(L, (lua_Integer)l);
	return 1;
}


static int buf_len (lua_State *L) {
	size_t l;
	luastream_checkbuffer(L, 1, &l);
	lua_pushinteger(L, (lua_Integer)l);
	return 1;
}


/* translate a relative string position: negative means back from end */
static lua_Integer posrelat (lua_Integer pos, size_t len) {
	if (pos >= 0) return pos;
	else if (0u - (size_t)pos > len) return 0;
	else return (lua_Integer)len + pos + 1;
}


static int str_newbuf (lua_State *L) {
	char *p;
	size_t l;
	const char *s = NULL;
	if (lua_type(L, 1) == LUA_TNUMBER) {
		l = luaL_checkinteger(L, 1);
	} else {
		lua_Integer i, j;
		s = luastream_checkstream(L, 1, &l);
		i = posrelat(luaL_optinteger(L, 2, 1), l);
		j = posrelat(luaL_optinteger(L, 3, -1), l);
		if (i < 1) i = 1;
		if (j > (lua_Integer)l) j = l;
		s += i-1;
		l = 1+j-i;
	}
	p = luastream_newbuffer(L, l);
	if (s) memcpy(p, s, l * sizeof(char));
	return 1;
}



static int str_isbuffer (lua_State *L) {
	lua_pushboolean(L, luastream_isbuffer(L, 1));
	return 1;
}


static int str_diff (lua_State *L) {
	size_t l1, l2;
	const char *s1 = luastream_checkstream(L, 1, &l1);
	const char *s2 = luastream_checkstream(L, 2, &l2);
	size_t i, n=(l1<l2 ? l1 : l2);
	for (i=0; (i<n) && (s1[i]==s2[i]); ++i);
	if (i<n) {
		lua_pushinteger(L, i+1);
		lua_pushboolean(L, s1[i]<s2[i]);
	} else if (l1==l2) {
		lua_pushnil(L);
		lua_pushboolean(L, 0);
	} else {
		lua_pushinteger(L, i+1);
		lua_pushboolean(L, l1<l2);
	}
	return 2;
}


static int str_tostring (lua_State *L) {
	size_t l;
	const char *s = luastream_checkstream(L, 1, &l);
	lua_Integer start = posrelat(luaL_optinteger(L, 2, 1), l);
	lua_Integer end = posrelat(luaL_optinteger(L, 3, -1), l);
	if (start < 1) start = 1;
	if (end > (lua_Integer)l) end = l;
	if (start == 1 && end == l && lua_type(L, 1) == LUA_TSTRING)
		lua_settop(L, 1);
	else if (start <= end)
		lua_pushlstring(L, s + start - 1, (size_t)(end - start + 1));
	else lua_pushliteral(L, "");
	return 1;
}


static int buf_tostring (lua_State *L) {
	size_t l;
	const char *s = luastream_checkbuffer(L, 1, &l);
	if (l>0) lua_pushlstring(L, s, l);
	else lua_pushliteral(L, "");
	return 1;
}


static const char *const outops[] = {"string", "buffer", NULL};
static const char *const inoutops[] = {"string", "buffer", "inplace", NULL};


static void copyreverse (char *p, const char *s, size_t l) {
	size_t i;
	for (i=0; i<l; ++i)
		p[i] = s[l - i - 1];
}


static int str_reverse (lua_State *L) {
	size_t l;
	int op = luaL_checkoption(L, 1, NULL, inoutops);
	if (op == 2) { /* inplace */
		char *p = luastream_checkbuffer(L, 2, &l);
		size_t i;
		for (i = 0; i <= l/2; ++i) {
			size_t j = l - i - 1;
			const char t = p[i];
			p[i] = p[j];
			p[j] = t;
		}
	} else {
		const char *s = luastream_checkstream(L, 2, &l);
		if (op == 0) { /* string */
			luastream_Buffer b;
			char *p = luastream_buffinitsize(L, &b, l);
			copyreverse(p, s, l);
			luastream_pushresultsize(&b, l);
		} else { /* buffer */
			char *p = luastream_newbuffer(L, l);
			copyreverse(p, s, l);
		}
	}
	return 1;
}


static void copylower (char *p, const char *s, size_t l) {
	size_t i;
	for (i=0; i<l; ++i)
		p[i] = tolower(uchar(s[i]));
}


static int str_lower (lua_State *L) {
	size_t l;
	int op = luaL_checkoption(L, 1, NULL, inoutops);
	const char *s = luastream_checkstream(L, 2, &l);
	if (op == 0) { /* string */
		luastream_Buffer b;
		char *p = luastream_buffinitsize(L, &b, l);
		copylower(p, s, l);
		luastream_pushresultsize(&b, l);
	} else {
		char *p = (op == 1) ? luastream_newbuffer(L, l)
		                    : luastream_checkbuffer(L, 2, NULL);
		copylower(p, s, l);
	}
	return 1;
}


static void copyupper (char *p, const char *s, size_t l) {
	size_t i;
	for (i=0; i<l; ++i)
		p[i] = toupper(uchar(s[i]));
}


static int str_upper (lua_State *L) {
	size_t l;
	int op = luaL_checkoption(L, 1, NULL, inoutops);
	const char *s = luastream_checkstream(L, 2, &l);
	if (op == 0) { /* string */
		luastream_Buffer b;
		char *p = luastream_buffinitsize(L, &b, l);
		copyupper(p, s, l);
		luastream_pushresultsize(&b, l);
	} else {
		char *p = (op == 1) ? luastream_newbuffer(L, l)
		                    : luastream_checkbuffer(L, 2, NULL);
		copyupper(p, s, l);
	}
	return 1;
}


static void copyrepeat (char *p,
                        const char *s,
                        size_t l,
                        const char *sep,
                        size_t lsep,
                        lua_Integer n) {
	while (n-- > 1) {  /* first n-1 copies (followed by separator) */
		memcpy(p, s, l * sizeof(char)); p += l;
		if (lsep > 0) {  /* empty 'memcpy' is not that cheap */
			memcpy(p, sep, lsep * sizeof(char));
			p += lsep;
		}
	}
	memcpy(p, s, l * sizeof(char));  /* last copy (not followed by separator) */
}


static int str_rep (lua_State *L) {
	size_t l, lsep;
	int op = luaL_checkoption(L, 1, NULL, outops);
	const char *s = luastream_checkstream(L, 2, &l);
	lua_Integer n = luaL_checkinteger(L, 3);
	const char *sep = luaL_optlstring(L, 4, "", &lsep);
	if (n <= 0) {
		if (op == 0) lua_pushliteral(L, "");
		else luastream_newbuffer(L, 0);
	} else if (l + lsep < l || l + lsep > MAXSIZE / n)  /* may overflow? */
		return luaL_error(L, "resulting string too large");
	else {
		size_t totallen = (size_t)n * l + (size_t)(n - 1) * lsep;
		if (op == 0) {  /* string */
			luastream_Buffer b;
			char *p = luastream_buffinitsize(L, &b, totallen);
			copyrepeat(p, s, l, sep, lsep, n);
			luastream_pushresultsize(&b, totallen);
		} else {
			char *p = luastream_newbuffer(L, totallen);
			copyrepeat(p, s, l, sep, lsep, n);
		}
	}
	return 1;
}


static int str2byte (lua_State *L, const char *s, size_t l) {
	lua_Integer posi = posrelat(luaL_optinteger(L, 2, 1), l);
	lua_Integer pose = posrelat(luaL_optinteger(L, 3, posi), l);
	int n, i;
	if (posi < 1) posi = 1;
	if (pose > (lua_Integer)l) pose = l;
	if (posi > pose) return 0;  /* empty interval; return no values */
	n = (int)(pose -  posi + 1);
	if (posi + n <= pose)  /* arithmetic overflow? */
		return luaL_error(L, "string slice too long");
	luaL_checkstack(L, n, "string slice too long");
	for (i=0; i<n; i++)
		lua_pushinteger(L, uchar(s[posi+i-1]));
	return n;
}


static int str_byte (lua_State *L) {
	size_t l;
	const char *s = luastream_checkstream(L, 1, &l);
	return str2byte(L, s, l);
}


static int buf_get (lua_State *L) {
	size_t l;
	const char *s = luastream_checkbuffer(L, 1, &l);
	return str2byte(L, s, l);
}


static void code2char (lua_State *L, int idx, char *p, int n) {
	int i;
	for (i=0; i<n; ++i, ++idx) {
		lua_Integer c = luaL_checkinteger(L, idx);
		luaL_argcheck(L, uchar(c) == c, idx, "value out of range");
		p[i] = uchar(c);
	}
}


static int str_char (lua_State *L) {
	int n = lua_gettop(L)-1;  /* number of bytes */
	int op = luaL_checkoption(L, 1, NULL, outops);
	if (op == 0) {
		luastream_Buffer b;
		char *p = luastream_buffinitsize(L, &b, n);
		code2char(L, 2, p, n);
		luastream_pushresultsize(&b, n);
	} else {
		char *p = luastream_newbuffer(L, n);
		code2char(L, 2, p, n);
	}
	return 1;
}


static int buf_set (lua_State *L) {
	size_t l;
	int n = lua_gettop(L)-2;  /* number of bytes */
	char *p = luastream_checkbuffer(L, 1, &l);
	lua_Integer i = posrelat(luaL_checkinteger(L, 2), l);
	luaL_argcheck(L, i>0 && i<=(lua_Integer)l, 2, "index out of bounds");
	l = 1+l-i;
	code2char(L, 3, p+i-1, n<l ? n : l);
	return 0;
}


static int buf_fill (lua_State *L) {
	size_t l, sl;
	char *p = luastream_checkbuffer(L, 1, &l);
	const char *s = luastream_checkstream(L, 2, &sl);
	lua_Integer i = posrelat(luaL_optinteger(L, 3, 1), l);
	lua_Integer j = posrelat(luaL_optinteger(L, 4, -1), l);
	lua_Integer os = posrelat(luaL_optinteger(L, 5, 1), sl);
	luaL_argcheck(L, i>0 && i<=(lua_Integer)l, 3, "index out of bounds");
	luaL_argcheck(L, j>0 && j<=(lua_Integer)l, 4, "index out of bounds");
	if (os < 1) os = 1;
	if (os <= (lua_Integer)sl) {
		--os;
		s += os;
		sl -= os;
		while (i <= j) {
			lua_Integer f = 1+j-i;
			lua_Integer n = f<sl ? f : sl;
			memcpy(p+i-1, s, n * sizeof(char));
			i += n;
		}
	}
	return 0;
}


static int writer (lua_State *L, const void *b, size_t size, void *B) {
	(void)L;
	luastream_addstream((luastream_Buffer *) B, (const char *)b, size);
	return 0;
}


static int str_dump (lua_State *L) {
	luastream_Buffer b;
	int strip = lua_toboolean(L, 3);
	int op = luaL_checkoption(L, 1, NULL, outops);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	lua_settop(L, 1);
	luastream_buffinit(L,&b);
	if (lua_dump(L, writer, &b, strip) != 0)
		return luaL_error(L, "unable to dump given function");
	if (op == 0) luastream_pushresult(&b);
	else luastream_pushresbuf(&b);
	return 1;
}



/*
** {======================================================
** PATTERN MATCHING
** =======================================================
*/


#define CAP_UNFINISHED	(-1)
#define CAP_POSITION	(-2)


typedef struct MatchState {
	int matchdepth;  /* control for recursive depth (to avoid C stack overflow) */
	const char *src_init;  /* init of source string */
	const char *src_end;  /* end ('\0') of source string */
	const char *p_end;  /* end ('\0') of pattern */
	lua_State *L;
	int level;  /* total number of captures (finished or unfinished) */
	struct {
		const char *init;
		ptrdiff_t len;
	} capture[LUA_MAXCAPTURES];
} MatchState;


/* recursive function */
static const char *match (MatchState *ms, const char *s, const char *p);


/* maximum recursion depth for 'match' */
#if !defined(MAXCCALLS)
#define MAXCCALLS	200
#endif


#define L_ESC		'%'
#define SPECIALS	"^$*+?.([%-"


static int check_capture (MatchState *ms, int l) {
	l -= '1';
	if (l < 0 || l >= ms->level || ms->capture[l].len == CAP_UNFINISHED)
		return luaL_error(ms->L, "invalid capture index %%%d", l + 1);
	return l;
}


static int capture_to_close (MatchState *ms) {
	int level = ms->level;
	for (level--; level>=0; level--)
		if (ms->capture[level].len == CAP_UNFINISHED) return level;
	return luaL_error(ms->L, "invalid pattern capture");
}


static const char *classend (MatchState *ms, const char *p) {
	switch (*p++) {
		case L_ESC: {
			if (p == ms->p_end)
				luaL_error(ms->L, "malformed pattern (ends with '%%')");
			return p+1;
		}
		case '[': {
			if (*p == '^') p++;
			do {  /* look for a ']' */
				if (p == ms->p_end)
					luaL_error(ms->L, "malformed pattern (missing ']')");
				if (*(p++) == L_ESC && p < ms->p_end)
					p++;  /* skip escapes (e.g. '%]') */
			} while (*p != ']');
			return p+1;
		}
		default: {
			return p;
		}
	}
}


static int match_class (int c, int cl) {
	int res;
	switch (tolower(cl)) {
		case 'a' : res = isalpha(c); break;
		case 'c' : res = iscntrl(c); break;
		case 'd' : res = isdigit(c); break;
		case 'g' : res = isgraph(c); break;
		case 'l' : res = islower(c); break;
		case 'p' : res = ispunct(c); break;
		case 's' : res = isspace(c); break;
		case 'u' : res = isupper(c); break;
		case 'w' : res = isalnum(c); break;
		case 'x' : res = isxdigit(c); break;
		case 'z' : res = (c == 0); break;  /* deprecated option */
		default: return (cl == c);
	}
	return (islower(cl) ? res : !res);
}


static int matchbracketclass (int c, const char *p, const char *ec) {
	int sig = 1;
	if (*(p+1) == '^') {
		sig = 0;
		p++;  /* skip the '^' */
	}
	while (++p < ec) {
		if (*p == L_ESC) {
			p++;
			if (match_class(c, uchar(*p)))
				return sig;
		}
		else if ((*(p+1) == '-') && (p+2 < ec)) {
			p+=2;
			if (uchar(*(p-2)) <= c && c <= uchar(*p))
				return sig;
		}
		else if (uchar(*p) == c) return sig;
	}
	return !sig;
}


static int singlematch (MatchState *ms, const char *s, const char *p,
                        const char *ep) {
	if (s >= ms->src_end)
		return 0;
	else {
		int c = uchar(*s);
		switch (*p) {
			case '.': return 1;  /* matches any char */
			case L_ESC: return match_class(c, uchar(*(p+1)));
			case '[': return matchbracketclass(c, p, ep-1);
			default:  return (uchar(*p) == c);
		}
	}
}


static const char *matchbalance (MatchState *ms, const char *s,
                                 const char *p) {
	if (p >= ms->p_end - 1)
		luaL_error(ms->L, "malformed pattern (missing arguments to '%%b')");
	if (*s != *p) return NULL;
	else {
		int b = *p;
		int e = *(p+1);
		int cont = 1;
		while (++s < ms->src_end) {
			if (*s == e) {
				if (--cont == 0) return s+1;
			}
			else if (*s == b) cont++;
		}
	}
	return NULL;  /* string ends out of balance */
}


static const char *max_expand (MatchState *ms, const char *s,
                               const char *p, const char *ep) {
	ptrdiff_t i = 0;  /* counts maximum expand for item */
	while (singlematch(ms, s + i, p, ep))
		i++;
	/* keeps trying to match with the maximum repetitions */
	while (i>=0) {
		const char *res = match(ms, (s+i), ep+1);
		if (res) return res;
		i--;  /* else didn't match; reduce 1 repetition to try again */
	}
	return NULL;
}


static const char *min_expand (MatchState *ms, const char *s,
                               const char *p, const char *ep) {
	for (;;) {
		const char *res = match(ms, s, ep+1);
		if (res != NULL)
			return res;
		else if (singlematch(ms, s, p, ep))
			s++;  /* try with one more repetition */
		else return NULL;
	}
}


static const char *start_capture (MatchState *ms, const char *s,
                                  const char *p, int what) {
	const char *res;
	int level = ms->level;
	if (level >= LUA_MAXCAPTURES) luaL_error(ms->L, "too many captures");
	ms->capture[level].init = s;
	ms->capture[level].len = what;
	ms->level = level+1;
	if ((res=match(ms, s, p)) == NULL)  /* match failed? */
		ms->level--;  /* undo capture */
	return res;
}


static const char *end_capture (MatchState *ms, const char *s,
                                const char *p) {
	int l = capture_to_close(ms);
	const char *res;
	ms->capture[l].len = s - ms->capture[l].init;  /* close capture */
	if ((res = match(ms, s, p)) == NULL)  /* match failed? */
		ms->capture[l].len = CAP_UNFINISHED;  /* undo capture */
	return res;
}


static const char *match_capture (MatchState *ms, const char *s, int l) {
	size_t len;
	l = check_capture(ms, l);
	len = ms->capture[l].len;
	if ((size_t)(ms->src_end-s) >= len &&
	    memcmp(ms->capture[l].init, s, len) == 0)
		return s+len;
	else return NULL;
}


static const char *match (MatchState *ms, const char *s, const char *p) {
	if (ms->matchdepth-- == 0)
		luaL_error(ms->L, "pattern too complex");
	init: /* using goto's to optimize tail recursion */
	if (p != ms->p_end) {  /* end of pattern? */
		switch (*p) {
			case '(': {  /* start capture */
				if (*(p + 1) == ')')  /* position capture? */
					s = start_capture(ms, s, p + 2, CAP_POSITION);
				else
					s = start_capture(ms, s, p + 1, CAP_UNFINISHED);
				break;
			}
			case ')': {  /* end capture */
				s = end_capture(ms, s, p + 1);
				break;
			}
			case '$': {
				if ((p + 1) != ms->p_end)  /* is the '$' the last char in pattern? */
					goto dflt;  /* no; go to default */
				s = (s == ms->src_end) ? s : NULL;  /* check end of string */
				break;
			}
			case L_ESC: {  /* escaped sequences not in the format class[*+?-]? */
				switch (*(p + 1)) {
					case 'b': {  /* balanced string? */
						s = matchbalance(ms, s, p + 2);
						if (s != NULL) {
							p += 4; goto init;  /* return match(ms, s, p + 4); */
						}  /* else fail (s == NULL) */
						break;
					}
					case 'f': {  /* frontier? */
						const char *ep; char previous;
						p += 2;
						if (*p != '[')
							luaL_error(ms->L, "missing '[' after '%%f' in pattern");
						ep = classend(ms, p);  /* points to what is next */
						previous = (s == ms->src_init) ? '\0' : *(s - 1);
						if (!matchbracketclass(uchar(previous), p, ep - 1) &&
						    matchbracketclass(uchar(*s), p, ep - 1)) {
							p = ep; goto init;  /* return match(ms, s, ep); */
						}
						s = NULL;  /* match failed */
						break;
					}
					case '0': case '1': case '2': case '3':
					case '4': case '5': case '6': case '7':
					case '8': case '9': {  /* capture results (%0-%9)? */
						s = match_capture(ms, s, uchar(*(p + 1)));
						if (s != NULL) {
							p += 2; goto init;  /* return match(ms, s, p + 2) */
						}
						break;
					}
					default: goto dflt;
				}
				break;
			}
			default: dflt: {  /* pattern class plus optional suffix */
				const char *ep = classend(ms, p);  /* points to optional suffix */
				/* does not match at least once? */
				if (!singlematch(ms, s, p, ep)) {
					if (*ep == '*' || *ep == '?' || *ep == '-') {  /* accept empty? */
						p = ep + 1; goto init;  /* return match(ms, s, ep + 1); */
					}
					else  /* '+' or no suffix */
						s = NULL;  /* fail */
				}
				else {  /* matched once */
					switch (*ep) {  /* handle optional suffix */
						case '?': {  /* optional */
							const char *res;
							if ((res = match(ms, s + 1, ep + 1)) != NULL)
								s = res;
							else {
								p = ep + 1; goto init;  /* else return match(ms, s, ep + 1); */
							}
							break;
						}
						case '+':  /* 1 or more repetitions */
							s++;  /* 1 match already done */
							/* go through */
						case '*':  /* 0 or more repetitions */
							s = max_expand(ms, s, p, ep);
							break;
						case '-':  /* 0 or more repetitions (minimum) */
							s = min_expand(ms, s, p, ep);
							break;
						default:  /* no suffix */
							s++; p = ep; goto init;  /* return match(ms, s + 1, ep); */
					}
				}
				break;
			}
		}
	}
	ms->matchdepth++;
	return s;
}



static const char *lmemfind (const char *s1, size_t l1,
                             const char *s2, size_t l2) {
	if (l2 == 0) return s1;  /* empty strings are everywhere */
	else if (l2 > l1) return NULL;  /* avoids a negative 'l1' */
	else {
		const char *init;  /* to search for a '*s2' inside 's1' */
		l2--;  /* 1st char will be checked by 'memchr' */
		l1 = l1-l2;  /* 's2' cannot be found after that */
		while (l1 > 0 && (init = (const char *)memchr(s1, *s2, l1)) != NULL) {
			init++;   /* 1st char is already checked */
			if (memcmp(init, s2+1, l2) == 0)
				return init-1;
			else {  /* correct 'l1' and 's1' to try again */
				l1 -= init-s1;
				s1 = init;
			}
		}
		return NULL;  /* not found */
	}
}


static void push_onecapture (MatchState *ms, int i, const char *s,
                                                    const char *e) {
	if (i >= ms->level) {
		if (i == 0)  /* ms->level == 0, too */
			lua_pushlstring(ms->L, s, e - s);  /* add whole match */
		else
			luaL_error(ms->L, "invalid capture index %%%d", i + 1);
	}
	else {
		ptrdiff_t l = ms->capture[i].len;
		if (l == CAP_UNFINISHED) luaL_error(ms->L, "unfinished capture");
		if (l == CAP_POSITION)
			lua_pushinteger(ms->L, ms->capture[i].init - ms->src_init + 1);
		else
			lua_pushlstring(ms->L, ms->capture[i].init, l);
	}
}


static int push_captures (MatchState *ms, const char *s, const char *e) {
	int i;
	int nlevels = (ms->level == 0 && s) ? 1 : ms->level;
	luaL_checkstack(ms->L, nlevels, "too many captures");
	for (i = 0; i < nlevels; i++)
		push_onecapture(ms, i, s, e);
	return nlevels;  /* number of strings pushed */
}


/* check whether pattern has no special characters */
static int nospecials (const char *p, size_t l) {
	size_t upto = 0;
	do {
		if (strpbrk(p + upto, SPECIALS))
			return 0;  /* pattern has a special character */
		upto += strlen(p + upto) + 1;  /* may have more after \0 */
	} while (upto <= l);
	return 1;  /* no special chars found */
}


static int str_find_aux (lua_State *L, int find) {
	size_t ls, lp;
	const char *s = luastream_checkstream(L, 1, &ls);
	const char *p = luastream_checkstream(L, 2, &lp);
	lua_Integer init = posrelat(luaL_optinteger(L, 3, 1), ls);
	if (init < 1) init = 1;
	else if (init > (lua_Integer)ls + 1) {  /* start after string's end? */
		lua_pushnil(L);  /* cannot find anything */
		return 1;
	}
	/* explicit request or no special characters? */
	if (find && (lua_toboolean(L, 4) || nospecials(p, lp))) {
		/* do a plain search */
		const char *s2 = lmemfind(s + init - 1, ls - (size_t)init + 1, p, lp);
		if (s2) {
			lua_pushinteger(L, s2 - s + 1);
			lua_pushinteger(L, s2 - s + lp);
			return 2;
		}
	}
	else {
		MatchState ms;
		const char *s1 = s + init - 1;
		int anchor = (*p == '^');
		if (anchor) {
			p++; lp--;  /* skip anchor character */
		}
		ms.L = L;
		ms.matchdepth = MAXCCALLS;
		ms.src_init = s;
		ms.src_end = s + ls;
		ms.p_end = p + lp;
		do {
			const char *res;
			ms.level = 0;
			lua_assert(ms.matchdepth == MAXCCALLS);
			if ((res=match(&ms, s1, p)) != NULL) {
				if (find) {
					lua_pushinteger(L, s1 - s + 1);  /* start */
					lua_pushinteger(L, res - s);   /* end */
					return push_captures(&ms, NULL, 0) + 2;
				}
				else
					return push_captures(&ms, s1, res);
			}
		} while (s1++ < ms.src_end && !anchor);
	}
	lua_pushnil(L);  /* not found */
	return 1;
}


static int str_find (lua_State *L) {
	return str_find_aux(L, 1);
}


static int str_match (lua_State *L) {
	return str_find_aux(L, 0);
}


static int gmatch_aux (lua_State *L) {
	MatchState ms;
	size_t ls, lp;
	const char *s = luastream_tostream(L, lua_upvalueindex(1), &ls);
	const char *p = luastream_tostream(L, lua_upvalueindex(2), &lp);
	const char *src;
	ms.L = L;
	ms.matchdepth = MAXCCALLS;
	ms.src_init = s;
	ms.src_end = s+ls;
	ms.p_end = p + lp;
	for (src = s + (size_t)lua_tointeger(L, lua_upvalueindex(3));
	     src <= ms.src_end;
	     src++) {
		const char *e;
		ms.level = 0;
		lua_assert(ms.matchdepth == MAXCCALLS);
		if ((e = match(&ms, src, p)) != NULL) {
			lua_Integer newstart = e-s;
			if (e == src) newstart++;  /* empty match? go at least one position */
			lua_pushinteger(L, newstart);
			lua_replace(L, lua_upvalueindex(3));
			return push_captures(&ms, src, e);
		}
	}
	return 0;  /* not found */
}


static int str_gmatch (lua_State *L) {
	luastream_checkstream(L, 1, NULL);
	luastream_checkstream(L, 2, NULL);
	lua_settop(L, 2);
	lua_pushinteger(L, 0);
	lua_pushcclosure(L, gmatch_aux, 3);
	return 1;
}


static void add_s (MatchState *ms, luastream_Buffer *b, const char *s,
                                                        const char *e) {
	size_t l, i;
	lua_State *L = ms->L;
	const char *news = luastream_tostream(L, 3, &l);
	for (i = 0; i < l; i++) {
		if (news[i] != L_ESC)
			luastream_addchar(b, news[i]);
		else {
			i++;  /* skip ESC */
			if (!isdigit(uchar(news[i]))) {
				if (news[i] != L_ESC)
					luaL_error(L, "invalid use of '%c' in replacement string", L_ESC);
				luastream_addchar(b, news[i]);
			}
			else if (news[i] == '0')
				luastream_addstream(b, s, e - s);
			else {
				push_onecapture(ms, news[i] - '1', s, e);
				luaL_tolstring(L, -1, NULL);  /* if number, convert it to string */
				lua_remove(L, -2);  /* remove original value */
				luaL_addvalue(b);  /* add capture to accumulated result */
			}
		}
	}
}


static void add_value (MatchState *ms,
                       luastream_Buffer *b,
                       const char *s,
                       const char *e,
                       int tr) {
	lua_State *L = ms->L;
	switch (tr) {
		case LUA_TFUNCTION: {
			int n;
			lua_pushvalue(L, 3);
			n = push_captures(ms, s, e);
			lua_call(L, n, 1);
			break;
		}
		case LUA_TTABLE: {
			push_onecapture(ms, 0, s, e);
			lua_gettable(L, 3);
			break;
		}
		default: {  /* LUA_TNUMBER or LUA_TSTRING */
			add_s(ms, b, s, e);
			return;
		}
	}
	if (!lua_toboolean(L, -1)) {  /* nil or false? */
		lua_pop(L, 1);
		lua_pushlstring(L, s, e - s);  /* keep original text */
	}
	else if (!lua_isstring(L, -1))
		luaL_error(L, "invalid replacement value (a %s)", luaL_typename(L, -1));
	luaL_addvalue(b);  /* add result to accumulator */
}


static int str_gsub (lua_State *L) {
	size_t srcl, lp;
	int op = luaL_checkoption(L, 1, NULL, outops);
	const char *src = luastream_checkstream(L, 2, &srcl);
	const char *p = luastream_checkstream(L, 3, &lp);
	int tr = lua_type(L, 4);
	lua_Integer max_s = luaL_optinteger(L, 5, srcl + 1);
	int anchor = (*p == '^');
	lua_Integer n = 0;
	MatchState ms;
	luastream_Buffer b;
	luaL_argcheck(L, tr == LUA_TNUMBER || tr == LUA_TSTRING ||
	                 tr == LUA_TFUNCTION || tr == LUA_TTABLE, 3,
	                    "string/function/table expected");
	luastream_buffinit(L, &b);
	if (anchor) {
		p++; lp--;  /* skip anchor character */
	}
	ms.L = L;
	ms.matchdepth = MAXCCALLS;
	ms.src_init = src;
	ms.src_end = src+srcl;
	ms.p_end = p + lp;
	while (n < max_s) {
		const char *e;
		ms.level = 0;
		lua_assert(ms.matchdepth == MAXCCALLS);
		e = match(&ms, src, p);
		if (e) {
			n++;
			add_value(&ms, &b, src, e, tr);
		}
		if (e && e>src) /* non empty match? */
			src = e;  /* skip it */
		else if (src < ms.src_end)
			luastream_addchar(&b, *src++);
		else break;
		if (anchor) break;
	}
	luastream_addstream(&b, src, ms.src_end-src);
	if (op == 0) luastream_pushresult(&b);
	else luastream_pushresbuf(&b);
	lua_pushinteger(L, n);  /* number of substitutions */
	return 2;
}

/* }====================================================== */



/*
** {======================================================
** STRING FORMAT
** =======================================================
*/

/* maximum size of each formatted item (> len(format('%99.99f', -1e308))) */
#define MAX_ITEM	512

/* valid flags in a format specification */
#define FLAGS	"-+ #0"

/*
** maximum size of each format specification (such as "%-099.99d")
** (+2 for length modifiers; +10 accounts for %99.99x plus margin of error)
*/
#define MAX_FORMAT	(sizeof(FLAGS) + 2 + 10)


static void addquoted (lua_State *L, luastream_Buffer *b, int arg) {
	size_t l;
	const char *s = luastream_checkstream(L, arg, &l);
	luastream_addchar(b, '"');
	while (l--) {
		if (*s == '"' || *s == '\\' || *s == '\n') {
			luastream_addchar(b, '\\');
			luastream_addchar(b, *s);
		}
		else if (*s == '\0' || iscntrl(uchar(*s))) {
			char buff[10];
			if (!isdigit(uchar(*(s+1))))
				sprintf(buff, "\\%d", (int)uchar(*s));
			else
				sprintf(buff, "\\%03d", (int)uchar(*s));
			luastream_addstring(b, buff);
		}
		else
			luastream_addchar(b, *s);
		s++;
	}
	luastream_addchar(b, '"');
}

static const char *scanformat (lua_State *L, const char *strfrmt, char *form) {
	const char *p = strfrmt;
	while (*p != '\0' && strchr(FLAGS, *p) != NULL) p++;  /* skip flags */
	if ((size_t)(p - strfrmt) >= sizeof(FLAGS)/sizeof(char))
		luaL_error(L, "invalid format (repeated flags)");
	if (isdigit(uchar(*p))) p++;  /* skip width */
	if (isdigit(uchar(*p))) p++;  /* (2 digits at most) */
	if (*p == '.') {
		p++;
		if (isdigit(uchar(*p))) p++;  /* skip precision */
		if (isdigit(uchar(*p))) p++;  /* (2 digits at most) */
	}
	if (isdigit(uchar(*p)))
	  luaL_error(L, "invalid format (width or precision too long)");
	*(form++) = '%';
	memcpy(form, strfrmt, (p - strfrmt + 1) * sizeof(char));
	form += p - strfrmt + 1;
	*form = '\0';
	return p;
}


/*
** add length modifier into formats
*/
static void addlenmod (char *form, const char *lenmod) {
	size_t l = strlen(form);
	size_t lm = strlen(lenmod);
	char spec = form[l - 1];
	strcpy(form + l - 1, lenmod);
	form[l + lm - 1] = spec;
	form[l + lm] = '\0';
}


static int str_format (lua_State *L) {
	int top = lua_gettop(L);
	int arg = 1;
	size_t sfl;
	int op = luaL_checkoption(L, arg++, NULL, outops);
	const char *strfrmt = luaL_checklstring(L, arg, &sfl);
	const char *strfrmt_end = strfrmt+sfl;
	luastream_Buffer b;
	luastream_buffinit(L, &b);
	while (strfrmt < strfrmt_end) {
		if (*strfrmt != L_ESC)
			luastream_addchar(&b, *strfrmt++);
		else if (*++strfrmt == L_ESC)
			luastream_addchar(&b, *strfrmt++);  /* %% */
		else { /* format item */
			char form[MAX_FORMAT];  /* to store the format ('%...') */
			char *buff = luastream_prepbuffsize(&b, MAX_ITEM);  /* to put formatted item */
			int nb = 0;  /* number of bytes in added item */
			if (++arg > top)
				luaL_argerror(L, arg, "no value");
			strfrmt = scanformat(L, strfrmt, form);
			switch (*strfrmt++) {
				case 'c': {
					nb = sprintf(buff, form, (int)luaL_checkinteger(L, arg));
					break;
				}
				case 'd': case 'i':
				case 'o': case 'u': case 'x': case 'X': {
					lua_Integer n = luaL_checkinteger(L, arg);
					addlenmod(form, LUA_INTEGER_FRMLEN);
					nb = sprintf(buff, form, n);
					break;
				}
#if defined(LUA_USE_AFORMAT)
				case 'a': case 'A':
#endif
				case 'e': case 'E': case 'f':
				case 'g': case 'G': {
					addlenmod(form, LUA_NUMBER_FRMLEN);
					nb = sprintf(buff, form, luaL_checknumber(L, arg));
					break;
				}
				case 'q': {
					addquoted(L, &b, arg);
					break;
				}
				case 's': {
					size_t l;
					const char *s = luaL_tolstring(L, arg, &l);
					if (!strchr(form, '.') && l >= 100) {
						/* no precision and string is too long to be formatted;
						   keep original string */
						luaL_addvalue(&b);
						break;
					}
					else {
						nb = sprintf(buff, form, s);
						lua_pop(L, 1);  /* remove result from 'luaL_tolstring' */
						break;
					}
				}
				default: {  /* also treat cases 'pnLlh' */
					return luaL_error(L, "invalid option '%%%c' to 'format'",
					                     *(strfrmt - 1));
				}
			}
			luaL_addsize(&b, nb);
		}
	}
	if (op == 0) luastream_pushresult(&b);
	else luastream_pushresbuf(&b);
	return 1;
}

/* }====================================================== */


/*
** {======================================================
** PACK/UNPACK
** =======================================================
*/


/* value used for padding */
#if !defined(LUA_PACKPADBYTE)
#define LUA_PACKPADBYTE		0x00
#endif

/* maximum size for the binary representation of an integer */
#define MAXINTSIZE	16

/* number of bits in a character */
#define NB	CHAR_BIT

/* mask for one character (NB 1's) */
#define MC	((1 << NB) - 1)

/* size of a lua_Integer */
#define SZINT	((int)sizeof(lua_Integer))


/* dummy union to get native endianness */
static const union {
	int dummy;
	char little;  /* true iff machine is little endian */
} nativeendian = {1};


/* dummy structure to get native alignment requirements */
struct cD {
	char c;
	union { double d; void *p; lua_Integer i; lua_Number n; } u;
};

#define MAXALIGN	(offsetof(struct cD, u))


/*
** Union for serializing floats
*/
typedef union Ftypes {
	float f;
	double d;
	lua_Number n;
	char buff[5 * sizeof(lua_Number)];  /* enough for any float type */
} Ftypes;


/*
** information to pack/unpack stuff
*/
typedef struct Header {
	lua_State *L;
	int islittle;
	int maxalign;
} Header;


/*
** options for pack/unpack
*/
typedef enum KOption {
	Kint,		/* signed integers */
	Kuint,	/* unsigned integers */
	Kfloat,	/* floating-point numbers */
	Kchar,	/* fixed-length strings */
	Kstring,	/* strings with prefixed length */
	Kzstr,	/* zero-terminated strings */
	Kpadding,	/* padding */
	Kpaddalign,	/* padding for alignment */
	Knop		/* no-op (configuration or spaces) */
} KOption;


/*
** Read an integer numeral from string 'fmt' or return 'df' if
** there is no numeral
*/
static int digit (int c) { return '0' <= c && c <= '9'; }

static int getnum (const char **fmt, int df) {
	if (!digit(**fmt))  /* no number? */
		return df;  /* return default value */
	else {
		int a = 0;
		do {
			a = a*10 + (*((*fmt)++) - '0');
		} while (digit(**fmt) && a <= ((int)MAXSIZE - 9)/10);
		return a;
	}
}


/*
** Read an integer numeral and raises an error if it is larger
** than the maximum size for integers.
*/
static int getnumlimit (Header *h, const char **fmt, int df) {
	int sz = getnum(fmt, df);
	if (sz > MAXINTSIZE || sz <= 0)
		luaL_error(h->L, "integral size (%d) out of limits [1,%d]",
		                 sz, MAXINTSIZE);
	return sz;
}


/*
** Initialize Header
*/
static void initheader (lua_State *L, Header *h) {
	h->L = L;
	h->islittle = nativeendian.little;
	h->maxalign = 1;
}


/*
** Read and classify next option. 'size' is filled with option's size.
*/
static KOption getoption (Header *h, const char **fmt, int *size) {
	int opt = *((*fmt)++);
	*size = 0;  /* default */
	switch (opt) {
		case 'b': *size = sizeof(char); return Kint;
		case 'B': *size = sizeof(char); return Kuint;
		case 'h': *size = sizeof(short); return Kint;
		case 'H': *size = sizeof(short); return Kuint;
		case 'l': *size = sizeof(long); return Kint;
		case 'L': *size = sizeof(long); return Kuint;
		case 'j': *size = sizeof(lua_Integer); return Kint;
		case 'J': *size = sizeof(lua_Integer); return Kuint;
		case 'T': *size = sizeof(size_t); return Kuint;
		case 'f': *size = sizeof(float); return Kfloat;
		case 'd': *size = sizeof(double); return Kfloat;
		case 'n': *size = sizeof(lua_Number); return Kfloat;
		case 'i': *size = getnumlimit(h, fmt, sizeof(int)); return Kint;
		case 'I': *size = getnumlimit(h, fmt, sizeof(int)); return Kuint;
		case 's': *size = getnumlimit(h, fmt, sizeof(size_t)); return Kstring;
		case 'c':
			*size = getnum(fmt, -1);
			if (*size == -1)
				luaL_error(h->L, "missing size for format option 'c'");
			return Kchar;
		case 'z': return Kzstr;
		case 'x': *size = 1; return Kpadding;
		case 'X': return Kpaddalign;
		case ' ': break;
		case '<': h->islittle = 1; break;
		case '>': h->islittle = 0; break;
		case '=': h->islittle = nativeendian.little; break;
		case '!': h->maxalign = getnumlimit(h, fmt, MAXALIGN); break;
		default: luaL_error(h->L, "invalid format option '%c'", opt);
	}
	return Knop;
}


/*
** Read, classify, and fill other details about the next option.
** 'psize' is filled with option's size, 'notoalign' with its
** alignment requirements.
** Local variable 'size' gets the size to be aligned. (Kpadal option
** always gets its full alignment, other options are limited by 
** the maximum alignment ('maxalign'). Kchar option needs no alignment
** despite its size.
*/
static KOption getdetails (Header *h, size_t totalsize,
                           const char **fmt, int *psize, int *ntoalign) {
	KOption opt = getoption(h, fmt, psize);
	int align = *psize;  /* usually, alignment follows size */
	if (opt == Kpaddalign) {  /* 'X' gets alignment from following option */
		if (**fmt == '\0' || getoption(h, fmt, &align) == Kchar || align == 0)
			luaL_argerror(h->L, 1, "invalid next option for option 'X'");
	}
	if (align <= 1 || opt == Kchar)  /* need no alignment? */
		*ntoalign = 0;
	else {
		if (align > h->maxalign)  /* enforce maximum alignment */
			align = h->maxalign;
		if ((align & (align - 1)) != 0)  /* is 'align' not a power of 2? */
			luaL_argerror(h->L, 1, "format asks for alignment not power of 2");
		*ntoalign = (align - (int)(totalsize & (align - 1))) & (align - 1);
	}
	return opt;
}


/*
** Pack integer 'n' with 'size' bytes and 'islittle' endianness.
** The final 'if' handles the case when 'size' is larger than
** the size of a Lua integer, correcting the extra sign-extension
** bytes if necessary (by default they would be zeros).
*/
static void packint (luastream_Buffer *b, lua_Unsigned n,
                     int islittle, int size, int neg) {
	char *buff = luastream_prepbuffsize(b, size);
	int i;
	buff[islittle ? 0 : size - 1] = (char)(n & MC);  /* first byte */
	for (i = 1; i < size; i++) {
		n >>= NB;
		buff[islittle ? i : size - 1 - i] = (char)(n & MC);
	}
	if (neg && size > SZINT) {  /* negative number need sign extension? */
		for (i = SZINT; i < size; i++)  /* correct extra bytes */
			buff[islittle ? i : size - 1 - i] = (char)MC;
	}
	luaL_addsize(b, size);  /* add result to buffer */
}


/*
** Copy 'size' bytes from 'src' to 'dest', correcting endianness if
** given 'islittle' is different from native endianness.
*/
static void copywithendian (volatile char *dest, volatile const char *src,
                            int size, int islittle) {
	if (islittle == nativeendian.little) {
		while (size-- != 0)
			*(dest++) = *(src++);
	}
	else {
		dest += size - 1;
		while (size-- != 0)
			*(dest--) = *(src++);
	}
}


static int str_pack (lua_State *L) {
	luastream_Buffer b;
	Header h;
	int arg = 1;  /* current argument to pack */
	int op = luaL_checkoption(L, arg++, NULL, outops);
	const char *fmt = luaL_checkstring(L, arg);  /* format string */
	size_t totalsize = 0;  /* accumulate total size of result */
	initheader(L, &h);
	lua_pushnil(L);  /* mark to separate arguments from string buffer */
	luastream_buffinit(L, &b);
	while (*fmt != '\0') {
		int size, ntoalign;
		KOption opt = getdetails(&h, totalsize, &fmt, &size, &ntoalign);
		totalsize += ntoalign + size;
		while (ntoalign-- > 0)
		 luastream_addchar(&b, LUA_PACKPADBYTE);  /* fill alignment */
		arg++;
		switch (opt) {
			case Kint: {  /* signed integers */
				lua_Integer n = luaL_checkinteger(L, arg);
				if (size < SZINT) {  /* need overflow check? */
					lua_Integer lim = (lua_Integer)1 << ((size * NB) - 1);
					luaL_argcheck(L, -lim <= n && n < lim, arg, "integer overflow");
				}
				packint(&b, (lua_Unsigned)n, h.islittle, size, (n < 0));
				break;
			}
			case Kuint: {  /* unsigned integers */
				lua_Integer n = luaL_checkinteger(L, arg);
				if (size < SZINT)  /* need overflow check? */
					luaL_argcheck(L, (lua_Unsigned)n < ((lua_Unsigned)1 << (size * NB)),
					                 arg, "unsigned overflow");
				packint(&b, (lua_Unsigned)n, h.islittle, size, 0);
				break;
			}
			case Kfloat: {  /* floating-point options */
				volatile Ftypes u;
				char *buff = luastream_prepbuffsize(&b, size);
				lua_Number n = luaL_checknumber(L, arg);  /* get argument */
				if (size == sizeof(u.f)) u.f = (float)n;  /* copy it into 'u' */
				else if (size == sizeof(u.d)) u.d = (double)n;
				else u.n = n;
				/* move 'u' to final result, correcting endianness if needed */
				copywithendian(buff, u.buff, size, h.islittle);
				luaL_addsize(&b, size);
				break;
			}
			case Kchar: {  /* fixed-size string */
				size_t len;
				const char *s = luastream_checkstream(L, arg, &len);
				luaL_argcheck(L, len == (size_t)size, arg, "wrong length");
				luastream_addstream(&b, s, size);
				break;
			}
			case Kstring: {  /* strings with length count */
				size_t len;
				const char *s = luastream_checkstream(L, arg, &len);
				luaL_argcheck(L, size >= (int)sizeof(size_t) ||
				                 len < ((size_t)1 << (size * NB)),
				                 arg, "string length does not fit in given size");
				packint(&b, (lua_Unsigned)len, h.islittle, size, 0);  /* pack length */
				luastream_addstream(&b, s, len);
				totalsize += len;
				break;
			}
			case Kzstr: {  /* zero-terminated string */
				size_t len;
				const char *s = luastream_checkstream(L, arg, &len);
				luaL_argcheck(L, strlen(s) == len, arg, "string contains zeros");
				luastream_addstream(&b, s, len);
				luastream_addchar(&b, '\0');  /* add zero at the end */
				totalsize += len + 1;
				break;
			}
			case Kpadding: luastream_addchar(&b, LUA_PACKPADBYTE);  /* go through */
			case Kpaddalign: case Knop:
				arg--;  /* undo increment */
				break;
		}
	}
	if (op == 0) luastream_pushresult(&b);
	else luastream_pushresbuf(&b);
	return 1;
}


static int str_packsize (lua_State *L) {
	Header h;
	int arg = 1;  /* current argument to pack */
	const char *fmt = luaL_checkstring(L, arg);  /* format string */
	size_t totalsize = 0;  /* accumulate total size of result */
	initheader(L, &h);
	while (*fmt != '\0') {
		int size, ntoalign;
		KOption opt = getdetails(&h, totalsize, &fmt, &size, &ntoalign);
		size += ntoalign;  /* total space used by option */
		luaL_argcheck(L, totalsize <= MAXSIZE - size, 1,
		                 "format result too large");
		totalsize += size;
		arg++;
		switch (opt) {
			case Kstring: {  /* strings with length count */
				size_t len;
				luastream_checkstream(L, arg, &len);
				luaL_argcheck(L, size >= (int)sizeof(size_t) ||
				                 len < ((size_t)1 << (size * NB)),
				                 arg, "string length does not fit in given size");
				totalsize += len;
				break;
			}
			case Kzstr: {  /* zero-terminated string */
				size_t len;
				const char *s = luastream_checkstream(L, arg, &len);
				luaL_argcheck(L, strlen(s) == len, arg, "string contains zeros");
				totalsize += len + 1;
				break;
			}
			default:  break;
		}
	}
	lua_pushinteger(L, (lua_Integer)totalsize);
	return 1;
}


/*
** Unpack an integer with 'size' bytes and 'islittle' endianness.
** If size is smaller than the size of a Lua integer and integer
** is signed, must do sign extension (propagating the sign to the
** higher bits); if size is larger than the size of a Lua integer,
** it must check the unread bytes to see whether they do not cause an
** overflow.
*/
static lua_Integer unpackint (lua_State *L, const char *str,
                              int islittle, int size, int issigned) {
	lua_Unsigned res = 0;
	int i;
	int limit = (size  <= SZINT) ? size : SZINT;
	for (i = limit - 1; i >= 0; i--) {
		res <<= NB;
		res |= (lua_Unsigned)(unsigned char)str[islittle ? i : size - 1 - i];
	}
	if (size < SZINT) {  /* real size smaller than lua_Integer? */
		if (issigned) {  /* needs sign extension? */
			lua_Unsigned mask = (lua_Unsigned)1 << (size*NB - 1);
			res = ((res ^ mask) - mask);  /* do sign extension */
		}
	}
	else if (size > SZINT) {  /* must check unread bytes */
		int mask = (!issigned || (lua_Integer)res >= 0) ? 0 : MC;
		for (i = limit; i < size; i++) {
			if ((unsigned char)str[islittle ? i : size - 1 - i] != mask)
				luaL_error(L, "%d-byte integer does not fit into Lua Integer", size);
		}
	}
	return (lua_Integer)res;
}


static int str_unpack (lua_State *L) {
	Header h;
	const char *fmt = luaL_checkstring(L, 1);
	size_t ld;
	const char *data = luastream_checkstream(L, 2, &ld);
	size_t pos = (size_t)posrelat(luaL_optinteger(L, 3, 1), ld) - 1;
	int n = 0;  /* number of results */
	luaL_argcheck(L, pos <= ld, 3, "initial position out of string");
	initheader(L, &h);
	while (*fmt != '\0') {
		int size, ntoalign;
		KOption opt = getdetails(&h, pos, &fmt, &size, &ntoalign);
		if ((size_t)ntoalign + size > ~pos || pos + ntoalign + size > ld)
			luaL_argerror(L, 2, "data string too short");
		pos += ntoalign;  /* skip alignment */
		/* stack space for item + next position */
		luaL_checkstack(L, 2, "too many results");
		n++;
		switch (opt) {
			case Kint:
			case Kuint: {
				lua_Integer res = unpackint(L, data + pos, h.islittle, size,
				                               (opt == Kint));
				lua_pushinteger(L, res);
				break;
			}
			case Kfloat: {
				volatile Ftypes u;
				lua_Number num;
				copywithendian(u.buff, data + pos, size, h.islittle);
				if (size == sizeof(u.f)) num = (lua_Number)u.f;
				else if (size == sizeof(u.d)) num = (lua_Number)u.d;
				else num = u.n;
				lua_pushnumber(L, num);
				break;
			}
			case Kchar: {
				lua_pushlstring(L, data + pos, size);
				break;
			}
			case Kstring: {
				size_t len = (size_t)unpackint(L, data + pos, h.islittle, size, 0);
				luaL_argcheck(L, pos + len + size <= ld, 2, "data string too short");
				lua_pushlstring(L, data + pos + size, len);
				pos += len;  /* skip string */
				break;
			}
			case Kzstr: {
				size_t len = (int)strlen(data + pos);
				lua_pushlstring(L, data + pos, len);
				pos += len + 1;  /* skip string plus final '\0' */
				break;
			}
			case Kpaddalign: case Kpadding: case Knop:
				n--;  /* undo increment */
				break;
		}
		pos += size;
	}
	lua_pushinteger(L, pos + 1);  /* next position */
	return n + 1;
}

/* }====================================================== */


static const luaL_Reg strlib[] = {
	{"buffer", str_newbuf},
	{"byte", str_byte},
	{"char", str_char},
	{"diff", str_diff},
	{"dump", str_dump},
	{"find", str_find},
	{"format", str_format},
	{"gmatch", str_gmatch},
	{"gsub", str_gsub},
	{"isbuffer", str_isbuffer},
	{"len", str_len},
	{"lower", str_lower},
	{"match", str_match},
	{"pack", str_pack},
	{"packsize", str_packsize},
	{"rep", str_rep},
	{"reverse", str_reverse},
	{"tostring", str_tostring},
	{"upper", str_upper},
	{"unpack", str_unpack},
	{NULL, NULL}
};


static const luaL_Reg buflib[] = {
	{"get", buf_get},
	{"set", buf_set},
	{"fill", buf_fill},
	//{"pack", buf_pack},
	//{"unpack", buf_unpack},
	{"__tostring", buf_tostring},
	{"__len", buf_len},
	{NULL, NULL}
};


static void createmetatable (lua_State *L) {
	if (!luaL_getmetatable(L, LUASTREAM_BUFFER)) {
		lua_pop(L, 1);  /* pop 'nil' */
		luaL_newmetatable(L, LUASTREAM_BUFFER);
	}
	lua_pushvalue(L, -1);  /* push metatable */
	lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
	luaL_setfuncs(L, buflib, 0);  /* add buffer methods to new metatable */
	lua_pop(L, 1);  /* pop new metatable */
}


/*
** Open stream library
*/
LUAMOD_API int luaopen_stream (lua_State *L) {
	luaL_newlib(L, strlib);
	createmetatable(L);
	return 1;
}
