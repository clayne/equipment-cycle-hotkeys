#pragma once

#include "keys.h"
#include "ui_state.h"

namespace ech {
namespace ui {
namespace internal {

using Action = std::function<void()>;

struct TableRowChanges final {
    size_t remove = std::numeric_limits<size_t>::max();
    size_t drag_source = std::numeric_limits<size_t>::max();
    size_t drag_target = std::numeric_limits<size_t>::max();
};

/// A table where rows can be reordered and deleted. Control buttons are located in the rightmost
/// column.
///
/// N is the number of columns excluding the control button column.
template <typename T, size_t N>
struct Table final {
  public:
    /// ImGui ID for the table element. Must not be nullptr or empty.
    const char* id;

    /// Elements must not be nullptr. If all elements are the empty string, then the header will not
    /// be shown.
    std::array<const char*, N> headers;

    /// The underlying objects should be mutable, as `Action` objects returned from draw functions
    /// are expected to modify them.
    std::vector<T>& viewmodel;

    /// Returns an action that should be performed when a change is made to the cell. For example,
    /// if this function draws a combo box, it should return a callback for selecting an item.
    std::function<Action(const T& obj, size_t row, size_t col)> draw_cell;

    /// Typically just a wrapper for `ImGui::Text`.
    std::function<void(const T& obj)> draw_drag_tooltip;

    /// Returns:
    /// 1. A callback to update `viewmodel` (the callback may be empty).
    /// 1. The row changes that that callback will actuate.
    ///
    /// Since `viewmodel` is the only object captured-by-reference by the returned callback, it's
    /// fine for the callback to outlive this table object.
    std::pair<Action, TableRowChanges>
    Draw() const {
        constexpr auto cell_padding = ImVec2(2, 4);
        constexpr auto table_flags = ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_BordersInnerH;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cell_padding);
        if (!ImGui::BeginTable(id, static_cast<int>(cols()), table_flags)) {
            ImGui::PopStyleVar();
            return {};
        }

        // Headers.
        for (int size_t = 0; size_t < ctrl_col(); size_t++) {
            ImGui::TableSetupColumn(headers[size_t]);
        }
        ImGui::TableSetupColumn("##controls", ImGuiTableColumnFlags_WidthFixed);
        if (std::any_of(headers.cbegin(), headers.cend(), [](const char* s) { return *s; })) {
            ImGui::TableHeadersRow();
        }

        std::pair<Action, TableRowChanges> draw_res;
        for (size_t r = 0; r < rows(); r++) {
            const auto& obj = viewmodel[r];
            ImGui::TableNextRow();

            // Main row cells.
            for (size_t c = 0; c < ctrl_col(); c++) {
                ImGui::TableSetColumnIndex(static_cast<int>(c));
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::PushID(cell_id(r, c));
                if (auto a = draw_cell(obj, r, c)) {
                    draw_res = {a, {}};
                }
                ImGui::PopID();
            }

            // Control buttons.
            ImGui::TableSetColumnIndex(static_cast<int>(ctrl_col()));
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);
            ImGui::PushID(cell_id(r, ctrl_col()));
            if (auto atrc = DrawDragButton(r, ImGuiDir_Up); atrc.first) {
                draw_res = atrc;
            }
            ImGui::SameLine(0, 0);
            if (auto atrc = DrawDragButton(r, ImGuiDir_Down); atrc.first) {
                draw_res = atrc;
            }
            ImGui::SameLine(0, 0);
            if (auto atrc = DrawCloseButton(r); atrc.first) {
                draw_res = atrc;
            }
            ImGui::PopID();
            ImGui::PopStyleColor();
        }

        ImGui::EndTable();
        ImGui::PopStyleVar();
        return draw_res;
    }

  private:
    size_t
    rows() const {
        return viewmodel.size();
    }

    size_t
    cols() const {
        return headers.size() + 1;
    }

    size_t
    ctrl_col() const {
        return headers.size();
    }

    int
    cell_id(size_t r, size_t c) const {
        return static_cast<int>(r * cols() + c);
    }

    std::pair<Action, TableRowChanges>
    DrawCloseButton(size_t row) const {
        if (!ImGui::Button("X")) {
            return {{}, {}};
        }
        auto a = [&viewmodel = viewmodel, row]() { viewmodel.erase(viewmodel.begin() + row); };
        auto trc = TableRowChanges{.remove = row};
        return {a, trc};
    }

    std::pair<Action, TableRowChanges>
    DrawDragButton(size_t row, ImGuiDir dir) const {
        auto action = Action();
        auto trc = TableRowChanges();
        if (dir == ImGuiDir_Up) {
            if (ImGui::ArrowButton("up", dir) && row > 0) {
                action = [&viewmodel = viewmodel, row]() {
                    std::swap(viewmodel[row], viewmodel[row - 1]);
                };
                trc = {.drag_source = row, .drag_target = row - 1};
            }
        } else if (dir == ImGuiDir_Down) {
            if (ImGui::ArrowButton("down", dir) && row + 1 < rows()) {
                action = [&viewmodel = viewmodel, row]() {
                    std::swap(viewmodel[row], viewmodel[row + 1]);
                };
                trc = {.drag_source = row, .drag_target = row + 1};
            };
        } else {
            return {{}, {}};
        }

        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(id, &row, sizeof(row));
            draw_drag_tooltip(viewmodel[row]);
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const auto* payload = ImGui::AcceptDragDropPayload(id)) {
                auto src_row = *static_cast<const size_t*>(payload->Data);
                action = [&viewmodel = viewmodel, src_row, row]() {
                    auto item = std::move(viewmodel[src_row]);
                    viewmodel.erase(viewmodel.begin() + src_row);
                    viewmodel.insert(viewmodel.begin() + row, std::move(item));
                };
                trc = TableRowChanges{.drag_source = src_row, .drag_target = row};
            }
            ImGui::EndDragDropTarget();
        }

        return {action, trc};
    }
};

inline Action
DrawProfilesMenu(UI& ui) {
    auto action = Action();
    auto confirm_export = false;

    if (ImGui::BeginMenu("Profiles")) {
        // Export new profile.
        ImGui::InputTextWithHint("##export_name", "Profile Name", &ui.export_name);
        if (ImGui::IsItemDeactivated()) {
            action = [&ui]() { ui.NormalizeExportName(); };
        }
        ImGui::SameLine();
        if (ImGui::Button("Export")) {
            confirm_export = !ui.export_name.empty();
        }

        // List of importable profiles.
        auto profiles = ui.GetSavedProfiles();
        if (!profiles.empty()) {
            ImGui::SeparatorText("Import");
            for (const auto& profile : profiles) {
                if (!ImGui::MenuItem(profile.c_str())) {
                    continue;
                }
                action = [&ui, profile]() {
                    if (ui.ImportProfile(profile)) {
                        return;
                    }
                    ui.status.show = true;
                    auto fp = ui.GetProfilePath(profile);
                    ui.status.msg = std::format("FILESYSTEM ERROR: Failed to read '{}'", fp);
                    SKSE::log::error("importing '{}' aborted: cannot read '{}'", profile, fp);
                };
            }
        }

        ImGui::EndMenu();
    }

    if (confirm_export) {
        ImGui::OpenPopup("confirm_export");
    }
    if (ImGui::BeginPopup("confirm_export")) {
        if (auto existing_profile = ui.GetSavedProfileMatchingExportName()) {
            ImGui::Text("Overwrite profile '%s'?", existing_profile->data());
        } else {
            ImGui::Text("Save as new profile '%s'?", ui.export_name.c_str());
        }
        if (ImGui::Button("Yes")) {
            action = [&ui]() {
                if (ui.ExportProfile()) {
                    return;
                }
                ui.status.show = true;
                auto fp = ui.GetProfilePath(ui.export_name);
                ui.status.msg = std::format("FILESYSTEM ERROR: Failed to write '{}'", fp);
                SKSE::log::error("exporting '{}' aborted: cannot write '{}'", ui.export_name, fp);
            };
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    return action;
}

inline Action
DrawHotkeyList(UI& ui) {
    auto table = Table<HotkeyUI<EquipsetUI>, 1>{
        .id = "hotkeys_list",
        .headers = std::array{""},
        .viewmodel = ui.hotkeys_ui,
        .draw_cell = [&ui](const HotkeyUI<EquipsetUI>& hotkey, size_t row, size_t) -> Action {
            auto a = Action();
            if (ImGui::RadioButton("##hotkey_radio", row == ui.hotkey_in_focus)) {
                a = [&ui, row]() { ui.hotkey_in_focus = row; };
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputTextWithHint(
                "##hotkey_name", "Hotkey Name", const_cast<std::string*>(&hotkey.name)
            );
            return a;
        },
        .draw_drag_tooltip = [](const HotkeyUI<EquipsetUI>& hotkey
                             ) { ImGui::Text("%s", hotkey.name.c_str()); },
    };

    auto atrc = table.Draw();
    if (ImGui::Button("New Hotkey", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        auto a = [&ui]() {
            ui.hotkeys_ui.emplace_back();
            // Adding a new hotkey puts that hotkey in focus.
            ui.hotkey_in_focus = ui.hotkeys_ui.size() - 1;
        };
        atrc = {std::move(a), {}};
    }
    if (!atrc.first) {
        return {};
    }

    return [atrc, &ui]() {
        const auto& [a, trc] = atrc;
        if (trc.remove < ui.hotkeys_ui.size()) {
            // If the in-focus hotkey is below the removed hotkey, then move focus upward.
            if (trc.remove < ui.hotkey_in_focus) {
                ui.hotkey_in_focus--;
            }
        } else if (trc.drag_source < ui.hotkeys_ui.size() && trc.drag_target < ui.hotkeys_ui.size()) {
            // Focus on the row that was dragged.
            ui.hotkey_in_focus = trc.drag_target;
        }
        a();
        if (ui.hotkey_in_focus >= ui.hotkeys_ui.size() && ui.hotkey_in_focus > 0) {
            ui.hotkey_in_focus--;
        }
    };
}

inline Action
DrawKeysets(std::vector<Keyset>& keysets) {
    constexpr auto keycode_names = []() {
        auto arr = kKeycodeNames;
        arr[0] = "(Unbound)";
        return arr;
    }();

    auto table = Table<Keyset, std::tuple_size_v<Keyset>>{
        .id = "keyset_table",
        .headers = std::array{"", "", "", ""},
        .viewmodel = keysets,
        .draw_cell = [](const Keyset& keyset, size_t, size_t col) -> Action {
            auto keycode = keyset[col];
            const char* preview = keycode_names[KeycodeNormalized(keycode)];
            constexpr auto combo_flags =
                ImGuiComboFlags_HeightLarge | ImGuiComboFlags_NoArrowButton;
            if (!ImGui::BeginCombo("##dropdown", preview, combo_flags)) {
                return {};
            }

            auto action = Action();
            for (uint32_t opt_keycode = 0; opt_keycode < keycode_names.size(); opt_keycode++) {
                const char* opt = keycode_names[opt_keycode];
                if (!*opt) {
                    continue;
                }
                auto is_selected = opt_keycode == keycode;
                if (ImGui::Selectable(opt, is_selected)) {
                    auto& keycode_mut = const_cast<uint32_t&>(keyset[col]);
                    action = [&keycode_mut, opt_keycode]() { keycode_mut = opt_keycode; };
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
            return action;
        },
        .draw_drag_tooltip =
            [](const Keyset& keyset) {
                auto names = std::array{
                    keycode_names[KeycodeNormalized(keyset[0])],
                    keycode_names[KeycodeNormalized(keyset[1])],
                    keycode_names[KeycodeNormalized(keyset[2])],
                    keycode_names[KeycodeNormalized(keyset[3])],
                };
                static_assert(std::tuple_size_v<decltype(names)> == std::tuple_size_v<Keyset>);
                ImGui::Text("%s+%s+%s+%s", names[0], names[1], names[2], names[3]);
            },
    };

    ImGui::SeparatorText("Keysets");
    auto action = table.Draw().first;
    if (ImGui::Button("New", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        action = [&keysets]() { keysets.emplace_back(); };
    }
    return action;
}

inline Action
DrawEquipsets(std::vector<EquipsetUI>& equipsets, UI::Status& status) {
    constexpr auto opts_template = []() {
        auto arr = std::array{"", "", ""};
        arr[static_cast<size_t>(EsItemUI::Choice::kIgnore)] = "(Ignore)";
        arr[static_cast<size_t>(EsItemUI::Choice::kUnequip)] = "(Unequip)";
        return arr;
    }();
    constexpr auto item_to_str = [](const EsItemUI& item) {
        if (item.canonical_choice() == EsItemUI::Choice::kGear) {
            return item.gos.gear()->form().GetName();
        }
        return opts_template[static_cast<size_t>(item.canonical_choice())];
    };

    auto table = Table<EquipsetUI, kGearslots.size()>{
        .id = "equipset_table",
        .headers = equipsets.empty() ? std::array{"", "", "", ""}
                                     : std::array{"Left", "Right", "Ammo", "Voice"},
        .viewmodel = equipsets,
        .draw_cell = [](const EquipsetUI& equipset, size_t, size_t col) -> Action {
            const auto& item = equipset[col];
            const char* preview = item_to_str(item);
            constexpr auto combo_flags =
                ImGuiComboFlags_HeightLarge | ImGuiComboFlags_NoArrowButton;
            if (!ImGui::BeginCombo("##dropdown", preview, combo_flags)) {
                return {};
            }

            auto opts = opts_template;
            if (const auto* gear = item.gos.gear()) {
                opts[static_cast<size_t>(EsItemUI::Choice::kGear)] = gear->form().GetName();
            }

            auto action = Action();
            for (size_t i = 0; i < opts.size(); i++) {
                const char* opt = opts[i];
                if (!*opt) {
                    continue;
                }
                auto opt_choice = static_cast<EsItemUI::Choice>(i);
                auto is_selected = opt_choice == item.canonical_choice();
                if (ImGui::Selectable(opt, is_selected)) {
                    auto& item_mut = const_cast<EsItemUI&>(item);
                    action = [&item_mut, opt_choice]() { item_mut.choice = opt_choice; };
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
            return action;
        },
        .draw_drag_tooltip =
            [](const EquipsetUI& equipset) {
                auto names = std::array{
                    item_to_str(equipset[static_cast<size_t>(Gearslot::kLeft)]),
                    item_to_str(equipset[static_cast<size_t>(Gearslot::kRight)]),
                    item_to_str(equipset[static_cast<size_t>(Gearslot::kAmmo)]),
                    item_to_str(equipset[static_cast<size_t>(Gearslot::kShout)]),
                };
                static_assert(std::tuple_size_v<decltype(names)> == kGearslots.size());
                ImGui::Text("%s, %s, %s, %s", names[0], names[1], names[2], names[3]);
            },
    };

    ImGui::SeparatorText("Equipsets");
    auto action = table.Draw().first;
    if (ImGui::Button("Add Currently Equipped", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        action = [&equipsets, &status]() {
#ifndef ECH_UI_DEV
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                equipsets.push_back(EquipsetUI::From(Equipset::FromEquipped(*player)));
                return;
            }
            status.show = true;
            status.msg = "INTERNAL ERROR: Failed to get RE::PlayerCharacter instance.";
            SKSE::log::error("cannot get RE::PlayerCharacter instance");
#else
            equipsets.emplace_back();
#endif
        };
    }
    return action;
}

inline Action
DrawStatusPopup(UI::Status& status) {
    auto action = Action();
    if (status.show) {
        action = [&status]() { status.show = false; };
        ImGui::OpenPopup("status");
    }
    if (ImGui::BeginPopup("status")) {
        ImGui::Text(status.msg.c_str());
        ImGui::EndPopup();
    }
    return action;
}

}  // namespace internal

inline void
Draw(UI& ui) {
    constexpr auto max_dims =
        ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    const auto window_initial_pos = UI::GetViewportSize() * ImVec2(.4f, .1f);
    const auto window_initial_size = UI::GetViewportSize() * ImVec2(.5f, .8f);
    const auto window_min_size = UI::GetViewportSize() * ImVec2(.25f, .25f);
    const auto hotkeylist_initial_size = UI::GetViewportSize() * ImVec2(.15f, .0f);
    const auto hotkeylist_min_size = UI::GetViewportSize() * ImVec2(.15f, .0f);

    auto action = internal::Action();

    // Set up main window.
    ImGui::SetNextWindowPos(window_initial_pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(window_initial_size, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(window_min_size, max_dims);
    ImGui::Begin(
        "Equipment Cycle Hotkeys", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar
    );

    // Menu bar.
    if (ImGui::BeginMenuBar()) {
        if (auto a = internal::DrawProfilesMenu(ui)) {
            action = a;
        }
        ImGui::EndMenuBar();
    }

    // List of hotkeys.
    ImGui::SetNextWindowSizeConstraints(hotkeylist_min_size, max_dims);
    ImGui::BeginChild(
        "hotkey_list", hotkeylist_initial_size, ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX
    );
    if (auto a = internal::DrawHotkeyList(ui)) {
        action = a;
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Hotkey details.
    ImGui::BeginChild("hotkey_in_focus", ImVec2(.0f, .0f));
    if (ui.hotkey_in_focus < ui.hotkeys_ui.size()) {
        auto& hotkey = ui.hotkeys_ui[ui.hotkey_in_focus];

        if (auto a = internal::DrawKeysets(hotkey.keysets)) {
            action = a;
        }

        ImGui::Dummy(ImVec2(.0f, ImGui::GetTextLineHeight()));
        if (auto a = internal::DrawEquipsets(hotkey.equipsets, ui.status)) {
            action = a;
        }
    }
    ImGui::EndChild();

    if (auto a = internal::DrawStatusPopup(ui.status)) {
        action = a;
    }

    ImGui::End();
    if (action) {
        action();
    }
}

}  // namespace ui
}  // namespace ech
