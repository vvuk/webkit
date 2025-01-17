/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DebugPageOverlays.h"

#include "ElementIterator.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "MainFrame.h"
#include "Page.h"
#include "PageOverlay.h"
#include "PageOverlayController.h"
#include "Region.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"

namespace WebCore {

DebugPageOverlays* DebugPageOverlays::sharedDebugOverlays;

class RegionOverlay : public RefCounted<RegionOverlay>, public PageOverlay::Client {
public:
    static Ref<RegionOverlay> create(MainFrame&, DebugPageOverlays::RegionType);
    virtual ~RegionOverlay();

    void recomputeRegion();
    PageOverlay& overlay() { return *m_overlay; }

protected:
    RegionOverlay(MainFrame&, Color);

private:
    void willMoveToPage(PageOverlay&, Page*) final;
    void didMoveToPage(PageOverlay&, Page*) final;
    void drawRect(PageOverlay&, GraphicsContext&, const IntRect& dirtyRect) override;
    bool mouseEvent(PageOverlay&, const PlatformMouseEvent&) final;
    void didScrollFrame(PageOverlay&, Frame&) final;

protected:
    // Returns true if the region changed.
    virtual bool updateRegion() = 0;
    void drawRegion(GraphicsContext&, const Region&, const Color&, const IntRect& dirtyRect);
    
    MainFrame& m_frame;
    RefPtr<PageOverlay> m_overlay;
    std::unique_ptr<Region> m_region;
    Color m_color;
};

class MouseWheelRegionOverlay final : public RegionOverlay {
public:
    static Ref<MouseWheelRegionOverlay> create(MainFrame& frame)
    {
        return adoptRef(*new MouseWheelRegionOverlay(frame));
    }

private:
    explicit MouseWheelRegionOverlay(MainFrame& frame)
        : RegionOverlay(frame, Color(0.5f, 0.0f, 0.0f, 0.4f))
    {
    }

    bool updateRegion() override;
};

bool MouseWheelRegionOverlay::updateRegion()
{
    auto region = std::make_unique<Region>();
    
    for (const Frame* frame = &m_frame; frame; frame = frame->tree().traverseNext()) {
        if (!frame->view() || !frame->document())
            continue;

        auto frameRegion = frame->document()->absoluteRegionForEventTargets(frame->document()->wheelEventTargets());
        frameRegion.first.translate(toIntSize(frame->view()->contentsToRootView(IntPoint())));
        region->unite(frameRegion.first);
    }
    
    region->translate(m_overlay->viewToOverlayOffset());

    bool regionChanged = !m_region || !(*m_region == *region);
    m_region = WTFMove(region);
    return regionChanged;
}

class NonFastScrollableRegionOverlay final : public RegionOverlay {
public:
    static Ref<NonFastScrollableRegionOverlay> create(MainFrame& frame)
    {
        return adoptRef(*new NonFastScrollableRegionOverlay(frame));
    }

private:
    explicit NonFastScrollableRegionOverlay(MainFrame& frame)
        : RegionOverlay(frame, Color(1.0f, 0.5f, 0.0f, 0.4f))
    {
    }

    bool updateRegion() override;
    void drawRect(PageOverlay&, GraphicsContext&, const IntRect& dirtyRect) final;
    
    EventTrackingRegions m_eventTrackingRegions;
};

bool NonFastScrollableRegionOverlay::updateRegion()
{
    bool regionChanged = false;

    if (Page* page = m_frame.page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator()) {
            EventTrackingRegions eventTrackingRegions = scrollingCoordinator->absoluteEventTrackingRegions();

            if (eventTrackingRegions != m_eventTrackingRegions) {
                m_eventTrackingRegions = eventTrackingRegions;
                regionChanged = true;
            }
        }
    }

    return regionChanged;
}

static HashMap<String, Color>& touchEventRegionColors()
{
    static NeverDestroyed<HashMap<String, Color>> regionColors;

    if (regionColors.get().isEmpty()) {
        regionColors.get().add("touchstart", Color(191, 191, 63, 80));
        regionColors.get().add("touchmove", Color(63, 191, 191, 80));
        regionColors.get().add("touchend", Color(191, 63, 127, 80));
        regionColors.get().add("touchforcechange", Color(63, 63, 191, 80));
        regionColors.get().add("wheel", Color(255, 128, 0, 80));
    }
    
    return regionColors;
}

static void drawRightAlignedText(const String& text, GraphicsContext& context, const FontCascade& font, const FloatPoint& boxLocation)
{
    float textGap = 10;
    float textBaselineFromTop = 14;

    TextRun textRun = TextRun(StringView(text));
    context.setFillColor(Color::transparent);
    float textWidth = context.drawText(font, textRun, { });
    context.setFillColor(Color::black);
    context.drawText(font, textRun, boxLocation + FloatSize(-(textWidth + textGap), textBaselineFromTop));
}

void NonFastScrollableRegionOverlay::drawRect(PageOverlay& pageOverlay, GraphicsContext& context, const IntRect&)
{
    IntRect bounds = pageOverlay.bounds();
    
    context.clearRect(bounds);
    
    FloatRect legendRect = { bounds.maxX() - 30.0f, 10, 20, 20 };
    
    FontCascadeDescription fontDescription;
    fontDescription.setOneFamily("Helvetica");
    fontDescription.setSpecifiedSize(12);
    fontDescription.setComputedSize(12);
    fontDescription.setWeight(FontSelectionValue(500));
    FontCascade font(fontDescription, 0, 0);
    font.update(nullptr);

#if ENABLE(TOUCH_EVENTS)
    context.setFillColor(touchEventRegionColors().get("touchstart"));
    context.fillRect(legendRect);
    drawRightAlignedText("touchstart", context, font, legendRect.location());

    legendRect.move(0, 30);
    context.setFillColor(touchEventRegionColors().get("touchmove"));
    context.fillRect(legendRect);
    drawRightAlignedText("touchmove", context, font, legendRect.location());

    legendRect.move(0, 30);
    context.setFillColor(touchEventRegionColors().get("touchend"));
    context.fillRect(legendRect);
    drawRightAlignedText("touchend", context, font, legendRect.location());

    legendRect.move(0, 30);
    context.setFillColor(touchEventRegionColors().get("touchforcechange"));
    context.fillRect(legendRect);
    drawRightAlignedText("touchforcechange", context, font, legendRect.location());

    legendRect.move(0, 30);
    context.setFillColor(m_color);
    context.fillRect(legendRect);
    drawRightAlignedText("passive listeners", context, font, legendRect.location());
#else
    // On desktop platforms, the "wheel" region includes the non-fast scrollable region.
    context.setFillColor(touchEventRegionColors().get("wheel"));
    context.fillRect(legendRect);
    drawRightAlignedText("non-fast region", context, font, legendRect.location());
#endif

    for (const auto& synchronousEventRegion : m_eventTrackingRegions.eventSpecificSynchronousDispatchRegions) {
        Color regionColor = touchEventRegionColors().get(synchronousEventRegion.key);
        drawRegion(context, synchronousEventRegion.value, regionColor, bounds);
    }

    drawRegion(context, m_eventTrackingRegions.asynchronousDispatchRegion, m_color, bounds);
}

Ref<RegionOverlay> RegionOverlay::create(MainFrame& frame, DebugPageOverlays::RegionType regionType)
{
    switch (regionType) {
    case DebugPageOverlays::RegionType::WheelEventHandlers:
        return MouseWheelRegionOverlay::create(frame);
    case DebugPageOverlays::RegionType::NonFastScrollableRegion:
        return NonFastScrollableRegionOverlay::create(frame);
    }
    ASSERT_NOT_REACHED();
    return MouseWheelRegionOverlay::create(frame);
}

RegionOverlay::RegionOverlay(MainFrame& frame, Color regionColor)
    : m_frame(frame)
    , m_overlay(PageOverlay::create(*this, PageOverlay::OverlayType::Document))
    , m_color(regionColor)
{
}

RegionOverlay::~RegionOverlay()
{
    if (m_overlay)
        m_frame.pageOverlayController().uninstallPageOverlay(*m_overlay, PageOverlay::FadeMode::DoNotFade);
}

void RegionOverlay::willMoveToPage(PageOverlay&, Page* page)
{
    if (!page)
        m_overlay = nullptr;
}

void RegionOverlay::didMoveToPage(PageOverlay&, Page* page)
{
    if (page)
        recomputeRegion();
}

void RegionOverlay::drawRect(PageOverlay&, GraphicsContext& context, const IntRect& dirtyRect)
{
    context.clearRect(dirtyRect);

    if (!m_region)
        return;

    drawRegion(context, *m_region, m_color, dirtyRect);
}

void RegionOverlay::drawRegion(GraphicsContext& context, const Region& region, const Color& color, const IntRect& dirtyRect)
{
    GraphicsContextStateSaver saver(context);
    context.setFillColor(color);
    for (auto rect : region.rects()) {
        if (rect.intersects(dirtyRect))
            context.fillRect(rect);
    }
}

bool RegionOverlay::mouseEvent(PageOverlay&, const PlatformMouseEvent&)
{
    return false;
}

void RegionOverlay::didScrollFrame(PageOverlay&, Frame&)
{
}

void RegionOverlay::recomputeRegion()
{
    if (updateRegion())
        m_overlay->setNeedsDisplay();
}

DebugPageOverlays& DebugPageOverlays::singleton()
{
    if (!sharedDebugOverlays)
        sharedDebugOverlays = new DebugPageOverlays;

    return *sharedDebugOverlays;
}

static inline size_t indexOf(DebugPageOverlays::RegionType regionType)
{
    return static_cast<size_t>(regionType);
}

RegionOverlay& DebugPageOverlays::ensureRegionOverlayForFrame(MainFrame& frame, RegionType regionType)
{
    auto it = m_frameRegionOverlays.find(&frame);
    if (it != m_frameRegionOverlays.end()) {
        auto& visualizer = it->value[indexOf(regionType)];
        if (!visualizer)
            visualizer = RegionOverlay::create(frame, regionType);
        return *visualizer;
    }

    Vector<RefPtr<RegionOverlay>> visualizers(NumberOfRegionTypes);
    auto visualizer = RegionOverlay::create(frame, regionType);
    visualizers[indexOf(regionType)] = visualizer.copyRef();
    m_frameRegionOverlays.add(&frame, WTFMove(visualizers));
    return visualizer;
}

void DebugPageOverlays::showRegionOverlay(MainFrame& frame, RegionType regionType)
{
    auto& visualizer = ensureRegionOverlayForFrame(frame, regionType);
    frame.pageOverlayController().installPageOverlay(visualizer.overlay(), PageOverlay::FadeMode::DoNotFade);
}

void DebugPageOverlays::hideRegionOverlay(MainFrame& frame, RegionType regionType)
{
    auto it = m_frameRegionOverlays.find(&frame);
    if (it == m_frameRegionOverlays.end())
        return;
    auto& visualizer = it->value[indexOf(regionType)];
    if (!visualizer)
        return;
    frame.pageOverlayController().uninstallPageOverlay(visualizer->overlay(), PageOverlay::FadeMode::DoNotFade);
    visualizer = nullptr;
}

void DebugPageOverlays::regionChanged(Frame& frame, RegionType regionType)
{
    if (auto* visualizer = regionOverlayForFrame(frame.mainFrame(), regionType))
        visualizer->recomputeRegion();
}

RegionOverlay* DebugPageOverlays::regionOverlayForFrame(MainFrame& frame, RegionType regionType) const
{
    auto it = m_frameRegionOverlays.find(&frame);
    if (it == m_frameRegionOverlays.end())
        return nullptr;
    return it->value.at(indexOf(regionType)).get();
}

void DebugPageOverlays::updateOverlayRegionVisibility(MainFrame& frame, DebugOverlayRegions visibleRegions)
{
    if (visibleRegions & NonFastScrollableRegion)
        showRegionOverlay(frame, RegionType::NonFastScrollableRegion);
    else
        hideRegionOverlay(frame, RegionType::NonFastScrollableRegion);

    if (visibleRegions & WheelEventHandlerRegion)
        showRegionOverlay(frame, RegionType::WheelEventHandlers);
    else
        hideRegionOverlay(frame, RegionType::WheelEventHandlers);
}

void DebugPageOverlays::settingsChanged(MainFrame& frame)
{
    DebugOverlayRegions activeOverlayRegions = frame.settings().visibleDebugOverlayRegions();
    if (!activeOverlayRegions && !hasOverlays(frame))
        return;

    DebugPageOverlays::singleton().updateOverlayRegionVisibility(frame, activeOverlayRegions);
}

}
