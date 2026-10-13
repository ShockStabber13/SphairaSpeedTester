// Minimal SphairaSpeedTester application shell.
// The native NanoVG/deko3d framebuffer path below is retained from upstream.
// Speed-test, HTTP, transfer, UI and progress code are not reimplemented here.
#include "app.hpp"
#include "i18n.hpp"
#include "ui/nvg_util.hpp"
#include "log.hpp"
#include <nanovg_dk.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace sphaira {
namespace {
App* g_app{};
Mutex g_screen_mutex{};
int g_screen_refs{};
}
App::App(const char*) {
    g_app = this;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&m_pad);

    romfsInit();
    i18n::init(0);

    alignas(0x1000) struct { char file_based_path[0x80]; char nintendo[0x80]; } paths{};
    SecmonArgs args{};
    args.X[0] = 0xF0000404;
    args.X[1] = 0;
    args.X[2] = (u64)&paths;
    svcCallSecureMonitor(&args);
    std::memcpy(m_emummc_file.data(), paths.file_based_path, 0x80);
    std::memcpy(m_emummc_nintendo.data(), paths.nintendo, 0x80);

    if (appletGetOperationMode() == AppletOperationMode_Console) {
        s_width = 1920;
        s_height = 1080;
    }

    // Same GPU creation and memory pool sizing as the original App.
    device = dk::DeviceMaker{}.create();
    queue = dk::QueueMaker{device}.setFlags(DkQueueFlags_Graphics).create();
    pool_images.emplace(device, DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image, 16*1024*1024);
    pool_code.emplace(device, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code, 128*1024);
    pool_data.emplace(device, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, 1*1024*1024);
    cmdbuf = dk::CmdBufMaker{device}.create();
    const auto cmdmem = pool_data->allocate(StaticCmdSize);
    cmdbuf.addMemory(cmdmem.getMemBlock(), cmdmem.getOffset(), cmdmem.getSize());
    createFramebufferResources();
    renderer.emplace(s_width, s_height, device, queue, *pool_images, *pool_code, *pool_data);
    vg = nvgCreateDk(&*renderer, NVG_ANTIALIAS | NVG_STENCIL_STROKES);

    PlFontData standard{}, extended{};
    if (R_SUCCEEDED(plGetSharedFontByType(&standard, PlSharedFontType_Standard))) {
        int standard_id = nvgCreateFontMem(vg, "Standard", (unsigned char*)standard.address, standard.size, 0);
        if (R_SUCCEEDED(plGetSharedFontByType(&extended, PlSharedFontType_NintendoExt))) {
            const int extended_id = nvgCreateFontMem(vg, "Extended", (unsigned char*)extended.address, extended.size, 0);
            nvgAddFallbackFontId(vg, standard_id, extended_id);
        }
    }

    // Keep original Sphaira theme indices and palette used by ProgressBox.
    for (auto& entry : m_theme.elements) {
        entry.type = ElementType::Colour;
        entry.colour = nvgRGBA(255,255,255,255);
    }
    auto set = [&](ThemeEntryID id, unsigned r, unsigned g, unsigned b, unsigned a=255) {
        m_theme.elements[id].colour = nvgRGBA(r,g,b,a);
    };
    set(ThemeEntryID_BACKGROUND, 17, 31, 40);
    set(ThemeEntryID_POPUP, 20, 49, 68);
    set(ThemeEntryID_TEXT, 251,251,251);
    set(ThemeEntryID_TEXT_INFO,190,208,214);
    set(ThemeEntryID_TEXT_SELECTED,50,255,207);
    set(ThemeEntryID_LINE_SEPARATOR,22,57,81);
    set(ThemeEntryID_PROGRESSBAR,50,255,207);
    set(ThemeEntryID_PROGRESSBAR_BACKGROUND,11,21,25);
    set(ThemeEntryID_HIGHLIGHT_1,105,255,143);
    set(ThemeEntryID_HIGHLIGHT_2,92,190,255);
    // Original Sphaira constructor drops the init-phase CPU boost here.
    App::SetBoostMode(false);
}
App::~App() {
    // Match original destructor's boost reference until userAppExit.
    App::SetBoostMode(true);
    while (!m_widgets.empty()) m_widgets.pop_back();
    if (vg) nvgDeleteDk(vg);
    renderer.reset();
    destroyFramebufferResources();
    pool_data.reset();
    pool_code.reset();
    pool_images.reset();
    i18n::exit();
    romfsExit();
    g_app = nullptr;
}
App* App::GetApp() { return g_app; }
NVGcontext* App::GetVg() { return g_app ? g_app->vg : nullptr; }
void App::Push(std::unique_ptr<ui::Widget>&& widget) {
    if (!g_app || g_app->m_quit || widget->ShouldPop()) return;
    if (!g_app->m_widgets.empty()) g_app->m_widgets.back()->OnFocusLost();
    g_app->m_widgets.emplace_back(std::move(widget));
    g_app->m_widgets.back()->OnFocusGained();
}
void App::Notify(const std::string& msg, ui::NotifEntry::Side side) {
    if (g_app) g_app->m_notif_manager.Push(ui::NotifEntry(msg, side));
}
void App::Exit() { if (g_app) g_app->m_quit = true; }
bool App::IsFileBaseEmummc() {
    return g_app && g_app->m_emummc_file[0] && g_app->m_emummc_nintendo[0];
}
bool App::AcquireScreenToggle() {
    mutexLock(&g_screen_mutex);
    if (!g_screen_refs && R_FAILED(lblInitialize())) {
        mutexUnlock(&g_screen_mutex);
        return false;
    }
    ++g_screen_refs;
    mutexUnlock(&g_screen_mutex);
    return true;
}
void App::ReleaseScreenToggle() {
    mutexLock(&g_screen_mutex);
    if (g_screen_refs && !--g_screen_refs) {
        appletSetLcdBacklightOffEnabled(false);
        lblExit();
    }
    mutexUnlock(&g_screen_mutex);
}
void App::SetScreenDisabled(bool disable) {
    appletSetLcdBacklightOffEnabled(disable);
}
void App::PlaySoundEffect(SoundEffect) {
    // Tester has no Sphaira audio system; preserve visual and transfer semantics.
}
void App::Loop() {
    while (!m_quit && appletMainLoop() && !m_widgets.empty()) {
        m_controller.Reset();
        padUpdate(&m_pad);
        m_controller.m_kdown = padGetButtonsDown(&m_pad);
        m_controller.m_kup = padGetButtonsUp(&m_pad);
        m_controller.m_kheld = padGetButtons(&m_pad);
        HidTouchScreenState touches{};
        hidGetTouchScreenStates(&touches,1);
        m_touch_info.is_clicked = false;
        if (touches.count == 1 && !m_touch_info.is_touching) {
            m_touch_info.initial = m_touch_info.cur = touches.touches[0];
            m_touch_info.is_touching = true;
            m_touch_info.is_tap = true;
        } else if (touches.count && m_touch_info.is_touching) {
            m_touch_info.cur = touches.touches[0];
            if (std::abs((s32)m_touch_info.initial.x - (s32)m_touch_info.cur.x) > 20 ||
                std::abs((s32)m_touch_info.initial.y - (s32)m_touch_info.cur.y) > 20)
                m_touch_info.is_tap = false;
        } else if (m_touch_info.is_touching) {
            m_touch_info.is_touching = false;
            if (m_touch_info.is_tap) m_touch_info.is_clicked = true;
        }
        m_widgets.back()->Update(&m_controller, &m_touch_info);
        bool popped = false;
        while (!m_widgets.empty() && m_widgets.back()->ShouldPop()) {
            m_widgets.pop_back();
            popped = true;
        }
        if (m_widgets.empty()) break;
        if (popped) m_widgets.back()->OnFocusGained();

        const auto slot = queue.acquireImage(swapchain);
        queue.submitCommands(framebuffer_cmdlists[slot]);
        queue.submitCommands(render_cmdlist);
        nvgBeginFrame(vg, s_width, s_height, 1.f);
        nvgScale(vg, s_width / SCREEN_WIDTH, s_height / SCREEN_HEIGHT);
        // Start at the last menu; draw the unchanged original ProgressBox above it.
        auto start = m_widgets.size()-1;
        while (start && !m_widgets[start]->IsMenu()) --start;
        for (size_t i=start; i<m_widgets.size(); ++i)
            if (!m_widgets[i]->IsHidden()) m_widgets[i]->Draw(vg,&m_theme);
        m_notif_manager.Draw(vg,&m_theme);
        nvgResetTransform(vg);
        nvgEndFrame(vg);
        queue.presentImage(swapchain,slot);
    }
}
void App::createFramebufferResources() {
    this->swapchain = nullptr;

    // Create layout for the depth buffer
    dk::ImageLayout layout_depthbuffer;
    dk::ImageLayoutMaker{device}
        .setFlags(DkImageFlags_UsageRender | DkImageFlags_HwCompression)
        .setFormat(DkImageFormat_S8)
        .setDimensions(s_width, s_height)
        .initialize(layout_depthbuffer);

    // Create the depth buffer
    this->depthBuffer_mem = this->pool_images->allocate(layout_depthbuffer.getSize(), layout_depthbuffer.getAlignment());
    this->depthBuffer.initialize(layout_depthbuffer, this->depthBuffer_mem.getMemBlock(), this->depthBuffer_mem.getOffset());

    // Create layout for the framebuffers
    dk::ImageLayout layout_framebuffer;
    dk::ImageLayoutMaker{device}
        .setFlags(DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression)
        .setFormat(DkImageFormat_RGBA8_Unorm)
        .setDimensions(s_width, s_height)
        .initialize(layout_framebuffer);

    // Create the framebuffers
    std::array<DkImage const*, NumFramebuffers> fb_array;
    const u64 fb_size  = layout_framebuffer.getSize();
    const uint32_t fb_align = layout_framebuffer.getAlignment();
    for (unsigned i = 0; i < fb_array.size(); i++) {
        // Allocate a framebuffer
        this->framebuffers_mem[i] = pool_images->allocate(fb_size, fb_align);
        this->framebuffers[i].initialize(layout_framebuffer, framebuffers_mem[i].getMemBlock(), framebuffers_mem[i].getOffset());

        // Generate a command list that binds it
        dk::ImageView colorTarget{ framebuffers[i] }, depthTarget{ depthBuffer };
        this->cmdbuf.bindRenderTargets(&colorTarget, &depthTarget);
        this->framebuffer_cmdlists[i] = cmdbuf.finishList();

        // Fill in the array for use later by the swapchain creation code
        fb_array[i] = &framebuffers[i];
    }

    // Create the swapchain using the framebuffers
    this->swapchain = dk::SwapchainMaker{device, nwindowGetDefault(), fb_array}.create();

    // Generate the main rendering cmdlist
    this->recordStaticCommands();
}

void App::destroyFramebufferResources() {
    // Return early if we have nothing to destroy
    if (!this->swapchain) {
        return;
    }

    this->queue.waitIdle();
    this->cmdbuf.clear();
    swapchain.destroy();

    // Destroy the framebuffers
    for (unsigned i = 0; i < NumFramebuffers; i++) {
        framebuffers_mem[i].destroy();
    }

    // Destroy the depth buffer
    this->depthBuffer_mem.destroy();
}

void App::recordStaticCommands() {
    // Initialize state structs with deko3d defaults
    dk::RasterizerState rasterizerState;
    dk::ColorState colorState;
    dk::ColorWriteState colorWriteState;
    dk::BlendState blendState;

    // Configure the viewport and scissor
    this->cmdbuf.setViewports(0, { { 0.0f, 0.0f, (float)s_width, (float)s_height, 0.0f, 1.0f } });
    this->cmdbuf.setScissors(0, { { 0, 0, (u32)s_width, (u32)s_height } });

    // Clear the color and depth buffers
    this->cmdbuf.clearColor(0, DkColorMask_RGBA, 0.2f, 0.3f, 0.3f, 1.0f);
    this->cmdbuf.clearDepthStencil(true, 1.0f, 0xFF, 0);

    // Bind required state
    this->cmdbuf.bindRasterizerState(rasterizerState);
    this->cmdbuf.bindColorState(colorState);
    this->cmdbuf.bindColorWriteState(colorWriteState);

    this->render_cmdlist = this->cmdbuf.finishList();
}


} // namespace sphaira
