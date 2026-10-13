#pragma once
#include "ui/widget.hpp"
#include <span>
#include <string>

namespace sphaira::ui::menu::main {
// The known-good main_menu.cpp exports an empty registry; no Sphaira tabs.
struct MiscMenuEntry {};
auto GetMenuMenuEntries() -> std::span<const MiscMenuEntry>;
struct MainMenu final : Widget {
    MainMenu();
    ~MainMenu();
    void Update(Controller*, TouchInfo*) override;
    void Draw(NVGcontext*, Theme*) override;
    void OnFocusGained() override;
    void OnFocusLost() override;
    bool IsMenu() const override { return true; }
private:
    std::string m_url_host{};
    std::string m_last_result{};
};
} // namespace sphaira::ui::menu::main
