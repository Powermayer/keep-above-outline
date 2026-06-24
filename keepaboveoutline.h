/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <effect/effect.h>
#include <QVector2D>
#include <QMap>

namespace KWin
{

struct OutlineCache {
    QRectF geometry;
    QList<QVector2D> borderVerts;
    QColor borderColor;
};

class KeepAboveOutlineEffect : public Effect
{
    Q_OBJECT

public:
    KeepAboveOutlineEffect();

    void reconfigure(ReconfigureFlags flags) override;

    // The outline is drawn in a screen-level pass (after all windows) rather
    // than per-window. Drawing per window required marking the window
    // transformed so the border could spill outside its bounds, but the
    // PAINT_WINDOW_TRANSFORMED flag makes KWin's blur/background-contrast
    // effects skip the window — which cleared the blurred background behind
    // Keep Above windows. Painting at screen level avoids the flag entirely.
    void prePaintScreen(ScreenPrePaintData &data) override;

    void paintScreen(const RenderTarget &renderTarget,
                     const RenderViewport &viewport,
                     int mask,
                     const Region &deviceRegion,
                     LogicalOutput *screen) override;

    bool isActive() const override;

    static bool supported();

private Q_SLOTS:
    void slotKeepAboveChanged(KWin::EffectWindow *w);
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotWindowFrameGeometryChanged(KWin::EffectWindow *w,
                                        const QRectF &oldGeometry);

private:
    QRectF expandedGeometryFor(EffectWindow *w) const;

    void generateRoundedRectStrip(const QRectF &outer, qreal outerRadius,
                                  const QRectF &inner, qreal innerRadius,
                                  qreal scale,
                                  QList<QVector2D> &verts);

    void renderOutline(const RenderTarget &renderTarget,
                       const RenderViewport &viewport,
                       const OutlineCache &cache,
                       const Region &clipRegion);

    void cacheWindowOutline(EffectWindow *w, const QRectF &geo);

    QColor resolveColor() const;

    QSet<EffectWindow *> m_keepAboveWindows;
    QMap<EffectWindow *, QRectF> m_lastGeometry;
    QMap<EffectWindow *, OutlineCache> m_outlineCache;

    // Config
    bool m_useAccentColor = true;
    QColor m_customColor = QColor(61, 174, 233);
    int m_width = 3;
    int m_radius = 0;
};

} // namespace KWin
