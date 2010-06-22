/******************************************************************************/
/*  This file is part of libtrace                                             */
/*                                                                            */
/*  Copyright (C) 2010 Nokia Corporation.                                     */
/*                                                                            */
/*  This library is free software; you can redistribute                       */
/*  it and/or modify it under the terms of the GNU Lesser General Public      */
/*  License as published by the Free Software Foundation                      */
/*  version 2.1 of the License.                                               */
/*                                                                            */
/*  This library is distributed in the hope that it will be useful,           */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU          */
/*  Lesser General Public License for more details.                           */
/*                                                                            */
/*  You should have received a copy of the GNU Lesser General Public          */
/*  License along with this library; if not, write to the Free Software       */
/*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  */
/*  USA.                                                                      */
/******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


/*
 * Argh... there is some evil PHP3-compatibility code in PHP5 that defines
 * list_entry as a macro. This conflicts with our list.h... Brutally block
 * the PHP3 compatibility header out by #defining its #ifdef wrapper here.
 * Yes, this is a kludge.
 */
#define PHP3_COMPAT_H                  /* block evil PHP3 list_entry macro */

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_nsntrace.h"

#include "trace/trace.h"

#define MODULE_NAME    "nsntrace"
#define MODULE_VERSION "0.1"           /* XXX should come externally */

#define KEY_NAME  "name"
#define KEY_FLAGS "flags"
#define KEY_DESCR "description"
#define KEY_FLAG  "flag"

#define __E(fmt, args...) do {											\
		fprintf(stderr, "### => %s [%u] "fmt" ###\n",					\
				__FUNCTION__, getpid(), ## args);						\
		fflush(stderr);													\
	} while (0)

static int le_context;                 /* resource id for trace_context_t */
#define le_name "trace context"        /* trace_context_t name */


trace_component_t *alloc_component(char *name, HashTable *hflags,
								   zval ****bitret, int *err TSRMLS_DC);


static PHP_MINIT_FUNCTION(nsntrace);
static PHP_MSHUTDOWN_FUNCTION(nsntrace);
static PHP_RINIT_FUNCTION(nsntrace);
static PHP_RSHUTDOWN_FUNCTION(nsntrace);
static PHP_MINFO_FUNCTION(nsntrace);

static PHP_FUNCTION(trace_init);
static PHP_FUNCTION(trace_exit);
static PHP_FUNCTION(trace_open);
static PHP_FUNCTION(trace_close);
static PHP_FUNCTION(trace_set_destination);
static PHP_FUNCTION(trace_set_header);
static PHP_FUNCTION(trace_add_component);
static PHP_FUNCTION(trace_del_component);
static PHP_FUNCTION(trace_enable);
static PHP_FUNCTION(trace_disable);
static PHP_FUNCTION(trace_on);
static PHP_FUNCTION(trace_off);
static PHP_FUNCTION(trace_write);
static PHP_FUNCTION(__trace_write);
static PHP_FUNCTION(trace_reset_filters);
static PHP_FUNCTION(trace_add_simple_filter);
static PHP_FUNCTION(trace_add_regexp_filter);


/*
 * extension function table
 */

zend_function_entry nsntrace_functions[] = {
	PHP_FE(trace_init, NULL)
	PHP_FE(trace_exit, NULL)
	PHP_FE(trace_open, NULL)
	PHP_FE(trace_close, NULL)
	PHP_FE(trace_set_header, NULL)
	PHP_FE(trace_set_destination, NULL)
	PHP_FE(trace_add_component, NULL)
	PHP_FE(trace_del_component, NULL)
	PHP_FE(trace_enable, NULL)
	PHP_FE(trace_disable, NULL)
	PHP_FE(trace_on, NULL)
	PHP_FE(trace_off, NULL)
	PHP_FE(trace_write, NULL)
	PHP_FE(__trace_write, NULL)
	PHP_FE(trace_reset_filters, NULL)
	PHP_FE(trace_add_simple_filter, NULL)
	PHP_FE(trace_add_regexp_filter, NULL)
	{NULL, NULL, NULL}
};


/*
 * extension descriptor
 */
zend_module_entry nsntrace_module_entry = {
	STANDARD_MODULE_HEADER,
	MODULE_NAME,
	nsntrace_functions,
	PHP_MINIT(nsntrace),
	PHP_MSHUTDOWN(nsntrace),
	PHP_RINIT(nsntrace),
	PHP_RSHUTDOWN(nsntrace),
	PHP_MINFO(nsntrace),
	MODULE_VERSION,
	STANDARD_MODULE_PROPERTIES
};


#ifdef COMPILE_DL_NSNTRACE
ZEND_GET_MODULE(nsntrace)
#endif


/********************
 * trace_context_dtor
 ********************/
static void
trace_context_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	trace_context_t *tc = (trace_context_t *)rsrc->ptr;
	
	trace_close(tc);
	efree(tc);
}


/********************
 * MINIT
 ********************/
PHP_MINIT_FUNCTION(nsntrace)
{

	/*
	 * initialize tracing library
	 */

	if (trace_init() != 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "failed to initialize tracing library");
		return FAILURE;
	}
	
	/*
	 * register resource destructors
	 */

	le_context = zend_register_list_destructors_ex(trace_context_dtor, NULL,
												   le_name, module_number);
	
	/*
	 * register module constants
	 */
	
	REGISTER_LONG_CONSTANT("TRACE_TO_STDOUT", TRACE_TO_STDOUT,
						   CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("TRACE_TO_STDERR", TRACE_TO_STDERR,
						   CONST_CS | CONST_PERSISTENT);
	REGISTER_STRING_CONSTANT("TRACE_FILTER_ALL_TAGS", TRACE_FILTER_ALL_TAGS,
							 CONST_CS | CONST_PERSISTENT);
	REGISTER_STRING_CONSTANT("TRACE_FILTER_NO_TAGS", TRACE_FILTER_NO_TAGS,
							 CONST_CS | CONST_PERSISTENT);


	return SUCCESS;
}


/********************
 * MSHUTDOWN
 ********************/
PHP_MSHUTDOWN_FUNCTION(nsntrace)
{
	trace_exit();
	return SUCCESS;
}


/********************
 * RINIT
 ********************/
PHP_RINIT_FUNCTION(nsntrace)
{
	return SUCCESS;
}


/********************
 * RSHUTDOWN
 ********************/
PHP_RSHUTDOWN_FUNCTION(nsntrace)
{
	return SUCCESS;
}

/********************
 * MINFO
 ********************/
PHP_MINFO_FUNCTION(nsntrace)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "nsntrace support", "enabled");
	php_info_print_table_end();
}


/********************
 * trace_init
 ********************/
PHP_FUNCTION(trace_init)
{
	/*
	 * check parameters and initalize tracing library
	 */

	if (ZEND_NUM_ARGS() != 0) {
		WRONG_PARAM_COUNT;
	}

	/*
	 * trace_init done during module initialization
	 */
	
	RETURN_TRUE;

}


/********************
 * trace_exit
 ********************/
PHP_FUNCTION(trace_exit)
{
	/*
	 * check parameters and deinitialize tracing library
	 */
	
	if (ZEND_NUM_ARGS() != 0) {
		WRONG_PARAM_COUNT;
	}

	/*
	 * trace_exit done during module deinitialization
	 */
}


/********************
 * trace_open
 ********************/
PHP_FUNCTION(trace_open)
{
	trace_context_t *tc;
	char            *appid;
	int              len;

	/*
	 * check and parse parameters
	 */

	if (ZEND_NUM_ARGS() != 1) {
		WRONG_PARAM_COUNT;
	}

	if (zend_parse_parameters(1 TSRMLS_CC, "s", &appid, &len) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid parameters");
		RETURN_NULL();
	}
	
	
	/*
	 * create trace context, register it as a resource
	 */
	
	tc = emalloc(sizeof(*tc));
	memset(tc, 0, sizeof(*tc));
	
	if (trace_open(tc, appid) != 0) {
		efree(tc);
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "failed to create new trace context");
		RETURN_NULL();
	}

#if 0
	/* XXX TODO: kludge until the filtering interface is mapped to PHP */
	trace_add_simple_filter(tc, TRACE_FILTER_NO_TAGS);
#endif
	
	ZEND_REGISTER_RESOURCE(return_value, tc, le_context);
}


/********************
 * get_trace_context
 ********************/
static int
get_trace_context(zval **ztc, trace_context_t **tcp TSRMLS_DC)
{
	int status;
	
	if (Z_TYPE_PP(ztc) == IS_RESOURCE) {
		*tcp = (trace_context_t *)
			zend_fetch_resource(ztc TSRMLS_CC,-1, le_name, NULL, 1, le_context);
		if (*tcp == NULL)
			status = ENOENT;
		else
			status = 0;
	}
	else {
		*tcp   = NULL;
		status = EINVAL;
	}
	
	if (status)
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid trace context");
	
	return status;
}


/********************
 * trace_close
 ********************/
PHP_FUNCTION(trace_close)
{
	zval            *ztc;
	trace_context_t *tc;
	
	/*
	 * check, and parse parameters, fetch context
	 */

	if (ZEND_NUM_ARGS() != 1) {
		WRONG_PARAM_COUNT;
	}

	if (zend_parse_parameters(1 TSRMLS_CC, "r", &ztc) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "failed to parse arguments");
		RETURN_FALSE;
	}

	if (get_trace_context(&ztc, &tc) != 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid trace context");
		RETURN_FALSE;
	}
	
	
	/*
	 * unreference the resource (will get deleted once all refs are gone)
	 */
	
	if (zend_list_delete(Z_LVAL_P(ztc))) {
		RETURN_TRUE;
	}
	else {
		RETURN_FALSE;
	}
}


/********************
 * trace_set_destination
 ********************/
PHP_FUNCTION(trace_set_destination)
{
	zval            *ztc, *zpath;
	int              narg;
	trace_context_t *tc;
	char            *path;
	
	/*
	 * parse & check parameters, fetch context
	 */

	if ((narg = ZEND_NUM_ARGS()) != 2) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "wrong number of arguments");
		RETURN_FALSE;
	}

	if (zend_parse_parameters(narg TSRMLS_CC, "rz", &ztc, &zpath) == FAILURE) {
		RETURN_FALSE;
	}
	
	if (get_trace_context(&ztc, &tc) != 0) {
		RETURN_FALSE;
	}
	
	switch (Z_TYPE_P(zpath)) {
	case IS_LONG:
		path = (char *)Z_LVAL_P(zpath);
		break;
	case IS_STRING:
		path = Z_STRVAL_P(zpath);
		break;
	default:
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid destination");
		RETURN_FALSE;
	}
	
	
	/*
	 * set trace destination
	 */

	if (trace_set_destination(tc, path) == 0) {
		RETURN_TRUE;
	}
	else {
		RETURN_FALSE;
	}
}


/********************
 * trace_set_header
 ********************/
PHP_FUNCTION(trace_set_header)
{
	zval            *ztc, *zformat;
	int              narg;
	trace_context_t *tc;

	/*
	 * parse & check parameters, fetch context
	 */

	if ((narg = ZEND_NUM_ARGS()) != 2) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "wrong number of arguments");
		return;
	}

	if (zend_parse_parameters(narg TSRMLS_CC, "rz", &ztc, &zformat) == FAILURE)
		return;
	
	if (get_trace_context(&ztc, &tc) != 0) {
		RETURN_FALSE;
	}
	
	if (Z_TYPE_P(zformat) != IS_STRING) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid header format");
		RETURN_FALSE;
	}


	/*
	 * set trace header
	 */

	if (trace_set_header(tc, Z_STRVAL_P(zformat)) == 0) {
		RETURN_TRUE;
	}
	else {
		RETURN_FALSE;
	}
}


/********************
 * trace_add_component
 ********************/
PHP_FUNCTION(trace_add_component)
{
	zval              *ztc, *zcomp, **zval, ***bitret, ***br;
	HashTable         *zflags;
	trace_context_t   *tc;
	trace_component_t *c;
	trace_flag_t      *f;
	char              *key;
	int                key_len;
	char              *name;
	int                err, status, i;

	/*
	 * parse & check parameters, fetch context
	 */

	if (ZEND_NUM_ARGS() != 2) {
		WRONG_PARAM_COUNT;
	}

	if (zend_parse_parameters(2 TSRMLS_CC, "ra", &ztc, &zcomp) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "failed to parse arguments");
		RETURN_FALSE;
	}
	
	if (get_trace_context(&ztc, &tc) != 0) {
		RETURN_FALSE;
	}
	

	/*
	 * dig out component name and array of flags
	 */

	/* 'name' => name */
	key     = KEY_NAME;
	key_len = sizeof(KEY_NAME);
	if (zend_hash_find(Z_ARRVAL_P(zcomp),
					   key, key_len, (void **)&zval) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "component array has no key \"%s\"", key);
		RETURN_FALSE;
	}
	
	if (Z_TYPE_PP(zval) != IS_STRING) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "component[%s] should be a string", key);
		RETURN_FALSE;
	}
	name = Z_STRVAL_PP(zval);
	
	/* 'flags' => array(...) */
	key     = KEY_FLAGS;
	key_len = sizeof(KEY_FLAGS);
	if (zend_hash_find(Z_ARRVAL_P(zcomp),
					   key, key_len, (void **)&zval) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "component array has no key \"%s\"", key);
		RETURN_FALSE;
	}

	if (Z_TYPE_PP(zval) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "component[%s] should be an array of flags", key);
		RETURN_FALSE;
	}
	zflags = Z_ARRVAL_PP(zval);
	

	/*
	 * allocate a corresponding trace_component_t *
	 */

	if ((c = alloc_component(name, zflags, &bitret, &err TSRMLS_CC)) == NULL) {
		RETURN_FALSE;
	}
	
	
	/*
	 * register component and let the caller know the flags
	 */

	br = bitret;
	if (!(status = trace_add_component(tc, c))) {
		for (f = c->flags; f->name; f++) {
			if (*br) {
				ZVAL_LONG(**br, f->bit);
				br++;
			}
		}
	}
	
	if (bitret)
		efree(bitret);

	if (status) {
		RETURN_FALSE;
	}
	else {
		RETURN_TRUE;
	}
}


/********************
 * trace_del_component
 ********************/
PHP_FUNCTION(trace_del_component)
{
	zval            *ztc;
	char            *name;
	int              name_len;
	trace_context_t *tc;

	/*
	 * parse & check parameters, fetch context
	 */

	if (ZEND_NUM_ARGS() != 2) {
		WRONG_PARAM_COUNT;
	}

	if (zend_parse_parameters(2 TSRMLS_CC, "rs",
							  &ztc, &name, &name_len) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "failed to parse arguments");
		RETURN_FALSE;
	}

	if (get_trace_context(&ztc, &tc) != 0) {
		RETURN_FALSE;
	}

	
	/*
	 * delete component
	 */

	if (trace_del_component(tc, name) != 0) {
		RETURN_FALSE;
	}
	else {
		RETURN_TRUE;
	}
}


/********************
 * trace_enable
 ********************/
PHP_FUNCTION(trace_enable)
{
	zval            *ztc;
	trace_context_t *tc;
	int              old;
	
	/*
	 * parse & check parameters, fetch context
	 */

	if (ZEND_NUM_ARGS() != 1) {
		WRONG_PARAM_COUNT;
	}

	if (zend_parse_parameters(1 TSRMLS_CC, "r", &ztc) != SUCCESS) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "failed to parse arguments");
		RETURN_FALSE;
	}
	
	if (get_trace_context(&ztc, &tc) != 0) { 
		RETURN_FALSE;
	}
	

	/*
	 * enable tracing
	 */

	old = trace_enable(tc);
	
	RETURN_LONG(old);
}


/********************
 * trace_disable
 ********************/
PHP_FUNCTION(trace_disable)
{
	zval            *ztc;
	trace_context_t *tc;
	int              old;
	
	/*
	 * parse & check parameters, fetch context
	 */

	if (ZEND_NUM_ARGS() != 1) {
		WRONG_PARAM_COUNT;
	}

	if (zend_parse_parameters(1 TSRMLS_CC, "r", &ztc) != SUCCESS) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "failed to parse arguments");
		RETURN_FALSE;
	}
	
	if (get_trace_context(&ztc, &tc) != 0) { 
		RETURN_FALSE;
	}
	
	
	/*
	 * disable tracing
	 */

	old = trace_disable(tc);
	
	RETURN_LONG(old);
}


/********************
 * trace_on
 ********************/
PHP_FUNCTION(trace_on)
{
	zval            *ztc;
	trace_context_t *tc;
	int              flag;
	
	/*
	 * parse & check parameters, fetch context
	 */

	if (ZEND_NUM_ARGS() != 2) {
		WRONG_PARAM_COUNT;
	}

	if (zend_parse_parameters(2 TSRMLS_CC, "rl", &ztc, &flag) != SUCCESS) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "failed to parse arguments");
		RETURN_FALSE;
	}
	
	if (get_trace_context(&ztc, &tc) != 0) { 
		RETURN_FALSE;
	}

	
	/*
	 * trace flag on
	 */
	
	if (trace_on(tc, flag)) {
		RETURN_FALSE;
	}
	else {
		RETURN_TRUE;
	}
}


/********************
 * trace_off
 ********************/
PHP_FUNCTION(trace_off)
{
	zval            *ztc;
	trace_context_t *tc;
	int              flag;

	/*
	 * parse & check parameters, fetch context
	 */
	
	if (ZEND_NUM_ARGS() != 2) {
		WRONG_PARAM_COUNT;
	}

	if (zend_parse_parameters(2 TSRMLS_CC, "rl", &ztc, &flag) != SUCCESS) {
		RETURN_LONG(EINVAL);
	}
	
	if (get_trace_context(&ztc, &tc) != 0) { 
		RETURN_FALSE;
	}

	/*
	 * turn flag off
	 */

	if (trace_off(tc, flag)) {
		RETURN_FALSE;
	}
	else {
		RETURN_TRUE;
	}
}


/********************
 * trace_write
 ********************/
PHP_FUNCTION(trace_write)
{
	zval      **arg;
	HashTable  *ztags;
	
	trace_context_t *tc;
	int              ntag, flag;
	char            *format;
	int              format_len;
	void            *args[32], **p;
	int              narg, n, status;

	p    = EG(argument_stack).top_element - 2;
	narg = (unsigned long)*p;

	if (narg < 4) {
		WRONG_PARAM_COUNT;
	}


	/*
	 * get context, flag, tags, and format string
	 */

	/* context */
	arg = (zval **)p - narg;
	switch (Z_TYPE_PP(arg)) {
	case IS_RESOURCE:
		if ((status = get_trace_context(arg, &tc)) != 0) { 
			RETURN_FALSE;
		}
		break;
	default:
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid context");
		RETURN_FALSE;
	}
	narg--;

	/* flag */
	arg = (zval **)p - narg;
	if (Z_TYPE_PP(arg) != IS_LONG) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "trace flags must be integers");
		RETURN_FALSE;
	}
	flag = Z_LVAL_PP(arg);
	narg--;

	/* tags */
	arg = (zval **)p - narg;
	switch (Z_TYPE_PP(arg)) {
	case IS_ARRAY:
		ztags = Z_ARRVAL_PP(arg);
		ntag  = zend_hash_num_elements(ztags);
		break;
	case IS_NULL:
		ztags = NULL;
		ntag  = 0;
		break;
	default:
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "tags must be an array or NULL");
		RETURN_FALSE;
	}
	narg--;
	
	/* format */
	arg = (zval **)p - narg;
	if (Z_TYPE_PP(arg) != IS_STRING) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "format not a string");
		RETURN_FALSE;
	}
	format = Z_STRVAL_PP(arg);
	narg--;
	
	
	if (narg > (sizeof(args) / sizeof(args[0]))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "too many arguments after format string (max 32)");
		RETURN_FALSE;
	}

	/*
	 * construct variable argument list for trace_writel
	 *
	 *     Notes: XXX TODO: Constructing argument lists dynamically needs
	 *       to be hidden behind behind a set of macros (stdcall ?) as it is
	 *       highly platform/CPU-dependent.
	 */
	
	n = 0;
	while (narg > 0) {
		arg = (zval **)p - narg;
		switch (Z_TYPE_PP(arg)) {
		case IS_NULL:
			args[n++] = NULL;
			break;
			
		case IS_BOOL:
			args[n++] = (void *)(int)Z_BVAL_PP(arg);
			break;
			
		case IS_LONG:
			args[n++] = (void *)Z_LVAL_PP(arg);
			break;
			
		case IS_CONSTANT:
			args[n++] = (void *)333;
			break;

		case IS_STRING:
			args[n++] = (void *)Z_STRVAL_PP(arg);
			break;
			
		case IS_DOUBLE:
			*(double *)(args + n) = Z_DVAL_PP(arg);
			n += 2;
			break;
		}
		narg--;
	}
	
	
	if (ztags) {
		TRACE_DECLARE_TAGS(tags, ntag, 0);
		HashPosition       hp;
		zval             **zv;
		char              *k, *v;
		int                kt, kl, vl;
		long               idx;
		
		zend_hash_internal_pointer_reset_ex(ztags, &hp);
		kt = !HASH_KEY_NON_EXISTANT;
		while (kt != HASH_KEY_NON_EXISTANT) {
			kt = zend_hash_get_current_key_ex(ztags, &k, &kl, &idx, 0, &hp);
			switch (kt) {
			case HASH_KEY_IS_STRING:
				break;
			case HASH_KEY_NON_EXISTANT:
				continue;
			default:
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
								 "ignoring non-string tag key");
				goto next;
			}

			status = zend_hash_get_current_data_ex(ztags, (void**)&zv, &hp);
			if (status != SUCCESS) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
								 "failed to fetch tag value");
				goto next;
			}
			switch (Z_TYPE_PP(zv)) {
			case IS_STRING: v = Z_STRVAL_PP(zv); break;
			case IS_NULL:   v = "";              break;
			default:
				if (Z_TYPE_PP(zv) != IS_STRING) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING,
									 "ignoring non-string type tag value (%d)",
									 Z_TYPE_PP(zv));
					goto next;
				}
			}
			trace_tag_add(&tags, k, v);
		next:
			kt = zend_hash_move_forward_ex(ztags, &hp);
		}

		__trace_writel(tc, 
					   zend_get_executed_filename(TSRMLS_C),
					   zend_get_executed_lineno(TSRMLS_C),
					   get_active_function_name(TSRMLS_C),
					   flag, &tags, format, (va_list)args);
	}
	else {
		__trace_writel(tc, 
					  zend_get_executed_filename(TSRMLS_C),
					  zend_get_executed_lineno(TSRMLS_C),
					  get_active_function_name(TSRMLS_C),
					  flag, NULL, format, (va_list)args);
	}

	RETURN_TRUE;
}


/********************
 * __trace_write
 ********************/
PHP_FUNCTION(__trace_write)
{
	zval      **arg;
	HashTable  *ztags;
	
	trace_context_t *tc;
	int              ntag, flag;
	char            *format, *file, *func;
	int              format_len, line;
	void            *args[32], **p;
	int              narg, n, status;

	p    = EG(argument_stack).top_element - 2;
	narg = (unsigned long)*p;

	if (narg < 7) {
		WRONG_PARAM_COUNT;
	}


	/*
	 * get context, flag, tags, and format string
	 */

	/* context */
	arg = (zval **)p - narg;
	switch (Z_TYPE_PP(arg)) {
	case IS_RESOURCE:
		if ((status = get_trace_context(arg, &tc)) != 0) { 
			RETURN_FALSE;
		}
		break;
	default:
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid context");
		RETURN_FALSE;
	}
	narg--;

	/* file */
	arg = (zval **)p - narg;
	if (Z_TYPE_PP(arg) != IS_STRING) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "trace location file must be a string");
		RETURN_FALSE;
	}
	file = Z_STRVAL_PP(arg);
	narg--;

	/* line */
	arg = (zval **)p - narg;
	if (Z_TYPE_PP(arg) != IS_LONG) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "trace location line must be an integer");
		RETURN_FALSE;
	}
	line = Z_LVAL_PP(arg);
	narg--;

	/* function */
	arg = (zval **)p - narg;
	if (Z_TYPE_PP(arg) != IS_STRING) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "trace location function must be a string");
		RETURN_FALSE;
	}
	func = Z_STRVAL_PP(arg);
	narg--;


	/* flag */
	arg = (zval **)p - narg;
	if (Z_TYPE_PP(arg) != IS_LONG) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "trace flags must be integers");
		RETURN_FALSE;
	}
	flag = Z_LVAL_PP(arg);
	narg--;

	/* tags */
	arg = (zval **)p - narg;
	switch (Z_TYPE_PP(arg)) {
	case IS_ARRAY:
		ztags = Z_ARRVAL_PP(arg);
		ntag  = zend_hash_num_elements(ztags);
		break;
	case IS_NULL:
		ztags = NULL;
		ntag  = 0;
		break;
	default:
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "tags must be an array or NULL");
		RETURN_FALSE;
	}
	narg--;
	
	/* format */
	arg = (zval **)p - narg;
	if (Z_TYPE_PP(arg) != IS_STRING) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "format not a string");
		RETURN_FALSE;
	}
	format = Z_STRVAL_PP(arg);
	narg--;
	
	
	if (narg > (sizeof(args) / sizeof(args[0]))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "too many arguments after format string (max 32)");
		RETURN_FALSE;
	}

	/*
	 * construct variable argument list for trace_writel
	 *
	 *     Notes: XXX TODO: Constructing argument lists dynamically needs
	 *       to be hidden behind behind a set of macros (stdcall ?) as it is
	 *       highly platform/CPU-dependent.
	 */
	
	n = 0;
	while (narg > 0) {
		arg = (zval **)p - narg;
		switch (Z_TYPE_PP(arg)) {
		case IS_NULL:
			args[n++] = NULL;
			break;
			
		case IS_BOOL:
			args[n++] = (void *)(int)Z_BVAL_PP(arg);
			break;
			
		case IS_LONG:
			args[n++] = (void *)Z_LVAL_PP(arg);
			break;
			
		case IS_CONSTANT:
			args[n++] = (void *)333;
			break;

		case IS_STRING:
			args[n++] = (void *)Z_STRVAL_PP(arg);
			break;
			
		case IS_DOUBLE:
			*(double *)(args + n) = Z_DVAL_PP(arg);
			n += 2;
			break;
		}
		narg--;
	}
	
	
	if (ztags) {
		TRACE_DECLARE_TAGS(tags, ntag, 0);
		HashPosition       hp;
		zval             **zv;
		char              *k, *v;
		int                kt, kl, vl;
		long               idx;
		
		zend_hash_internal_pointer_reset_ex(ztags, &hp);
		kt = !HASH_KEY_NON_EXISTANT;
		while (kt != HASH_KEY_NON_EXISTANT) {
			kt = zend_hash_get_current_key_ex(ztags, &k, &kl, &idx, 0, &hp);
			switch (kt) {
			case HASH_KEY_IS_STRING:
				break;
			case HASH_KEY_NON_EXISTANT:
				continue;
			default:
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
								 "ignoring non-string tag key");
				goto next;
			}

			status = zend_hash_get_current_data_ex(ztags, (void**)&zv, &hp);
			if (status != SUCCESS) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
								 "failed to fetch tag value");
				goto next;
			}
			switch (Z_TYPE_PP(zv)) {
			case IS_STRING: v = Z_STRVAL_PP(zv); break;
			case IS_NULL:   v = "";              break;
			default:
				if (Z_TYPE_PP(zv) != IS_STRING) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING,
									 "ignoring non-string type tag value (%d)",
									 Z_TYPE_PP(zv));
					goto next;
				}
			}
			trace_tag_add(&tags, k, v);
		next:
			kt = zend_hash_move_forward_ex(ztags, &hp);
		}

		__trace_writel(tc, file, line, func,
					   flag, &tags, format, (va_list)args);
	}
	else
		__trace_writel(tc, file, line, func,
					   flag, NULL, format, (va_list)args);
	
	RETURN_TRUE;
}


/********************
 * trace_reset_filters
 ********************/
static
PHP_FUNCTION(trace_reset_filters)
{
	zval            *ztc;
	trace_context_t *tc;
	
	/*
	 * parse & check arguments, fetch context
	 */

	if (ZEND_NUM_ARGS() != 1) {
		WRONG_PARAM_COUNT;
	}
	
	if (zend_parse_parameters(1 TSRMLS_CC, "r", &ztc) != SUCCESS) {
		RETURN_FALSE;
	}

	if (get_trace_context(&ztc, &tc) != 0) { 
		RETURN_FALSE;
	}
	
	trace_reset_filters(tc);
	RETURN_TRUE;
}


/********************
 * trace_add_simple_filter
 ********************/
static
PHP_FUNCTION(trace_add_simple_filter)
{
	zval            *ztc;
	trace_context_t *tc;
	char             *filter;
	int               filter_len;
	
	/*
	 * parse & check arguments, fetch context
	 */

	if (ZEND_NUM_ARGS() != 2) {
		WRONG_PARAM_COUNT;
	}

	if (zend_parse_parameters(2 TSRMLS_CC, "rs",
							  &ztc, &filter, &filter_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (get_trace_context(&ztc, &tc) != 0) { 
		RETURN_FALSE;
	}

	if (trace_add_simple_filter(tc, filter)) {
		RETURN_FALSE;
	}
	else {
		RETURN_TRUE;
	}
}


/********************
 * trace_add_regexp_filter
 ********************/
static
PHP_FUNCTION(trace_add_regexp_filter)
{
	zval            *ztc;
	trace_context_t *tc;
	char             *filter;
	int               filter_len;
	
	/*
	 * parse & check arguments, fetch context
	 */

	if (ZEND_NUM_ARGS() != 2) {
		WRONG_PARAM_COUNT;
	}

	if (zend_parse_parameters(2 TSRMLS_CC, "rs",
							  &ztc, &filter, &filter_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (get_trace_context(&ztc, &tc) != 0) { 
		RETURN_FALSE;
	}

	if (trace_add_regexp_filter(tc, filter)) {
		RETURN_FALSE;
	}
	else {
		RETURN_TRUE;
	}
}



/********************
 * alloc_component
 ********************/
trace_component_t *
alloc_component(char *name, HashTable *hflags, zval ****bitret,
				int *err TSRMLS_DC)
{
	trace_component_t *c;
	trace_flag_t      *flags, *f;
	int                nflag, len;

	zval              **zval, **zfd, **zflag, ***br;
	HashPosition        hp;
	char               *k;
	int                 kl, kt;
	long                idx;
	int                 status;

	*bitret = NULL;
	nflag   = zend_hash_num_elements(hflags);

	if ((c = emalloc(sizeof(*c))) == NULL) {
		status = ENOMEM;
		goto failed;
	}
	memset(c, 0, sizeof(*c));

	if ((c->name = estrdup(name)) == NULL) {
		status = ENOMEM;
		goto failed;
	}		   

	if ((c->flags = emalloc((nflag + 1) * sizeof(*c->flags))) == NULL) {
		status = ENOMEM;
		goto failed;
	}
	memset(c->flags, 0, (nflag + 1) * sizeof(*c->flags));

	if ((br = emalloc((nflag + 1) * sizeof(br[0]))) == NULL) {
		status = ENOMEM;
		goto failed;
	}
	memset(br, 0, (nflag + 1) * sizeof(br[0]));
	*bitret = br;

	zend_hash_internal_pointer_reset_ex(hflags, &hp);
	kt = !HASH_KEY_NON_EXISTANT;
	f  = c->flags;
	while (kt != HASH_KEY_NON_EXISTANT) {
		kt = zend_hash_get_current_key_ex(hflags, &k, &kl, &idx, 0, &hp);
		switch (kt) {
		case HASH_KEY_IS_STRING:
			break;
		case HASH_KEY_NON_EXISTANT:
			continue;
		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
							 "ignoring flag with non-string key");
			goto next;
		}

		if ((f->name = estrdup(k)) == NULL) {
			status = ENOMEM;
			goto failed;
		}
		/*printf("  flag \"%s\"\n", f->name);*/
		

		status = zend_hash_get_current_data_ex(hflags, (void**)&zval, &hp);
		if (status != SUCCESS || Z_TYPE_PP(zval) != IS_ARRAY) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
							 "ignoring non-array flag");
			goto next;
		}
		
		status = zend_hash_find(Z_ARRVAL_PP(zval),
								KEY_DESCR, sizeof(KEY_DESCR), (void **)&zfd);
		
		if (status != SUCCESS || Z_TYPE_PP(zfd) != IS_STRING) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
							 "ignore flag with missing or invalid description");
			goto next;
		}

		if ((f->description = estrdup(Z_STRVAL_PP(zfd))) == NULL) {
			status = ENOMEM;
			goto failed;
		}
		
		/*printf("    description: %s\n", f->description);*/

		status = zend_hash_find(Z_ARRVAL_PP(zval),
								KEY_FLAG, sizeof(KEY_FLAG), (void **)&zflag);
		if (status != SUCCESS || !PZVAL_IS_REF(*zflag)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
							 "ignore flag with missing or invalid reference");
			goto next;
		}
		*br++ = zflag;

		f++;

	next:
		kt = zend_hash_move_forward_ex(hflags, &hp);
	}
	
	f->name = f->description = NULL;
	*br     = NULL;
	
	return c;

 failed:
	if (c) {
		efree(c->name);
		if (c->flags) {
			for (f = c->flags; f->name; f++) {
				if (f->name)
					efree(f->name);
				if (f->description)
					efree(f->description);
			}
			efree(c->flags);
		}
		efree(c);
	}
	if (*bitret)
		efree(*bitret);
	*bitret = NULL;
	
	return NULL;
}




/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
