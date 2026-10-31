// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <TGFrame.h>
#include <memory>
#include <functional>

class TCanvas;
class TRootEmbeddedCanvas;

namespace rootdoom {
class InputQueue;
class InputBridge;

// Owned by main. CloseWindow ends the event loop; it never deletes this object.
class RootDoomWindow final : public TGMainFrame {
public:
    explicit RootDoomWindow(InputQueue* input = nullptr, std::function<void()> close = {}, bool geometry = false);
    ~RootDoomWindow() override;
    void CloseWindow() override;
    TCanvas& Canvas();
    Bool_t HandleKey(Event_t* event) override;
    Bool_t HandleFocusChange(Event_t* event) override;
    void FlushInput();
    void FocusInput();
    bool HasGameInput() const { return static_cast<bool>(input_); }

private:
    TRootEmbeddedCanvas* embedded_ = nullptr; // Owned by TGMainFrame's deep cleanup.
    bool closing_ = false;
    bool check_focus_ = false;
    std::unique_ptr<InputBridge> input_;
    std::function<void()> close_;
};

} // namespace rootdoom
