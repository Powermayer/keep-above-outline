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
#include <KConfigGroup>
#include <QGuiApplication>
#include <QPalette>
#include <QtMath>

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(KeepAboveOutlineEffect, "metadata.json",
                              return KeepAboveOutlineEffect::supported();)

KeepAboveOutlineEffect::KeepAboveOutlineEffect()
{
    reconfigure(ReconfigureAll);

    const auto windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        if (w->keepAbove() && !w->isDesktop() && !w->isDock()) {
            m_keepAboveWindows.insert(w);
        }
    }

    connect(effects, &EffectsHandler::windowAdded,
            this, &KeepAboveOutlineEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted,
            this, &KeepAboveOutlineEffect::slotWindowDeleted);
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

    if (w->keepAbove() && !w->isDesktop() && !w->isDock()) {
        m_keepAboveWindows.insert(w);
        w->addRepaintFull();
    }
}

void KeepAboveOutlineEffect::slotWindowDeleted(EffectWindow *w)
{
    m_keepAboveWindows.remove(w);
    m_lastGeometry.remove(w);
    m_outlineCache.remove(w);
}

void KeepAboveOutlineEffect::renderOutline(const RenderTarget &renderTarget,
                                           const RenderViewport &viewport,
                                           const OutlineCache &cache,
                                           const Region &clipRegion)
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

    vbo->render(clipRegion, GL_TRIANGLES, true);
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
    if (w->keepAbove() && !w->isDesktop() && !w->isDock()) {
        m_keepAboveWindows.insert(w);
    } else {
        m_keepAboveWindows.remove(w);
    }
    w->addRepaintFull();
}

bool KeepAboveOutlineEffect::isActive() const
{
    return !m_keepAboveWindows.isEmpty();
}

void KeepAboveOutlineEffect::prePaintWindow(RenderView *view, EffectWindow *w,
                                            WindowPrePaintData &data)
{
    if (m_keepAboveWindows.contains(w)) {
        // The outline is drawn outside the window's own geometry. Marking the
        // window transformed tells the scene not to occlusion-clip it to its
        // bounds, so our extra border pixels survive to paintWindow().
        data.setTransformed();
    }
    effects->prePaintWindow(view, w, data);
}

void KeepAboveOutlineEffect::paintWindow(const RenderTarget &renderTarget,
                                         const RenderViewport &viewport,
                                         EffectWindow *w,
                                         int mask,
                                         const Region &region,
                                         WindowPaintData &data)
{
    effects->paintWindow(renderTarget, viewport, w, mask, region, data);

    if (m_keepAboveWindows.contains(w)) {
        const QRectF currentGeo = w->frameGeometry();
        const QRectF lastGeo = m_lastGeometry.value(w, QRectF());

        if (currentGeo != lastGeo) {
            cacheWindowOutline(w, currentGeo);
            m_lastGeometry[w] = currentGeo;
        }

        if (m_outlineCache.contains(w)) {
            renderOutline(renderTarget, viewport, m_outlineCache[w], region);
        }
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
