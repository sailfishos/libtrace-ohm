/*************************************************************************
This file is part of libtrace

Copyright (C) 2010 Nokia Corporation.

This library is free software; you can redistribute
it and/or modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation
version 2.1 of the License.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
USA.
*************************************************************************/


#include <Python.h>
#include <traceback.h>
#include <frameobject.h>

#include <stdarg.h>
#include <trace/trace.h>

#if PY_VERSION_HEX < 0x02050000 && !defined(PY_SSIZE_T_MIN)
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif

/* key names for dictionaries in component registration */
#define KEY_NAME  "name"
#define KEY_FLAGS "flags"
#define KEY_DESCR "description"
#define KEY_FLAG  "flag"

static trace_component_t *alloc_component(const char *, PyObject *,PyObject ***);


/********************
 * trace_init
 ********************/
static PyObject *
nsntrace_init(PyObject *self, PyObject *args)
{
	int status;
	
	if ((status = trace_init()) != 0) {
		PyErr_SetString(PyExc_RuntimeError,
						"failed to initialize trace library");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}


/********************
 * trace_exit
 ********************/
static PyObject *
nsntrace_exit(PyObject *self, PyObject *args)
{
	trace_exit();

	Py_INCREF(Py_None);
	return Py_None;
}


/********************
 * trace_context_destr
 ********************/
static void
trace_context_destr(void *ptr)
{
	trace_close((trace_context_t *)ptr);
	free(ptr);
}


/********************
 * trace_open
 ********************/
static PyObject *
nsntrace_open(PyObject *self, PyObject *args)
{
	trace_context_t *tc;
	const char      *name;
	int              status;
	
	
	/*
	 * parse arguments
	 */

	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;
	

	/*
	 * allocate and open a new trace context
	 */

	if ((tc = malloc(sizeof(*tc))) == NULL) {
		PyErr_SetString(PyExc_MemoryError, "failed to allocate trace context");
		return NULL;
	}
	memset(tc, 0, sizeof(*tc));
	
	if ((status = trace_open(tc, (char *)name)) != 0) {
		free(tc);
		PyErr_SetString(PyExc_RuntimeError, "failed to open trace context");
		return NULL;
	}

#if 0
	/* XXX TODO: kludge until the filtering interface is mapped to PHP */
	trace_add_simple_filter(tc, TRACE_FILTER_NO_TAGS);
#endif

	return PyCObject_FromVoidPtr((void *)tc, trace_context_destr);
}


/********************
 * trace_close
 ********************/
static PyObject *
nsntrace_close(PyObject *self, PyObject *args)
{
	PyObject *ctc;

	if (!PyArg_ParseTuple(args, "O", &ctc))
		return NULL;

	if (!PyCObject_Check(ctc)) {
		PyErr_SetString(PyExc_RuntimeError, "invalid trace context");
		return NULL;
	}

	Py_DECREF(ctc);

	Py_INCREF(Py_None);
	return Py_None;
}


/********************
 * get_trace_context
 ********************/
static trace_context_t *
get_trace_context(PyObject *ctc)
{
	return PyCObject_AsVoidPtr(ctc);
}


/********************
 * trace_add_component
 ********************/
static PyObject *
nsntrace_add_component(PyObject *self, PyObject *args)
{
	PyObject          *ctc;
	trace_context_t   *tc;
	trace_component_t *c;
	trace_flag_t      *f;
	PyObject          *dict, *obj, **flags, *retval;
	const char        *name;
	int                i;

	if (!PyArg_ParseTuple(args, "OO", &ctc, &dict))
		return NULL;
	
	if ((tc = get_trace_context(ctc)) == NULL)
		return NULL;
	
	if (!PyDict_Check(dict)) {
		PyErr_SetString(PyExc_RuntimeError, "requires a dictionary argument");
		return NULL;
	}
	
	/*
	 * dig out component name and dictionary of flags
	 */
	
	/* 'name': name */
	if ((obj = PyDict_GetItemString(dict, KEY_NAME)) == NULL) {
		PyErr_Format(PyExc_RuntimeError,
					 "no key \"%s\" in dictionary", KEY_NAME);
		return NULL;
	}
	if ((name = PyString_AsString(obj)) == NULL)
		return NULL;
	if (!name[0]) {
		PyErr_Format(PyExc_RuntimeError,
					 "key \"%s\" has empty value", KEY_NAME);
		return NULL;
	}

	/* 'flags': { ... } */
	if ((obj = PyDict_GetItemString(dict, KEY_FLAGS)) == NULL) {
		PyErr_Format(PyExc_RuntimeError,
					 "no key \"%s\" in dictionary", KEY_FLAGS);
		return NULL;
	}
	if (!PyDict_Check(obj)) {
		PyErr_Format(PyExc_RuntimeError,
					 "key \"%s\" should have a dictionary as value", KEY_FLAGS);
		return NULL;
	}
	
	/*
	 * allocate a corresponding trace_component_t *
	 */
	
	if ((c = alloc_component(name, obj, &flags)) == NULL)
		return NULL;
	

	/*
	 * register component and let the caller know the flags
	 */

	if (trace_add_component(tc, c)) {
		PyErr_SetString(PyExc_RuntimeError,
						"failed to register trace component");
		retval = NULL;
	}
	else {
		for (f = c->flags, i = 0; f->name; f++, i++) {
			if (flags[i])
				PyDict_SetItemString(flags[i], KEY_FLAG,
									 Py_BuildValue("i", f->bit));
		}
		Py_INCREF(Py_None);
		retval = Py_None;
	}
	
	if (flags)
		free(flags);
	
	return retval;
}


/********************
 * trace_del_component
 ********************/
static PyObject *
nsntrace_del_component(PyObject *self, PyObject *args)
{
	PyObject        *ctc;
	trace_context_t *tc;
	const char      *name;
	
	if (!PyArg_ParseTuple(args, "Os", &ctc, &name))
		return NULL;

	if ((tc = get_trace_context(ctc)) == NULL)
		return NULL;
	
	if (trace_del_component(tc, name)) {
		PyErr_Format(PyExc_RuntimeError,
					 "failed to delete trace component \"%s\"", name);
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}


/********************
 * trace_set_header
 ********************/
static PyObject *
nsntrace_set_header(PyObject *self, PyObject *args)
{
	PyObject        *ctc;
	trace_context_t *tc;
	const char      *format;
	
	if (!PyArg_ParseTuple(args, "Os", &ctc, &format))
		return NULL;

	if ((tc = get_trace_context(ctc)) == NULL)
		return NULL;
	
	if (trace_set_header(tc, format)) {
		PyErr_SetString(PyExc_RuntimeError, "failed to set trace header");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}


/********************
 * trace_set_destination
 ********************/
static PyObject *
nsntrace_set_destination(PyObject *self, PyObject *args)
{
	PyObject        *ctc;
	PyObject        *dest_obj;
	trace_context_t *tc;
	long             td;
	const char      *destination;
	
	if (!PyArg_ParseTuple(args, "OO", &ctc, &dest_obj))
		return NULL;
	
	if ((tc = get_trace_context(ctc)) == NULL)
		return NULL;

	if (PyInt_Check(dest_obj)) {
		td = PyInt_AsLong(dest_obj);
		switch (td) {
		case (int)TRACE_TO_STDERR:
		case (int)TRACE_TO_STDOUT:
			destination = (const char *)td;
			break;
		default:
			PyErr_SetString(PyExc_RuntimeError,
							"invalid trace context destination");
			return NULL;
		}
	}
	else if (PyString_Check(dest_obj)) {
		destination = PyString_AsString(dest_obj);
	}
	else {
		PyErr_SetString(PyExc_RuntimeError,
						"invalid trace context destination");
		return NULL;
	}
	
	if (trace_set_destination(tc, destination)) {
		PyErr_SetString(PyExc_RuntimeError, "failed to set trace destination");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}


/********************
 * trace_enable
 ********************/
static PyObject *
nsntrace_enable(PyObject *self, PyObject *args)
{
	PyObject        *ctc;
	trace_context_t *tc;

	if (!PyArg_ParseTuple(args, "O", &ctc))
		return NULL;
	
	if ((tc = get_trace_context(ctc)) == NULL)
		return NULL;

	return Py_BuildValue("i", trace_enable(tc));
}


/********************
 * trace_disable
 ********************/
static PyObject *
nsntrace_disable(PyObject *self, PyObject *args)
{
	PyObject        *ctc;
	trace_context_t *tc;

	if (!PyArg_ParseTuple(args, "O", &ctc))
		return NULL;
	
	if ((tc = get_trace_context(ctc)) == NULL)
		return NULL;

	return Py_BuildValue("i", trace_disable(tc));
}


/********************
 * trace_on
 ********************/
static PyObject *
nsntrace_on(PyObject *self, PyObject *args)
{
	PyObject        *ctc;
	trace_context_t *tc;
	int              flag;

	if (!PyArg_ParseTuple(args, "Oi", &ctc, &flag))
		return NULL;
	
	if ((tc = get_trace_context(ctc)) == NULL)
		return NULL;

	if (trace_on(tc, flag) != 0) {
		PyErr_Format(PyExc_RuntimeError,
					 "failed to turn on trace flag %d", flag);
		return NULL;
	}
	else {
		Py_INCREF(Py_None);
		return Py_None;
	}
}


/********************
 * trace_off
 ********************/
static PyObject *
nsntrace_off(PyObject *self, PyObject *args)
{
	PyObject        *ctc;
	trace_context_t *tc;
	int              flag;

	if (!PyArg_ParseTuple(args, "Oi", &ctc, &flag))
		return NULL;
	
	if ((tc = get_trace_context(ctc)) == NULL)
		return NULL;

	if (trace_off(tc, flag) != 0) {
		PyErr_Format(PyExc_RuntimeError,
					 "failed to turn on trace flag %d", flag);
		return NULL;
	}
	else {
		Py_INCREF(Py_None);
		return Py_None;
	}
}


/********************
 * next_directive
 ********************/
static const char *
next_directive(const char *p)
{
	if (p == NULL)
		return NULL;

	while (*p) {
		if (*p != '%') {
			p++;
			continue;
		}
		
		p++;
		/* pass %% through */
		if (*p == '%') {
			p++;
			continue;
		}
		/* skip optional +|- */
		if (*p == '-' || *p == '+')
			p++;
		/* skip optional flags */
		while (*p == '#' || *p == '0' || *p == 'I')
			p++;
		/* skip optional field width */
		while ('0' <= *p && *p <= '9')
			p++;
		/* skip optional precision */
		if (*p == '.') {
			p++;
			while ('0' <= *p && *p <= '9')
				p++;
		}
		/* skip optional length modifiers */
		switch (*p) {
		case 'h': p = p + (p[1] == 'h' ? 2 : 1); break;
		case 'l': p = p + (p[1] == 'l' ? 2 : 1); break;
		case 'L': p++; break;
		case 'j': p++; break;
		case 'z': p++; break;
		case 't': p++; break;
		}
		
		return p;
		p++;
	}

	return NULL;
}


/********************
 * get_location
 ********************/
static int
get_location(const char **function, const char **file, int *line)
{
	PyThreadState *ts = PyThreadState_GET();
	
	if (ts == NULL) {
		*function = *file = "unknown";
		*line = 0;
		return -1;
	}
	
	*function = PyString_AsString(ts->frame->f_code->co_name);
	*file     = PyString_AsString(ts->frame->f_code->co_filename);
	*line     = PyCode_Addr2Line(ts->frame->f_code, ts->frame->f_lasti);
	return 0;

}


/********************
 * nsntrace_write
 ********************/
static PyObject *
nsntrace_write(PyObject *self, PyObject *args)
{
	PyObject        *obj, *tags_obj, *key, *value;
	Py_ssize_t       n, i, ai, pos;
	trace_context_t *tc;
	int              flag;
	const char      *file, *function;
	int              line, ntag;
	const char      *format, *dp, *s;
	long             l;
	double           d;
	void            *argv[64];
	
	if ((n = PyTuple_Size(args)) < 4) {
		PyErr_SetString(PyExc_RuntimeError, "too few arguments");
		return NULL;
	}

	i = 0;

	/* get trace context */
	obj = PyTuple_GET_ITEM(args, i++);
	if ((tc = get_trace_context(obj)) == NULL)
		return NULL;
	
	/* get trace flag */
	obj = PyTuple_GET_ITEM(args, i++);
	if ((flag = PyInt_AsLong(obj)) == -1 && PyErr_Occurred() != NULL)
		return NULL;
	
	/* get tags */
	tags_obj = PyTuple_GET_ITEM(args, i++);
	if (!PyDict_Check(tags_obj) && tags_obj != Py_None) {
		PyErr_SetString(PyExc_TypeError, "tags must be a dictionary or None");
		return NULL;
	}
	if (tags_obj == Py_None)
		ntag = 0;
	else
		ntag = PyDict_Size(tags_obj);

	/* get format string */
	obj = PyTuple_GET_ITEM(args, i++);
	if ((format = PyString_AsString(obj)) == NULL)
		return NULL;
	
	/* check and 'push' arguments */
	for (dp = next_directive(format), ai = 0;
		 dp != NULL && i < n && ai < sizeof(argv) / sizeof(argv[0]);
		 dp = next_directive(dp), i++, ai++) {
		obj = PyTuple_GET_ITEM(args, i);
		switch (*dp) {
		case 'd':
		case 'i':
		case 'o':
		case 'u':
		case 'x':
		case 'X':
		case 'c':
			if ((l = PyInt_AsLong(obj)) == -1 && PyErr_Occurred() != NULL)
				return NULL;
			argv[ai] = (void *)l;
			break;
		case 's':
			if ((s = PyString_AsString(obj)) == NULL)
				return NULL;
			argv[ai] = (void *)s;
			break;
		case 'e': case 'E':
		case 'f': case 'F':
		case 'g': case 'G':
		case 'a': case 'A':
			d = PyFloat_AsDouble(obj);
			if (PyErr_Occurred())
				return NULL;
			*(double *)(argv + ai) = d;
			ai++;
			break;
		default:
			PyErr_Format(PyExc_RuntimeError,
						 "invalid format directive %c", *dp);
			return NULL;
		}
	}

	/*
	 * determine location, construct tags, and produce a trace message
	 */
	
	get_location(&function, &file, &line);	

	{
		TRACE_DECLARE_TAGS(tags, ntag, 0);
		
		pos = 0;
		while (PyDict_Next(tags_obj, &pos, &key, &value)) {
			if (!PyString_Check(key) || !PyString_Check(value)) {
				PyErr_Warn(PyExc_ValueError,
						   "tag key or value not a string");
				continue;
			}
			trace_tag_add(&tags,
						  PyString_AsString(key), PyString_AsString(value));
		}
		
		__trace_writel(tc, file, line, function, flag, ntag ? &tags : NULL,
					   (char *)format, (va_list)argv);
	}
	
	/*log_write(lc, level, format);*/
	
	Py_INCREF(Py_None);
	return Py_None;
}


/********************
 * nsn__trace_write
 ********************/
static PyObject *
nsn__trace_write(PyObject *self, PyObject *args)
{
	PyObject        *obj, *tags_obj, *key, *value;
	Py_ssize_t       n, i, ai, pos;
	trace_context_t *tc;
	int              flag;
	const char      *file, *function;
	int              line, ntag;
	const char      *format, *dp, *s;
	long             l;
	double           d;
	void            *argv[64];
	
	if ((n = PyTuple_Size(args)) < 4) {
		PyErr_SetString(PyExc_RuntimeError, "too few arguments");
		return NULL;
	}

	i = 0;

	/* get trace context */
	obj = PyTuple_GET_ITEM(args, i++);
	if ((tc = get_trace_context(obj)) == NULL)
		return NULL;
	
	/* get trace file */
	obj = PyTuple_GET_ITEM(args, i++);
	if ((file = PyString_AsString(obj)) == NULL && PyErr_Occurred() != NULL)
		return NULL;

	/* get trace line */
	obj = PyTuple_GET_ITEM(args, i++);
	if ((line = PyInt_AsLong(obj)) == -1 && PyErr_Occurred() != NULL)
		return NULL;

	/* get trace function */
	obj = PyTuple_GET_ITEM(args, i++);
	if ((function = PyString_AsString(obj)) == NULL && PyErr_Occurred() != NULL)
		return NULL;

	/* get trace flag */
	obj = PyTuple_GET_ITEM(args, i++);
	if ((flag = PyInt_AsLong(obj)) == -1 && PyErr_Occurred() != NULL)
		return NULL;
	
	/* get tags */
	tags_obj = PyTuple_GET_ITEM(args, i++);
	if (!PyDict_Check(tags_obj) && tags_obj != Py_None) {
		PyErr_SetString(PyExc_TypeError, "tags must be a dictionary or None");
		return NULL;
	}
	if (tags_obj == Py_None)
		ntag = 0;
	else
		ntag = PyDict_Size(tags_obj);

	/* get format string */
	obj = PyTuple_GET_ITEM(args, i++);
	if ((format = PyString_AsString(obj)) == NULL)
		return NULL;
	
	/* check and 'push' arguments */
	for (dp = next_directive(format), ai = 0;
		 dp != NULL && i < n && ai < sizeof(argv) / sizeof(argv[0]);
		 dp = next_directive(dp), i++, ai++) {
		obj = PyTuple_GET_ITEM(args, i);
		switch (*dp) {
		case 'd':
		case 'i':
		case 'o':
		case 'u':
		case 'x':
		case 'X':
		case 'c':
			if ((l = PyInt_AsLong(obj)) == -1 && PyErr_Occurred() != NULL)
				return NULL;
			argv[ai] = (void *)l;
			break;
		case 's':
			if ((s = PyString_AsString(obj)) == NULL)
				return NULL;
			argv[ai] = (void *)s;
			break;
		case 'e': case 'E':
		case 'f': case 'F':
		case 'g': case 'G':
		case 'a': case 'A':
			d = PyFloat_AsDouble(obj);
			if (PyErr_Occurred())
				return NULL;
			*(double *)(argv + ai) = d;
			ai++;
			break;
		default:
			PyErr_Format(PyExc_RuntimeError,
						 "invalid format directive %c", *dp);
			return NULL;
		}
	}

	/*
	 * construct tags, and produce a trace message
	 */
	
	{
		TRACE_DECLARE_TAGS(tags, ntag, 0);
		
		pos = 0;
		while (PyDict_Next(tags_obj, &pos, &key, &value)) {
			if (!PyString_Check(key) || !PyString_Check(value)) {
				PyErr_Warn(PyExc_ValueError,
						   "tag key or value not a string");
				continue;
			}
			trace_tag_add(&tags,
						  PyString_AsString(key), PyString_AsString(value));
		}
		
		__trace_writel(tc, file, line, function, flag, ntag ? &tags : NULL,
					   (char *)format, (va_list)argv);
	}
	
	/*log_write(lc, level, format);*/
	
	Py_INCREF(Py_None);
	return Py_None;
}


/********************
 * trace_reset_filters
 ********************/
static PyObject *
nsntrace_reset_filters(PyObject *self, PyObject *args)
{
	PyObject        *ctc;
	trace_context_t *tc;
	
	if (!PyArg_ParseTuple(args, "O", &ctc))
		return NULL;

	if ((tc = get_trace_context(ctc)) == NULL)
		return NULL;

	trace_reset_filters(tc);
	Py_INCREF(Py_None);
	return Py_None;
}


/********************
 * trace_add_simple_filter
 ********************/
static PyObject *
nsntrace_add_simple_filter(PyObject *self, PyObject *args)
{
	PyObject        *ctc;
	trace_context_t *tc;
	char            *filter;
	
	if (!PyArg_ParseTuple(args, "Os", &ctc, &filter))
		return NULL;

	if ((tc = get_trace_context(ctc)) == NULL)
		return NULL;
	
	if (trace_add_simple_filter(tc, filter)) {
		PyErr_SetString(PyExc_RuntimeError, "failed to add simple filter");
		return NULL;
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}


/********************
 * trace_add_regexp_filter
 ********************/
static PyObject *
nsntrace_add_regexp_filter(PyObject *self, PyObject *args)
{
	PyObject        *ctc;
	trace_context_t *tc;
	char            *filter;
	
	if (!PyArg_ParseTuple(args, "Os", &ctc, &filter))
		return NULL;

	if ((tc = get_trace_context(ctc)) == NULL)
		return NULL;
	
	if (trace_add_regexp_filter(tc, filter)) {
		PyErr_SetString(PyExc_RuntimeError, "failed to add regexp filter");
		return NULL;
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}


/*
 * module methods, docstrings
 */

#define DOC(name, args, docstr) \
	PyDoc_STRVAR(name##__doc__, #name #args " -- " docstr);

DOC(trace_init, (), "Initialize the low-level NSN tracing library.");
DOC(trace_exit, (), "Uninitialize the low-level NSN tracing library.");
DOC(trace_open, (name), "Create a new trace context.");
DOC(trace_close, (ctx), "Close a trace context.");
DOC(trace_add_component, (ctx, comp), "Register a component for tracing.");
DOC(trace_del_component, (ctx, name), "Unregister the specified component from tracing.");
DOC(trace_enable, (ctx), "Enable tracing for the given context.");
DOC(trace_disable, (ctx), "Disable tracing for the given context.");
DOC(trace_on, (ctx, flag), "Turn on tracing for flag in the given context.");
DOC(trace_off, (ctx, flag), "Turn off tracing for flag in the given context.");
DOC(trace_set_header, (ctx, format), "Set trace header format for ctx.");
DOC(trace_set_destination, (ctx, dest), "Set trace destination for ctx.");
DOC(trace_write, (ctx, flag, format, ...), "Emit a trace message in ctx.");
DOC(__trace_write, (ctx, file, line, function, flag, format, ...), "Emit a trace message in ctx.");
DOC(trace_reset_filters, (ctx), "Clear all filters from ctx.");
DOC(trace_add_simple_filter, (ctx, filter), "Add a simple filter to the ctx.");
DOC(trace_add_regexp_filter, (ctx, filter), "Add a regexp filter to the ctx.");

#define METHOD(m, args) { #m, nsn##m, METH_##args, m##__doc__ }

static PyMethodDef nsntrace_methods[] = {
	METHOD(trace_init             , NOARGS),
	METHOD(trace_exit             , NOARGS),
	METHOD(trace_open             , VARARGS),
	METHOD(trace_close            , VARARGS),
	METHOD(trace_add_component    , VARARGS),
	METHOD(trace_del_component    , VARARGS),
	METHOD(trace_enable           , VARARGS),
	METHOD(trace_disable          , VARARGS),
	METHOD(trace_on               , VARARGS),
	METHOD(trace_off              , VARARGS),
	METHOD(trace_set_header       , VARARGS),
	METHOD(trace_set_destination  , VARARGS),
	METHOD(trace_write            , VARARGS),
	METHOD(__trace_write          , VARARGS),
	METHOD(trace_reset_filters    , VARARGS),
	METHOD(trace_add_simple_filter, VARARGS),
	METHOD(trace_add_regexp_filter, VARARGS),
	{NULL, NULL, 0, NULL}
};


/********************
 * init_nsntrace
 ********************/
PyMODINIT_FUNC
init_nsntrace(void)
{
	PyObject *m;


	/*
	 * register our module
	 */

	if ((m = Py_InitModule("_nsntrace", nsntrace_methods)) == NULL)
		return;

	/*
	 * define our constants
	 */
	
#define ADD_INT(n, v)   if (PyModule_AddIntConstant(m, (n), (v)) < 0) return
#define ADD_CONSTANT(i) ADD_INT(#i, i)
	
	/* special trace destinations */
	ADD_INT("TRACE_TO_STDERR", (int)TRACE_TO_STDERR);
	ADD_INT("TRACE_TO_STDOUT", (int)TRACE_TO_STDOUT);
}



/********************
 * alloc_component
 ********************/
static trace_component_t *
alloc_component(const char *name, PyObject *fo, PyObject ***flagret)
{
	trace_component_t *c;
	trace_flag_t      *f;
	int                nflag;
	char              *what;
	int                pos;
	PyObject          *key, *value, *obj, **fr;
	char              *fname, *fdescr;

	*flagret = NULL;
	
	nflag = PyDict_Size(fo);
	
	if ((c = malloc(sizeof(*c))) == NULL) {
		what = "trace component";
		goto nomem;
	}
	memset(c, 0, sizeof(*c));
	
	if ((c->name = strdup(name)) == NULL) {
		what = "component name";
		goto nomem;
	}
	
	if ((c->flags = malloc((nflag + 1) * sizeof(*c->flags))) == NULL) {
		what = "component flags";
		goto nomem;
	}
	memset(c->flags, 0, (nflag + 1) * sizeof(*c->flags));
	
	if ((fr = malloc((nflag + 1) * sizeof(fr[0]))) == NULL) {
		what = "component flags";
		goto nomem;
	}
	memset(fr, 0, (nflag + 1) * sizeof(fr[0]));
	*flagret = fr;
	
	pos = 0;
	f   = c->flags;
	while (PyDict_Next(fo, &pos, &key, &value)) {
		/* 'flagname': { 'description': description } */
		if (!PyString_Check(key)) {
			PyErr_SetString(PyExc_TypeError, "trace flag key not a string");
			goto failed;
		}
		if (!PyDict_Check(value)) {
			PyErr_SetString(PyExc_TypeError,
							"trace flag declaration must be a dictionary");
			goto failed;
		}
		fname = PyString_AS_STRING(key);
		if (!fname[0]) {
			PyErr_SetString(PyExc_ValueError, "empty flag name");
			goto failed;
		}
		if ((obj = PyDict_GetItemString(value, KEY_DESCR)) == NULL) {
			PyErr_SetString(PyExc_ValueError, "empty flag description");
			goto failed;
		}
		fdescr = PyString_AsString(obj);
		if ((f->name = strdup(fname)) == NULL) {
			what = "trace flag name";
			goto nomem;
		}
		if ((f->description = strdup(fdescr)) == NULL) {
			what = "trace flag description";
			goto nomem;
		}
		
		/*printf("##### %s: %s\n", f->name, f->description);*/
		
		*fr++ = value;
		f++;
	}

	f->name = f->description = NULL;
	*fr     = NULL;
	return c;

 nomem:
	PyErr_Format(PyExc_MemoryError, "failed to allocate memory for %s", what);
 failed:
	if (c) {
		if (c->name)
			free(c->name);
		if (c->flags) {
			for (f = c->flags; f->name; f++) {
				if (f->name)
					free(f->name);
				if (f->description)
					free(f->description);
			}
			free(c->flags);
		}
		free(c);
	}
	if (*flagret)
		free(*flagret);
	*flagret = NULL;
	
	return NULL;
}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */



