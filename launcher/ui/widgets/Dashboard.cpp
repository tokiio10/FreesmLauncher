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
#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QHash>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLinearGradient>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QProgressBar>
#include <QRadialGradient>
#include <QRegularExpression>
#include <QSlider>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QtMath>

#include "Application.h"
#include "BaseInstance.h"
#include "InstanceList.h"
#include "MMCTime.h"
#include "icons/IconList.h"
#include "minecraft/auth/AccountList.h"
#include "settings/SettingsObject.h"

namespace {

// ---------------------------------------------------------------------------------------------
// Small colour helpers
// ---------------------------------------------------------------------------------------------
const QColor kCyan(0x22, 0xd3, 0xee);
const QColor kPurple(0xa7, 0x8b, 0xfa);
const QColor kTextBright(0xf4, 0xf4, 0xf5);
const QColor kText(0xe4, 0xe4, 0xe7);
const QColor kTextMuted(0xa1, 0xa1, 0xaa);
const QColor kTextDim(0x71, 0x71, 0x7a);

QColor withAlpha(QColor c, int alpha)
{
    c.setAlpha(qBound(0, alpha, 255));
    return c;
}

QColor whiteA(int alpha)
{
    return QColor(255, 255, 255, qBound(0, alpha, 255));
}

QColor mixColors(const QColor& a, const QColor& b, qreal t)
{
    t = qBound<qreal>(0.0, t, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

// ---------------------------------------------------------------------------------------------
// Live theme: accent colour, base tone and wallpapers. Persisted next to the launcher data.
// ---------------------------------------------------------------------------------------------
struct DashTheme {
    int hue = 160;  // emerald
    bool amoled = false;
    int dim = 55;
    QString wallpaper;                        // file name inside the backgrounds folder
    QHash<QString, QString> instanceWallpapers;  // instance id -> file name

    QColor accent() const { return QColor::fromHslF(hue / 360.0, 0.84, 0.39); }
    QColor accentLight() const { return QColor::fromHslF(hue / 360.0, 0.66, 0.52); }
    QColor accentText() const { return QColor::fromHslF(hue / 360.0, 0.72, 0.70); }
    QColor accentDark() const { return QColor::fromHslF(hue / 360.0, 0.58, 0.13); }
    QColor accentDark2() const { return QColor::fromHslF(hue / 360.0, 0.55, 0.115); }
    QColor bg() const { return amoled ? QColor(0, 0, 0) : QColor(0x0c, 0x0c, 0x10); }
    QColor panelTop() const { return amoled ? QColor(0x0b, 0x0b, 0x0d) : QColor(0x18, 0x18, 0x1e); }
    QColor panelBottom() const { return amoled ? QColor(0x04, 0x04, 0x05) : QColor(0x10, 0x10, 0x15); }

    static QString rgb(const QColor& c) { return QStringLiteral("%1, %2, %3").arg(c.red()).arg(c.green()).arg(c.blue()); }

    QString backgroundsDir() const { return QDir(APPLICATION->dataRoot()).filePath(QStringLiteral("dashboard/backgrounds")); }
    QString filePath() const { return QDir(APPLICATION->dataRoot()).filePath(QStringLiteral("dashboard-theme.json")); }

    QString wallpaperPathFor(const QString& instanceId) const
    {
        QString name = instanceWallpapers.value(instanceId);
        if (name.isEmpty()) {
            name = wallpaper;
        }
        if (name.isEmpty()) {
            return {};
        }
        const QString path = QDir(backgroundsDir()).filePath(name);
        return QFileInfo::exists(path) ? path : QString();
    }

    void load()
    {
        QFile file(filePath());
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
        hue = qBound(0, obj.value(QStringLiteral("hue")).toInt(hue), 359);
        amoled = obj.value(QStringLiteral("amoled")).toBool(amoled);
        dim = qBound(0, obj.value(QStringLiteral("dim")).toInt(dim), 90);
        wallpaper = obj.value(QStringLiteral("wallpaper")).toString();
        instanceWallpapers.clear();
        const QJsonObject perInstance = obj.value(QStringLiteral("instanceWallpapers")).toObject();
        for (auto it = perInstance.begin(); it != perInstance.end(); ++it) {
            instanceWallpapers.insert(it.key(), it.value().toString());
        }
    }

    void save() const
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("hue"), hue);
        obj.insert(QStringLiteral("amoled"), amoled);
        obj.insert(QStringLiteral("dim"), dim);
        obj.insert(QStringLiteral("wallpaper"), wallpaper);
        QJsonObject perInstance;
        for (auto it = instanceWallpapers.constBegin(); it != instanceWallpapers.constEnd(); ++it) {
            perInstance.insert(it.key(), it.value());
        }
        obj.insert(QStringLiteral("instanceWallpapers"), perInstance);
        QFile file(filePath());
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        }
    }

    QString styleSheet() const;
};

DashTheme& theme()
{
    static DashTheme instance;
    return instance;
}

const char* const kStyleTemplate = R"QSS(
#dashRoot { background-color: @BG@; }
#sidebar, #rightPanel {
    background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @PT@, stop:1 @PB@);
    border: 1px solid rgba(255, 255, 255, 0.07);
    border-radius: 22px;
}
#heroCard {
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(255, 255, 255, 0.085), stop:1 rgba(255, 255, 255, 0.03));
    border: 1px solid rgba(255, 255, 255, 0.10);
    border-radius: 28px;
}
#card {
    background-color: rgba(255, 255, 255, 0.035);
    border: 1px solid rgba(255, 255, 255, 0.07);
    border-radius: 18px;
}
QLabel { background: transparent; color: #d4d4d8; }
#heroName { color: #ffffff; font-size: 24px; font-weight: 600; }
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
    background-color: rgba(@A@, 0.18);
    border: 1px solid rgba(@AL@, 0.25);
    border-radius: 9px;
    padding: 2px 9px;
    color: @AT@;
    font-size: 11px;
    font-weight: 600;
}
#accBadge {
    background-color: rgba(@A@, 0.14);
    border: 1px solid rgba(@AL@, 0.55);
    border-radius: 13px;
    color: @AT@;
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
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 @ACC@, stop:1 @ACCL@);
}
QSlider::groove:horizontal { height: 6px; border-radius: 3px; background-color: rgba(255, 255, 255, 0.14); }
QSlider::sub-page:horizontal { border-radius: 3px; background-color: @ACCL@; }
QSlider::handle:horizontal {
    width: 14px; height: 14px; margin: -5px 0; border-radius: 8px;
    background-color: #121217; border: 2px solid #ffffff;
}
QSlider::handle:horizontal:hover { border-color: @AT@; }
#hueSlider::groove:horizontal { height: 10px; border-radius: 5px; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, @HUES@); }
#hueSlider::sub-page:horizontal { background: transparent; }
#hueSlider::add-page:horizontal { background: transparent; }
#hueSlider::handle:horizontal { width: 14px; height: 14px; margin: -4px 0; border-radius: 8px; background-color: #121217; border: 2px solid #ffffff; }
QListView { background: transparent; border: none; outline: none; }
QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }
QScrollBar::handle:vertical { background: rgba(255, 255, 255, 0.16); border-radius: 3px; min-height: 28px; }
QScrollBar::handle:vertical:hover { background: rgba(@AL@, 0.55); }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QToolTip {
    background-color: #20202a; color: #e4e4e7;
    border: 1px solid rgba(255, 255, 255, 0.16); padding: 4px 8px;
}
QMenu { background-color: #15151b; color: #e4e4e7; border: 1px solid rgba(255, 255, 255, 0.14); padding: 4px; }
QMenu::item { padding: 6px 26px 6px 12px; border-radius: 6px; }
QMenu::item:selected { background-color: rgba(@A@, 0.25); color: #ffffff; }
QMenu::separator { height: 1px; background: rgba(255, 255, 255, 0.08); margin: 4px 8px; }
)QSS";

QString DashTheme::styleSheet() const
{
    QString hues;
    for (int i = 0; i <= 6; ++i) {
        const QColor c = QColor::fromHslF(qMin(i * 60, 359) / 360.0, 0.75, 0.52);
        if (!hues.isEmpty()) {
            hues += QStringLiteral(", ");
        }
        hues += QStringLiteral("stop:%1 %2").arg(i / 6.0, 0, 'f', 3).arg(c.name());
    }
    QString css = QString::fromLatin1(kStyleTemplate);
    css.replace(QStringLiteral("@BG@"), bg().name());
    css.replace(QStringLiteral("@PT@"), panelTop().name());
    css.replace(QStringLiteral("@PB@"), panelBottom().name());
    css.replace(QStringLiteral("@AL@"), rgb(accentLight()));
    css.replace(QStringLiteral("@AT@"), accentText().name());
    css.replace(QStringLiteral("@ACCL@"), accentLight().name());
    css.replace(QStringLiteral("@ACC@"), accent().name());
    css.replace(QStringLiteral("@A@"), rgb(accent()));
    css.replace(QStringLiteral("@HUES@"), hues);
    return css;
}

// ---------------------------------------------------------------------------------------------
// Vector glyphs, drawn on a 24x24 grid (no image resources needed)
// ---------------------------------------------------------------------------------------------
enum class Glyph {
    None,
    Grid,
    Cube,
    Puzzle,
    Brush,
    Sun,
    Image,
    Camera,
    Globe,
    Terminal,
    Gear,
    Play,
    Stop,
    Plus,
    Pencil,
    Dots,
    Palette
};

void drawGlyph(QPainter& p, Glyph glyph, const QRectF& target, const QColor& color)
{
    if (glyph == Glyph::None) {
        return;
    }
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal scale = qMin(target.width(), target.height()) / 24.0;
    p.translate(target.center());
    p.scale(scale, scale);
    p.translate(-12.0, -12.0);
    p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    switch (glyph) {
        case Glyph::None:
            break;
        case Glyph::Grid:
            for (int row = 0; row < 2; ++row) {
                for (int col = 0; col < 2; ++col) {
                    p.drawRoundedRect(QRectF(4 + col * 10, 4 + row * 10, 6, 6), 1.6, 1.6);
                }
            }
            break;
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
        case Glyph::Puzzle: {
            QPainterPath body;
            body.addRoundedRect(QRectF(4, 9, 12, 11), 2, 2);
            QPainterPath knobTop;
            knobTop.addEllipse(QPointF(10, 7.2), 2.6, 2.6);
            QPainterPath knobSide;
            knobSide.addEllipse(QPointF(18, 14.5), 2.6, 2.6);
            p.drawPath(body.united(knobTop).united(knobSide));
            break;
        }
        case Glyph::Brush: {
            p.drawLine(QPointF(19.5, 4.5), QPointF(11.5, 12.5));
            QPainterPath bristles;
            bristles.moveTo(11.5, 12.5);
            bristles.cubicTo(11.5, 15.5, 9.5, 19.5, 4.5, 19.5);
            bristles.cubicTo(5.5, 16.5, 6, 14.5, 8.5, 12.8);
            bristles.closeSubpath();
            p.drawPath(bristles);
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
        case Glyph::Camera: {
            p.drawRoundedRect(QRectF(3, 7.5, 18, 12), 2.4, 2.4);
            QPolygonF hump;
            hump << QPointF(8.5, 7.5) << QPointF(10, 4.8) << QPointF(14, 4.8) << QPointF(15.5, 7.5);
            p.drawPolyline(hump);
            p.drawEllipse(QPointF(12, 13.5), 3.4, 3.4);
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
        case Glyph::Pencil: {
            QPolygonF body;
            body << QPointF(4, 20) << QPointF(4.8, 15.4) << QPointF(16, 4.2) << QPointF(19.8, 8) << QPointF(8.6, 19.2);
            p.drawPolygon(body);
            p.drawLine(QPointF(13.6, 6.6), QPointF(17.4, 10.4));
            break;
        }
        case Glyph::Dots:
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawEllipse(QPointF(5.5, 12), 1.9, 1.9);
            p.drawEllipse(QPointF(12, 12), 1.9, 1.9);
            p.drawEllipse(QPointF(18.5, 12), 1.9, 1.9);
            break;
        case Glyph::Palette: {
            p.drawEllipse(QPointF(12, 12), 9, 9);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawEllipse(QPointF(8, 10.5), 1.3, 1.3);
            p.drawEllipse(QPointF(12, 7.5), 1.3, 1.3);
            p.drawEllipse(QPointF(16, 10.5), 1.3, 1.3);
            p.drawEllipse(QPointF(9.2, 15.2), 1.3, 1.3);
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
// Instance helpers: version + loader are read straight from mmc-pack.json (cheap, cached)
// ---------------------------------------------------------------------------------------------
struct PackInfo {
    QString version;
    QString loader;
};

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

QString describeInstance(BaseInstance* inst)
{
    if (!inst) {
        return {};
    }
    const PackInfo info = readPackInfo(inst);
    if (info.version.isEmpty()) {
        return {};
    }
    return info.loader.isEmpty() ? info.version : info.version + QStringLiteral("  \u00b7  ") + info.loader;
}

}  // namespace

// =============================================================================================
// Custom painted controls. They are global (not anonymous) because Dashboard.h forward-declares them.
// =============================================================================================

class DashButton : public QToolButton {
   public:
    enum class Kind { Square, Round, Play, Bar, Chip, Mini, Header, Swatch };

    DashButton(Glyph glyph, Kind kind, QWidget* parent = nullptr) : QToolButton(parent), m_glyph(glyph), m_kind(kind)
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
            case Kind::Mini:
                setFixedSize(30, 30);
                break;
            case Kind::Swatch:
                setFixedSize(28, 28);
                break;
            case Kind::Play:
                setFixedSize(300, 54);
                break;
            case Kind::Bar:
                setMinimumHeight(48);
                setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                break;
            case Kind::Header:
                setMinimumHeight(46);
                setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                break;
            case Kind::Chip:
                setFixedHeight(30);
                break;
        }
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(170);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_hover = value.toReal();
            update();
        });
    }

    void setActive(bool active)
    {
        m_active = active;
        update();
    }
    void setAccented(bool accented)
    {
        m_accented = accented;
        update();
    }
    void setDiameter(int d) { setFixedSize(d, d); }
    void setSwatchHue(int hue)
    {
        m_swatchHue = hue;
        update();
    }
    void setExpandProgress(qreal progress)
    {
        m_expand = progress;
        update();
    }

    QSize sizeHint() const override
    {
        if (m_kind == Kind::Chip) {
            const int glyphWidth = m_glyph == Glyph::None ? 0 : 22;
            return QSize(fontMetrics().horizontalAdvance(text()) + 30 + glyphWidth, 30);
        }
        return QToolButton::sizeHint();
    }

   protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent* event) override
#else
    void enterEvent(QEvent* event) override
#endif
    {
        animateTo(1.0);
        QToolButton::enterEvent(event);
    }
    void leaveEvent(QEvent* event) override
    {
        animateTo(0.0);
        QToolButton::leaveEvent(event);
    }

    void paintEvent(QPaintEvent*) override
    {
        const DashTheme& t = theme();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setOpacity(isEnabled() ? 1.0 : 0.38);
        const qreal h = m_hover;
        const bool down = isDown();
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

        switch (m_kind) {
            case Kind::Square: {
                const QColor bg = m_active ? withAlpha(t.accent(), 46) : whiteA(down ? 6 : qRound(10 + 12 * h));
                const QColor border = m_active ? withAlpha(t.accentLight(), qRound(120 + 40 * h)) : whiteA(qRound(24 + 40 * h));
                p.setPen(QPen(border, 1));
                p.setBrush(bg);
                p.drawRoundedRect(r, 14, 14);
                const QColor glyphColor = m_active ? t.accentLight() : mixColors(kTextMuted, QColor(255, 255, 255), h);
                const qreal g = down ? 18 : 20;
                drawGlyph(p, m_glyph, QRectF(r.center().x() - g / 2, r.center().y() - g / 2, g, g), glyphColor);
                break;
            }
            case Kind::Round:
            case Kind::Mini: {
                p.setPen(QPen(m_accented ? withAlpha(t.accentLight(), qRound(150 + 80 * h)) : whiteA(qRound(28 + 40 * h)), 1.2));
                p.setBrush(m_accented ? withAlpha(t.accent(), down ? 30 : qRound(42 + 30 * h)) : whiteA(down ? 6 : qRound(12 + 12 * h)));
                p.drawEllipse(r);
                const qreal g = r.width() * (m_kind == Kind::Mini ? 0.52 : 0.46);
                drawGlyph(p, m_glyph, QRectF(r.center().x() - g / 2, r.center().y() - g / 2, g, g),
                          m_accented ? t.accentLight() : mixColors(kText, QColor(255, 255, 255), h));
                break;
            }
            case Kind::Play: {
                QLinearGradient g(r.topLeft(), r.topRight());
                g.setColorAt(0, mixColors(t.accentDark(), t.accent(), 0.10 + 0.25 * h));
                g.setColorAt(1, mixColors(t.accentDark2(), t.accent(), 0.06 + 0.20 * h));
                p.setPen(QPen(withAlpha(t.accentLight(), down ? 130 : qRound(150 + 80 * h)), 1.2));
                p.setBrush(g);
                p.drawRoundedRect(r, 20, 20);
                QFont f = font();
                f.setBold(true);
                p.setFont(f);
                const int textWidth = QFontMetrics(f).horizontalAdvance(text());
                const qreal total = 20 + 12 + textWidth;
                const qreal x = r.center().x() - total / 2;
                drawGlyph(p, Glyph::Play, QRectF(x, r.center().y() - 10, 20, 20), QColor(0xf0, 0xfd, 0xf6));
                p.setPen(QColor(0xf0, 0xfd, 0xf6));
                p.drawText(QRectF(x + 32, r.top(), textWidth + 4, r.height()), Qt::AlignVCenter | Qt::AlignLeft, text());
                break;
            }
            case Kind::Bar: {
                p.setPen(QPen(whiteA(qRound(26 + 30 * h)), 1));
                p.setBrush(whiteA(down ? 6 : qRound(9 + 9 * h)));
                p.drawRoundedRect(r, 15, 15);
                const QRectF badge(r.left() + 10, r.center().y() - 14, 28, 28);
                p.setPen(QPen(withAlpha(t.accentLight(), 150), 1.2));
                p.setBrush(withAlpha(t.accent(), 36));
                p.drawEllipse(badge);
                drawGlyph(p, m_glyph, QRectF(badge.center().x() - 7, badge.center().y() - 7, 14, 14), t.accentLight());
                p.setPen(mixColors(kText, QColor(255, 255, 255), h));
                p.drawText(QRectF(r.left() + 50, r.top(), r.width() - 80, r.height()), Qt::AlignVCenter | Qt::AlignLeft, text());
                QPolygonF arrow;
                const qreal ax = r.right() - 20 + 3 * h;
                arrow << QPointF(ax - 3, r.center().y() - 5) << QPointF(ax + 3, r.center().y()) << QPointF(ax - 3, r.center().y() + 5);
                p.setPen(QPen(kTextMuted, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p.setBrush(Qt::NoBrush);
                p.drawPolyline(arrow);
                break;
            }
            case Kind::Chip: {
                const QColor bg = m_active ? withAlpha(t.accent(), 46) : whiteA(down ? 6 : qRound(8 + 10 * h));
                const QColor border = m_active ? withAlpha(t.accentLight(), 150) : whiteA(qRound(22 + 30 * h));
                p.setPen(QPen(border, 1));
                p.setBrush(bg);
                p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
                const int textWidth = fontMetrics().horizontalAdvance(text());
                const qreal glyphWidth = m_glyph == Glyph::None ? 0 : 20;
                const qreal x = r.center().x() - (glyphWidth + textWidth) / 2;
                const QColor content = m_active ? t.accentText() : mixColors(kText, QColor(255, 255, 255), h);
                if (m_glyph != Glyph::None) {
                    drawGlyph(p, m_glyph, QRectF(x, r.center().y() - 7, 14, 14), content);
                }
                p.setPen(content);
                p.drawText(QRectF(x + glyphWidth, r.top(), textWidth + 4, r.height()), Qt::AlignVCenter | Qt::AlignLeft, text());
                break;
            }
            case Kind::Header: {
                if (h > 0.01) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(whiteA(qRound(8 * h)));
                    p.drawRoundedRect(r, 16, 16);
                }
                const QRectF badge(r.left() + 12, r.center().y() - 14, 28, 28);
                p.setPen(QPen(withAlpha(t.accentLight(), 150), 1.2));
                p.setBrush(withAlpha(t.accent(), 36));
                p.drawEllipse(badge);
                drawGlyph(p, m_glyph, QRectF(badge.center().x() - 8, badge.center().y() - 8, 16, 16), t.accentLight());
                QFont f = font();
                f.setBold(true);
                p.setFont(f);
                p.setPen(mixColors(kText, QColor(255, 255, 255), h));
                p.drawText(QRectF(r.left() + 52, r.top(), r.width() - 90, r.height()), Qt::AlignVCenter | Qt::AlignLeft, text());
                p.save();
                p.translate(r.right() - 22, r.center().y());
                p.rotate(180.0 * m_expand);
                p.setPen(QPen(kTextMuted, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                QPolygonF chevron;
                chevron << QPointF(-4.5, -2) << QPointF(0, 2.5) << QPointF(4.5, -2);
                p.drawPolyline(chevron);
                p.restore();
                break;
            }
            case Kind::Swatch: {
                const QColor color = QColor::fromHslF(m_swatchHue / 360.0, 0.84, 0.42);
                const bool current = qAbs(t.hue - m_swatchHue) <= 4;
                const qreal grow = 1.5 * h;
                const QRectF dot = r.adjusted(4 - grow, 4 - grow, -4 + grow, -4 + grow);
                if (current) {
                    p.setPen(QPen(QColor(255, 255, 255, 230), 1.6));
                    p.setBrush(Qt::NoBrush);
                    p.drawEllipse(r.adjusted(0.5, 0.5, -0.5, -0.5));
                }
                p.setPen(Qt::NoPen);
                p.setBrush(color);
                p.drawEllipse(dot);
                break;
            }
        }
    }

   private:
    void animateTo(qreal target)
    {
        m_anim->stop();
        m_anim->setStartValue(m_hover);
        m_anim->setEndValue(target);
        m_anim->start();
    }

    Glyph m_glyph;
    Kind m_kind;
    bool m_active = false;
    bool m_accented = false;
    qreal m_hover = 0.0;
    qreal m_expand = 0.0;
    int m_swatchHue = 160;
    QVariantAnimation* m_anim = nullptr;
};

class DashSwitch : public QAbstractButton {
   public:
    explicit DashSwitch(QWidget* parent = nullptr) : QAbstractButton(parent)
    {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(46, 26);
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(160);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_pos = value.toReal();
            update();
        });
        connect(this, &QAbstractButton::toggled, this, [this](bool on) {
            m_anim->stop();
            m_anim->setStartValue(m_pos);
            m_anim->setEndValue(on ? 1.0 : 0.0);
            m_anim->start();
        });
    }
    QSize sizeHint() const override { return QSize(46, 26); }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        const DashTheme& t = theme();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setPen(QPen(mixColors(whiteA(48), withAlpha(t.accentLight(), 210), m_pos), 1.2));
        p.setBrush(mixColors(whiteA(14), t.accentDark(), m_pos));
        p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
        const qreal d = r.height() - 8;
        const qreal x = r.left() + 4 + (r.width() - d - 8) * m_pos;
        p.setPen(Qt::NoPen);
        p.setBrush(mixColors(kTextMuted, QColor(0xf0, 0xfd, 0xf6), m_pos));
        p.drawEllipse(QRectF(x, r.top() + 4, d, d));
    }

   private:
    qreal m_pos = 0.0;
    QVariantAnimation* m_anim = nullptr;
};

class DashAvatars : public QWidget {
   public:
    explicit DashAvatars(QWidget* parent = nullptr) : QWidget(parent)
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
        const DashTheme& t = theme();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const qreal d = 36;
        const qreal step = 26;
        const qreal top = (height() - d) / 2;
        const int count = static_cast<int>(m_faces.size());

        for (int i = count - 1; i >= 0; --i) {
            const QRectF box(2 + i * step, top, d, d);
            p.setPen(Qt::NoPen);
            p.setBrush(t.panelBottom());
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
            p.setPen(QPen(i == m_defaultIndex ? t.accentLight() : whiteA(50), i == m_defaultIndex ? 2.0 : 1.2));
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

class DashHeroIcon : public QWidget {
   public:
    explicit DashHeroIcon(QWidget* parent = nullptr) : QWidget(parent)
    {
        setFixedSize(128, 120);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(170);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_hover = value.toReal();
            update();
        });
    }

    std::function<void()> onClick;

    void setPixmap(const QPixmap& pixmap)
    {
        m_pixmap = pixmap;
        update();
    }

   protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent* event) override
#else
    void enterEvent(QEvent* event) override
#endif
    {
        animateTo(1.0);
        QWidget::enterEvent(event);
    }
    void leaveEvent(QEvent* event) override
    {
        animateTo(0.0);
        QWidget::leaveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()) && onClick) {
            onClick();
        }
        QWidget::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent*) override
    {
        const DashTheme& t = theme();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QPointF c(width() / 2.0, height() / 2.0);

        QRadialGradient glow(c, 60);
        glow.setColorAt(0, withAlpha(t.accent(), qRound(46 + 34 * m_hover)));
        glow.setColorAt(1, withAlpha(t.accent(), 0));
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawEllipse(c, 60, 60);

        const QRectF box(c.x() - 46, c.y() - 46, 92, 92);
        p.setPen(QPen(withAlpha(t.accentLight(), qRound(70 + 90 * m_hover)), 1));
        p.setBrush(withAlpha(t.accent(), 24));
        p.drawRoundedRect(box, 26, 26);
        if (!m_pixmap.isNull()) {
            p.drawPixmap(QRectF(c.x() - 30, c.y() - 30, 60, 60), m_pixmap, QRectF(m_pixmap.rect()));
        }
    }

   private:
    void animateTo(qreal target)
    {
        m_anim->stop();
        m_anim->setStartValue(m_hover);
        m_anim->setEndValue(target);
        m_anim->start();
    }
    QPixmap m_pixmap;
    qreal m_hover = 0.0;
    QVariantAnimation* m_anim = nullptr;
};

class DashWallpaper : public QFrame {
   public:
    explicit DashWallpaper(QWidget* parent = nullptr) : QFrame(parent) { setObjectName(QStringLiteral("centerPanel")); }

    void setWallpaper(const QString& path)
    {
        if (path == m_path) {
            update();
            return;
        }
        m_path = path;
        if (m_movie) {
            m_movie->stop();
            delete m_movie;
            m_movie = nullptr;
        }
        m_still = QPixmap();
        if (!path.isEmpty()) {
            QImageReader reader(path);
            if (reader.supportsAnimation()) {
                m_movie = new QMovie(path, QByteArray(), this);
                if (m_movie->isValid()) {
                    connect(m_movie, &QMovie::frameChanged, this, [this]() { update(); });
                    m_movie->start();
                } else {
                    delete m_movie;
                    m_movie = nullptr;
                }
            }
            if (!m_movie) {
                m_still = QPixmap(path);
            }
        }
        update();
    }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        const DashTheme& t = theme();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath shape;
        shape.addRoundedRect(r, 22, 22);
        p.setClipPath(shape);

        QLinearGradient base(r.topLeft(), r.bottomLeft());
        base.setColorAt(0, t.panelTop());
        base.setColorAt(1, t.panelBottom());
        p.fillRect(r, base);

        const QPixmap frame = m_movie ? m_movie->currentPixmap() : m_still;
        if (!frame.isNull() && frame.width() > 0 && frame.height() > 0) {
            const qreal scale = qMax(r.width() / frame.width(), r.height() / frame.height());
            const QSizeF sourceSize(r.width() / scale, r.height() / scale);
            const QRectF source((frame.width() - sourceSize.width()) / 2, (frame.height() - sourceSize.height()) / 2, sourceSize.width(),
                                sourceSize.height());
            p.drawPixmap(r, frame, source);
            p.fillRect(r, QColor(0, 0, 0, qRound(t.dim * 2.55)));
            QLinearGradient vignette(r.topLeft(), r.bottomLeft());
            vignette.setColorAt(0, QColor(0, 0, 0, 50));
            vignette.setColorAt(0.5, QColor(0, 0, 0, 0));
            vignette.setColorAt(1, QColor(0, 0, 0, 110));
            p.fillRect(r, vignette);
        } else {
            QRadialGradient ambient(QPointF(r.center().x(), r.bottom() + 40), r.width() * 0.75);
            ambient.setColorAt(0, withAlpha(t.accent(), 34));
            ambient.setColorAt(1, withAlpha(t.accent(), 0));
            p.fillRect(r, ambient);
        }

        p.setClipping(false);
        p.setPen(QPen(whiteA(20), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, 22, 22);
    }

   private:
    QString m_path;
    QMovie* m_movie = nullptr;
    QPixmap m_still;
};

namespace {

class InstanceRowDelegate : public QStyledItemDelegate {
   public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return QSize(0, 60); }

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const DashTheme& t = theme();
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
            g.setColorAt(0, withAlpha(t.accent(), 52));
            g.setColorAt(1, withAlpha(t.accent(), 16));
            p->fillPath(path, g);
            p->setPen(QPen(withAlpha(t.accentLight(), 140), 1));
        } else {
            p->fillPath(path, whiteA(hot ? 15 : 7));
            p->setPen(QPen(whiteA(hot ? 46 : 20), 1));
        }
        p->setBrush(Qt::NoBrush);
        p->drawPath(path);

        const QColor rings[] = { kCyan, t.accentLight(), kPurple };
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
            p->setBrush(withAlpha(t.accent(), 60));
            p->drawRoundedRect(pill, 10, 10);
            p->setPen(t.accentText());
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

QString stripStamp(const QString& fileName)
{
    QString name = fileName;
    name.remove(QRegularExpression(QStringLiteral("^\\d+_")));
    return name;
}

}  // namespace

// =============================================================================================
// Dashboard
// =============================================================================================

Dashboard::Dashboard(QAbstractItemModel* model, QItemSelectionModel* selection, const DashboardActions& actions, QWidget* parent)
    : QWidget(parent), m_model(model), m_selection(selection), m_actions(actions)
{
    theme().load();
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

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(12);

    // ------------------------------------------------------------------ left sidebar
    auto* sidebar = makeFrame("sidebar", this);
    sidebar->setFixedWidth(74);
    auto* side = new QVBoxLayout(sidebar);
    side->setContentsMargins(13, 14, 13, 14);
    side->setSpacing(10);

    auto* home = new DashButton(Glyph::Grid, DashButton::Kind::Square, sidebar);
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
        { Glyph::Cube, QStringLiteral(u"Versi\u00f3n y loaders"), "version" },
        { Glyph::Puzzle, QStringLiteral("Mods"), "mods" },
        { Glyph::Brush, QStringLiteral("Resource packs"), "resourcepacks" },
        { Glyph::Sun, QStringLiteral("Shaders"), "shaderpacks" },
        { Glyph::Camera, QStringLiteral("Capturas de pantalla"), "screenshots" },
        { Glyph::Globe, QStringLiteral("Mundos"), "worlds" },
        { Glyph::Terminal, QStringLiteral("Registro del juego"), "console" },
    };
    for (const auto& def : pages) {
        auto* button = new DashButton(def.glyph, DashButton::Kind::Square, sidebar);
        button->setToolTip(def.tip);
        const QString page = QString::fromLatin1(def.page);
        connect(button, &QToolButton::clicked, this, [this, page]() { openInstancePage(page); });
        m_pageButtons.append(button);
        side->addWidget(button);
    }
    side->addStretch(1);
    side->addWidget(makeFrame("divider", sidebar));

    // "more" menu: everything that used to live in the toolbar and is not worth a permanent button
    auto* moreMenu = new QMenu(this);
    if (a.folders && a.folders->menu()) {
        moreMenu->addMenu(a.folders->menu());
    }
    if (a.help && a.help->menu()) {
        moreMenu->addMenu(a.help->menu());
    }
    if (a.accounts && a.accounts->menu()) {
        moreMenu->addMenu(a.accounts->menu());
    }
    moreMenu->addSeparator();
    if (a.checkUpdate) {
        moreMenu->addAction(a.checkUpdate);
    }
    if (a.moreNews) {
        moreMenu->addAction(a.moreNews);
    }
    auto* moreButton = new DashButton(Glyph::Dots, DashButton::Kind::Square, sidebar);
    moreButton->setMenu(moreMenu);
    moreButton->setPopupMode(QToolButton::InstantPopup);
    moreButton->setToolTip(QStringLiteral(u"M\u00e1s: carpetas, ayuda, cuentas y actualizaciones"));
    side->addWidget(moreButton);

    auto* settingsButton = new DashButton(Glyph::Gear, DashButton::Kind::Square, sidebar);
    if (a.settings) {
        settingsButton->setDefaultAction(a.settings);
    }
    settingsButton->setToolTip(QStringLiteral("Ajustes"));
    side->addWidget(settingsButton);
    root->addWidget(sidebar);

    // ------------------------------------------------------------------ center column
    auto* center = new QVBoxLayout();
    center->setSpacing(12);

    m_centerPanel = new DashWallpaper(this);
    auto* pl = new QVBoxLayout(m_centerPanel);
    pl->setContentsMargins(22, 18, 22, 16);
    pl->setSpacing(10);

    auto* top = new QHBoxLayout();
    m_countChip = makeLabel(QString(), "chip", m_centerPanel);
    top->addWidget(m_countChip);
    top->addStretch(1);
    pl->addLayout(top);
    pl->addStretch(1);

    // hero card
    auto* hero = makeFrame("heroCard", m_centerPanel);
    hero->setMinimumWidth(440);
    hero->setMaximumWidth(560);
    auto* hl = new QVBoxLayout(hero);
    hl->setContentsMargins(30, 26, 30, 28);
    hl->setSpacing(4);

    m_heroContent = new QWidget(hero);
    auto* hc = new QVBoxLayout(m_heroContent);
    hc->setContentsMargins(0, 0, 0, 0);
    hc->setSpacing(4);
    m_heroIcon = new DashHeroIcon(m_heroContent);
    m_heroIcon->setToolTip(QStringLiteral("Cambiar el icono de la instancia"));
    m_heroIcon->onClick = [this]() {
        if (m_actions.changeIcon && m_actions.changeIcon->isEnabled()) {
            m_actions.changeIcon->trigger();
        }
    };
    hc->addWidget(m_heroIcon, 0, Qt::AlignHCenter);
    m_heroName = makeLabel(QString(), "heroName", m_heroContent);
    m_heroName->setAlignment(Qt::AlignCenter);
    hc->addWidget(m_heroName);
    m_heroSub = makeLabel(QString(), "heroSub", m_heroContent);
    m_heroSub->setAlignment(Qt::AlignCenter);
    hc->addWidget(m_heroSub);
    m_heroFade = new QGraphicsOpacityEffect(m_heroContent);
    m_heroFade->setOpacity(1.0);
    m_heroContent->setGraphicsEffect(m_heroFade);
    hl->addWidget(m_heroContent);
    hl->addSpacing(14);

    auto* playRow = new QHBoxLayout();
    playRow->setSpacing(10);
    playRow->addStretch(1);
    m_playButton = new DashButton(Glyph::Play, DashButton::Kind::Play, hero);
    if (a.launch) {
        m_playButton->setDefaultAction(a.launch);
    }
    m_playGlow = new QGraphicsDropShadowEffect(m_playButton);
    m_playGlow->setBlurRadius(26);
    m_playGlow->setOffset(0, 0);
    m_playButton->setGraphicsEffect(m_playGlow);
    playRow->addWidget(m_playButton);
    m_stopButton = new DashButton(Glyph::Stop, DashButton::Kind::Round, hero);
    if (a.kill) {
        m_stopButton->setDefaultAction(a.kill);
        m_stopButton->setVisible(a.kill->isEnabled());
        QAction* kill = a.kill;
        connect(kill, &QAction::changed, m_stopButton, [this, kill]() { m_stopButton->setVisible(kill->isEnabled()); });
    } else {
        m_stopButton->setVisible(false);
    }
    m_stopButton->setToolTip(QStringLiteral("Forzar cierre del juego"));
    playRow->addWidget(m_stopButton);
    playRow->addStretch(1);
    hl->addLayout(playRow);
    pl->addWidget(hero, 0, Qt::AlignHCenter);
    pl->addStretch(1);

    m_heroAnim = new QVariantAnimation(this);
    m_heroAnim->setDuration(300);
    m_heroAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_heroAnim->setStartValue(0.0);
    m_heroAnim->setEndValue(1.0);
    connect(m_heroAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) { m_heroFade->setOpacity(value.toReal()); });

    // footer: status + activity bar
    auto* foot = new QHBoxLayout();
    foot->setSpacing(8);
    m_statusText = makeLabel(QString(), "statusText", m_centerPanel);
    m_statusText->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_playtime = makeLabel(QString(), "playtime", m_centerPanel);
    foot->addWidget(m_statusText, 1);
    foot->addWidget(m_playtime, 0);
    pl->addLayout(foot);
    m_activity = new QProgressBar(m_centerPanel);
    m_activity->setTextVisible(false);
    m_activity->setRange(0, 1);
    m_activity->setValue(0);
    pl->addWidget(m_activity);
    center->addWidget(m_centerPanel, 1);

    // instance details card (full width)
    auto* card1 = makeFrame("card", this);
    card1->setFixedHeight(136);
    auto* c1 = new QVBoxLayout(card1);
    c1->setContentsMargins(18, 14, 18, 12);
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
    center->addWidget(card1);
    root->addLayout(center, 1);

    // ------------------------------------------------------------------ right panel
    auto* rightPanel = makeFrame("rightPanel", this);
    rightPanel->setFixedWidth(340);
    auto* rl = new QVBoxLayout(rightPanel);
    rl->setContentsMargins(18, 18, 18, 16);
    rl->setSpacing(12);

    auto* accountRow = new QHBoxLayout();
    m_avatars = new DashAvatars(rightPanel);
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

    // appearance drawer (replaces the old memory card)
    auto* drawer = makeFrame("card", rightPanel);
    auto* dl = new QVBoxLayout(drawer);
    dl->setContentsMargins(0, 0, 0, 0);
    dl->setSpacing(0);
    auto* drawerHeader = new DashButton(Glyph::Palette, DashButton::Kind::Header, drawer);
    drawerHeader->setObjectName(QStringLiteral("drawerHeader"));
    drawerHeader->setText(QStringLiteral("Apariencia"));
    dl->addWidget(drawerHeader);

    m_drawerBody = new QWidget(drawer);
    auto* bl = new QVBoxLayout(m_drawerBody);
    bl->setContentsMargins(16, 2, 16, 16);
    bl->setSpacing(8);

    bl->addWidget(makeLabel(QStringLiteral("COLOR DE ACENTO"), "sectionTitle", m_drawerBody));
    m_hueSlider = new QSlider(Qt::Horizontal, m_drawerBody);
    m_hueSlider->setObjectName(QStringLiteral("hueSlider"));
    m_hueSlider->setRange(0, 359);
    m_hueSlider->setFixedHeight(24);
    bl->addWidget(m_hueSlider);
    auto* swatches = new QHBoxLayout();
    swatches->setSpacing(6);
    const struct {
        int hue;
        const char16_t* name;
    } presets[] = {
        { 160, u"Esmeralda" }, { 195, u"Oc\u00e9ano" }, { 222, u"Zafiro" }, { 265, u"Violeta" },
        { 330, u"Rosa" },      { 22, u"Atardecer" },     { 350, u"Rub\u00ed" },
    };
    for (const auto& preset : presets) {
        auto* swatch = new DashButton(Glyph::None, DashButton::Kind::Swatch, m_drawerBody);
        swatch->setSwatchHue(preset.hue);
        swatch->setToolTip(QString::fromUtf16(preset.name));
        const int hue = preset.hue;
        connect(swatch, &QToolButton::clicked, this, [this, hue]() {
            theme().hue = hue;
            syncThemeControls();
            scheduleThemeApply();
        });
        swatches->addWidget(swatch);
    }
    swatches->addStretch(1);
    bl->addLayout(swatches);

    bl->addSpacing(4);
    bl->addWidget(makeLabel(QStringLiteral("BASE"), "sectionTitle", m_drawerBody));
    auto* baseRow = new QHBoxLayout();
    baseRow->setSpacing(8);
    m_baseDark = new DashButton(Glyph::None, DashButton::Kind::Chip, m_drawerBody);
    m_baseDark->setText(QStringLiteral("Oscuro"));
    m_baseAmoled = new DashButton(Glyph::None, DashButton::Kind::Chip, m_drawerBody);
    m_baseAmoled->setText(QStringLiteral("Medianoche"));
    baseRow->addWidget(m_baseDark);
    baseRow->addWidget(m_baseAmoled);
    baseRow->addStretch(1);
    bl->addLayout(baseRow);
    connect(m_baseDark, &QToolButton::clicked, this, [this]() {
        theme().amoled = false;
        syncThemeControls();
        scheduleThemeApply();
    });
    connect(m_baseAmoled, &QToolButton::clicked, this, [this]() {
        theme().amoled = true;
        syncThemeControls();
        scheduleThemeApply();
    });

    bl->addSpacing(4);
    bl->addWidget(makeLabel(QStringLiteral("FONDO"), "sectionTitle", m_drawerBody));
    auto* wallRow = new QHBoxLayout();
    wallRow->setSpacing(8);
    auto* chooseButton = new DashButton(Glyph::Image, DashButton::Kind::Chip, m_drawerBody);
    chooseButton->setText(QStringLiteral(u"Elegir imagen o GIF\u2026"));
    auto* removeButton = new DashButton(Glyph::None, DashButton::Kind::Chip, m_drawerBody);
    removeButton->setText(QStringLiteral("Quitar"));
    wallRow->addWidget(chooseButton);
    wallRow->addWidget(removeButton);
    wallRow->addStretch(1);
    bl->addLayout(wallRow);
    connect(chooseButton, &QToolButton::clicked, this, [this]() { chooseWallpaper(); });
    connect(removeButton, &QToolButton::clicked, this, [this]() { removeWallpaper(); });
    m_wallpaperName = makeLabel(QString(), "statusText", m_drawerBody);
    m_wallpaperName->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    bl->addWidget(m_wallpaperName);

    auto* dimRow = new QHBoxLayout();
    dimRow->addWidget(makeLabel(QStringLiteral("Oscurecer fondo"), "statusText", m_drawerBody));
    m_dimSlider = new QSlider(Qt::Horizontal, m_drawerBody);
    m_dimSlider->setRange(0, 90);
    m_dimSlider->setFixedHeight(24);
    dimRow->addWidget(m_dimSlider, 1);
    bl->addLayout(dimRow);
    connect(m_dimSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_syncingTheme) {
            return;
        }
        theme().dim = value;
        m_centerPanel->update();
        scheduleThemeApply();
    });

    auto* onlyRow = new QHBoxLayout();
    m_onlyThisInstance = new DashSwitch(m_drawerBody);
    onlyRow->addWidget(m_onlyThisInstance);
    onlyRow->addWidget(makeLabel(QStringLiteral("Solo para esta instancia"), "statusText", m_drawerBody));
    onlyRow->addStretch(1);
    bl->addLayout(onlyRow);
    connect(m_onlyThisInstance, &QAbstractButton::toggled, this, [this]() { syncThemeControls(); });

    connect(m_hueSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_syncingTheme) {
            return;
        }
        theme().hue = value;
        scheduleThemeApply();
    });

    m_drawerBody->setVisible(false);
    m_drawerBody->setMaximumHeight(0);
    dl->addWidget(m_drawerBody);
    m_drawerAnim = new QVariantAnimation(this);
    m_drawerAnim->setDuration(280);
    m_drawerAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_drawerAnim, &QVariantAnimation::valueChanged, this, [this, drawerHeader](const QVariant& value) {
        const qreal progress = value.toReal();
        m_drawerBody->setProperty("progress", progress);
        m_drawerBody->setMaximumHeight(qRound(m_drawerBody->property("targetHeight").toReal() * progress));
        drawerHeader->setExpandProgress(progress);
    });
    connect(m_drawerAnim, &QVariantAnimation::finished, this, [this]() {
        if (m_drawerOpen) {
            m_drawerBody->setMaximumHeight(QWIDGETSIZE_MAX);
        } else {
            m_drawerBody->setVisible(false);
        }
    });
    connect(drawerHeader, &QToolButton::clicked, this, [this]() { setDrawerExpanded(!m_drawerOpen); });
    rl->addWidget(drawer);

    // instance list header + list
    auto* listHead = new QHBoxLayout();
    listHead->addWidget(makeLabel(QStringLiteral("INSTANCIAS"), "sectionTitle", rightPanel));
    listHead->addStretch(1);
    auto* addButton = new DashButton(Glyph::Plus, DashButton::Kind::Mini, rightPanel);
    addButton->setAccented(true);
    if (a.addInstance) {
        addButton->setDefaultAction(a.addInstance);
    }
    addButton->setToolTip(QStringLiteral(u"A\u00f1adir instancia"));
    listHead->addWidget(addButton);
    rl->addLayout(listHead);

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

    auto* editBar = new DashButton(Glyph::Pencil, DashButton::Kind::Bar, rightPanel);
    if (a.edit) {
        editBar->setDefaultAction(a.edit);
    }
    rl->addWidget(editBar);
    root->addWidget(rightPanel);

    // ------------------------------------------------------------------ wiring
    m_themeTimer = new QTimer(this);
    m_themeTimer->setSingleShot(true);
    m_themeTimer->setInterval(40);
    connect(m_themeTimer, &QTimer::timeout, this, [this]() {
        applyTheme();
        theme().save();
    });

    if (auto* accounts = APPLICATION->accounts()) {
        connect(accounts, &AccountList::listChanged, this, [this]() { refresh(); });
        connect(accounts, &AccountList::defaultAccountChanged, this, [this]() { refresh(); });
    }
    connect(m_model, &QAbstractItemModel::rowsInserted, this, [this]() { updateInstanceCount(); });
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, [this]() { updateInstanceCount(); });
    connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { updateInstanceCount(); });

    updateInstanceCount();
    syncThemeControls();
    applyTheme();
    refresh();
}

void Dashboard::updateInstanceCount()
{
    const int n = m_model ? m_model->rowCount() : 0;
    m_countChip->setText(QStringLiteral("<span style='color:%3'>\u25cf</span>&nbsp;&nbsp;%1 %2")
                             .arg(n)
                             .arg(n == 1 ? QStringLiteral("instancia") : QStringLiteral("instancias"))
                             .arg(theme().accentLight().name()));
}

// ---------------------------------------------------------------------------------------------
// Appearance
// ---------------------------------------------------------------------------------------------

void Dashboard::setDrawerExpanded(bool expanded)
{
    m_drawerOpen = expanded;
    m_drawerAnim->stop();
    if (expanded) {
        m_drawerBody->setVisible(true);
        m_drawerBody->setProperty("targetHeight", m_drawerBody->sizeHint().height());
    }
    const qreal current = m_drawerBody->property("progress").toReal();
    m_drawerAnim->setStartValue(current);
    m_drawerAnim->setEndValue(expanded ? 1.0 : 0.0);
    m_drawerAnim->start();
}

void Dashboard::scheduleThemeApply()
{
    m_themeTimer->start();
}

void Dashboard::applyTheme()
{
    const DashTheme& t = theme();
    setStyleSheet(t.styleSheet());
    if (m_playGlow) {
        m_playGlow->setColor(withAlpha(t.accent(), 95));
    }
    applyWallpaper();
    updateInstanceCount();
    const auto widgets = findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        widget->update();
    }
    update();
}

void Dashboard::applyWallpaper()
{
    if (m_centerPanel) {
        m_centerPanel->setWallpaper(theme().wallpaperPathFor(m_lastInstanceId));
        m_centerPanel->update();
    }
}

void Dashboard::syncThemeControls()
{
    if (!m_hueSlider) {
        return;
    }
    const DashTheme& t = theme();
    m_syncingTheme = true;
    m_hueSlider->setValue(t.hue);
    m_dimSlider->setValue(t.dim);
    m_syncingTheme = false;
    m_baseDark->setActive(!t.amoled);
    m_baseAmoled->setActive(t.amoled);

    QString text;
    const QString own = t.instanceWallpapers.value(m_lastInstanceId);
    if (m_onlyThisInstance->isChecked()) {
        text = own.isEmpty() ? QStringLiteral("Esta instancia usa el fondo general") : QStringLiteral("Esta instancia: ") + stripStamp(own);
    } else {
        text = t.wallpaper.isEmpty() ? QStringLiteral("Sin fondo (se usa el degradado)") : QStringLiteral("General: ") + stripStamp(t.wallpaper);
    }
    m_wallpaperName->setText(text);
    for (auto* swatch : m_drawerBody->findChildren<DashButton*>()) {
        swatch->update();
    }
}

void Dashboard::chooseWallpaper()
{
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("Elegir fondo"), QString(),
        QStringLiteral(u"Im\u00e1genes y animaciones (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.apng);;Todos los archivos (*)"));
    if (file.isEmpty()) {
        return;
    }
    QImageReader check(file);
    if (!check.canRead()) {
        QMessageBox::warning(this, QStringLiteral("Fondo"),
                             QStringLiteral(u"No se pudo leer ese archivo como imagen o animaci\u00f3n. Prueba con PNG, JPG, GIF o WebP."));
        return;
    }
    DashTheme& t = theme();
    QDir().mkpath(t.backgroundsDir());
    const QString name = QString::number(QDateTime::currentMSecsSinceEpoch()) + QStringLiteral("_") + QFileInfo(file).fileName();
    if (!QFile::copy(file, QDir(t.backgroundsDir()).filePath(name))) {
        QMessageBox::warning(this, QStringLiteral("Fondo"), QStringLiteral("No se pudo copiar el archivo a la carpeta del launcher."));
        return;
    }
    if (m_onlyThisInstance->isChecked() && !m_lastInstanceId.isEmpty()) {
        t.instanceWallpapers.insert(m_lastInstanceId, name);
    } else {
        t.wallpaper = name;
    }
    syncThemeControls();
    scheduleThemeApply();
}

void Dashboard::removeWallpaper()
{
    DashTheme& t = theme();
    if (m_onlyThisInstance->isChecked()) {
        t.instanceWallpapers.remove(m_lastInstanceId);
    } else {
        t.wallpaper.clear();
    }
    syncThemeControls();
    scheduleThemeApply();
}

// ---------------------------------------------------------------------------------------------
// Instance data
// ---------------------------------------------------------------------------------------------

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
    const DashTheme& t = theme();

    for (auto* button : m_pageButtons) {
        button->setEnabled(has);
    }

    const QString id = inst ? inst->id() : QString();
    const bool instanceChanged = id != m_lastInstanceId;
    m_lastInstanceId = id;

    const QString dotVersion = QStringLiteral("<span style='color:#22d3ee'>\u25cf</span>&nbsp;&nbsp;%1");
    const QString dotLoader = QStringLiteral("<span style='color:%1'>\u25cf</span>&nbsp;&nbsp;%2");
    const QString dotRam = QStringLiteral("<span style='color:#a78bfa'>\u25cf</span>&nbsp;&nbsp;%1");

    if (!has) {
        m_heroName->setText(QStringLiteral("Sin instancia seleccionada"));
        m_heroSub->setText(QStringLiteral("Elige una instancia de la lista"));
        m_heroIcon->setPixmap(glyphPixmap(Glyph::Cube, t.accentLight(), 56, dpr));
        m_chipVersion->setText(dotVersion.arg(QStringLiteral("\u2014")));
        m_chipLoader->setText(dotLoader.arg(t.accentLight().name(), QStringLiteral("\u2014")));
        m_chipRam->setText(dotRam.arg(QStringLiteral("\u2014")));
        m_cardBadge->setText(QStringLiteral(u"Sin selecci\u00f3n"));
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
            m_heroIcon->setPixmap(glyphPixmap(Glyph::Cube, t.accentLight(), 56, dpr));
        } else {
            m_heroIcon->setPixmap(icon.pixmap(QSize(60, 60), dpr));
        }

        const PackInfo info = readPackInfo(inst);
        m_chipVersion->setText(dotVersion.arg(info.version.isEmpty() ? QStringLiteral("Minecraft") : info.version));
        m_chipLoader->setText(dotLoader.arg(t.accentLight().name(), info.loader.isEmpty() ? QStringLiteral("Vanilla") : info.loader));
        m_chipRam->setText(dotRam.arg(QStringLiteral("%1 MiB").arg(inst->settings()->get(QStringLiteral("MaxMemAlloc")).toInt())));
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

    if (instanceChanged) {
        applyWallpaper();
        syncThemeControls();
        m_heroAnim->stop();
        m_heroAnim->start();
    }

    const int total = APPLICATION->instances()->getTotalPlayTime();
    m_playtime->setText(total > 0 ? QStringLiteral("Tiempo total: %1").arg(Time::prettifyDuration(total)) : QString());

    if (m_avatars) {
        m_avatars->reload();
        m_accountBadge->setText(QString::number(m_avatars->total()));
    }
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
