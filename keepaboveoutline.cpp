/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "keepaboveoutline.h"
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <opengl/glshader.h>
#include <opengl/glshadermanager.h>
#include <opengl/glvertexbuffer.h>
#include <core/rendertarget.h>
#include <core/renderviewport.h>
#ifndef KEEPABOVE_LEGACY_PAINT_TYPES
#include <core/rect.h>
#endif
#include <KConfigGroup>
#include <QGuiApplication>
#include <QPalette>
#include <QtMath>
#include <utility>

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(KeepAboveOutlineEffect, "metadata.json",
                              return KeepAboveOutlineEffect::supported();)

KeepAboveOutlineEffect::KeepAboveOutlineEffect()
{
    reconfigure(ReconfigureAll);

    const auto windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        connect(w, &EffectWindow::windowKeepAboveChanged,
                this, &KeepAboveOutlineEffect::slotKeepAboveChanged);
        connect(w, &EffectWindow::windowFrameGeometryChanged,
                this, &KeepAboveOutlineEffect::slotWindowFrameGeometryChanged);
        connect(w, &EffectWindow::minimizedChanged,
                this, &KeepAboveOutlineEffect::slotWindowMinimizedChanged);
        connect(w, &EffectWindow::windowHiddenChanged,
                this, &KeepAboveOutlineEffect::slotWindowHiddenChanged);
        if (w->isAppletPopup() && w->isVisible()) {
            m_openAppletPopups.insert(w);
        }
        if (shouldOutlineWindow(w)) {
            m_keepAboveWindows.insert(w);
        }
    }

    connect(effects, &EffectsHandler::windowAdded,
            this, &KeepAboveOutlineEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted,
            this, &KeepAboveOutlineEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::showingDesktopChanged,
            this, &KeepAboveOutlineEffect::slotShowingDesktopChanged);
}

bool KeepAboveOutlineEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void KeepAboveOutlineEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup config = effects->config()->group(QStringLiteral("Effect-keep-above-outline"));
    m_useAccentColor = config.readEntry("UseAccentColor", true);
    m_customColor = config.readEntry("CustomColor", QColor(61, 174, 233));
    m_width = qBound(1, config.readEntry("BorderWidth", 3), 20);
    m_radius = qBound(0, config.readEntry("BorderRadius", 0), 30);

    effects->addRepaintFull();
}

QColor KeepAboveOutlineEffect::resolveColor() const
{
    if (m_useAccentColor) {
        return qApp->palette().color(QPalette::Highlight);
    }
    return m_customColor;
}

void KeepAboveOutlineEffect::slotWindowAdded(EffectWindow *w)
{
    connect(w, &EffectWindow::windowKeepAboveChanged,
            this, &KeepAboveOutlineEffect::slotKeepAboveChanged);
    connect(w, &EffectWindow::windowFrameGeometryChanged,
            this, &KeepAboveOutlineEffect::slotWindowFrameGeometryChanged);
    connect(w, &EffectWindow::minimizedChanged,
            this, &KeepAboveOutlineEffect::slotWindowMinimizedChanged);
    connect(w, &EffectWindow::windowHiddenChanged,
            this, &KeepAboveOutlineEffect::slotWindowHiddenChanged);

    if (w->isAppletPopup() && w->isVisible()) {
        m_openAppletPopups.insert(w);
        repaintAllOutlines();
    }

    if (shouldOutlineWindow(w)) {
        m_keepAboveWindows.insert(w);
        effects->addRepaint(expandedGeometryFor(w));
    }
}

bool KeepAboveOutlineEffect::shouldOutlineWindow(EffectWindow *w) const
{
    // Plasma applet popups, including the application launcher, are kept above
    // by the shell itself. That is not a user-applied Keep Above state and
    // should not receive an outline.
    return w->keepAbove() && !w->isDesktop() && !w->isDock()
        && !w->isAppletPopup();
}

void KeepAboveOutlineEffect::slotWindowDeleted(EffectWindow *w)
{
    const bool popupWasOpen = m_openAppletPopups.remove(w);
    m_keepAboveWindows.remove(w);
    m_suppressedWindows.remove(w);
    m_lastGeometry.remove(w);
    m_outlineCache.remove(w);
    if (popupWasOpen) {
        repaintAllOutlines();
    }
}

void KeepAboveOutlineEffect::slotWindowHiddenChanged(EffectWindow *w)
{
    if (!w->isAppletPopup()) {
        return;
    }

    const bool wasOpen = m_openAppletPopups.contains(w);
    const bool isOpen = w->isVisible();
    if (wasOpen == isOpen) {
        return;
    }

    if (isOpen) {
        m_openAppletPopups.insert(w);
    } else {
        m_openAppletPopups.remove(w);
    }
    repaintAllOutlines();
}

void KeepAboveOutlineEffect::repaintAllOutlines()
{
    for (EffectWindow *w : std::as_const(m_keepAboveWindows)) {
        effects->addRepaint(expandedGeometryFor(w));
    }
}

void KeepAboveOutlineEffect::renderOutline(const RenderTarget &renderTarget,
                                           const RenderViewport &viewport,
                                           const OutlineCache &cache,
                                           const KeepAboveRegion &clipRegion)
{
    if (cache.borderVerts.isEmpty()) {
        return;
    }

    const qreal scale = viewport.scale();
    auto vbo = GLVertexBuffer::streamingBuffer();

    QList<QVector2D> scaledVerts;
    for (const auto &v : cache.borderVerts) {
        scaledVerts.append(QVector2D(v.x() * scale, v.y() * scale));
    }

    auto mapped = vbo->map<QVector2D>(scaledVerts.size());
    if (!mapped) {
        return;
    }
    memcpy(mapped->data(), scaledVerts.data(), scaledVerts.size() * sizeof(QVector2D));
    vbo->unmap();
    vbo->setVertexCount(scaledVerts.size());

    static constexpr GLVertexAttrib layout{
        .attributeIndex = VA_Position,
        .componentCount = 2,
        .type = GL_FLOAT,
        .relativeOffset = 0,
    };
    vbo->setAttribLayout(std::span(&layout, 1), sizeof(QVector2D));

    ShaderBinder binder(ShaderTrait::UniformColor | ShaderTrait::TransformColorspace);
    binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix,
                                viewport.projectionMatrix());
    binder.shader()->setColorspaceUniforms(ColorDescription::sRGB,
                                           renderTarget.colorDescription(),
                                           RenderingIntent::Perceptual);
    binder.shader()->setUniform(GLShader::ColorUniform::Color, cache.borderColor);

    // The custom color may carry an alpha component, so blend the outline over
    // whatever is already on screen. The color is passed straight (not
    // premultiplied), which is why we use SRC_ALPHA here. Opaque colours don't
    // need blending, so skip the state change for them.
    const bool translucent = cache.borderColor.alpha() < 255;
    if (translucent) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    vbo->render(clipRegion, GL_TRIANGLES, true);

    if (translucent) {
        glDisable(GL_BLEND);
    }
}

void KeepAboveOutlineEffect::cacheWindowOutline(EffectWindow *w, const QRectF &geo)
{
    OutlineCache cache;
    cache.geometry = geo;

    const qreal bw = m_width;
    const qreal r = m_radius;

    const QRectF outerRect = geo.adjusted(-bw, -bw, bw, bw);
    const qreal outerRadius = r + bw;
    const qreal innerRadius = r;

    generateRoundedRectStrip(outerRect, outerRadius, geo, innerRadius, 1.0, cache.borderVerts);
    cache.borderColor = resolveColor();

    m_outlineCache[w] = cache;
}

void KeepAboveOutlineEffect::slotKeepAboveChanged(EffectWindow *w)
{
    if (shouldOutlineWindow(w)) {
        m_keepAboveWindows.insert(w);
    } else {
        m_keepAboveWindows.remove(w);
        m_suppressedWindows.remove(w);
        m_lastGeometry.remove(w);
        m_outlineCache.remove(w);
    }
    // Repaint the expanded area so the outline appears (or its leftover pixels
    // are cleared when Keep Above is turned off).
    effects->addRepaint(expandedGeometryFor(w));
}

void KeepAboveOutlineEffect::slotWindowFrameGeometryChanged(EffectWindow *w,
                                                            const QRectF &oldGeometry)
{
    if (!m_keepAboveWindows.contains(w)) {
        return;
    }
    const qreal bw = m_width;
    // Repaint the old location to erase the previous outline, and the new one
    // to draw it. Without setTransformed there is nothing else extending the
    // window's damage out to the border, so we schedule it explicitly.
    effects->addRepaint(oldGeometry.adjusted(-bw - 1, -bw - 1, bw + 1, bw + 1));
    effects->addRepaint(expandedGeometryFor(w));
}

void KeepAboveOutlineEffect::slotWindowMinimizedChanged(EffectWindow *w)
{
    if (!m_keepAboveWindows.contains(w)) {
        return;
    }
    // Minimizing hides the outline and un-minimizing brings it back; in both
    // cases the border area needs to be repainted.
    effects->addRepaint(expandedGeometryFor(w));
}

void KeepAboveOutlineEffect::slotShowingDesktopChanged()
{
    // KWin's show-desktop animation does not minimize windows: it marks them
    // hiddenByShowDesktop and transforms/fades them in a separate effect. The
    // outline is painted at screen level, so explicitly damage its old band on
    // both transitions instead of leaving it above the desktop.
    repaintAllOutlines();
}

QRectF KeepAboveOutlineEffect::expandedGeometryFor(EffectWindow *w) const
{
    const qreal bw = m_width;
    return w->frameGeometry().adjusted(-bw - 1, -bw - 1, bw + 1, bw + 1);
}

bool KeepAboveOutlineEffect::isActive() const
{
    return !m_keepAboveWindows.isEmpty();
}

void KeepAboveOutlineEffect::prepareScreenPaint(ScreenPrePaintData &data)
{
    for (EffectWindow *w : std::as_const(m_keepAboveWindows)) {
        if (w->isMinimized() || w->isHiddenByShowDesktop()) {
            // The outline sits in a band just *outside* the window's rectangle.
            // Hiding a window only damages the window rectangle itself, not
            // that surrounding band, so the outline would be left behind as a
            // ghost. Repaint the band once in the same pass so it is cleared.
            if (!m_suppressedWindows.contains(w)) {
                m_suppressedWindows.insert(w);
                data.paint += expandedGeometryFor(w).toRect();
            }
            continue;
        }

        m_suppressedWindows.remove(w);
        // Make sure the border area is part of the region being painted this
        // frame, otherwise our outline would be scissored away.
        if (w->isOnCurrentDesktop()) {
            data.paint += expandedGeometryFor(w).toRect();
        }
    }
}

#ifdef KEEPABOVE_PREPAINT_PRESENT_TIME
void KeepAboveOutlineEffect::prePaintScreen(ScreenPrePaintData &data,
                                            std::chrono::milliseconds presentTime)
{
    prepareScreenPaint(data);
    effects->prePaintScreen(data, presentTime);
}
#else
void KeepAboveOutlineEffect::prePaintScreen(ScreenPrePaintData &data)
{
    prepareScreenPaint(data);
    effects->prePaintScreen(data);
}
#endif

void KeepAboveOutlineEffect::paintScreen(const RenderTarget &renderTarget,
                                         const RenderViewport &viewport,
                                         int mask,
                                         const KeepAboveRegion &deviceRegion,
                                         KeepAboveOutput *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);

    if (m_keepAboveWindows.isEmpty() || !m_openAppletPopups.isEmpty()) {
        return;
    }

    // Draw the outlines on top of the painted windows, walking the stacking
    // order so a higher Keep Above window's outline lands above a lower one's.
    const auto stacking = effects->stackingOrder();
    for (EffectWindow *w : stacking) {
        if (!m_keepAboveWindows.contains(w) || w->isMinimized()
            || w->isHiddenByShowDesktop() || !w->isOnCurrentDesktop()) {
            continue;
        }

        const QRectF currentGeo = w->frameGeometry();
        if (!m_outlineCache.contains(w) || m_lastGeometry.value(w) != currentGeo) {
            cacheWindowOutline(w, currentGeo);
            m_lastGeometry[w] = currentGeo;
        }

        renderOutline(renderTarget, viewport, m_outlineCache[w], deviceRegion);
    }
}

// Generate a triangle strip between two concentric rounded rectangles.
// 'outer' is the outer rect, 'inner' is the inner rect.
// outerRadius/innerRadius are the corner radii for each.
void KeepAboveOutlineEffect::generateRoundedRectStrip(
    const QRectF &outer, qreal outerRadius,
    const QRectF &inner, qreal innerRadius,
    qreal scale,
    QList<QVector2D> &verts)
{
    constexpr int cornerSegments = 8;

    // Clamp radii so they don't exceed half the rect dimensions
    outerRadius = qMin(outerRadius, qMin(outer.width(), outer.height()) / 2.0);
    innerRadius = qMin(innerRadius, qMin(inner.width(), inner.height()) / 2.0);

    // Corner centers (shared between inner and outer since they're concentric)
    const QPointF centers[4] = {
        {inner.x() + innerRadius, inner.y() + innerRadius},             // top-left
        {inner.right() - innerRadius, inner.y() + innerRadius},         // top-right
        {inner.right() - innerRadius, inner.bottom() - innerRadius},    // bottom-right
        {inner.x() + innerRadius, inner.bottom() - innerRadius},        // bottom-left
    };

    // Start angles for each corner in screen coordinates (Y points down).
    // Going clockwise: top-left, top-right, bottom-right, bottom-left.
    const qreal startAngles[4] = {M_PI, 1.5 * M_PI, 0.0, 0.5 * M_PI};

    struct PointPair {
        QPointF outer, inner;
    };
    QList<PointPair> ring;

    for (int corner = 0; corner < 4; ++corner) {
        const qreal startAngle = startAngles[corner];
        for (int i = 0; i <= cornerSegments; ++i) {
            const qreal angle = startAngle + (M_PI / 2.0) * i / cornerSegments;
            const qreal cosA = qCos(angle);
            const qreal sinA = qSin(angle);

            PointPair pp;
            pp.inner = centers[corner] + QPointF(innerRadius * cosA, innerRadius * sinA);
            pp.outer = centers[corner] + QPointF(outerRadius * cosA, outerRadius * sinA);
            ring.append(pp);
        }
    }

    // Close the ring by repeating the first pair
    ring.append(ring.first());

    // Convert the ring into triangles (2 per segment)
    for (int i = 0; i < ring.size() - 1; ++i) {
        const auto &a = ring[i];
        const auto &b = ring[i + 1];

        // Triangle 1: a.outer, b.outer, a.inner
        verts.append(QVector2D(a.outer.x() * scale, a.outer.y() * scale));
        verts.append(QVector2D(b.outer.x() * scale, b.outer.y() * scale));
        verts.append(QVector2D(a.inner.x() * scale, a.inner.y() * scale));

        // Triangle 2: b.outer, b.inner, a.inner
        verts.append(QVector2D(b.outer.x() * scale, b.outer.y() * scale));
        verts.append(QVector2D(b.inner.x() * scale, b.inner.y() * scale));
        verts.append(QVector2D(a.inner.x() * scale, a.inner.y() * scale));
    }
}

} // namespace KWin

#include "keepaboveoutline.moc"
