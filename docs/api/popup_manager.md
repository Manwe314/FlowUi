# Popup Manager API

`PopupManager` is the window-scoped service for popup placement and behavior. It turns a stable popup identity plus a `PopupRequest` into the `Clay_FloatingElementConfig` needed for the current frame. It also owns first-frame measurement, viewport overflow correction, popup z-order allocation, and automatic dismissal.

Access it through `UiManager`:

```cpp
FlowUi::PopupManager& popups = app.ui().popups();
```

The manager does not draw a surface, dim the application, trap keyboard focus, or decide what children belong in a popup. Those remain element and application policy. This keeps the service usable by arbitrary custom Flow elements as well as `FlowUi::FSEL::kPopupSurface`.

## Popup Request

```cpp
struct PopupRequest {
    PopupAnchor anchor = PopupAnchor::parent();
    PopupPlacement placement{};
    PopupOverflowPolicy overflow{};
    PopupFirstFramePolicy firstFrame = PopupFirstFramePolicy::DeferUntilMeasured;
    std::optional<Clay_Dimensions> expectedSize = std::nullopt;
    PopupLayer layer = PopupLayer::CasualPopup;
    Clay_PointerCaptureMode pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE;
    Clay_FloatingClipToElement clipTo = CLAY_CLIP_TO_NONE;
    PopupOutsidePressPolicy outsidePress =
        PopupOutsidePressPolicy::DismissAndBlockAnchor;
    bool dismissOnEscape = true;
};
```

`PopupAnchor` supports the current Clay parent, a specific Flow element, a pointer snapshot, a pointer-following anchor, an absolute position, or the viewport. Element anchors resolve from completed Clay geometry, so the usual anchor is a stable element that was present on the preceding frame. Keep the anchor authored before its popup as well.

`PopupPlacement` names both attachment points with the same nine-point vocabulary: `TopLeft`, `TopMiddle`, `TopRight`, `MiddleLeft`, `Center`, `MiddleRight`, `BottomLeft`, `BottomMiddle`, and `BottomRight`. Pointer and absolute-position anchors have no size, so all nine anchor points resolve to the same logical coordinate.

The overflow policy first keeps the requested placement when it fits. Otherwise it can try horizontal and vertical mirrors, select the candidate with the least overflow, and shift the result into the viewport.

`PopupLayer` provides three conventional z-index bands starting at 10000:

- `CasualPopup`
- `WarningPopup`
- `CriticalPopup`

These bands order manager-authored popups but do not reserve Clay z-index values globally. Custom elements may still use overlapping values when their application deliberately wants that behavior.

## First-Frame Placement

The default `DeferUntilMeasured` policy returns a measurement-only floating configuration when no prior measured size is available. The element must still construct its complete, normally styled popup tree on this frame; the manager places it offscreen with pointer passthrough. On the next frame the measured size is available for overflow correction and normal display.

Supplying a correct `expectedSize` avoids that hidden measurement frame:

```cpp
FlowUi::PopupRequest request{
    .anchor = FlowUi::PopupAnchor::viewport(),
    .placement = {
        .anchorPoint = FlowUi::PopupAttachmentPoint::TopMiddle,
        .popupPoint = FlowUi::PopupAttachmentPoint::TopMiddle,
        .offset = {0.0f, 24.0f},
    },
    .expectedSize = Clay_Dimensions{340.0f, 66.0f},
    .layer = FlowUi::PopupLayer::WarningPopup,
};
```

`PlaceImmediately` also draws on the first frame, but without a known size overflow correction cannot account for the popup extent.

## Request and Dismissal API

`request()` accepts `FlowElementID`, `GlobalFlowID`, and `FlowElementPartID`:

```cpp
FlowUi::PopupFrame frame = ui.popups().request(popupId, request);

Clay_ElementDeclaration root{};
if (frame.visible || frame.measureOnly) {
    root.floating = frame.floating;
}
```

Submit each present popup exactly once per frame under a stable identity. `PopupFrame::visible` means the surface should be drawn normally. `measureOnly` means it must be constructed completely but remains offscreen for this frame. When both are false, custom construct-only elements must keep their children out of normal layout, typically by returning an inert offscreen floating declaration.

Automatic outside-press and Escape dismissal is evaluated at frame start from the last committed popup bounds. Only the topmost popup owns the outside-press decision for one input transition. `PopupOutsidePressPolicy` makes the resulting input ownership explicit:

- `DismissAndConsume` dismisses the popup and hides the complete primary-pointer gesture from every other UI consumer.
- `DismissAndBlockAnchor` dismisses the popup and removes its anchor from the press targets, preventing the common dismiss-then-reopen trigger bug while leaving unrelated controls usable.
- `Ignore` does not dismiss and leaves the press untouched.

The anchor-only policy is the default. It is most appropriate for dropdowns and similar anchored surfaces. Full consumption is useful for heads-up or modal-like surfaces whose dismissing click must not activate anything underneath. Programmatic dismissal uses the same state path but does not consume input:

```cpp
ui.popups().dismiss(popupId);
```

The popup owner observes the edge once with `consumeDismissed()`:

```cpp
if (ui.popups().consumeDismissed(popupId)) {
    popupOpen = false;
}
```

A dismissed identity remains dismissed while it continues to be requested. Omit the popup after consuming dismissal; requesting the same identity again after it has been absent creates a fresh popup session.

## FSEL PopupSurface

When FSEL is enabled, `FlowUi::FSEL::kPopupSurface` is the standard construct-only adapter. It requests `PopupManager`, applies the returned floating configuration, supplies a themed Box-like surface, and invokes `onDismissed` when the manager reports dismissal. The application still owns the open flag and conditionally authors the element; bind `onDismissed` to clear that flag:

```cpp
if (menuOpen) {
    ui.createElement(FlowUi::FSEL::kPopupSurface, "edit-menu")
        .setParameters(FlowUi::FSEL::PopupSurfaceParameters{
            .popupRequest = {
                .anchor = FlowUi::PopupAnchor::element(editButtonId),
            },
            .onDismissed = closeMenuAction,
        })
        .construct();

    CLAY_TEXT(CLAY_STRING("Menu contents"), CLAY_TEXT_CONFIG(textConfig));
    ui.drawConstructed();
}
```

The surface exposes sizing, padding, child layout, clipping, background, border, and corner-radius overrides. It intentionally has no `isOpen` parameter, built-in content, modal scrim, focus trap, animation policy, or semantic menu/dialog behavior.
