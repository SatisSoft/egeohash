-module(egeohash).

-ifdef(TEST).
-include_lib("eunit/include/eunit.hrl").
-endif.

-export([init/0, encode/3, decode/1, adjacent/2, rgn_to_hashes/6]).

-nifs([encode/3, decode/1, adjacent/2, rgn_to_hashes/6]).

-on_load(init/0).

init() ->
    SoName = case code:priv_dir(?MODULE) of
        {error, bad_name} ->
            case code:which(?MODULE) of
                Filename when is_list(Filename) ->
                    filename:join([filename:dirname(Filename),"../priv", "egeohash"]);
                _ ->
                    filename:join("../priv", "egeohash")
                end;
        Dir ->
            filename:join(Dir, "egeohash")
    end,
    erlang:load_nif(SoName, 0).

-spec encode(Lat::float()|integer(), Lon::float()|integer(), Prec::integer()) -> {ok, string()}.
encode(_Lat, _Lon, _Prec) ->
    erlang:nif_error("NIF library not loaded").

-spec decode(Hash::string()) -> {ok, {{float(), float()}, {float(), float()}}}.
decode(_Hash) ->
    erlang:nif_error("NIF library not loaded").

-spec adjacent(Hash::string(), Direction::north|south|east|west) -> {ok, list()}|{error, end_of_map}.
adjacent(_Hash, _Direction) ->
    erlang:nif_error("NIF library not loaded").

-spec rgn_to_hashes(Lat0::float()|integer(),
                    Lat1::float()|integer(),
                    Lon0::float()|integer(),
                    Lon1::float()|integer(),
                    Prec::integer(),
                    CellsLimit::integer()) -> {ok, list(string())}|{error, cells_limit}.
rgn_to_hashes(_Lat0, _Lat1, _Lon0, _Lon1, _Prec, _CellsLimit) ->
    erlang:nif_error("NIF library not loaded").

-ifdef(TEST).
encode_test() ->
    [
        ?assertEqual({ok, "000000"}, encode(-90, -180, 6)),
        ?assertEqual({ok, "s00000"}, encode(0.0, 0.0, 6)),
        ?assertEqual({ok, "zzzzzz"}, encode(90, 180, 6)),
        ?assertEqual({ok, "ucfv0n"}, encode(55.755864, 37.617698, 6)),
        ?assertEqual({ok, "ucfv0"},  encode(55.755864, 37.617698, 5)),
        ?assertEqual({ok, "pbpbpb"}, encode(-91, 181, 6)),
        ?assertException(error, badarg, encode(0, 0, six))
    ].

decode_test() ->
    [
        ?assertEqual({ok, {{-90.0, 90.0},{-180.0, 180.0}}},     decode("")),
        ?assertEqual({ok, {{-90.0, -89.9945068359375},
                           {-180.0, -179.989013671875}}},       decode("000000")),
        ?assertEqual({ok, {{89.9945068359375, 90.0}, 
                           {179.989013671875, 180.0}}},         decode("zzzzzz")), 
        ?assertEqual({ok, {{55.634765625, 55.6402587890625}, 
                           {37.63916015625, 37.650146484375}}}, decode("ucfu88")),
        ?assertEqual({ok, {{55.634765625,55.6787109375},
                           {37.6171875,37.6611328125}}},        decode("ucfu8")),
        ?assertException(error, badarg,                         decode("a")),
        ?assertException(error, badarg,                         decode(1))
    ].

adjacent_test() ->
    [
        ?assertEqual({ok, "ucfu89"},      adjacent("ucfu88", north)),
        ?assertEqual({ok, "ucfu2x"},      adjacent("ucfu88", south)),
        ?assertEqual({ok, "ucfu8b"},      adjacent("ucfu88", east)),
        ?assertEqual({ok, "ucfu82"},      adjacent("ucfu88", west)),
        ?assertEqual({ok, "ucfub"},       adjacent("ucfu8",  north)),
        ?assertEqual({ok, "ucfu2"},       adjacent("ucfu8",  south)),
        ?assertEqual({ok, "ucfu9"},       adjacent("ucfu8",  east)),
        ?assertEqual({ok, "ucfsx"},       adjacent("ucfu8",  west)),
        ?assertEqual({error, end_of_map}, adjacent("zzzzzz", north)),
        ?assertEqual({error, end_of_map}, adjacent("00000",  south)),
        ?assertException(error, badarg,   adjacent("a", north)),
        ?assertException(error, badarg,   adjacent("000000", norh))
    ].

    reg_to_hashes_test() ->
        [
            ?assertEqual({ok, ["ucfv0n"]},       rgn_to_hashes(55.7584, 55.7584, 37.623, 37.623, 6, 9)),
            ?assertEqual({ok, lists:sort([
                "ucftpz", "ucfv0p", "ucfv0r",
                "ucftpy", "ucfv0n", "ucfv0q",
                "ucftpv", "ucfv0j", "ucfv0m"])}, rgn_to_hashes(55.7529, 55.7639, 37.612, 37.634, 6, 9)),
            ?assertEqual({error,cells_limit},    rgn_to_hashes(55.7529, 55.7693, 37.612, 37.645, 6, 9)),
            ?assertEqual({ok, lists:sort([
                "ucftr", "ucfv2", "ucfv3",
                "ucftp", "ucfv0", "ucfv1",
                "ucfsz", "ucfub", "ucfuc"])},   rgn_to_hashes(55.701, 55.789, 37.595, 37.683, 5, 9)),
            ?assertEqual({ok, lists:sort([
                "ucftpt", "ucftpv", "ucfv0j",
                "ucfv0m", "ucfv0t", "ucfv0v",
                "ucfv1j", "ucfv1m", "ucfv1t"])}, rgn_to_hashes(55.7519, 55.7538, 37.601,  37.689, 6, 9)),
            ?assertEqual({ok, []},               rgn_to_hashes(56, 55.7639, 38, 37.634, 6, 9)),
            ?assertException(error, badarg,      rgn_to_hashes(55.7639, 56, 37.634, 38, six, 9))
        ].
-endif.
