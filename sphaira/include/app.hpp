#pragma once
// Dedicated benchmark shell. The known-good speed-test, HTTP, transfer,
// progress/option widget and NanoVG draw functions are separate, unchanged files.
#include <switch.h>
#include <nanovg.h>
#include <nanovg/dk_renderer.hpp>
#include "ui/widget.hpp"
#include "ui/notification.hpp"
#include "utils/audio.hpp"
#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace sphaira {
using SoundEffect = audio::SoundEffect;

class App {
public:
    struct BoolOption { bool Get() const { return true; } };
    BoolOption m_progress_boost_mode{};
    explicit App(const char* argv0);
    ~App();
    void Loop();
    static App* GetApp();
    static NVGcontext* GetVg();
    static void Push(std::unique_ptr<ui::Widget>&& widget);
    template<ui::DerivedFromWidget T, typename... Args>
    static void Push(Args&&... args) {
        Push(std::make_unique<T>(std::forward<Args>(args)...));
    }
    static void Notify(const std::string& msg, ui::NotifEntry::Side side = ui::NotifEntry::Side::RIGHT);
    static void Exit();
    static long GetTextScrollSpeed() { return 1; }
    static bool IsApplication() {
        const auto type = appletGetAppletType();
        return type == AppletType_Application || type == AppletType_SystemApplication;
    }
    static bool IsFileBaseEmummc();
    static bool AcquireScreenToggle();
    static void ReleaseScreenToggle();
    static void SetScreenDisabled(bool disable);
    static void PlaySoundEffect(SoundEffect effect);
    static void SetAutoSleepDisabled(bool enable) {
        static Mutex mutex{};
        static int refs{};
        mutexLock(&mutex);
        if (enable) ++refs;
        else if (refs) --refs;
        appletSetAutoSleepDisabled(refs != 0);
        mutexUnlock(&mutex);
    }
    // Match the original Sphaira's reference-counted CPU boost.
    static void SetBoostMode(bool enable, bool force = false) {
        static Mutex mutex{};
        static int refs{};
        mutexLock(&mutex);
        if (enable) {
            ++refs;
            appletSetCpuBoostMode(ApmCpuBoostMode_FastLoad);
        } else if (refs) --refs;
        if (!refs || force) {
            refs = 0;
            appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
        }
        mutexUnlock(&mutex);
    }
private:
    static constexpr unsigned NumFramebuffers = 2;
    static constexpr unsigned StaticCmdSize = 0x1000;
    unsigned s_width{1280}, s_height{720};
    dk::UniqueDevice device;
    dk::UniqueQueue queue;
    std::optional<CMemPool> pool_images, pool_code, pool_data;
    dk::UniqueCmdBuf cmdbuf;
    CMemPool::Handle depthBuffer_mem;
    CMemPool::Handle framebuffers_mem[NumFramebuffers];
    dk::Image depthBuffer;
    dk::Image framebuffers[NumFramebuffers];
    DkCmdList framebuffer_cmdlists[NumFramebuffers];
    dk::UniqueSwapchain swapchain;
    DkCmdList render_cmdlist;
    std::optional<nvg::DkRenderer> renderer;
    NVGcontext* vg{};
    PadState m_pad{};
    Controller m_controller{};
    TouchInfo m_touch_info{};
    Theme m_theme{};
    ui::NotifMananger m_notif_manager{};
    std::vector<std::unique_ptr<ui::Widget>> m_widgets{};
    std::array<char,0x80> m_emummc_file{};
    std::array<char,0x80> m_emummc_nintendo{};
    bool m_quit{};
    void createFramebufferResources();
    void destroyFramebufferResources();
    void recordStaticCommands();
};
} // namespace sphaira
