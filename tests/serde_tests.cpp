#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "equipsets.h"
#include "hotkeys.h"
#include "keys.h"
#include "serde.h"

namespace ech {

TEST_CASE("Hotkeys<int> serde") {
    // Hotkeys<int> will be deserialized from `src_str`, then serialized to `boost::json::value`.
    // That result will be compared to the JSON parse result of `jv_str`.
    struct Testcase {
        std::string_view name;
        std::string_view src_str;
        std::string_view jv_str;
    };

    auto testcase = GENERATE(
        Testcase{
            .name = "normal",
            .src_str = R"({
                "active_hotkey": 1,
                "hotkeys": [
                    {
                        "name": "hk0",
                        "keysets": [["0"]],
                        // Default value, discarded from reserialization.
                        "active_equipset": 0,
                        "equipsets": [0, 1, 2, 3],
                    },
                    {
                        "name": "hk1",
                        "keysets": [
                            // Order of Shift and 1 will be swapped due to sorting by Keyset
                            // deserialization.
                            ["LShift", "1"],
                            ["RShift", "1"],
                        ],
                        "active_equipset": 1,
                        "equipsets": [0, 1, 2, 3],
                    },
                ],
            })",
            .jv_str = R"({
                "active_hotkey": 1,
                "hotkeys": [
                    {
                        "name": "hk0",
                        "keysets": [
                            ["0"]
                        ],
                        "equipsets": [0, 1, 2, 3],
                    },
                    {
                        "name": "hk1",
                        "keysets": [
                            ["1", "LShift"],
                            ["1", "RShift"],
                        ],
                        "active_equipset": 1,
                        "equipsets": [0, 1, 2, 3],
                    },
                ],
            })",
        },

        Testcase{
            .name = "no_hotkeys_because_empty",
            .src_str = "{}",
            .jv_str = "{}",
        },
        Testcase{
            .name = "no_hotkeys_because_wrong_type_1",
            .src_str = R"({"active_hotkey": 1, "hotkeys": null})",
            .jv_str = "{}",
        },
        Testcase{
            .name = "no_hotkeys_because_wrong_type_2",
            .src_str = R"({"active_hotkey": 1, "hotkeys": {}})",
            .jv_str = "{}",
        },
        Testcase{
            .name = "no_hotkeys_because_wrong_type_3",
            .src_str = R"({"active_hotkey": 1, "hotkeys": [null, 1]})",
            .jv_str = "{}",
        },

        Testcase{
            .name = "no_keysets_because_empty_1",
            .src_str = R"({
                "hotkeys": [{
                    "keysets": [],
                    "equipsets": [0],
                }],
            })",
            .jv_str = R"({
                "hotkeys": [{
                    "equipsets": [0]
                }],
            })",
        },
        Testcase{
            .name = "no_keysets_because_empty_2",
            .src_str = R"({
                "hotkeys": [{
                    // Empty keysets get pruned during deserialization.
                    "keysets": [[]],
                    "equipsets": [0],
                }],
            })",
            .jv_str = R"({
                "hotkeys": [{
                    "equipsets": [0]
                }],
            })",
        },
        Testcase{
            .name = "no_keysets_because_empty_3",
            .src_str = R"({
                "hotkeys": [{
                    "keysets": [[""]],
                    "equipsets": [0],
                }],
            })",
            .jv_str = R"({
                "hotkeys": [{
                    "equipsets": [0]
                }],
            })",
        },
        Testcase{
            .name = "no_keysets_because_wrong_type_1",
            .src_str = R"({
                "hotkeys": [{
                    "keysets": null,
                    "equipsets": [0],
                }],
            })",
            .jv_str = R"({
                "hotkeys": [{
                    "equipsets": [0]
                }],
            })",
        },
        Testcase{
            .name = "no_keysets_because_wrong_type_2",
            .src_str = R"({
                "hotkeys": [{
                    "keysets": 1,
                    "equipsets": [0],
                }],
            })",
            .jv_str = R"({
                "hotkeys": [{
                    "equipsets": [0]
                }],
            })",
        },
        Testcase{
            .name = "keysets_prune_wrong_type",
            .src_str = R"({
                "hotkeys": [{
                    "keysets": [["0"], ["1", 1, null, "2", "3"],
                    ],
                    "equipsets": [0],
                }],
            })",
            .jv_str = R"({
                "hotkeys": [{
                    "keysets": [["0"]],
                    "equipsets": [0]
                }],
            })",
        },

        Testcase{
            .name = "no_equipsets_because_empty",
            .src_str = R"({
                "hotkeys": [{
                    "keysets": [["0"]],
                    "equipsets": [],
                }],
            })",
            .jv_str = R"({
                "hotkeys": [{
                    "keysets": [["0"]],
                }],
            })",
        },
        Testcase{
            .name = "no_equipsets_because_wrong_type_1",
            .src_str = R"({
                "hotkeys": [{
                    "keysets": [["0"]],
                    "equipsets": null,
                }],
            })",
            .jv_str = R"({
                "hotkeys": [{
                    "keysets": [["0"]],
                }],
            })",
        },
        Testcase{
            .name = "no_equipsets_because_wrong_type_2",
            .src_str = R"({
                "hotkeys": [{
                    "keysets": [["0"]],
                    "equipsets": 1,
                }],
            })",
            .jv_str = R"({
                "hotkeys": [{
                    "keysets": [["0"]],
                }],
            })",
        },
        Testcase{
            .name = "no_equipsets_because_wrong_type_3",
            .src_str = R"({
                "hotkeys": [{
                    "keysets": [["0"]],
                    "equipsets": [0, null],
                }],
            })",
            .jv_str = R"({
                "hotkeys": [{
                    "keysets": [["0"]],
                }],
            })",
        },

        Testcase{
            .name = "default_scalar_values_not_serialized",
            .src_str = R"({
                "active_hotkey": -1,
                "hotkeys": [
                    {
                        "name": "",
                        "keysets": [["0"]],
                        "active_equipset": 0,
                        "equipsets": [0],
                    },
                ],
            })",
            .jv_str = R"({
                "hotkeys": [
                    {
                        "keysets": [["0"]],
                        "equipsets": [0],
                    }
                ],
            })",
        },

        Testcase{
            .name = "empty1",
            .src_str = "{}",
            .jv_str = "{}",
        }
    );

    CAPTURE(testcase.name);
    auto want_jv = Deserialize(testcase.jv_str);
    REQUIRE(want_jv);

    auto hotkeys = Deserialize<Hotkeys<int>>(testcase.src_str);
    REQUIRE(hotkeys);
    auto got_jv = boost::json::value_from(*hotkeys, SerdeContext());
    REQUIRE(got_jv == *want_jv);
}

TEST_CASE("Equipset serde") {
    struct Testcase {
        std::string_view name;
        std::string_view src_str;
        std::string_view jv_str;
    };

    auto testcase = GENERATE(
        // We can't unit test equipped-gear since that requires Skyrim to be running.
        Testcase{
            .name = "normal",
            .src_str = R"([
                {"slot": 1, "unequip": true},
                {"slot": 1, "unequip": true},
                {"slot": 2, "unequip": true},
            ])",
            .jv_str = R"([
                {"slot": 1, "unequip": true},
                {"slot": 2, "unequip": true},
            ])",
        },
        Testcase{
            .name = "empty",
            .src_str = "[]",
            .jv_str = "[]",
        },
        Testcase{
            .name = "wrong_type_1",
            .src_str = "{}",
            .jv_str = "[]",
        },
        Testcase{
            .name = "wrong_type_2",
            .src_str = "null",
            .jv_str = "[]",
        },
        Testcase{
            .name = "wrong_type_3",
            .src_str = "1",
            .jv_str = "[]",
        },
        Testcase{
            .name = "wrong_type_4",
            .src_str = R"([{"slot": 1, "unequip": true}, null])",
            .jv_str = "[]",
        },
        Testcase{
            .name = "slot_overflow",
            .src_str = R"([{"slot": 1, "unequip": true}, {"slot": 4, "unequip": true}])",
            .jv_str = "[]",
        },
        Testcase{
            .name = "slot_wrong_type",
            .src_str = R"([{"slot": 1, "unequip": true}, {"slot": false, "unequip": true}])",
            .jv_str = "[]",
        },
        Testcase{
            .name = "unequip_wrong_type",
            .src_str = R"([{"slot": 1, "unequip": true}, {"slot": 2, "unequip": "str"}])",
            .jv_str = "[]",
        }
    );

    CAPTURE(testcase.name);
    auto want_jv = Deserialize(testcase.jv_str);
    REQUIRE(want_jv);

    auto equipset = Deserialize<Equipset>(testcase.src_str);
    REQUIRE(equipset);
    auto got_jv = boost::json::value_from(*equipset, SerdeContext());
    REQUIRE(got_jv == *want_jv);
}

}  // namespace ech
