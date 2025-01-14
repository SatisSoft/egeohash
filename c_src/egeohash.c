/* egeohash.c */

#include <erl_nif.h>
#include <string.h>

#define MAX_PRECISION 12

const char base32[32] = {"0123456789bcdefghjkmnpqrstuvwxyz"};

const char neighbour[4][2][32] = {
    {{"14365h7k9dcfesgujnmqp0r2twvyx8zb"}, {"238967debc01fg45kmstqrwxuvhjyznp"}},
    {{"p0r21436x8zb9dcf5h7kjnmqesgutwvy"}, {"bc01fg45238967deuvhjyznpkmstqrwx"}},
    {{"238967debc01fg45kmstqrwxuvhjyznp"}, {"14365h7k9dcfesgujnmqp0r2twvyx8zb"}},
    {{"bc01fg45238967deuvhjyznpkmstqrwx"}, {"p0r21436x8zb9dcf5h7kjnmqesgutwvy"}}};

const char * border[4][2] = {
    {"prxz", "bcfguvyz"},
    {"028b", "0145hjnp"},
    {"bcfguvyz", "prxz"},
    {"0145hjnp", "028b"}};

static ERL_NIF_TERM north_atom;
static ERL_NIF_TERM south_atom;
static ERL_NIF_TERM east_atom;
static ERL_NIF_TERM west_atom;
static ERL_NIF_TERM ok_atom;
static ERL_NIF_TERM error_atom;
static ERL_NIF_TERM end_of_map_atom;
static ERL_NIF_TERM cells_limit_atom;

static inline unsigned int lat2num(double, unsigned int);
static inline unsigned int lon2num(double, unsigned int);
static inline void make_geohash(unsigned int, unsigned int, unsigned int, char [MAX_PRECISION]);
static int compare(const void *, const void *);
static int hashcmp_reverse(const void *, const void *);

static int load(ErlNifEnv* env, void** priv_data, ERL_NIF_TERM load_info) {
    north_atom = enif_make_atom(env, "north");
    south_atom = enif_make_atom(env, "south");
    east_atom = enif_make_atom(env, "east");
    west_atom = enif_make_atom(env, "west");
    ok_atom = enif_make_atom(env, "ok");
    error_atom = enif_make_atom(env, "error");
    end_of_map_atom = enif_make_atom(env, "end_of_map");
    cells_limit_atom =  enif_make_atom(env, "cells_limit");

    *priv_data = NULL;

    return 0;
}

static int upgrade(ErlNifEnv* env, void** priv_data, void** old_priv_data, ERL_NIF_TERM load_info) {
    if(*old_priv_data != NULL) {
        return -1; /* Don't know how to do that */
    }
    if(*priv_data != NULL) {
        return -1; /* Don't know how to do that */
    }
    if(load(env, priv_data, load_info)) {
        return -1;
    }
    return 0;
}

static void unload(ErlNifEnv *env, void* priv_data)
{

}

static ERL_NIF_TERM encode_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    double lat, lon;

    unsigned int precision, latlon0, latlon1;
    precision = latlon0 = latlon1 = 0;

    char hash[MAX_PRECISION];

    if (argc != 3)
        return enif_make_badarg(env);

    if (!enif_get_double(env, argv[0], &lat)) {
        int lat_i;
        if (!enif_get_int(env, argv[0], &lat_i))
            return enif_make_badarg(env);
        lat = (double)lat_i;
    }

    if (!enif_get_double(env, argv[1], &lon)) {
        int lon_i;
        if (!enif_get_int(env, argv[1], &lon_i))
            return enif_make_badarg(env);
        lon = (double)lon_i;
    }

    if (!enif_get_uint(env, argv[2], &precision) || precision > MAX_PRECISION)
        return enif_make_badarg(env);

    if (precision % 2) {
        unsigned int lat_max_num = 1 << (precision / 2 * 5 + 2);
        unsigned int lon_max_num = 1 << (precision / 2 * 5 + 3);
        latlon0 = lat2num(lat, lat_max_num);
        latlon1 = lon2num(lon, lon_max_num);
    } else {
        unsigned int latlon_max_num = 1 << (precision / 2 * 5);
        latlon0 = lon2num(lon, latlon_max_num);
        latlon1 = lat2num(lat, latlon_max_num);
    }

    make_geohash(latlon0, latlon1, precision, hash);

    return enif_make_tuple2(env, ok_atom, enif_make_string_len(env, hash, precision, ERL_NIF_LATIN1));
}

static ERL_NIF_TERM decode_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    unsigned int precision = 0;
    unsigned int current = 0;
    unsigned int latlon0, latlon1, latlontmp;
    double lat, lon, cell_vsize, cell_hsize;

    latlon0 = latlon1 = latlontmp = 0;
    lat = lon = cell_vsize = cell_hsize = 0.0;

    if(argc != 1)
        return enif_make_badarg(env);

    if(!enif_get_list_length(env, argv[0], &precision) || precision > MAX_PRECISION)
        return enif_make_badarg(env);

    ERL_NIF_TERM head, tail = argv[0];
    for(int i = 0; i < precision; i++) {
        if(!enif_get_list_cell(env, tail, &head, &tail) || !enif_get_uint(env, head, &current))
            return enif_make_badarg(env);
        char *symbol_addr = (char*)bsearch(&current, base32, sizeof(base32), sizeof(char), compare);
        if (symbol_addr) {
            char cell_code = symbol_addr - base32;
            latlontmp = latlon1 << 2 | (cell_code >> 1 & 1) | (cell_code >> 2 & 2);
            latlon1 = latlon0 << 3 | (cell_code & 1) | (cell_code >> 1 & 2) | (cell_code >> 2 & 4);
            latlon0 = latlontmp;
        } else {
            return enif_make_badarg(env);
        }
    }

    if (precision % 2) {
        cell_vsize = 180.0 / (1 << (precision / 2 * 5 + 2));
        cell_hsize = 360.0 / (1 << (precision / 2 * 5 + 3));
        lat = (double)latlon0 * cell_vsize - 90.0;
        lon = (double)latlon1 * cell_hsize - 180.0;
    } else {
        cell_vsize = 180.0 / (1 << (precision / 2 * 5));
        cell_hsize = 360.0 / (1 << (precision / 2 * 5));
        lat = (double)latlon1 * cell_vsize - 90.0;
        lon = (double)latlon0 * cell_hsize - 180.0;
    }

    ERL_NIF_TERM lat_bounds = enif_make_tuple2(env, enif_make_double(env, lat), enif_make_double(env, lat + cell_vsize));
    ERL_NIF_TERM lon_bounds = enif_make_tuple2(env, enif_make_double(env, lon), enif_make_double(env, lon + cell_hsize));

    return enif_make_tuple2(env, ok_atom, enif_make_tuple2(env, lat_bounds, lon_bounds));
}

static ERL_NIF_TERM adjacent_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    unsigned int precision = 0;
    char hash[MAX_PRECISION] = {0};
    unsigned char direction_code = 0;

    if(argc != 2)
        return enif_make_badarg(env);

    if(!enif_get_list_length(env, argv[0], &precision) || precision > MAX_PRECISION)
        return enif_make_badarg(env);

    ERL_NIF_TERM head, tail = argv[0];
    for(int i = 0; i < precision; i++) {
        unsigned int current;
        if(!enif_get_list_cell(env, tail, &head, &tail) || !enif_get_uint(env, head, &current))
            return enif_make_badarg(env);
        hash[i] = current;
    }

    if(!enif_compare(argv[1], north_atom))
        direction_code = 0;
    else if(!enif_compare(argv[1], south_atom))
        direction_code = 1;
    else if(!enif_compare(argv[1], east_atom))
        direction_code = 2;
    else if(!enif_compare(argv[1], west_atom))
        direction_code = 3;
    else
        return enif_make_badarg(env);

    for(int i = precision - 1; i >= 0; i--) {
        unsigned char geohash_odd = (i + 1) % 2;
        char *symbol_addr = (char*)bsearch(&hash[i], base32, sizeof(base32), sizeof(char), compare);
        if (!symbol_addr)
            return enif_make_badarg(env);

        if (!strchr(border[direction_code][geohash_odd], hash[i])) {
            hash[i] = neighbour[direction_code][geohash_odd][symbol_addr - base32];
            return enif_make_tuple2(env, ok_atom, enif_make_string_len(env, hash, precision, ERL_NIF_LATIN1));
        }
        hash[i] = neighbour[direction_code][geohash_odd][symbol_addr - base32];
    }
    return enif_make_tuple2(env, error_atom, end_of_map_atom);
}

static ERL_NIF_TERM rgn_to_hashes_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    double lat0, lat1, lon0, lon1;
    unsigned int precision, cells_limit;
    unsigned int cells_num0, cells_num1, cells_num_total, cell_min0, cell_max0, cell_min1, cell_max1;
    char hash[MAX_PRECISION] = {0};
    ERL_NIF_TERM ehashes;

    precision = cells_limit = 0;
    cells_num0 = cells_num1 = cells_num_total = cell_min0 = cell_max0 = cell_min1 = cell_max1 = 0;
    ehashes = enif_make_list(env, 0);

    if (argc != 6)
        return enif_make_badarg(env);

    if (!enif_get_double(env, argv[0], &lat0)) {
        int lat0_i;
        if (!enif_get_int(env, argv[0], &lat0_i))
            return enif_make_badarg(env);
        lat0 = (double)lat0_i;
    }

    if (!enif_get_double(env, argv[1], &lat1)) {
        int lat1_i;
        if (!enif_get_int(env, argv[1], &lat1_i))
            return enif_make_badarg(env);
        lat1 = (double)lat1_i;
    }

    if (!enif_get_double(env, argv[2], &lon0)) {
        int lon0_i;
        if (!enif_get_int(env, argv[2], &lon0_i))
            return enif_make_badarg(env);
        lon0 = (double)lon0_i;
    }

    if (!enif_get_double(env, argv[3], &lon1)) {
        int lon1_i;
        if (!enif_get_int(env, argv[3], &lon1_i))
            return enif_make_badarg(env);
        lon1 = (double)lon1_i;
    }

    if ((lat0 > lat1) || (lon0 > lon1))
        return enif_make_tuple2(env, ok_atom, enif_make_list(env, 0));

    if (!enif_get_uint(env, argv[4], &precision) || precision > MAX_PRECISION)
        return enif_make_badarg(env);

    if (!enif_get_uint(env, argv[5], &cells_limit))
        return enif_make_badarg(env);

    if (precision % 2) {
        unsigned int lat_max_num = 1 << (precision / 2 * 5 + 2);
        unsigned int lon_max_num = 1 << (precision / 2 * 5 + 3);
        cell_min0 = lat2num(lat0, lat_max_num);
        cell_max0 = lat2num(lat1, lat_max_num);
        cell_min1 = lon2num(lon0, lon_max_num);
        cell_max1 = lon2num(lon1, lon_max_num);
    } else {
        unsigned int latlon_max_num = 1 << (precision / 2 * 5);
        cell_min0 = lon2num(lon0, latlon_max_num);
        cell_max0 = lon2num(lon1, latlon_max_num);
        cell_min1 = lat2num(lat0, latlon_max_num);
        cell_max1 = lat2num(lat1, latlon_max_num);
    }

    cells_num0 = cell_max0 - cell_min0 + 1;
    cells_num1 = cell_max1 - cell_min1 + 1;
    cells_num_total = cells_num0 * cells_num1;

    if (cells_num_total > cells_limit)
        return enif_make_tuple2(env, error_atom, cells_limit_atom);

    char hashes[cells_num_total][MAX_PRECISION];

    for (int hashidx = 0; cell_min0 <= cell_max0; cell_min0++) {
        for (int i = cell_min1; i <= cell_max1; i++, hashidx++) {
	        make_geohash(cell_min0, i, precision, hash);
            memcpy(hashes[hashidx], hash, MAX_PRECISION);
        }
	}

    qsort(hashes, cells_num_total, sizeof(char) * MAX_PRECISION, hashcmp_reverse);

    for (int i = 0; i < cells_num_total; i++)
        ehashes = enif_make_list_cell(env, enif_make_string_len(env, hashes[i], precision, ERL_NIF_LATIN1), ehashes);

    return enif_make_tuple2(env, ok_atom, ehashes);
}

static inline unsigned int lat2num(double lat, unsigned int lat_max_num) {
    if (lat >= 90.0)
        return lat_max_num - 1;
    else if (lat > -90.0)
        return (lat + 90.0) * lat_max_num / 180.0;
    return 0;
}

static inline unsigned int lon2num(double lon, unsigned int lon_max_num) {
    if (lon >= 180.0)
        return lon_max_num - 1;
    else if (lon > -180.0)
        return (lon + 180.0) * lon_max_num / 360.0;
    return 0;
}

static inline void make_geohash(unsigned int latlon0, unsigned int latlon1, unsigned int precision, char hash[MAX_PRECISION]) {
    unsigned char cell_code = 0;
    unsigned int latlontmp = 0;
    for (int i = precision - 1; i >= 0; i--) {
                cell_code = (latlon1 & 1)       |
                            (latlon0 << 1 & 2)  |
                            (latlon1 << 1 & 4)  |
                            (latlon0 << 2 & 8)  |
                            (latlon1 << 2 & 16);
                latlontmp = latlon0 >> 2;
                latlon0 = latlon1 >> 3;
                latlon1 = latlontmp;
                hash[(unsigned)i] = base32[cell_code];
	        }
}

static int compare(const void * a, const void * b) {
   return ( *(char*)a - *(char*)b );
}

static int hashcmp_reverse(const void * a, const void * b) {
   return memcmp(b, a, MAX_PRECISION);
}

static ErlNifFunc nif_funcs[] =
{
    {"encode", 3, encode_nif},
    {"decode", 1, decode_nif},
    {"adjacent", 2, adjacent_nif},
    {"rgn_to_hashes", 6, rgn_to_hashes_nif}
};

ERL_NIF_INIT(egeohash,nif_funcs,load,NULL,upgrade,unload)
