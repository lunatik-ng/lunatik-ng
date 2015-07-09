#ifndef _LUNATIK_H
#define _LUNATIK_H

/*
 * lunatik.h
 * See Copyright Notice at the end of this file.
 */

#include <linux/lunatik/lauxlib.h>

struct lunatik_result {
	/*
	 * Supported return types:
	 *
	 * - LUA_TNIL (no further details)
	 * - LUA_TNUMBER (value in r_number)
	 * - LUA_TBOOLEAN (value in r_boolean)
	 * - LUA_TSTRING (copy in r_string, length in r_string_size)
	 * - LUA_TUSERDATA (copy data in r_userdata, length in r_userdata_size,
	 *   name of metatable in r_userdata_type)
	 * - LUA_TLIGHTUSERDATA (pointer value in r_lightuserdata)
	 */
	int r_type;
	union {
		s64 r_number;
		int r_boolean;
		struct {
			char *r_string;
			size_t r_string_size;
		};
		struct {
			void *r_userdata;
			size_t r_userdata_size;
			char *r_userdata_type;
		};
		void *r_lightuserdata;
	};
};

lua_State *lunatik_get_global_state(void);

int lunatik_loadcode(char *code, size_t sz_code,
		struct lunatik_result **presult);
void lunatik_result_free(const struct lunatik_result *result);
int lunatik_openlib(lua_CFunction luaopen_func);

/******************************************************************************
 *  Copyright (C) 2009 Lourival Vieira Neto.  All rights reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 ******************************************************************************/

#endif /* _LUNATIK_H */
