#include "frame_stats.h"
#include "generated/default/warriors_orochi_2_init.h"
#include <rex/rex_app.h>
#include <rex/cvar.h>
#include <rex/runtime.h>
void wo2_test_events(rex::system::KernelState* kernel);

class WarriorsOrochi2App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;
  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& defaults,
      std::function<void(rex::PathConfig)> /*resume*/) override {
    // The SDK resolves paths before loading the runtime TOML. Apply its
    // configured paths now, after loading and before constructing Runtime.
    auto paths = defaults;
    const auto game = REXCVAR_QUERY(std::string, game_data_root);
    const auto user = REXCVAR_QUERY(std::string, user_data_root);
    const auto update = REXCVAR_QUERY(std::string, update_data_root);
    const auto cache = REXCVAR_QUERY(std::string, cache_root);
    const auto metadata = REXCVAR_QUERY(std::string, metadata_root);
    if (!game.empty()) paths.game_data_root = game;
    if (!user.empty()) {
      paths.user_data_root = user;
      paths.cache_root = paths.user_data_root / "cache";
    }
    if (!update.empty()) paths.update_data_root = update;
    if (!cache.empty()) paths.cache_root = cache;
    if (!metadata.empty()) paths.metadata_root = metadata;
    return paths;
  }
  void OnPostSetup() override {
    SetGuestFrameStats(wo2_frame_stats);
    wo2_test_events(runtime()->kernel_state());
  }
  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<WarriorsOrochi2App>(
        new WarriorsOrochi2App(ctx, "warriors_orochi_2", PPCImageConfig));
  }
};
REX_DEFINE_APP(warriors_orochi_2, WarriorsOrochi2App::Create)

