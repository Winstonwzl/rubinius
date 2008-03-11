OBJECT hash_new(STATE);
OBJECT hash_new_sized(STATE, int size);
void hash_setup(STATE, OBJECT hsh, int size);
OBJECT hash_add(STATE, OBJECT h, unsigned int hsh, OBJECT key, OBJECT data);
OBJECT hash_set(STATE, OBJECT hash, OBJECT key, OBJECT val);
OBJECT hash_get(STATE, OBJECT hash, unsigned int hsh);
OBJECT hash_get_undef(STATE, OBJECT hash, unsigned int hsh);
OBJECT hash_delete(STATE, OBJECT self, unsigned int hsh);
OBJECT hash_s_from_tuple(STATE, OBJECT tup);
OBJECT hash_get_undef(STATE, OBJECT hash, unsigned int hsh);
OBJECT hash_find_entry(STATE, OBJECT h, unsigned int hsh);
OBJECT hash_dup(STATE, OBJECT hsh);
void hash_redistribute(STATE, OBJECT hsh);

int hash_lookup(STATE, OBJECT tbl, OBJECT key, unsigned int hash, OBJECT *value);
int hash_lookup2(STATE, int (*compare)(STATE, OBJECT, OBJECT), OBJECT tbl, OBJECT key, unsigned int hash, OBJECT *value);
void hash_assign(STATE, int (*compare)(STATE, OBJECT, OBJECT), OBJECT tbl, OBJECT key, unsigned int hash, OBJECT value);


#define hash_find(state, hash, key) (hash_get(state, hash, object_hash_int(state, key)))

#define hash_find_undef(state, hash, key) (hash_get_undef(state, hash, object_hash_int(state, key)))

#define MAX_DENSITY 0.75

/* TODO: fix to determine whether to redistribute both up and down */
#define hash_redistribute_p(hash) (N2I(hash_get_entries(hash)) >= MAX_DENSITY * N2I(hash_get_bins(hash)))

#define CSM_SIZE 12

#define csm_new(st) tuple_new(st, CSM_SIZE)
#define csm_size(st, obj) (NUM_FIELDS(obj) / 2)

OBJECT csm_find(STATE, OBJECT csm, OBJECT key);
OBJECT csm_add(STATE, OBJECT csm, OBJECT key, OBJECT val);
OBJECT csm_into_hash(STATE, OBJECT csm);
OBJECT csm_into_lookuptable(STATE, OBJECT csm);

