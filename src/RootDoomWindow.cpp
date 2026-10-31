// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/RootDoomWindow.h"
#include "rootdoom/InputBridge.h"
#include "rootdoom/KeyMapping.h"

#include <TApplication.h>
#include <TCanvas.h>
#include <TGClient.h>
#include <TRootEmbeddedCanvas.h>
#include <TVirtualX.h>
#include <TStyle.h>
#include <chrono>

namespace rootdoom {
namespace {
std::uint64_t NowMs()
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

class InputCanvas final : public TRootEmbeddedCanvas {
public:
    explicit InputCanvas(RootDoomWindow& owner) : TRootEmbeddedCanvas("ROOTDoomCanvas", &owner, 800, 500), owner_(owner)
    { if (owner.HasGameInput()) SetDNDTarget(kFALSE); }
protected:
    Bool_t HandleContainerKey(Event_t* event) override
    { return owner_.HasGameInput() ? owner_.HandleKey(event) : TRootEmbeddedCanvas::HandleContainerKey(event); }
    Bool_t HandleContainerButton(Event_t* event) override
    {
        if (!owner_.HasGameInput()) return TRootEmbeddedCanvas::HandleContainerButton(event);
        if (event->fType == kButtonPress) owner_.FocusInput();
        return kTRUE;
    }
    Bool_t HandleContainerMotion(Event_t* event) override
    { return owner_.HasGameInput() ? kTRUE : TRootEmbeddedCanvas::HandleContainerMotion(event); }
    Bool_t HandleContainerDoubleClick(Event_t* event) override
    { return owner_.HasGameInput() ? kTRUE : TRootEmbeddedCanvas::HandleContainerDoubleClick(event); }
    Bool_t HandleContainerCrossing(Event_t* event) override
    { return owner_.HasGameInput() ? kTRUE : TRootEmbeddedCanvas::HandleContainerCrossing(event); }
private:
    RootDoomWindow& owner_;
};
} // namespace

RootDoomWindow::RootDoomWindow(InputQueue* input, std::function<void()> close, bool geometry)
    : TGMainFrame(gClient->GetRoot(), 800, 500), close_(std::move(close))
{
    if (input) input_ = std::make_unique<InputBridge>(*input);
    AddInput(kKeyPressMask | kKeyReleaseMask | kFocusChangeMask);
    SetCleanup(kDeepCleanup);
    SetWindowName("ROOTDoom — native ROOT GUI baseline");
    {
        struct CanvasPreference {
            Bool_t previous = gStyle->GetCanvasPreferGL();
            ~CanvasPreference() { gStyle->SetCanvasPreferGL(previous); }
        } preference;
        gStyle->SetCanvasPreferGL(geometry);
        embedded_ = new InputCanvas(*this);
    }
    AddFrame(embedded_, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
    Canvas().SetFillColor(1);
    Canvas().Modified();
    Canvas().Update();
    MapSubwindows();
    Resize(GetDefaultSize());
    MapWindow();
}

RootDoomWindow::~RootDoomWindow()
{
    if (input_) input_->ReleaseAll();
    Cleanup();
}

void RootDoomWindow::CloseWindow()
{
    if (!closing_) {
        closing_ = true;
        if (input_) input_->ReleaseAll();
        // Keep the drawable mapped for final capture, after the timer is stopped.
        if (close_) close_();
        else gApplication->Terminate(0);
    }
}

TCanvas& RootDoomWindow::Canvas() { return *embedded_->GetCanvas(); }

Bool_t RootDoomWindow::HandleKey(Event_t* event)
{
    if (!input_) return TGMainFrame::HandleKey(event);
    if (event->fType == kGKeyPress) {
        UInt_t symbol = 0;
        char text[16]{};
        gVirtualX->LookupString(event, text, sizeof(text), symbol);
        input_->Press(event->fCode, DoomKey(symbol), static_cast<std::uint64_t>(event->fTime));
    } else if (event->fType == kKeyRelease) {
        input_->Release(event->fCode, static_cast<std::uint64_t>(event->fTime), NowMs());
    }
    return kTRUE;
}
Bool_t RootDoomWindow::HandleFocusChange(Event_t* event)
{
    if (input_ && event->fType == kFocusOut) check_focus_ = true;
    return kTRUE;
}
void RootDoomWindow::FocusInput()
{
    if (input_) gVirtualX->SetInputFocus(GetId());
}
void RootDoomWindow::FlushInput()
{
    if (!input_) return;
    if (check_focus_) {
        const auto focused = gVirtualX->GetInputFocus();
        if (focused != GetId() && focused != embedded_->GetContainer()->GetId()) input_->ReleaseAll();
        check_focus_ = false;
    }
    input_->Flush(NowMs());
}

} // namespace rootdoom
