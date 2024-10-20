#ifndef UUID_H
#define UUID_H

typedef unsigned char uuid_t[16];

#ifdef __cplusplus
extern "C" {
#endif
#define uuid_generate(out) uuid_generate_random(out)

extern void uuid_generate_random(uuid_t out);
extern void uuid_unparse(const uuid_t uuid, char *out);
extern void uuid_copy(uuid_t dst, const uuid_t src);
extern void uuid_parse(const char *in, uuid_t uuid);

#ifdef __cplusplus
}
#endif

#endif
