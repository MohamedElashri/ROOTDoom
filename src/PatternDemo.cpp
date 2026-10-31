// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/PatternDemo.h"
#include "rootdoom/Frame.h"
#include "rootdoom/RootDoomWindow.h"
#include "rootdoom/RootImagePresenter.h"
#include "rootdoom/DoomRunner.h"
#include "rootdoom/FrameMailbox.h"
#include "rootdoom/InputQueue.h"
#include "rootdoom/DoomEngine.h"
#include "rootdoom/RootEntry.h"
#include "rootdoom/Wad.h"
#include "doomkeys.h"

#include <TApplication.h>
#include <TCanvas.h>
#include <TROOT.h>
#include <TSystem.h>
#include <TTimer.h>
#include <TGClient.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace rootdoom {
namespace {
using Clock = std::chrono::steady_clock;
constexpr auto kFramePeriod = std::chrono::microseconds(33333);

double Milliseconds(Clock::duration delta) { return std::chrono::duration<double, std::milli>(delta).count(); }

struct Runtime {
    FrameMailbox frames;
    InputQueue input;
    EngineReport engine_report;
    bool game = false;
    std::mutex title_mutex;
    std::string pending_title;
    DoomRunner runner; // Declared last: always joined before its services die.

    bool TakeTitle(std::string& title)
    {
        std::lock_guard<std::mutex> lock(title_mutex);
        if (pending_title.empty()) return false;
        title.swap(pending_title);
        pending_title.clear();
        return true;
    }

    void Start(const DemoOptions& options)
    {
        game = options.mode == DemoMode::NativeGame || options.mode == DemoMode::BatchGame;
        if (game) {
            const EngineOptions engine_options{options.iwad, options.data_directory, options.doom_arguments, options.log_input};
            runner.Start([this, engine_options](rootdoom::Clock& clock) {
                RunDoom(clock, frames, input, engine_options, engine_report, [this](const std::string& title) {
                    std::lock_guard<std::mutex> lock(title_mutex);
                    pending_title = title.substr(0, 256);
                });
            });
            return;
        }
        const bool log_input = options.log_input;
        runner.Start([this, log_input](rootdoom::Clock& clock) {
            std::vector<std::uint32_t> frame;
            std::array<bool, 256> held{};
            std::uint64_t sequence = 0;
            constexpr std::array<unsigned char, 9> controls{
                KEY_LEFTARROW, KEY_UPARROW, KEY_RIGHTARROW, KEY_DOWNARROW,
                KEY_FIRE, KEY_USE, KEY_RSHIFT, KEY_ENTER, KEY_ESCAPE};
            while (!clock.StopRequested()) {
                KeyEvent event;
                while (input.Pop(event)) {
                    held[event.key] = event.pressed;
                    if (log_input) std::cout << "[input] key=" << static_cast<unsigned>(event.key)
                                             << (event.pressed ? " down" : " up") << std::endl;
                }
                MakeTestPattern(frame, sequence++);
                for (unsigned i = 0; i < controls.size(); ++i) {
                    const std::uint32_t color = held[controls[i]] ? 0x00ff00 : 0x303030;
                    for (unsigned y = 340; y < 392; ++y) {
                        for (unsigned x = 8 + i * 69; x < 68 + i * 69; ++x) frame[y * kFrameWidth + x] = color;
                    }
                }
                frames.Publish(frame.data(), frame.size());
                clock.SleepMs(16); // Faster producer exercises stale-frame dropping.
            }
        });
    }
    void Stop()
    {
        input.ReleaseAll();
        runner.RequestStop();
        runner.Join();
    }
};

struct StopRuntime {
    Runtime* runtime;
    ~StopRuntime() { if (runtime) runtime->Stop(); }
};

// Percentiles use at most 4096 recent samples; sums/count/max cover the full run.
class Samples {
public:
    void Add(double value)
    {
        recent_[count_ % recent_.size()] = value;
        ++count_;
        total_ += value;
        max_ = std::max(max_, value);
    }
    double Mean() const { return count_ ? total_ / count_ : 0; }
    double Max() const { return max_; }
    double P95() const
    {
        const auto size = std::min<std::uint64_t>(count_, recent_.size());
        if (!size) return 0;
        std::vector<double> ordered(recent_.begin(), recent_.begin() + size);
        std::sort(ordered.begin(), ordered.end());
        return ordered[static_cast<std::size_t>(std::ceil(0.95 * size)) - 1];
    }
private:
    std::array<double, 4096> recent_{};
    std::uint64_t count_ = 0;
    double total_ = 0;
    double max_ = 0;
};

class PatternSession {
public:
    PatternSession(const DemoOptions& options, TCanvas& canvas, RootImagePresenter* presenter,
                   std::function<void(unsigned, unsigned)> resize, Runtime* runtime = nullptr,
                   std::function<void(const std::string&)> title = {})
        : options_(options), canvas_(canvas), presenter_(presenter), resize_(std::move(resize)),
          start_(Clock::now()), next_frame_(start_), last_proc_(start_), runtime_(runtime), title_(std::move(title))
    {
        proc_available_ = gSystem->GetProcInfo(&proc_start_) == 0;
        proc_end_ = proc_start_;
        peak_rss_ = proc_start_.fMemResident;
        frame_.reserve(kFramePixels);
    }

    bool Step()
    {
        if (runtime_ && runtime_->runner.State() == WorkerState::Failed) throw std::runtime_error(runtime_->runner.Error());
        if (runtime_ && runtime_->runner.State() == WorkerState::Finished) return true;
        if (runtime_ && title_) {
            std::string title;
            if (runtime_->TakeTitle(title)) title_("ROOTDoom — " + title);
        }
        auto now = Clock::now();
        const double elapsed = Milliseconds(now - start_) / 1000;
        if (options_.duration_seconds && elapsed >= options_.duration_seconds) {
            return true;
        }
        if (now - last_proc_ >= std::chrono::seconds(1)) {
            SampleProcess(elapsed);
            last_proc_ = now;
        }
        if (!presenter_ || now < next_frame_) {
            return false;
        }
        if (options_.exercise_resize) {
            const unsigned stage = static_cast<unsigned>(elapsed / 10);
            if (stage != resize_stage_) {
                constexpr std::array<std::array<unsigned, 2>, 4> sizes{{{1000, 500}, {640, 640}, {900, 600}, {800, 500}}};
                const auto& size = sizes[(stage - 1) % sizes.size()];
                resize_(size[0], size[1]);
                resize_stage_ = stage;
            }
        }
        if (runtime_) {
            if (!runtime_->frames.Consume(frame_, source_sequence_)) return false;
        } else {
            MakeTestPattern(frame_, frames_);
        }
        if (frames_) {
            intervals_.Add(Milliseconds(now - previous_frame_));
        }
        previous_frame_ = now;
        const auto generated = Clock::now();
        const auto times = presenter_->Present(frame_);
        generation_.Add(Milliseconds(generated - now));
        copies_.Add(times.copy_ms);
        images_.Add(times.image_ms);
        paints_.Add(times.paint_ms);
        total_.Add(Milliseconds(Clock::now() - now));
        ++frames_;
        next_frame_ += kFramePeriod;
        const auto done = Clock::now();
        if (next_frame_ < done) {
            const auto missed = (done - next_frame_) / kFramePeriod + 1;
            missed_deadlines_ += static_cast<std::uint64_t>(missed);
            next_frame_ += kFramePeriod * missed;
        }
        return false;
    }

    void Fail(const std::string& message) { error_ = message; }
    const std::string& Error() const { return error_; }

    void Finish()
    {
        elapsed_ = Milliseconds(Clock::now() - start_) / 1000;
        SampleProcess(elapsed_);
    }

    void Report() const
    {
        if (!presenter_) return;
        const double fps = elapsed_ > 0 ? frames_ / elapsed_ : 0;
        const bool native = options_.mode == DemoMode::NativePattern || options_.mode == DemoMode::NativeWorker || options_.mode == DemoMode::NativeGame;
        std::cout << (native ? "Native" : "Batch") << (runtime_ && runtime_->game ? " Doom" : " pattern") << ": frames=" << frames_
                  << ", elapsed=" << elapsed_ << "s, rate=" << fps
                  << " FPS, paint mean/p95=" << paints_.Mean() << '/' << paints_.P95()
                  << "ms, RSS peak=" << peak_rss_ << " KiB\n";
        if (options_.metrics.empty()) return;
        const std::filesystem::path file(options_.metrics);
        if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path());
        std::ofstream stream(file);
        if (!stream) throw std::runtime_error("Cannot write metrics: " + options_.metrics);
        stream << std::fixed << std::setprecision(6) << std::boolalpha;
        stream << "{\n  \"schema\": 1,\n  \"mode\": \"" << (native ? "native" : "batch") << "\",\n"
               << "  \"source\": \"" << (runtime_ ? (runtime_->game ? "doom" : "worker") : "gui") << "\",\n"
               << "  \"root_version\": " << std::quoted(gROOT->GetVersion()) << ",\n"
               << "  \"platform\": " << std::quoted(gSystem->GetBuildArch()) << ",\n"
               << "  \"framebuffer_width\": " << kFrameWidth << ",\n  \"framebuffer_height\": " << kFrameHeight << ",\n"
               << "  \"canvas_width\": " << canvas_.GetWw() << ",\n  \"canvas_height\": " << canvas_.GetWh() << ",\n"
               << "  \"target_fps\": 30,\n  \"requested_seconds\": " << options_.duration_seconds << ",\n"
               << "  \"elapsed_seconds\": " << elapsed_ << ",\n  \"presented_frames\": " << frames_ << ",\n"
               << "  \"completed_duration\": " << (options_.duration_seconds > 0 && elapsed_ >= options_.duration_seconds) << ",\n"
               << "  \"presented_fps\": " << fps << ",\n  \"missed_present_deadlines\": " << missed_deadlines_ << ",\n"
               << "  \"resize_events\": " << presenter_->ResizeCount() << ",\n"
               << "  \"scene_objects\": " << presenter_->SceneObjectCount() << ",\n"
               << "  \"percentile_window_frames\": 4096,\n  \"timing_excludes_screenshot\": true,\n";
        WriteSamples(stream, "pattern", generation_);
        WriteSamples(stream, "copy", copies_);
        WriteSamples(stream, "image_update", images_);
        WriteSamples(stream, "root_paint", paints_);
        WriteSamples(stream, "total", total_);
        WriteSamples(stream, "frame_interval", intervals_);
        const auto frames = runtime_ ? runtime_->frames.Counts() : FrameCounts{};
        const auto input = runtime_ ? runtime_->input.Counts() : InputCounts{};
        stream << "  \"mailbox_published\": " << frames.published << ",\n"
               << "  \"mailbox_consumed\": " << frames.consumed << ",\n"
               << "  \"mailbox_overwritten\": " << frames.overwritten << ",\n"
               << "  \"mailbox_pending\": " << frames.published - frames.consumed - frames.overwritten << ",\n"
               << "  \"input_delivered\": " << input.delivered << ",\n"
               << "  \"input_overflows\": " << input.overflows << ",\n";
        if (runtime_ && runtime_->game) {
            stream << "  \"engine_peak_allocations\": " << runtime_->engine_report.peak_allocations << ",\n"
                   << "  \"engine_peak_bytes\": " << runtime_->engine_report.peak_bytes << ",\n"
                   << "  \"engine_allocations_after_cleanup\": " << runtime_->engine_report.allocations_after_cleanup << ",\n"
                   << "  \"engine_files_after_cleanup\": " << runtime_->engine_report.files_after_cleanup << ",\n";
            const auto& initial = runtime_->engine_report.initial_player;
            const auto& final = runtime_->engine_report.final_player;
            stream << "  \"player_state_available\": " << final.valid << ",\n"
                   << "  \"player_initial_x\": " << initial.x / 65536.0 << ",\n"
                   << "  \"player_initial_y\": " << initial.y / 65536.0 << ",\n"
                   << "  \"player_final_x\": " << final.x / 65536.0 << ",\n"
                   << "  \"player_final_y\": " << final.y / 65536.0 << ",\n"
                   << "  \"player_final_health\": " << final.health << ",\n"
                   << "  \"player_initial_bullets\": " << initial.bullets << ",\n"
                   << "  \"player_final_bullets\": " << final.bullets << ",\n"
                   << "  \"use_presses\": " << runtime_->engine_report.use_presses << ",\n";
        }
        stream << "  \"process_metrics_available\": " << proc_available_ << ",\n"
               << "  \"cpu_seconds\": " << proc_end_.fCpuUser + proc_end_.fCpuSys - proc_start_.fCpuUser - proc_start_.fCpuSys << ",\n"
               << "  \"rss_start_kib\": " << proc_start_.fMemResident << ",\n"
               << "  \"rss_end_kib\": " << proc_end_.fMemResident << ",\n  \"rss_peak_kib\": " << peak_rss_ << ",\n"
               << "  \"rss_warmup_seconds\": 5,\n  \"rss_warm_samples\": " << warm_samples_ << ",\n"
               << "  \"rss_warm_min_kib\": " << warm_min_ << ",\n  \"rss_warm_max_kib\": " << warm_max_ << "\n}\n";
        stream.close();
        if (!stream) throw std::runtime_error("Failed writing metrics: " + options_.metrics);
    }

private:
    static void WriteSamples(std::ostream& stream, const char* name, const Samples& samples)
    {
        stream << "  \"" << name << "_ms\": {\"mean\": " << samples.Mean()
               << ", \"p95\": " << samples.P95() << ", \"max\": " << samples.Max() << "},\n";
    }
    void SampleProcess(double elapsed)
    {
        ProcInfo_t proc;
        if (gSystem->GetProcInfo(&proc) != 0) {
            proc_available_ = false;
            return;
        }
        proc_end_ = proc;
        peak_rss_ = std::max(peak_rss_, proc.fMemResident);
        if (elapsed >= 5) {
            if (!warm_samples_) warm_min_ = proc.fMemResident;
            warm_min_ = std::min(warm_min_, proc.fMemResident);
            warm_max_ = std::max(warm_max_, proc.fMemResident);
            ++warm_samples_;
        }
    }
    const DemoOptions& options_;
    TCanvas& canvas_;
    RootImagePresenter* presenter_;
    std::function<void(unsigned, unsigned)> resize_;
    Clock::time_point start_, next_frame_, last_proc_, previous_frame_;
    std::vector<std::uint32_t> frame_;
    Samples generation_, copies_, images_, paints_, total_, intervals_;
    std::uint64_t frames_ = 0, missed_deadlines_ = 0;
    unsigned resize_stage_ = 0, warm_samples_ = 0;
    ProcInfo_t proc_start_, proc_end_;
    bool proc_available_ = false;
    long peak_rss_ = 0, warm_min_ = 0, warm_max_ = 0;
    double elapsed_ = 0;
    std::string error_;
    Runtime* runtime_;
    std::uint64_t source_sequence_ = 0;
    std::function<void(const std::string&)> title_;
};

class DemoTimer final : public TTimer {
public:
    DemoTimer(PatternSession& session, RootDoomWindow& window) : TTimer(5, kTRUE), session_(session), window_(window) {}
    Bool_t Notify() override
    {
        bool finished = false;
        try {
            window_.FlushInput();
            finished = session_.Step();
        } catch (const std::exception& error) {
            session_.Fail(error.what());
            finished = true;
        }
        if (finished) {
            TurnOff();
            // Exercise the same ROOT client-message route as a WM close button.
            window_.SendCloseMessage();
        } else {
            Reset();
        }
        return kTRUE;
    }
private:
    PatternSession& session_;
    RootDoomWindow& window_;
};

void FinishDemo(PatternSession& session, RootImagePresenter* presenter, const DemoOptions& options)
{
    session.Finish();
    if (!session.Error().empty()) throw std::runtime_error(session.Error());
    if (presenter && !options.screenshot.empty()) presenter->Capture(options.screenshot);
    session.Report();
}

class InteractiveSession;
class InteractiveTimer final : public TTimer {
public:
    explicit InteractiveTimer(InteractiveSession& owner) : TTimer(5, kTRUE), owner_(owner) {}
    Bool_t Notify() override;
private:
    InteractiveSession& owner_;
};

// Survives macro return. On close, only this game's GUI/resources are destroyed;
// the stopped timer/owner remain valid until shutdown, never self-delete in Notify.
class InteractiveSession {
public:
    explicit InteractiveSession(DemoOptions options) : options_(std::move(options)), runtime_(std::make_unique<Runtime>())
    {
        window_ = std::make_unique<RootDoomWindow>(&runtime_->input, [this] { closing_ = true; });
        window_->SetWindowName("ROOTDoom — Doom inside the ROOT interpreter");
        presenter_ = std::make_unique<RootImagePresenter>(window_->Canvas());
        try {
            runtime_->Start(options_);
            session_ = std::make_unique<PatternSession>(options_, window_->Canvas(), presenter_.get(),
                [this](unsigned w, unsigned h) { window_->Resize(w, h); window_->Layout(); }, runtime_.get(),
                [this](const std::string& title) { window_->SetWindowName(title.c_str()); });
            timer_ = std::make_unique<InteractiveTimer>(*this);
            timer_->TurnOn();
        } catch (...) {
            runtime_->Stop();
            throw;
        }
    }
    ~InteractiveSession() { Finish(); }
    bool Running() const { return !closed_ && !closing_; }
    bool Tick()
    {
        if (closing_) { Finish(); return false; }
        try {
            window_->FlushInput();
            if (session_->Step()) window_->SendCloseMessage();
        } catch (const std::exception& error) {
            session_->Fail(error.what());
            window_->SendCloseMessage();
        }
        return true;
    }
    void Finish() noexcept
    {
        if (closed_) return;
        closed_ = true;
        if (timer_) timer_->TurnOff();
        if (runtime_) runtime_->Stop();
        if (session_) {
            try { FinishDemo(*session_, presenter_.get(), options_); }
            catch (const std::exception& error) { std::cerr << "ROOTDoom: " << error.what() << '\n'; }
        }
        session_.reset();
        presenter_.reset();
        window_.reset();
        runtime_.reset();
    }
private:
    DemoOptions options_;
    std::unique_ptr<Runtime> runtime_;
    std::unique_ptr<RootDoomWindow> window_;
    std::unique_ptr<RootImagePresenter> presenter_;
    std::unique_ptr<PatternSession> session_;
    std::unique_ptr<InteractiveTimer> timer_;
    bool closing_ = false, closed_ = false;
};

Bool_t InteractiveTimer::Notify()
{
    if (owner_.Tick()) Reset();
    else TurnOff();
    return kTRUE;
}

std::unique_ptr<InteractiveSession>& InteractiveOwner()
{
    // Constructed after TApplication; also cleaned before ROOT's atexit handlers.
    static std::unique_ptr<InteractiveSession> owner;
    return owner;
}

// Use a compiled virtual ROOT slot, rather than an interpreted global-function
// slot (which crashed in the tested ROOT release). This timer never starts.
class RootShutdownHook final : public TTimer {
public:
    ~RootShutdownHook() override
    {
        if (gApplication) gApplication->Disconnect("Terminate(Int_t)", this, "TurnOff()");
    }
    void TurnOff() override
    {
        TTimer::TurnOff();
        RootDoomShutdown();
    }
};

void InstallShutdownHook()
{
    static RootShutdownHook hook;
    static bool connected = false;
    if (!connected) {
        if (!gApplication->Connect("Terminate(Int_t)", "TTimer", &hook, "TurnOff()")) {
            throw std::runtime_error("Could not attach ROOT application shutdown handler");
        }
        connected = true;
    }
}
} // namespace

int LaunchInteractive(const char* iwad, const char* data_directory)
{
    if (!gApplication) throw std::runtime_error("Interpreter entry requires an existing ROOT application");
    InstallShutdownHook();
    if (gROOT->IsBatch()) throw std::runtime_error("Interpreter entry requires an existing native ROOT session (omit -b)");
    DemoOptions options;
    options.iwad = ValidateWad(iwad ? iwad : "").path;
    options.data_directory = data_directory && *data_directory ? data_directory : "rootdoom-data";
    options.metrics = (std::filesystem::path(options.data_directory) / "root-session.json").string();
    if (DoomHasRun() || InteractiveOwner()) throw std::logic_error("Doom can run only once per ROOT process; restart ROOT for a new session");
    TApplication::NeedGraphicsLibs();
    gApplication->InitializeGraphics();
    if (!gClient || gROOT->IsBatch()) throw std::runtime_error("ROOT native GUI initialization failed; check DISPLAY");
    InteractiveOwner() = std::make_unique<InteractiveSession>(std::move(options));
    std::cout << "ROOTDoom is running in this ROOT process. The prompt remains usable; close the game to stop it.\n";
    return 0;
}

int InteractiveRunning() { return InteractiveOwner() && InteractiveOwner()->Running() ? 1 : 0; }
void ShutdownInteractive() { if (InteractiveOwner()) InteractiveOwner()->Finish(); }

int RunDemo(const DemoOptions& options, char* executable)
{
    const bool game = options.mode == DemoMode::NativeGame || options.mode == DemoMode::BatchGame;
    const bool batch = options.mode == DemoMode::BatchPattern || options.mode == DemoMode::BatchWorker || options.mode == DemoMode::BatchGame;
    const bool threaded = options.mode == DemoMode::NativeWorker || options.mode == DemoMode::BatchWorker || game;
    char batch_flag[] = "-b";
    char* root_argv[] = {executable, batch ? batch_flag : nullptr, nullptr};
    int root_argc = batch ? 2 : 1;
    TApplication application("ROOTDoom", &root_argc, root_argv);
    std::unique_ptr<Runtime> runtime;
    if (threaded) runtime = std::make_unique<Runtime>();
    if (batch) {
        TCanvas canvas("rootdoom_batch", "ROOTDoom batch framebuffer test", 800, 500);
        canvas.SetCanvasSize(800, 500);
        RootImagePresenter presenter(canvas);
        if (runtime) runtime->Start(options);
        StopRuntime stop{runtime.get()};
        PatternSession session(options, canvas, &presenter,
                               [&canvas](unsigned w, unsigned h) { canvas.SetCanvasSize(w, h); }, runtime.get());
        while (!session.Step()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (runtime) runtime->Stop();
        FinishDemo(session, &presenter, options);
        return 0;
    }
    if (gROOT->IsBatch()) {
        std::cerr << "ROOT could not initialize its native GUI. Check DISPLAY and X11 permissions.\n";
        return 1;
    }
    RootDoomWindow window(runtime ? &runtime->input : nullptr);
    std::unique_ptr<RootImagePresenter> presenter;
    if (options.mode == DemoMode::NativePattern || threaded) {
        presenter = std::make_unique<RootImagePresenter>(window.Canvas());
        window.SetWindowName("ROOTDoom — 640 x 400 framebuffer test");
        std::cout << "Animated ROOT canvas: resize/move/minimize/restore the window; close to stop.\n";
        if (threaded && !game) {
            window.SetWindowName("ROOTDoom — worker input: Left Up Right Down Ctrl Space Shift Enter Esc");
            std::cout << "Input boxes, left to right: Left Up Right Down Ctrl Space Shift Enter Esc. Click canvas to focus.\n";
        }
        if (game) {
            window.SetWindowName("ROOTDoom — starting Doom");
            std::cout << "Doom: arrows move/turn, Ctrl fire, Space use, Shift run, Enter select, Escape menu. Sound disabled.\n";
        }
    }
    if (runtime) runtime->Start(options);
    StopRuntime stop{runtime.get()};
    PatternSession session(options, window.Canvas(), presenter.get(), [&window](unsigned w, unsigned h) {
        window.Resize(w, h);
        window.Layout();
    }, runtime.get(), [&window](const std::string& title) { window.SetWindowName(title.c_str()); });
    DemoTimer timer(session, window);
    timer.TurnOn();
    application.Run(kTRUE);
    timer.TurnOff();
    if (runtime) runtime->Stop();
    FinishDemo(session, presenter.get(), options);
    return 0;
}

} // namespace rootdoom

extern "C" int RootDoomLaunch(const char* iwad, const char* data_directory)
{
    try { return rootdoom::LaunchInteractive(iwad, data_directory); }
    catch (const std::exception& error) { std::cerr << "ROOTDoom: " << error.what() << '\n'; return 1; }
}
extern "C" int RootDoomIsRunning() { return rootdoom::InteractiveRunning(); }
extern "C" void RootDoomShutdown() { rootdoom::ShutdownInteractive(); }
