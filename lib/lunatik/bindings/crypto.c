#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/random.h>

#include <crypto/hash.h>

#include "../lunatik.h"
#include "buffer.h"

int
lunatik_sha1(lua_State *L) {
	struct hash_desc desc;
	struct crypto_hash *tfm;
	struct scatterlist sg[1];
	char *buf, *result;
	size_t buflen;

	buf = (char *) luaL_checkudata(L, -1, "buffer");
	buflen = lua_objlen(L, -1);

	/* TODO: handle buf longer than PAGE_SIZE */
	sg_init_table(sg, ARRAY_SIZE(sg));
	sg_set_buf(&sg[0], buf, buflen);

	/* TODO: make hash user selectable */
	tfm = crypto_alloc_hash("sha1", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		lua_pushstring(L, "failed to allocate hash \"sha1\"");
		lua_error(L);
	}

	lua_pushinteger(L, crypto_hash_digestsize(tfm));
	lunatik_buf_new(L);
	result = lua_touserdata(L, -1);

	desc.tfm = tfm;
	desc.flags = 0;

	crypto_hash_init(&desc);
	crypto_hash_update(&desc, sg, ARRAY_SIZE(sg));
	crypto_hash_final(&desc, result);
	crypto_free_hash(tfm);

	return 1;
}

int
lunatik_get_random_bytes(lua_State *L) {
	int n;
	char *buf;

	n = luaL_checknumber(L, -1);
	if (n < 0) {
		lua_pushstring(L, "expected a positive param");
		lua_error(L);
	}

	lua_pushinteger(L, n);
	lunatik_buf_new(L);
	buf = (char *) lua_touserdata(L, -1);
	get_random_bytes(buf, n);	
	return 1;
}
