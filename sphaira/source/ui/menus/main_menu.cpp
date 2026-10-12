// Sphaira Speed Tester: original Sphaira App + UI, dedicated benchmark
// home screen. No file browser, shop, installer, updater or tab navigation.
#include "ui/menus/main_menu.hpp"
#include "app.hpp"
#include "speed_test.hpp"
#include "ui/widget.hpp"
#include <nanovg.h>
#include <cstdio>
#include <fstream>
#include <string>

namespace sphaira::ui::menu::main {
namespace {
constexpr const char* URL_PATH = "sdmc:/switch/SphairaSpeedTester/url.txt";
constexpr const char* RESULT_PATH = "sdmc:/switch/SphairaSpeedTester-result.txt";
const NVGcolor BACK = nvgRGBA(0, 0, 0, 255);
const NVGcolor GREEN = nvgRGBA(21, 190, 25, 255);
const NVGcolor WHITE = nvgRGBA(255, 255, 255, 255);
const NVGcolor GRAY = nvgRGBA(166, 166, 166, 255);

std::string loadLine(const char* path) {
    std::ifstream f(path);
    std::string line;
    std::getline(f, line);
    return line;
}

std::string hostname(const std::string& u) {
    const auto start = u.find("://");
    if (start == std::string::npos) return {};
    const auto first = start + 3;
    return u.substr(first, u.find_first_of("/?#", first) - first);
}

void text(NVGcontext* vg, float x, float y, float font,
          NVGcolor color, const char* content) {
    nvgFontFace(vg, "Standard");
    nvgFontSize(vg, font);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg, color);
    nvgText(vg, x, y, content, nullptr);
}
} // namespace

// Hide Sphaira's tab registry; the only available entry point is this tester.
auto GetMenuMenuEntries() -> std::span<const MiscMenuEntry> { return {}; }

MainMenu::MainMenu() {
    SetAction(Button::A, Action{[] { speedtest::Start(false); }});
    SetAction(Button::X, Action{[] { speedtest::Start(true); }});
    SetAction(Button::START, Action{[] { App::Exit(); }});
}
MainMenu::~MainMenu() = default;

void MainMenu::Update(Controller* controller, TouchInfo* touch) {
    Widget::Update(controller, touch);
}
void MainMenu::OnFocusGained() {
    Widget::OnFocusGained();
    const auto url = loadLine(URL_PATH);
    m_url_host = hostname(url);
    m_last_result = loadLine(RESULT_PATH);
}
void MainMenu::OnFocusLost() {
    Widget::OnFocusLost();
}

void MainMenu::Draw(NVGcontext* vg, Theme*) {
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    nvgFillColor(vg, BACK);
    nvgFill(vg);

    text(vg, 70, 60, 48, GREEN, "SPHAIRA SPEED TESTER");
    text(vg, 73, 126, 22, GRAY, "ALLDEBRID  /  ORIGINAL SPHAIRA DOWNLOAD ENGINE");

    nvgBeginPath(vg);
    nvgRect(vg, 73, 185, 1134, 2);
    nvgFillColor(vg, GREEN);
    nvgFill(vg);

    text(vg, 73, 228, 23, WHITE, "SOURCE");
    text(vg, 73, 271, 25, m_url_host.empty() ? GRAY : GREEN,
         m_url_host.empty() ? "No URL. Add /switch/SphairaSpeedTester/url.txt" :
                              m_url_host.c_str());

    nvgBeginPath(vg);
    nvgRoundedRect(vg, 72, 352, 540, 110, 13);
    nvgFillColor(vg, GREEN);
    nvgFill(vg);
    text(vg, 102, 388, 28, BACK, "A   HTTP ONLY");

    nvgBeginPath(vg);
    nvgRoundedRect(vg, 642, 352, 540, 110, 13);
    nvgStrokeColor(vg, GREEN);
    nvgStrokeWidth(vg, 3);
    nvgStroke(vg);
    text(vg, 675, 388, 28, WHITE, "X   HTTP + SD WRITE");

    text(vg, 73, 508, 20, GRAY, "LAST RESULT");
    if (m_last_result.empty()) {
        text(vg, 73, 544, 20, WHITE, "Run either test to measure throughput.");
    } else {
        // Results include only counts, rate and status, never the signed URL.
        text(vg, 73, 544, 18, WHITE, m_last_result.c_str());
    }

    text(vg, 73, 662, 20, GRAY, "+  EXIT   |   B  CANCEL DURING DOWNLOAD");
}
} // namespace sphaira::ui::menu::main
