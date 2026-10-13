// SphairaSpeedTester: compatibility shims for deliberately removed UI features.
// No HTTP, SD, socket, CPU boost, or transfer implementation is changed.
// These functions are unreachable from the dedicated benchmark menu.
#include "utils/audio.hpp"
#include "owo.hpp"
#include "ui/menus/album_menu.hpp"

namespace sphaira::audio {
Result Init() { R_SUCCEED(); }
void ExitSignal() {}
void Exit() {}
Result PlaySoundEffect(SoundEffect) { R_SUCCEED(); }
Result OpenSong(fs::Fs*, const fs::FsPath&, u32, SongID* id) {
    if (id) *id = nullptr;
    R_THROW(1);
}
Result CloseSong(SongID* id) {
    if (id) *id = nullptr;
    R_SUCCEED();
}
Result PlaySong(SongID) { R_THROW(1); }
Result PauseSong(SongID) { R_THROW(1); }
Result SeekSong(SongID, u64) { R_THROW(1); }
Result GetVolumeSong(SongID, float*) { R_THROW(1); }
Result SetVolumeSong(SongID, float) { R_THROW(1); }
Result GetPitchSong(SongID, float*) { R_THROW(1); }
Result SetPitchSong(SongID, float) { R_THROW(1); }
Result GetInfo(SongID, Info*) { R_THROW(1); }
Result GetMeta(SongID, Meta*) { R_THROW(1); }
Result GetProgress(SongID, Progress*, State* state) {
    if (state) *state = State::Free;
    R_THROW(1);
}
}

namespace sphaira::ui::menu::album {
void SignalChange() {}
}

namespace sphaira {
Result install_forwarder(OwoConfig&, NcmStorageId) { R_THROW(1); }
Result install_forwarder(ui::ProgressBox*, OwoConfig&, NcmStorageId) { R_THROW(1); }
Result prepare_core_launch(const std::string&, const std::string&, CpuCoreMode) { R_THROW(1); }
bool core_launch_pending() { return false; }
}
