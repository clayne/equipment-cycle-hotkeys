#pragma once

#include "dev_util.h"
#include "equipsets.h"
#include "hotkeys.h"
#include "ir.h"
#include "keys.h"
#include "tes_util.h"

namespace ech {
namespace internal {

inline bool
GameIsAcceptingInput() {
    auto* ui = RE::UI::GetSingleton();
    auto* control_map = RE::ControlMap::GetSingleton();
    // clang-format off
    return ui
        && !ui->GameIsPaused()
        && !ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)
        && !ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME)
        && control_map
        && control_map->IsMovementControlsEnabled();
    // clang-format on
}

}  // namespace internal

class EventHandler final : public RE::BSTEventSink<RE::InputEvent*>,
                           public RE::BSTEventSink<RE::TESEquipEvent> {
  public:
    static std::expected<void, std::string_view>
    Register() {
        auto* idm = RE::BSInputDeviceManager::GetSingleton();
        auto* sesh = RE::ScriptEventSourceHolder::GetSingleton();
        if (!idm || !sesh) {
            return std::unexpected("failed to get event sources");
        }

        static EventHandler h;
        idm->AddEventSink<RE::InputEvent*>(&h);
        sesh->AddEventSink<RE::TESEquipEvent>(&h);
        return {};
    }

    /// Triggers hotkey activations.
    RE::BSEventNotifyControl
    ProcessEvent(RE::InputEvent* const* events, RE::BSTEventSource<RE::InputEvent*>*) override {
        HandleInputEvents(events);
        return RE::BSEventNotifyControl::kContinue;
    }

    /// Deactivates hotkeys if something else equips/unequips player gear.
    RE::BSEventNotifyControl
    ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*) override {
        HandleEquipEvent(event);
        return RE::BSEventNotifyControl::kContinue;
    }

  private:
    void
    HandleInputEvents(RE::InputEvent* const* events) {
        if (!events || !internal::GameIsAcceptingInput()) {
            return;
        }
        auto* control_map = RE::ControlMap::GetSingleton();
        if (!control_map || !control_map->IsMovementControlsEnabled()) {
            return;
        }

        keystroke_buf_.clear();
        Keystroke::InputEventsToBuffer(*events, keystroke_buf_);
        if (keystroke_buf_.empty()) {
            return;
        }

        auto* aem = RE::ActorEquipManager::GetSingleton();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!aem || !player) {
            return;
        }

        const auto* equipset = dev_util::input_handlers::UseHotkeys(
            hotkeys_, keystroke_buf_, *player
        );
        if (equipset) {
            // Note the ordering here. Most-recent-equip-time must be reset prior to equipset-apply
            // because the latter will trigger equip events.
            most_recent_hotkey_equip_time_ = RE::GetDurationOfApplicationRunTime();
            equipset->Apply(*aem, *player);
        }
    }

    void
    HandleEquipEvent(const RE::TESEquipEvent* event) {
        if (!event || !event->actor || !event->actor->IsPlayerRef()) {
            return;
        }
        const auto* form = RE::TESForm::LookupByID(event->baseObject);
        if (!form) {
            return;
        }

        // Ignore equip/unequip actions on items that don't map to supported gear slots.
        switch (form->GetFormType()) {
            case RE::FormType::Armor:
                if (!tes_util::IsShield(form)) {
                    return;
                }
            case RE::FormType::Spell:
            case RE::FormType::Weapon:
            case RE::FormType::Light:
            case RE::FormType::Ammo:
            case RE::FormType::Shout:
                break;
            default:
                return;
        }

        auto now = RE::GetDurationOfApplicationRunTime();
        if (now >= most_recent_hotkey_equip_time_ + 500) {
            hotkeys_.Deactivate();
        }
    }

    EventHandler() = default;
    EventHandler(const EventHandler&) = delete;
    EventHandler& operator=(const EventHandler&) = delete;
    EventHandler(EventHandler&&) = delete;
    EventHandler& operator=(EventHandler&&) = delete;

    /// Reusable buffer for storing input keystrokes and avoiding per-input-event allocations.
    std::vector<Keystroke> keystroke_buf_;
    /// In milliseconds since application start.
    uint32_t most_recent_hotkey_equip_time_ = 0;
    Hotkeys<> hotkeys_ = HotkeysIR(std::vector{
                                       HotkeyIR<Keyset, EquipsetUI>{
                                           .name = "1",
                                           .keysets = {{2}},
                                       },
                                       HotkeyIR<Keyset, EquipsetUI>{
                                           .name = "2",
                                           .keysets = {{3}},
                                       },
                                       HotkeyIR<Keyset, EquipsetUI>{
                                           .name = "3",
                                           .keysets = {{4}},
                                       },
                                       HotkeyIR<Keyset, EquipsetUI>{
                                           .name = "4",
                                           .keysets = {{5}},
                                       },
                                   })
                             .ConvertEquipset(std::mem_fn(&EquipsetUI::To))
                             .Into();
};

}  // namespace ech
