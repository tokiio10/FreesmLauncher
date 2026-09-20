// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Emerald Glass dashboard - a full visual replacement for the main window contents.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "Dashboard.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QListView>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QProgressBar>
#include <QSlider>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

#include "Application.h"
#include "BaseInstance.h"
#include "HardwareInfo.h"
#include "InstanceList.h"
#include "MMCTime.h"
#include "icons/IconList.h"
#include "minecraft/auth/AccountList.h"
#include "settings/Setting.h"
#include "settings/SettingsObject.h"

namespace {

// ---------------------------------------------------------------------------------------------
// Palette (taken from the Stitch mockup)
// ---------------------------------------------------------------------------------------------
const QColor kEmerald(0x10, 0xb9, 0x81);
const QColor kEmeraldLight(0x34, 0xd3, 0x99);
const QColor kCyan(0x22, 0xd3, 0xee);
const QColor kPurple(0xa7, 0x8b, 0xfa);
const QColor kTextBright(0xf4, 0xf4, 0xf5);
const QColor kText(0xe4, 0xe4, 0xe7);
const QColor kTextMuted(0xa1, 0xa1, 0xaa);
const QColor kTextDim(0x71, 0x71, 0x7a);

QColor withAlpha(QColor c, int alpha)
{
    c.setAlpha(alpha);
    return c;
}

QColor whiteA(int alpha)
{
    return QColor(255, 255, 255, alpha);
}

const char* const kStyleSheet = R"QSS(
#dashRoot { background-color: #0c0c10; }
#sidebar, #centerPanel, #rightPanel {
    background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #18181e, stop:1 #101015);
    border: 1px solid rgba(255, 255, 255, 0.07);
    border-radius: 22px;
}
#heroCard {
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #2b2b34, stop:1 #1b1b22);
    border: 1px solid rgba(255, 255, 255, 0.11);
    border-radius: 26px;
}
#heroIconBox {
    background-color: rgba(16, 185, 129, 0.14);
    border: 1px solid rgba(52, 211, 153, 0.45);
    border-radius: 20px;
}
#card {
    background-color: rgba(255, 255, 255, 0.03);
    border: 1px solid rgba(255, 255, 255, 0.07);
    border-radius: 18px;
}
#pillBar {
    background-color: rgba(255, 255, 255, 0.04);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 21px;
}
QLabel { background: transparent; color: #d4d4d8; }
#heroName { color: #ffffff; font-size: 22px; font-weight: 600; }
#heroSub { color: #a1a1aa; font-size: 12px; }
#cardTitle { color: #a1a1aa; font-size: 12px; font-weight: 600; }
#sectionTitle { color: #71717a; font-size: 11px; font-weight: 600; }
#statusText { color: #a1a1aa; font-size: 11px; }
#playtime { color: #71717a; font-size: 11px; }
#chip {
    background-color: rgba(255, 255, 255, 0.05);
    border: 1px solid rgba(255, 255, 255, 0.09);
    border-radius: 11px;
    padding: 4px 10px;
    color: #d4d4d8;
    font-size: 11px;
}
#badge {
    background-color: rgba(16, 185, 129, 0.22);
    border: 1px solid rgba(52, 211, 153, 0.30);
    border-radius: 9px;
    padding: 2px 9px;
    color: #6ee7b7;
    font-size: 11px;
    font-weight: 600;
}
#accBadge {
    background-color: rgba(16, 185, 129, 0.12);
    border: 1px solid rgba(52, 211, 153, 0.55);
    border-radius: 13px;
    color: #34d399;
    font-size: 11px;
    font-weight: 600;
    min-width: 26px;
    max-width: 26px;
    min-height: 26px;
    max-height: 26px;
}
#divider { background-color: rgba(255, 255, 255, 0.07); border: none; min-height: 1px; max-height: 1px; }
QProgressBar {
    background-color: rgba(0, 0, 0, 0.45);
    border: none;
    border-radius: 3px;
    min-height: 6px;
    max-height: 6px;
    text-align: center;
    color: transparent;
}
QProgressBar::chunk {
    border-radius: 3px;
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #10b981, stop:1 #22d3ee);
}
QSlider::groove:horizontal { height: 8px; border-radius: 4px; background-color: rgba(255, 255, 255, 0.78); }
QSlider::sub-page:horizontal { border-radius: 4px; background-color: #34d399; }
QSlider::handle:horizontal {
    width: 16px; height: 16px; margin: -6px 0; border-radius: 10px;
    background-color: #121217; border: 2px solid #ffffff;
}
QSlider::handle:horizontal:hover { border-color: #6ee7b7; }
QListView { background: transparent; border: none; outline: none; }
QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }
QScrollBar::handle:vertical { background: rgba(255, 255, 255, 0.16); border-radius: 3px; min-height: 28px; }
QScrollBar::handle:vertical:hover { background: rgba(52, 211, 153, 0.55); }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QToolTip {
    background-color: #20202a; color: #e4e4e7;
    border: 1px solid rgba(255, 255, 255, 0.16); padding: 4px 8px;
}
)QSS";

// ---------------------------------------------------------------------------------------------
// Vector glyphs, drawn on a 24x24 grid (no image resources needed)
// ---------------------------------------------------------------------------------------------
enum class Glyph { Grid, Tag, Layers, Folder, Sun, Image, Globe, Terminal, Gear, User, Play, Stop, Plus, Download, Cube, Pencil };

void drawGlyph(QPainter& p, Glyph glyph, const QRectF& target, const QColor& color)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal scale = qMin(target.width(), target.height()) / 24.0;
    p.translate(target.center());
    p.scale(scale, scale);
    p.translate(-12.0, -12.0);
    p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    switch (glyph) {
        case Glyph::Grid:
            for (int row = 0; row < 2; ++row) {
                for (int col = 0; col < 2; ++col) {
                    p.drawRoundedRect(QRectF(4 + col * 10, 4 + row * 10, 6, 6), 1.6, 1.6);
                }
            }
            break;
        case Glyph::Tag: {
            QPolygonF poly;
            poly << QPointF(3.5, 3.5) << QPointF(12, 3.5) << QPointF(20.5, 12) << QPointF(12, 20.5) << QPointF(3.5, 12);
            p.drawPolygon(poly);
            p.drawEllipse(QPointF(7.6, 7.6), 1.3, 1.3);
            break;
        }
        case Glyph::Layers: {
            QPolygonF top;
            top << QPointF(12, 3.5) << QPointF(20.5, 8) << QPointF(12, 12.5) << QPointF(3.5, 8);
            p.drawPolygon(top);
            QPolygonF mid;
            mid << QPointF(3.5, 12) << QPointF(12, 16.5) << QPointF(20.5, 12);
            p.drawPolyline(mid);
            QPolygonF low;
            low << QPointF(3.5, 16) << QPointF(12, 20.5) << QPointF(20.5, 16);
            p.drawPolyline(low);
            break;
        }
        case Glyph::Folder: {
            QPolygonF poly;
            poly << QPointF(3.5, 6) << QPointF(9.5, 6) << QPointF(11.5, 8.5) << QPointF(20.5, 8.5) << QPointF(20.5, 19) << QPointF(3.5, 19);
            p.drawPolygon(poly);
            break;
        }
        case Glyph::Sun:
            p.drawEllipse(QPointF(12, 12), 4.0, 4.0);
            for (int i = 0; i < 8; ++i) {
                const qreal a = qDegreesToRadians(i * 45.0);
                p.drawLine(QPointF(12 + qCos(a) * 7.2, 12 + qSin(a) * 7.2), QPointF(12 + qCos(a) * 9.6, 12 + qSin(a) * 9.6));
            }
            break;
        case Glyph::Image: {
            p.drawRoundedRect(QRectF(3.5, 4.5, 17, 15), 2.2, 2.2);
            p.drawEllipse(QPointF(9, 10), 1.6, 1.6);
            QPolygonF hills;
            hills << QPointF(3.5, 17) << QPointF(9, 12.5) << QPointF(14, 16.5) << QPointF(16.5, 14.5) << QPointF(20.5, 18);
            p.drawPolyline(hills);
            break;
        }
        case Glyph::Globe:
            p.drawEllipse(QPointF(12, 12), 9, 9);
            p.drawEllipse(QPointF(12, 12), 4, 9);
            p.drawLine(QPointF(3, 12), QPointF(21, 12));
            break;
        case Glyph::Terminal: {
            p.drawRoundedRect(QRectF(3, 4.5, 18, 15), 2.2, 2.2);
            QPolygonF chevron;
            chevron << QPointF(7, 9.5) << QPointF(10.5, 12) << QPointF(7, 14.5);
            p.drawPolyline(chevron);
            p.drawLine(QPointF(12.5, 15), QPointF(16.5, 15));
            break;
        }
        case Glyph::Gear:
            p.drawEllipse(QPointF(12, 12), 3.2, 3.2);
            p.drawEllipse(QPointF(12, 12), 6.6, 6.6);
            for (int i = 0; i < 8; ++i) {
                const qreal a = qDegreesToRadians(i * 45.0);
                p.drawLine(QPointF(12 + qCos(a) * 6.6, 12 + qSin(a) * 6.6), QPointF(12 + qCos(a) * 9.2, 12 + qSin(a) * 9.2));
            }
            break;
        case Glyph::User: {
            p.drawEllipse(QPointF(12, 8.5), 3.6, 3.6);
            QPainterPath shoulders;
            shoulders.moveTo(4.5, 20);
            shoulders.cubicTo(4.5, 15.5, 8, 14, 12, 14);
            shoulders.cubicTo(16, 14, 19.5, 15.5, 19.5, 20);
            p.drawPath(shoulders);
            break;
        }
        case Glyph::Play: {
            QPolygonF tri;
            tri << QPointF(8, 5) << QPointF(19, 12) << QPointF(8, 19);
            p.setBrush(color);
            p.drawPolygon(tri);
            break;
        }
        case Glyph::Stop:
            p.setBrush(color);
            p.drawRoundedRect(QRectF(6.5, 6.5, 11, 11), 2.2, 2.2);
            break;
        case Glyph::Plus:
            p.drawLine(QPointF(12, 5), QPointF(12, 19));
            p.drawLine(QPointF(5, 12), QPointF(19, 12));
            break;
        case Glyph::Download: {
            p.drawLine(QPointF(12, 4), QPointF(12, 15));
            QPolygonF arrow;
            arrow << QPointF(7, 10.5) << QPointF(12, 15.5) << QPointF(17, 10.5);
            p.drawPolyline(arrow);
            QPolygonF tray;
            tray << QPointF(4.5, 15) << QPointF(4.5, 19.5) << QPointF(19.5, 19.5) << QPointF(19.5, 15);
            p.drawPolyline(tray);
            break;
        }
        case Glyph::Cube: {
            QPolygonF hex;
            hex << QPointF(12, 2.5) << QPointF(20.5, 7.25) << QPointF(20.5, 16.75) << QPointF(12, 21.5) << QPointF(3.5, 16.75)
                << QPointF(3.5, 7.25);
            p.drawPolygon(hex);
            QPolygonF inner;
            inner << QPointF(3.5, 7.25) << QPointF(12, 12) << QPointF(20.5, 7.25);
            p.drawPolyline(inner);
            p.drawLine(QPointF(12, 12), QPointF(12, 21.5));
            break;
        }
        case Glyph::Pencil: {
            QPolygonF body;
            body << QPointF(4, 20) << QPointF(4.8, 15.4) << QPointF(16, 4.2) << QPointF(19.8, 8) << QPointF(8.6, 19.2);
            p.drawPolygon(body);
            p.drawLine(QPointF(13.6, 6.6), QPointF(17.4, 10.4));
            break;
        }
    }
    p.restore();
}

QPixmap glyphPixmap(Glyph glyph, const QColor& color, int logicalSize, qreal dpr)
{
    QPixmap pm(QSize(qRound(logicalSize * dpr), qRound(logicalSize * dpr)));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    drawGlyph(p, glyph, QRectF(0, 0, logicalSize, logicalSize), color);
    return pm;
}

// ---------------------------------------------------------------------------------------------
// Instance helpers
// ---------------------------------------------------------------------------------------------
struct PackInfo {
    QString version;
    QString loader;
};

// Reads the Minecraft version and mod loader straight from the instance's mmc-pack.json.
// It is cheap, cached by file timestamp and never triggers metadata downloads.
PackInfo readPackInfo(BaseInstance* inst)
{
    struct Cached {
        qint64 stamp = -1;
        PackInfo info;
    };
    static QHash<QString, Cached> cache;

    if (!inst) {
        return {};
    }
    const QString path = QDir(inst->instanceRoot()).filePath(QStringLiteral("mmc-pack.json"));
    const QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        return {};
    }
    const qint64 stamp = fileInfo.lastModified().toMSecsSinceEpoch();
    const auto found = cache.constFind(path);
    if (found != cache.constEnd() && found->stamp == stamp) {
        return found->info;
    }

    struct Loader {
        const char* uid;
        const char* name;
    };
    static const Loader loaders[] = {
        { "net.neoforged", "NeoForge" },
        { "net.minecraftforge", "Forge" },
        { "net.fabricmc.fabric-loader", "Fabric" },
        { "org.quiltmc.quilt-loader", "Quilt" },
        { "com.mumfrey.liteloader", "LiteLoader" },
    };

    PackInfo info;
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonArray components = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("components")).toArray();
        for (const QJsonValue& value : components) {
            const QJsonObject component = value.toObject();
            const QString uid = component.value(QStringLiteral("uid")).toString();
            if (uid == QStringLiteral("net.minecraft")) {
                info.version = component.value(QStringLiteral("version")).toString();
                continue;
            }
            if (!info.loader.isEmpty()) {
                continue;
            }
            for (const auto& loader : loaders) {
                if (uid == QLatin1String(loader.uid)) {
                    info.loader = QString::fromLatin1(loader.name);
                    break;
                }
            }
        }
    }
    Cached entry;
    entry.stamp = stamp;
    entry.info = info;
    cache.insert(path, entry);
    return info;
}

QString minecraftVersionOf(BaseInstance* inst)
{
    return readPackInfo(inst).version;
}

QString loaderOf(BaseInstance* inst)
{
    return readPackInfo(inst).loader;
}

QString describeInstance(BaseInstance* inst)
{
    if (!inst) {
        return {};
    }
    const QString version = minecraftVersionOf(inst);
    const QString loader = loaderOf(inst);
    if (version.isEmpty()) {
        return {};
    }
    return loader.isEmpty() ? version : version + QStringLiteral("  \u00b7  ") + loader;
}

// ---------------------------------------------------------------------------------------------
// Custom painted controls
// ---------------------------------------------------------------------------------------------
class GlyphButton : public QToolButton {
   public:
    enum class Kind { Square, Round, Pill, Play, Bar };

    GlyphButton(Glyph glyph, Kind kind, QWidget* parent = nullptr) : QToolButton(parent), m_glyph(glyph), m_kind(kind)
    {
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
        setAutoRaise(true);
        setToolButtonStyle(Qt::ToolButtonTextOnly);
        switch (kind) {
            case Kind::Square:
                setFixedSize(46, 46);
                break;
            case Kind::Round:
                setFixedSize(40, 40);
                break;
            case Kind::Pill:
                setFixedHeight(32);
                break;
            case Kind::Play:
                setFixedSize(300, 54);
                break;
            case Kind::Bar:
                setMinimumHeight(48);
                setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                break;
        }
    }

    void setActive(bool active)
    {
        m_active = active;
        update();
    }
    void setAccent(const QColor& accent)
    {
        m_accent = accent;
        update();
    }
    void setDiameter(int d) { setFixedSize(d, d); }

    QSize sizeHint() const override
    {
        if (m_kind == Kind::Pill) {
            const int menu = (defaultAction() && defaultAction()->menu()) ? 16 : 0;
            return QSize(fontMetrics().horizontalAdvance(text()) + 38 + menu, 32);
        }
        return QToolButton::sizeHint();
    }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setOpacity(isEnabled() ? 1.0 : 0.38);
        const bool hot = (underMouse() || hasFocus()) && isEnabled();
        const bool down = isDown();
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

        switch (m_kind) {
            case Kind::Square: {
                const QColor bg = m_active ? withAlpha(kEmerald, 40) : whiteA(down ? 6 : (hot ? 20 : 10));
                const QColor border = m_active ? withAlpha(kEmeraldLight, 125) : whiteA(hot ? 64 : 26);
                p.setPen(QPen(border, 1));
                p.setBrush(bg);
                p.drawRoundedRect(r, 14, 14);
                const QColor glyphColor = m_active ? kEmeraldLight : (hot ? QColor(255, 255, 255) : kTextMuted);
                drawGlyph(p, m_glyph, QRectF(r.center().x() - 10, r.center().y() - 10, 20, 20), glyphColor);
                break;
            }
            case Kind::Round: {
                const bool accented = m_accent.isValid();
                p.setPen(QPen(accented ? withAlpha(m_accent, hot ? 230 : 170) : whiteA(hot ? 64 : 30), 1.2));
                p.setBrush(accented ? withAlpha(m_accent, down ? 28 : (hot ? 60 : 42)) : whiteA(down ? 6 : (hot ? 22 : 12)));
                p.drawEllipse(r);
                const qreal g = r.width() * 0.46;
                drawGlyph(p, m_glyph, QRectF(r.center().x() - g / 2, r.center().y() - g / 2, g, g),
                          accented ? kEmeraldLight : (hot ? QColor(255, 255, 255) : kText));
                break;
            }
            case Kind::Pill: {
                if (hot || down) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(whiteA(down ? 10 : 20));
                    p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
                }
                p.setPen(Qt::NoPen);
                p.setBrush(m_accent.isValid() ? m_accent : kEmeraldLight);
                p.drawEllipse(QPointF(r.left() + 14, r.center().y()), 3.6, 3.6);
                p.setPen(hot ? QColor(255, 255, 255) : kText);
                const bool hasMenu = defaultAction() && defaultAction()->menu();
                QRectF textRect = r.adjusted(26, 0, hasMenu ? -18 : -10, 0);
                p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());
                if (hasMenu) {
                    p.setPen(QPen(kTextMuted, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    const qreal cx = r.right() - 13;
                    const qreal cy = r.center().y();
                    QPolygonF chevron;
                    chevron << QPointF(cx - 3.5, cy - 1.5) << QPointF(cx, cy + 2) << QPointF(cx + 3.5, cy - 1.5);
                    p.drawPolyline(chevron);
                }
                break;
            }
            case Kind::Play: {
                QLinearGradient g(r.topLeft(), r.topRight());
                g.setColorAt(0, hot ? QColor(0x10, 0x43, 0x31) : QColor(0x0d, 0x33, 0x26));
                g.setColorAt(1, hot ? QColor(0x0f, 0x3b, 0x2d) : QColor(0x0c, 0x2e, 0x24));
                p.setPen(QPen(down ? kEmerald : kEmeraldLight, 1.4));
                p.setBrush(g);
                p.drawRoundedRect(r, 18, 18);
                QFont f = font();
                f.setBold(true);
                p.setFont(f);
                const int textWidth = QFontMetrics(f).horizontalAdvance(text());
                const qreal total = 20 + 12 + textWidth;
                const qreal x = r.center().x() - total / 2;
                drawGlyph(p, Glyph::Play, QRectF(x, r.center().y() - 10, 20, 20), QColor(0xec, 0xfd, 0xf5));
                p.setPen(QColor(0xec, 0xfd, 0xf5));
                p.drawText(QRectF(x + 32, r.top(), textWidth + 4, r.height()), Qt::AlignVCenter | Qt::AlignLeft, text());
                break;
            }
            case Kind::Bar: {
                p.setPen(QPen(whiteA(hot ? 56 : 26), 1));
                p.setBrush(whiteA(down ? 6 : (hot ? 18 : 9)));
                p.drawRoundedRect(r, 15, 15);
                const QRectF badge(r.left() + 10, r.center().y() - 14, 28, 28);
                p.setPen(QPen(withAlpha(kEmeraldLight, 150), 1.2));
                p.setBrush(withAlpha(kEmerald, 36));
                p.drawEllipse(badge);
                drawGlyph(p, m_glyph, QRectF(badge.center().x() - 7, badge.center().y() - 7, 14, 14), kEmeraldLight);
                p.setPen(hot ? QColor(255, 255, 255) : kText);
                p.drawText(QRectF(r.left() + 50, r.top(), r.width() - 80, r.height()), Qt::AlignVCenter | Qt::AlignLeft, text());
                QPolygonF arrow;
                const qreal ax = r.right() - 20;
                arrow << QPointF(ax - 3, r.center().y() - 5) << QPointF(ax + 3, r.center().y()) << QPointF(ax - 3, r.center().y() + 5);
                p.setPen(QPen(kTextMuted, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p.setBrush(Qt::NoBrush);
                p.drawPolyline(arrow);
                break;
            }
        }
    }

   private:
    Glyph m_glyph;
    Kind m_kind;
    bool m_active = false;
    QColor m_accent;
};

}  // namespace (SwitchToggle and AvatarStrip are forward-declared in Dashboard.h)

class SwitchToggle : public QAbstractButton {
   public:
    explicit SwitchToggle(QWidget* parent = nullptr) : QAbstractButton(parent)
    {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(54, 28);
        setAttribute(Qt::WA_Hover, true);
    }
    QSize sizeHint() const override { return QSize(54, 28); }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const bool on = isChecked();
        p.setPen(QPen(on ? kEmerald : whiteA(48), 1.2));
        p.setBrush(on ? QColor(0x0d, 0x33, 0x26) : whiteA(14));
        p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
        const qreal d = r.height() - 8;
        const qreal x = on ? r.right() - d - 4 : r.left() + 4;
        p.setPen(Qt::NoPen);
        p.setBrush(on ? QColor(0xec, 0xfd, 0xf5) : kTextMuted);
        p.drawEllipse(QRectF(x, r.top() + 4, d, d));
    }
};

class AvatarStrip : public QWidget {
   public:
    explicit AvatarStrip(QWidget* parent = nullptr) : QWidget(parent)
    {
        setFixedHeight(42);
        setMinimumWidth(150);
        setCursor(Qt::PointingHandCursor);
    }

    std::function<void()> onClick;

    void reload()
    {
        m_faces.clear();
        m_names.clear();
        m_defaultIndex = -1;
        m_total = 0;
        auto* accounts = APPLICATION->accounts();
        if (!accounts) {
            update();
            return;
        }
        m_total = accounts->count();
        const auto defaultAccount = accounts->defaultAccount();
        for (int i = 0; i < m_total && i < 3; ++i) {
            const auto account = accounts->at(i);
            m_faces.append(account ? account->getFace(64, 64) : QPixmap());
            m_names.append(account ? account->profileName() : QString());
            if (account && defaultAccount && account == defaultAccount) {
                m_defaultIndex = i;
            }
        }
        update();
    }

    int total() const { return m_total; }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const qreal d = 36;
        const qreal step = 26;
        const qreal top = (height() - d) / 2;
        const int count = static_cast<int>(m_faces.size());

        for (int i = count - 1; i >= 0; --i) {
            const QRectF box(2 + i * step, top, d, d);
            // dark outline to separate overlapping avatars
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0x12, 0x12, 0x17));
            p.drawEllipse(box.adjusted(-2, -2, 2, 2));
            p.setBrush(QColor(0x26, 0x26, 0x2e));
            p.drawEllipse(box);
            if (!m_faces[i].isNull()) {
                p.save();
                QPainterPath clip;
                clip.addEllipse(box.adjusted(1, 1, -1, -1));
                p.setClipPath(clip);
                p.drawPixmap(box.adjusted(1, 1, -1, -1).toRect(), m_faces[i]);
                p.restore();
            } else if (!m_names[i].isEmpty()) {
                p.setPen(kText);
                p.drawText(box, Qt::AlignCenter, m_names[i].left(1).toUpper());
            }
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(i == m_defaultIndex ? kEmeraldLight : whiteA(50), i == m_defaultIndex ? 2.0 : 1.2));
            p.drawEllipse(box);
        }

        const QRectF plus(2 + count * step + (count > 0 ? 6 : 0), top, d, d);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(whiteA(85), 1.2, Qt::DashLine));
        p.drawEllipse(plus);
        drawGlyph(p, Glyph::Plus, QRectF(plus.center().x() - 8, plus.center().y() - 8, 16, 16), kTextMuted);
        if (count == 0) {
            p.setPen(kTextMuted);
            p.drawText(QRectF(plus.right() + 10, top, width() - plus.right() - 10, d), Qt::AlignVCenter | Qt::AlignLeft,
                       QStringLiteral(u"A\u00f1adir cuenta"));
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()) && onClick) {
            onClick();
        }
        QWidget::mouseReleaseEvent(event);
    }

   private:
    QList<QPixmap> m_faces;
    QStringList m_names;
    int m_defaultIndex = -1;
    int m_total = 0;
};

namespace {

class InstanceRowDelegate : public QStyledItemDelegate {
   public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return QSize(0, 60); }

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QRectF r = QRectF(option.rect).adjusted(2, 3, -2, -3);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hot = option.state & QStyle::State_MouseOver;

        QPainterPath path;
        path.addRoundedRect(r, 15, 15);
        if (selected) {
            QLinearGradient g(r.topLeft(), r.topRight());
            g.setColorAt(0, withAlpha(kEmerald, 52));
            g.setColorAt(1, withAlpha(kEmerald, 18));
            p->fillPath(path, g);
            p->setPen(QPen(withAlpha(kEmeraldLight, 145), 1));
        } else {
            p->fillPath(path, whiteA(hot ? 15 : 7));
            p->setPen(QPen(whiteA(hot ? 46 : 20), 1));
        }
        p->setBrush(Qt::NoBrush);
        p->drawPath(path);

        static const QColor rings[] = { kCyan, kEmeraldLight, kPurple };
        const QColor ring = rings[index.row() % 3];
        const QRectF badge(r.left() + 11, r.center().y() - 18, 36, 36);
        p->setBrush(withAlpha(ring, 34));
        p->setPen(QPen(withAlpha(ring, 175), 1.5));
        p->drawEllipse(badge);

        const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        if (!icon.isNull()) {
            const QPixmap pm = icon.pixmap(QSize(24, 24));
            p->drawPixmap(QRectF(badge.center().x() - 12, badge.center().y() - 12, 24, 24), pm, QRectF(pm.rect()));
        }

        auto* inst = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
        const bool running = inst && inst->isRunning();

        const qreal textLeft = badge.right() + 12;
        const qreal pillWidth = running ? 68 : 0;
        const qreal textWidth = r.right() - textLeft - 12 - (running ? pillWidth + 6 : 0);

        QFont titleFont = option.font;
        titleFont.setBold(true);
        p->setFont(titleFont);
        p->setPen(selected ? QColor(255, 255, 255) : kTextBright);
        const QString name = QFontMetrics(titleFont).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, static_cast<int>(textWidth));
        p->drawText(QRectF(textLeft, r.top() + 9, textWidth, 20), Qt::AlignVCenter | Qt::AlignLeft, name);

        QFont subFont = option.font;
        subFont.setPointSizeF(qMax<qreal>(7.0, subFont.pointSizeF() * 0.85));
        p->setFont(subFont);
        p->setPen(kTextDim);
        QString sub = describeInstance(inst);
        if (sub.isEmpty()) {
            sub = index.data(InstanceList::GroupRole).toString();
        }
        p->drawText(QRectF(textLeft, r.top() + 30, textWidth, 18), Qt::AlignVCenter | Qt::AlignLeft,
                    QFontMetrics(subFont).elidedText(sub, Qt::ElideRight, static_cast<int>(textWidth)));

        if (running) {
            const QRectF pill(r.right() - pillWidth - 10, r.center().y() - 10, pillWidth, 20);
            p->setPen(Qt::NoPen);
            p->setBrush(withAlpha(kEmerald, 60));
            p->drawRoundedRect(pill, 10, 10);
            p->setPen(kEmeraldLight);
            p->setFont(subFont);
            p->drawText(pill, Qt::AlignCenter, QStringLiteral("EN JUEGO"));
        }
        p->restore();
    }
};

QFrame* makeFrame(const char* objectName, QWidget* parent)
{
    auto* frame = new QFrame(parent);
    frame->setObjectName(QString::fromLatin1(objectName));
    frame->setAttribute(Qt::WA_StyledBackground, true);
    return frame;
}

QLabel* makeLabel(const QString& text, const char* objectName, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QString::fromLatin1(objectName));
    return label;
}

void addGlow(QWidget* widget, const QColor& color, int blur)
{
    auto* effect = new QGraphicsDropShadowEffect(widget);
    effect->setBlurRadius(blur);
    effect->setOffset(0, 0);
    effect->setColor(color);
    widget->setGraphicsEffect(effect);
}

}  // namespace

// =============================================================================================
// Dashboard
// =============================================================================================

Dashboard::Dashboard(QAbstractItemModel* model, QItemSelectionModel* selection, const DashboardActions& actions, QWidget* parent)
    : QWidget(parent), m_model(model), m_selection(selection), m_actions(actions)
{
    setAcceptDrops(true);
    buildUi(actions);
}

void Dashboard::setSelectedInstanceGetter(std::function<BaseInstance*()> getter)
{
    m_selected = std::move(getter);
}

void Dashboard::buildUi(const DashboardActions& a)
{
    setObjectName(QStringLiteral("dashRoot"));
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumSize(1020, 640);
    setStyleSheet(QString::fromUtf8(kStyleSheet));

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(12);

    // ------------------------------------------------------------------ left sidebar
    auto* sidebar = makeFrame("sidebar", this);
    sidebar->setFixedWidth(74);
    auto* side = new QVBoxLayout(sidebar);
    side->setContentsMargins(13, 14, 13, 14);
    side->setSpacing(10);

    auto* home = new GlyphButton(Glyph::Grid, GlyphButton::Kind::Square, sidebar);
    home->setActive(true);
    home->setToolTip(QStringLiteral("Instancias"));
    connect(home, &QToolButton::clicked, this, [this]() { m_list->setFocus(); });
    side->addWidget(home);

    struct PageDef {
        Glyph glyph;
        QString tip;
        const char* page;
    };
    const PageDef pages[] = {
        { Glyph::Tag, QStringLiteral(u"Versi\u00f3n y loaders"), "version" },
        { Glyph::Layers, QStringLiteral("Mods"), "mods" },
        { Glyph::Folder, QStringLiteral("Resource packs"), "resourcepacks" },
        { Glyph::Sun, QStringLiteral("Shaders"), "shaderpacks" },
        { Glyph::Image, QStringLiteral("Capturas"), "screenshots" },
        { Glyph::Globe, QStringLiteral("Mundos"), "worlds" },
        { Glyph::Terminal, QStringLiteral("Registro del juego"), "console" },
    };
    for (const auto& def : pages) {
        auto* button = new GlyphButton(def.glyph, GlyphButton::Kind::Square, sidebar);
        button->setToolTip(def.tip);
        const QString page = QString::fromLatin1(def.page);
        connect(button, &QToolButton::clicked, this, [this, page]() { openInstancePage(page); });
        m_pageButtons.append(button);
        side->addWidget(button);
    }
    side->addStretch(1);
    auto* sideDivider = makeFrame("divider", sidebar);
    side->addWidget(sideDivider);

    auto* settingsButton = new GlyphButton(Glyph::Gear, GlyphButton::Kind::Square, sidebar);
    if (a.settings) {
        settingsButton->setDefaultAction(a.settings);
    }
    settingsButton->setToolTip(QStringLiteral("Ajustes"));
    side->addWidget(settingsButton);

    auto* accountButton = new GlyphButton(Glyph::User, GlyphButton::Kind::Square, sidebar);
    if (a.accounts) {
        accountButton->setDefaultAction(a.accounts);
        accountButton->setPopupMode(QToolButton::InstantPopup);
    }
    accountButton->setToolTip(QStringLiteral("Cuenta"));
    side->addWidget(accountButton);
    root->addWidget(sidebar);

    // ------------------------------------------------------------------ center column
    auto* center = new QVBoxLayout();
    center->setSpacing(12);

    auto* panel = makeFrame("centerPanel", this);
    auto* pl = new QVBoxLayout(panel);
    pl->setContentsMargins(22, 18, 22, 16);
    pl->setSpacing(10);

    auto* top = new QHBoxLayout();
    top->setSpacing(8);
    m_countChip = makeLabel(QString(), "chip", panel);
    top->addWidget(m_countChip);
    top->addStretch(1);

    auto* pillBar = makeFrame("pillBar", panel);
    auto* pillLayout = new QHBoxLayout(pillBar);
    pillLayout->setContentsMargins(6, 4, 6, 4);
    pillLayout->setSpacing(2);
    const struct {
        QAction* action;
        QColor accent;
        const char16_t* fallback;
    } pills[] = {
        { a.folders, kEmeraldLight, u"Carpetas" },
        { a.help, kCyan, u"Ayuda" },
        { a.checkUpdate, kPurple, u"Actualizar" },
        { a.moreNews, kTextMuted, u"Noticias" },
    };
    for (const auto& def : pills) {
        if (!def.action) {
            continue;
        }
        auto* pill = new GlyphButton(Glyph::Plus, GlyphButton::Kind::Pill, pillBar);
        pill->setDefaultAction(def.action);
        pill->setAccent(def.accent);
        if (def.action->menu()) {
            pill->setPopupMode(QToolButton::InstantPopup);
        }
        pill->setVisible(def.action->isVisible());
        QAction* act = def.action;
        connect(act, &QAction::changed, pill, [pill, act]() { pill->setVisible(act->isVisible()); });
        pillLayout->addWidget(pill);
    }
    top->addWidget(pillBar);
    top->addStretch(1);

    auto* addButton = new GlyphButton(Glyph::Download, GlyphButton::Kind::Round, panel);
    if (a.addInstance) {
        addButton->setDefaultAction(a.addInstance);
    }
    addButton->setToolTip(QStringLiteral(u"A\u00f1adir instancia"));
    top->addWidget(addButton);
    pl->addLayout(top);
    pl->addStretch(1);

    // hero card
    auto* hero = makeFrame("heroCard", panel);
    hero->setMinimumWidth(440);
    hero->setMaximumWidth(580);
    auto* hl = new QVBoxLayout(hero);
    hl->setContentsMargins(30, 30, 30, 28);
    hl->setSpacing(6);

    auto* iconBox = makeFrame("heroIconBox", hero);
    iconBox->setFixedSize(96, 96);
    auto* iconLayout = new QVBoxLayout(iconBox);
    iconLayout->setContentsMargins(0, 0, 0, 0);
    m_heroIcon = new QLabel(iconBox);
    m_heroIcon->setAlignment(Qt::AlignCenter);
    iconLayout->addWidget(m_heroIcon);
    addGlow(iconBox, withAlpha(kEmerald, 120), 46);
    hl->addWidget(iconBox, 0, Qt::AlignHCenter);
    hl->addSpacing(10);

    m_heroName = makeLabel(QString(), "heroName", hero);
    m_heroName->setAlignment(Qt::AlignCenter);
    hl->addWidget(m_heroName);
    m_heroSub = makeLabel(QString(), "heroSub", hero);
    m_heroSub->setAlignment(Qt::AlignCenter);
    hl->addWidget(m_heroSub);
    hl->addSpacing(14);

    auto* play = new GlyphButton(Glyph::Play, GlyphButton::Kind::Play, hero);
    if (a.launch) {
        play->setDefaultAction(a.launch);
    }
    addGlow(play, withAlpha(kEmerald, 110), 34);
    hl->addWidget(play, 0, Qt::AlignHCenter);
    pl->addWidget(hero, 0, Qt::AlignHCenter);
    pl->addStretch(1);

    // footer: status + activity bar
    auto* foot = new QHBoxLayout();
    foot->setSpacing(8);
    m_statusText = makeLabel(QString(), "statusText", panel);
    m_statusText->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_playtime = makeLabel(QString(), "playtime", panel);
    foot->addWidget(m_statusText, 1);
    foot->addWidget(m_playtime, 0);
    pl->addLayout(foot);
    m_activity = new QProgressBar(panel);
    m_activity->setTextVisible(false);
    m_activity->setRange(0, 1);
    m_activity->setValue(0);
    pl->addWidget(m_activity);
    center->addWidget(panel, 1);

    // bottom cards
    auto* bottom = new QHBoxLayout();
    bottom->setSpacing(12);

    auto* card1 = makeFrame("card", this);
    card1->setFixedHeight(136);
    auto* c1 = new QVBoxLayout(card1);
    c1->setContentsMargins(16, 12, 16, 12);
    c1->setSpacing(10);
    auto* c1head = new QHBoxLayout();
    c1head->addWidget(makeLabel(QStringLiteral(u"\u25cf  Instancia"), "cardTitle", card1));
    c1head->addStretch(1);
    m_cardBadge = makeLabel(QString(), "badge", card1);
    c1head->addWidget(m_cardBadge);
    c1->addLayout(c1head);
    auto* chips = new QHBoxLayout();
    chips->setSpacing(8);
    m_chipVersion = makeLabel(QString(), "chip", card1);
    m_chipLoader = makeLabel(QString(), "chip", card1);
    m_chipRam = makeLabel(QString(), "chip", card1);
    chips->addWidget(m_chipVersion);
    chips->addWidget(m_chipLoader);
    chips->addWidget(m_chipRam);
    chips->addStretch(1);
    c1->addLayout(chips);
    c1->addStretch(1);
    c1->addWidget(makeFrame("divider", card1));
    auto* c1foot = new QHBoxLayout();
    c1foot->addWidget(makeLabel(QStringLiteral("Tiempo jugado"), "statusText", card1));
    c1foot->addStretch(1);
    m_cardPlayed = makeLabel(QString(), "statusText", card1);
    c1foot->addWidget(m_cardPlayed);
    c1->addLayout(c1foot);
    bottom->addWidget(card1, 5);

    auto* card2 = makeFrame("card", this);
    card2->setFixedHeight(136);
    auto* c2 = new QVBoxLayout(card2);
    c2->setContentsMargins(16, 12, 16, 12);
    c2->addWidget(makeLabel(QStringLiteral(u"\u25cf  Consola"), "cardTitle", card2));
    c2->addStretch(1);
    m_consoleToggle = new SwitchToggle(card2);
    c2->addWidget(m_consoleToggle, 0, Qt::AlignHCenter);
    auto* c2hint = makeLabel(QStringLiteral(u"Mostrar al jugar"), "statusText", card2);
    c2hint->setAlignment(Qt::AlignCenter);
    c2->addWidget(c2hint);
    c2->addStretch(1);
    bottom->addWidget(card2, 3);

    auto* card3 = makeFrame("card", this);
    card3->setFixedHeight(136);
    auto* c3 = new QVBoxLayout(card3);
    c3->setContentsMargins(16, 12, 16, 12);
    c3->addWidget(makeLabel(QStringLiteral(u"\u25cf  Juego"), "cardTitle", card3));
    c3->addStretch(1);
    auto* controls = new QHBoxLayout();
    controls->setSpacing(14);
    controls->addStretch(1);
    auto* stopButton = new GlyphButton(Glyph::Stop, GlyphButton::Kind::Round, card3);
    if (a.kill) {
        stopButton->setDefaultAction(a.kill);
    }
    auto* playButton = new GlyphButton(Glyph::Play, GlyphButton::Kind::Round, card3);
    playButton->setDiameter(52);
    playButton->setAccent(kEmerald);
    if (a.launch) {
        playButton->setDefaultAction(a.launch);
    }
    auto* logButton = new GlyphButton(Glyph::Terminal, GlyphButton::Kind::Round, card3);
    if (a.viewLog) {
        logButton->setDefaultAction(a.viewLog);
    }
    controls->addWidget(stopButton);
    controls->addWidget(playButton);
    controls->addWidget(logButton);
    controls->addStretch(1);
    c3->addLayout(controls);
    c3->addStretch(1);
    bottom->addWidget(card3, 4);
    center->addLayout(bottom);
    root->addLayout(center, 1);

    // ------------------------------------------------------------------ right panel
    auto* rightPanel = makeFrame("rightPanel", this);
    rightPanel->setFixedWidth(340);
    auto* rl = new QVBoxLayout(rightPanel);
    rl->setContentsMargins(18, 18, 18, 16);
    rl->setSpacing(12);

    auto* accountRow = new QHBoxLayout();
    m_avatars = new AvatarStrip(rightPanel);
    m_avatars->onClick = [this]() {
        if (m_actions.accounts && m_actions.accounts->menu()) {
            m_actions.accounts->menu()->exec(QCursor::pos());
        }
    };
    accountRow->addWidget(m_avatars, 1);
    m_accountBadge = makeLabel(QStringLiteral("0"), "accBadge", rightPanel);
    m_accountBadge->setAlignment(Qt::AlignCenter);
    accountRow->addWidget(m_accountBadge, 0, Qt::AlignVCenter);
    rl->addLayout(accountRow);
    rl->addWidget(makeFrame("divider", rightPanel));

    auto* ramCard = makeFrame("card", rightPanel);
    auto* rc = new QVBoxLayout(ramCard);
    rc->setContentsMargins(16, 12, 16, 12);
    rc->setSpacing(10);
    auto* ramHead = new QHBoxLayout();
    ramHead->addWidget(makeLabel(QStringLiteral(u"Memoria m\u00e1xima"), "cardTitle", ramCard));
    ramHead->addStretch(1);
    m_ramValue = makeLabel(QString(), "badge", ramCard);
    ramHead->addWidget(m_ramValue);
    rc->addLayout(ramHead);
    m_ramSlider = new QSlider(Qt::Horizontal, ramCard);
    rc->addWidget(m_ramSlider);
    auto* ramScale = new QHBoxLayout();
    m_ramMin = makeLabel(QString(), "statusText", ramCard);
    m_ramMax = makeLabel(QString(), "statusText", ramCard);
    ramScale->addWidget(m_ramMin);
    ramScale->addStretch(1);
    ramScale->addWidget(m_ramMax);
    rc->addLayout(ramScale);
    rl->addWidget(ramCard);

    rl->addWidget(makeLabel(QStringLiteral("INSTANCIAS"), "sectionTitle", rightPanel));

    m_list = new QListView(rightPanel);
    m_list->setModel(m_model);
    m_list->setSelectionModel(m_selection);
    m_list->setItemDelegate(new InstanceRowDelegate(m_list));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::EditKeyPressed);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setUniformItemSizes(true);
    m_list->setMouseTracking(true);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setAttribute(Qt::WA_MacShowFocusRect, false);
    connect(m_list, &QAbstractItemView::activated, this, &Dashboard::instanceActivated);
    connect(m_list, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex index = m_list->indexAt(pos);
        if (!index.isValid()) {
            return;
        }
        m_list->setCurrentIndex(index);
        emit instanceContextMenuRequested(m_list->viewport()->mapToGlobal(pos));
    });
    rl->addWidget(m_list, 1);

    auto* editBar = new GlyphButton(Glyph::Pencil, GlyphButton::Kind::Bar, rightPanel);
    if (a.edit) {
        editBar->setDefaultAction(a.edit);
    }
    rl->addWidget(editBar);
    root->addWidget(rightPanel);

    // ------------------------------------------------------------------ wiring
    m_ramSlider->setSingleStep(1);
    m_ramSlider->setPageStep(8);
    auto* ramTimer = new QTimer(this);
    ramTimer->setSingleShot(true);
    ramTimer->setInterval(450);
    connect(ramTimer, &QTimer::timeout, this, [this]() {
        const int mib = m_ramSlider->value() * 128;
        auto* settings = APPLICATION->settings();
        settings->set(QStringLiteral("MaxMemAlloc"), mib);
        if (settings->get(QStringLiteral("MinMemAlloc")).toInt() > mib) {
            settings->set(QStringLiteral("MinMemAlloc"), mib);
        }
    });
    connect(m_ramSlider, &QSlider::valueChanged, this, [this, ramTimer](int value) {
        if (m_syncingSlider) {
            return;
        }
        m_ramValue->setText(QStringLiteral("%1 MiB").arg(value * 128));
        ramTimer->start();
    });
    if (auto setting = APPLICATION->settings()->getSetting(QStringLiteral("MaxMemAlloc"))) {
        connect(setting.get(), &Setting::SettingChanged, this, [this](const Setting&, const QVariant&) { syncMemorySlider(); });
    }

    connect(m_consoleToggle, &QAbstractButton::toggled, this,
            [](bool on) { APPLICATION->settings()->set(QStringLiteral("ShowConsole"), on); });
    if (auto setting = APPLICATION->settings()->getSetting(QStringLiteral("ShowConsole"))) {
        connect(setting.get(), &Setting::SettingChanged, this, [this](const Setting&, const QVariant& value) {
            const QSignalBlocker blocker(m_consoleToggle);
            m_consoleToggle->setChecked(value.toBool());
        });
    }

    if (auto* accounts = APPLICATION->accounts()) {
        connect(accounts, &AccountList::listChanged, this, [this]() { refresh(); });
        connect(accounts, &AccountList::defaultAccountChanged, this, [this]() { refresh(); });
    }
    connect(m_model, &QAbstractItemModel::rowsInserted, this, [this]() { updateInstanceCount(); });
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, [this]() { updateInstanceCount(); });
    connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { updateInstanceCount(); });

    updateInstanceCount();
    refresh();
}

void Dashboard::updateInstanceCount()
{
    const int n = m_model ? m_model->rowCount() : 0;
    m_countChip->setText(QStringLiteral("<span style='color:#34d399'>\u25cf</span>&nbsp;&nbsp;%1 %2")
                             .arg(n)
                             .arg(n == 1 ? QStringLiteral("instancia") : QStringLiteral("instancias")));
}

void Dashboard::syncMemorySlider()
{
    const int totalMiB = static_cast<int>(HardwareInfo::totalRamMiB());
    const int maxMiB = qMax(4096, (totalMiB / 128) * 128);
    const int current = qBound(512, APPLICATION->settings()->get(QStringLiteral("MaxMemAlloc")).toInt(), maxMiB);

    m_syncingSlider = true;
    m_ramSlider->setRange(512 / 128, maxMiB / 128);
    m_ramSlider->setValue(current / 128);
    m_syncingSlider = false;

    m_ramValue->setText(QStringLiteral("%1 MiB").arg((current / 128) * 128));
    m_ramMin->setText(QStringLiteral("512 MiB"));
    m_ramMax->setText(QStringLiteral("%1 GiB").arg(QString::number(maxMiB / 1024.0, 'f', 1)));
}

void Dashboard::openInstancePage(const QString& page)
{
    BaseInstance* inst = m_selected ? m_selected() : nullptr;
    if (inst) {
        APPLICATION->showInstanceWindow(inst, page);
    }
}

void Dashboard::editCurrent()
{
    const QModelIndex index = m_selection->currentIndex();
    if (index.isValid()) {
        m_list->edit(index);
    }
}

void Dashboard::refresh()
{
    BaseInstance* inst = m_selected ? m_selected() : nullptr;
    const bool has = inst != nullptr;
    const qreal dpr = devicePixelRatioF();

    for (auto* button : m_pageButtons) {
        button->setEnabled(has);
    }

    if (!has) {
        m_heroName->setText(QStringLiteral("Sin instancia seleccionada"));
        m_heroSub->setText(QStringLiteral("Elige una instancia de la lista"));
        m_heroIcon->setPixmap(glyphPixmap(Glyph::Cube, kEmeraldLight, 52, dpr));
        m_chipVersion->setText(QStringLiteral("<span style='color:#22d3ee'>\u25cf</span>&nbsp;&nbsp;\u2014"));
        m_chipLoader->setText(QStringLiteral("<span style='color:#34d399'>\u25cf</span>&nbsp;&nbsp;\u2014"));
        m_chipRam->setText(QStringLiteral("<span style='color:#a78bfa'>\u25cf</span>&nbsp;&nbsp;\u2014"));
        m_cardBadge->setText(QStringLiteral("Sin selecci\u00f3n"));
        m_cardPlayed->setText(QStringLiteral("\u2014"));
        m_statusText->setText(QStringLiteral("Sin instancia seleccionada"));
        m_activity->setRange(0, 1);
        m_activity->setValue(0);
    } else {
        m_heroName->setText(inst->name());
        const QString desc = describeInstance(inst);
        m_heroSub->setText(desc.isEmpty() ? QStringLiteral("Minecraft") : desc);

        const QIcon icon = APPLICATION->icons()->getIcon(inst->iconKey());
        if (icon.isNull()) {
            m_heroIcon->setPixmap(glyphPixmap(Glyph::Cube, kEmeraldLight, 52, dpr));
        } else {
            m_heroIcon->setPixmap(icon.pixmap(QSize(60, 60), dpr));
        }

        const QString version = minecraftVersionOf(inst);
        const QString loader = loaderOf(inst);
        m_chipVersion->setText(QStringLiteral("<span style='color:#22d3ee'>\u25cf</span>&nbsp;&nbsp;%1")
                                   .arg(version.isEmpty() ? QStringLiteral("Minecraft") : version));
        m_chipLoader->setText(QStringLiteral("<span style='color:#34d399'>\u25cf</span>&nbsp;&nbsp;%1")
                                  .arg(loader.isEmpty() ? QStringLiteral("Vanilla") : loader));
        m_chipRam->setText(QStringLiteral("<span style='color:#a78bfa'>\u25cf</span>&nbsp;&nbsp;%1 MiB")
                               .arg(inst->settings()->get(QStringLiteral("MaxMemAlloc")).toInt()));
        const qint64 played = inst->totalTimePlayed();
        m_cardPlayed->setText(played > 0 ? Time::prettifyDuration(played) : QStringLiteral(u"A\u00fan sin jugar"));

        if (inst->isRunning()) {
            m_cardBadge->setText(QStringLiteral("En juego"));
            m_statusText->setText(QStringLiteral(u"\u25cf  En ejecuci\u00f3n \u2014 ") + inst->name());
            m_activity->setRange(0, 0);
        } else {
            m_cardBadge->setText(QStringLiteral("Listo"));
            m_statusText->setText(inst->getStatusbarDescription());
            m_activity->setRange(0, 1);
            m_activity->setValue(0);
        }
    }

    const int total = APPLICATION->instances()->getTotalPlayTime();
    m_playtime->setText(total > 0 ? QStringLiteral("Tiempo total: %1").arg(Time::prettifyDuration(total)) : QString());

    if (m_avatars) {
        m_avatars->reload();
        m_accountBadge->setText(QString::number(m_avatars->total()));
    }
    const QSignalBlocker blocker(m_consoleToggle);
    m_consoleToggle->setChecked(APPLICATION->settings()->get(QStringLiteral("ShowConsole")).toBool());
    syncMemorySlider();
}

void Dashboard::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refresh();
}

void Dashboard::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void Dashboard::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        emit urlsDropped(event->mimeData()->urls());
    }
}
