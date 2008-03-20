#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <trace/list.h>
#include <trace/trace.h>

#define TRACE_COMPSEP  '.'
#define TRACE_MODSEP   '='
#define TRACE_FLAGSEP  ','
#define TRACE_TRCSEP   ';'
#define TRACE_WILDCARD '*'
#define TRACE_ALLFLAGS "all"
#define TRACE_MAXNAME 32

#define ERR_MSG(fmt, args...) fprintf(stderr, "parse error: "fmt"\n", ## args)


static trace_component_t *find_component(trace_context_t *tc, const char *);
static trace_flag_t *find_flag(trace_component_t *, const char *);
int parse_context_flags(trace_context_t *tc, char *compname, char **flags);
int parse_flags(trace_context_t *tc, trace_component_t *c, char **flags);



/*****************************************************************************
 *                             *** flag parsing ***                          *
 *****************************************************************************/

/********************
 * trace_parse_flags
 ********************/
int
trace_parse_flags(char *command)
{
#define TRACE_COMMAND "trace "

	char *p, *ctx, *comp, *flags;
	char ctxname[TRACE_MAXNAME+1], compname[TRACE_MAXNAME+1];
	trace_context_t    *tc;
	int                 retval;
	
	retval = 0;
	
	if (!strncmp(command, TRACE_COMMAND, sizeof(TRACE_COMMAND) - 1))
		command += sizeof(TRACE_COMMAND) - 1;
	
	
	/*
	 * the expected input is context.component=[+|-]flag1,[+|-]flag2,...;...
	 */

	p = command;
	while (p && *p) {
		
		ctx = p;


		/*
		 * find context name separator (.)
		 */

		comp = ctx;
		p = ctxname;
		while (*comp &&
			   *comp != ' ' && *comp != '\t' && *comp != TRACE_COMPSEP &&
			   (unsigned long)p - (unsigned long)ctxname < TRACE_MAXNAME)
			*p++ = *comp++;
		
		if ((unsigned long)p - (unsigned long)ctxname >= TRACE_MAXNAME) {
			ERR_MSG("component name too long");
			retval = -EINVAL;
			break;
		}

		if (*comp != TRACE_COMPSEP) {
			ERR_MSG("missing component name separator (%c)", TRACE_COMPSEP);
			retval = -EINVAL;
			break;
		}
		
		*p = '\0';
		comp++;

		
		/*
		 * find component name separator (=)
		 */

		flags = comp;
		p = compname;
		while (*flags && *flags != ' ' && *flags != '\t' && 
			   *flags != TRACE_MODSEP &&
			   (unsigned long)p - (unsigned long)compname < TRACE_MAXNAME)
			*p++ = *flags++;

		if ((unsigned long)p - (unsigned long)compname >= TRACE_MAXNAME) {
			ERR_MSG("component name too long");
			retval = -EINVAL;
			break;
		}

		if (*flags != TRACE_MODSEP) {
			ERR_MSG("missing module name separator (%c)", TRACE_MODSEP);
			retval = -EINVAL;
			break;
		}
			
		*p = '\0';
		flags++;

		
		/*
		 * parse flags
		 */
		
		if ((tc = trace_find_context(ctxname)) == NULL) {
			ERR_MSG("cannot find context \"%s\"", ctxname);
			retval = -ESRCH;
			break;
		}

		retval = parse_context_flags(tc, compname, &flags);
		if (retval != 0)
			break;
		
		p = flags && *flags == TRACE_TRCSEP ? flags + 1 : flags;
	}
	

	return retval;
}


/********************
 * parse_context_flags
 ********************/
int
parse_context_flags(trace_context_t *tc, char *compname, char **flags)
{
	trace_component_t *c;
	list_hook_t       *p;
	char              *saved;
	int                retval;

	retval = 0;


	/*
	 * for wildcard, loop through all components
	 */

	if (compname[0] == TRACE_WILDCARD && compname[1] == '\0') {
		saved = NULL;
		list_iter_forw(p, &tc->components) {
			c = list_entry(p, trace_component_t, hook);
			if (saved == NULL)
				saved = *flags;
			else
				*flags = saved;
			if ((retval = parse_flags(tc, c, flags)) != 0) {
				break;
			}
		}
	}


	/*
	 * otherwise find specific component
	 */
	
	else {
		if ((c = find_component(tc, compname)) != NULL) {
			retval = parse_flags(tc, c, flags);
		}
		else {
			ERR_MSG("cannot find component \"%s\"", compname);
			retval = -ESRCH;
		}
	}
					
	return retval;
}


/********************
 * parse_flags
 ********************/
int
parse_flags(trace_context_t *tc, trace_component_t *c, char **flags)
{
	char *p,     *f, flagname[TRACE_MAXNAME+1];
	trace_flag_t *flag;
	int           on;
	int           retval;

	retval = 0;

	p = *flags;
	while (p && *p && *p != TRACE_TRCSEP) {
		
		/*
		 * extract next flag name
		 */

		f = p;
		p = flagname;
		while (*f && *f != ' ' && *f != '\t' &&
			   *f != TRACE_FLAGSEP && *f != TRACE_TRCSEP &&
			   (unsigned long)p - (unsigned long)flagname < TRACE_MAXNAME)
			*p++ = *f++;

		if ((unsigned long)p - (unsigned long)flagname >= TRACE_MAXNAME) {
			ERR_MSG("flag name too long");
			retval = -EINVAL;
			p = f;
			break;
		}

		if (*f && *f != TRACE_FLAGSEP && *f != TRACE_TRCSEP) {
			ERR_MSG("missing flag name separator");
			p = f;
			retval = -EINVAL;
			break;
		}
			
		*p = '\0';

		p = !*f || *f == TRACE_TRCSEP ? f : f + 1;

		/*
		 * see which way to turn the flag
		 */

		f = flagname;		
		switch (*f) {
		default:  on = 1;      break;
		case '+': on = 1; f++; break;
		case '-': on = 0; f++; break;
		}
		
		
		/*
		 * for wildcard, loop through all flags
		 */

		if (!strcmp(f, TRACE_ALLFLAGS)) {
			for (flag = c->flags; flag->name != NULL; flag++) {
				if (on)
					trace_on(tc, flag->bit);
				else
					trace_off(tc, flag->bit);
			}
		}


		/*
		 * otherwise find specific flag
		 */

		else {
			if ((flag = find_flag(c, f)) == NULL) {
				ERR_MSG("unknown flag \"%s\"", f);
				retval = -EINVAL;
				break;
			}
		
			if (on)
				trace_on(tc, flag->bit);
			else
				trace_off(tc, flag->bit);
		}
		
	}
	
	
	if (!retval)
		*flags = p;
	
	
	return retval;
}


/********************
 * find_component
 ********************/
static trace_component_t *
find_component(trace_context_t *tc, const char *name)
{
	trace_component_t *c;
	list_hook_t       *p;

	list_iter_forw(p, &tc->components) {
		c = list_entry(p, trace_component_t, hook);
		if (!strcmp(c->name, name))
			return c;
	}

	return NULL;
}


/********************
 * find_flag
 ********************/
static trace_flag_t *
find_flag(trace_component_t *c, const char *name)
{
	trace_flag_t *f;

	for (f = c->flags; f->name; f++)
		if (!strcmp(f->name, name))
			return f;

	return NULL;
}



/*****************************************************************************
 *                             *** filter parsing ***                        *
 *****************************************************************************/

/********************
 * trace_parse_filter
 ********************/
trace_tag_t *
trace_parse_filter(char *filter)
{
#define SKIP_WHITESPACE(ptr) while (*(ptr) == ' ' || *(ptr) == '\t') (ptr)++

	trace_tag_t *tags, *tag;
	size_t       len, ntag, size;
	char        *buf, *key, *val, *p, sep;
	int          i;

	len  = strlen(filter);
	ntag = len / 3 + 1;                         /* worst case: "k=v k=v..." */

	size = ntag * sizeof(*tags) + len + 1;      /* co-allocate everything */
	if ((tags = malloc(size)) == NULL)
		return NULL;
	
	memset(tags, 0, size);
	buf = ((char *)tags) + ntag * sizeof(*tags);
	
	p   = filter;
	key = buf;
	tag = tags;
	i   = 0;
	val = NULL;
	while (*p) {
		if (i > ntag)
			goto failed;
		tag->key = key;

		/* skip leading key whitespace */
		SKIP_WHITESPACE(p);
		if (*p == '\0')
			break;
		/* copy key */
		while (*p && *p != ' ' && *p != '\t' && *p != '=')
			*key++ = *p++;
		*key++ = '\0';
		/* skip traling key whitespace */
		SKIP_WHITESPACE(p);

		if (*p != '=')
			goto failed;
		p++;
		
		val = key;
		tag->value = val;
		
		/* skip leading value whitespace */
		SKIP_WHITESPACE(p);
		/* copy value */
		if (*p == (sep = '"') || *p == (sep = '\'')) {         /* quoted */
			p++;
			while (*p && *p != sep)
				*val++ = *p++;
			if (*p != sep)
				goto failed;
			p++;
			*val++ = '\0';
		}
		else {                                  /* whitespace-terminated */
			while (*p && *p != ' ' && *p != '\t')
				*val++ = *p++;
			*val++ = '\0';
		}

		/*printf("##### filter tag: { %s=%s }\n", tag->key, tag->value);*/

		key = val;
		tag++;
		i++;
	}
	
	if (val > ((char *)tags) + size + 1) {
		printf("#### buf overflow in trace_parse_filter!!! ####\n");
		abort();
	}
	
	return tags;

 failed:
	free(tags);
	return NULL;
}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
