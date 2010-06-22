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

#ifndef PHP_NSNTRACE_H
#define PHP_NSNTRACE_H

extern zend_module_entry nsntrace_module_entry;
#define phpext_nsntrace_ptr &nsntrace_module_entry

#ifdef PHP_WIN32
#define PHP_NSNTRACE_API __declspec(dllexport)
#else
#define PHP_NSNTRACE_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif


/* 
  	Declare any global variables you may need between the BEGIN
	and END macros here:     

ZEND_BEGIN_MODULE_GLOBALS(nsntrace)
	long  global_value;
	char *global_string;
ZEND_END_MODULE_GLOBALS(nsntrace)
*/

/* In every utility function you add that needs to use variables 
   in php_nsntrace_globals, call TSRMLS_FETCH(); after declaring other 
   variables used by that function, or better yet, pass in TSRMLS_CC
   after the last function argument and declare your utility function
   with TSRMLS_DC after the last declared argument.  Always refer to
   the globals in your function as NSNTRACE_G(variable).  You are 
   encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define NSNTRACE_G(v) TSRMG(nsntrace_globals_id, zend_nsntrace_globals *, v)
#else
#define NSNTRACE_G(v) (nsntrace_globals.v)
#endif

#endif	/* PHP_NSNTRACE_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
