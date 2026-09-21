// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Tokio dashboard - a full visual replacement for the main window contents.
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
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QHash>
#include <QImage>
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
#include <QRegion>
#include <QRegularExpression>
#include <QSlider>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QVector>
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
QColor withAlpha(QColor c, int alpha)
{
    c.setAlpha(qBound(0, alpha, 255));
    return c;
}

QColor mixColors(const QColor& a, const QColor& b, qreal t)
{
    t = qBound<qreal>(0.0, t, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

QFont monoFont(int pixelSize, bool bold)
{
    QFont f;
    f.setFamilies({ QStringLiteral("Cascadia Mono"), QStringLiteral("Consolas"), QStringLiteral("Lucida Console"),
                    QStringLiteral("DejaVu Sans Mono"), QStringLiteral("Courier New") });
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(pixelSize);
    f.setBold(bold);
    return f;
}

// ---------------------------------------------------------------------------------------------
// Palettes ("estilos"). Every style keeps exactly the same layout, only the mood changes.
// ---------------------------------------------------------------------------------------------
enum Weather { WeatherStars = 0, WeatherRain = 1, WeatherPetals = 2, WeatherMist = 3 };

struct Palette {
    const char16_t* name;
    QRgb bg, panelTop, panelBottom, ink, text, muted, dim;
    QRgb accent, accentLight, secondary, tertiary;
    QRgb sky[5];
    QRgb sun, skyline, window;
    int weather;
};

const Palette kPalettes[] = {
    // 0 - Atardecer: city-pop dusk over the city
    { u"Atardecer", 0x0f0d1a, 0x1b1733, 0x120f24, 0xf1cfa8, 0xf4e2c8, 0xa79dc4, 0x6f6791, 0xf2553f, 0xff8a6a, 0xd6479a, 0x8b7bd8,
      { 0x1d1240, 0x4a1f78, 0x9b3a92, 0xe0567a, 0xff9a4a }, 0xff5a3c, 0x0d0a1a, 0xffc45a, WeatherStars },
    // 1 - Neon: rainy night
    { u"Ne\u00f3n", 0x07060d, 0x0f0c1c, 0x090813, 0x9fe8ff, 0xe6e2ff, 0x8e89b5, 0x5e5a86, 0xff2fa3, 0xff7bc8, 0x21d4fd, 0x9d5cff,
      { 0x05030f, 0x150736, 0x3a0d6b, 0x8a1a8f, 0xff2fa3 }, 0x21d4fd, 0x05040c, 0x21d4fd, WeatherRain },
    // 2 - 1-bit: peach and navy, like an old monochrome game
    { u"1-bit", 0x1c2036, 0x2a3050, 0x222741, 0xf2c9a0, 0xf2c9a0, 0xb39a86, 0x7d7284, 0xf2c9a0, 0xffe0bf, 0xd9433f, 0x8f98c0,
      { 0x232842, 0x4a4560, 0x8a7378, 0xc99b8c, 0xf2c9a0 }, 0xd9433f, 0x232842, 0xf2c9a0, WeatherStars },
    // 3 - Sakura: pink dusk with falling petals
    { u"Sakura", 0x150c16, 0x24121f, 0x1a0d18, 0xffd6e4, 0xffe4ee, 0xc9a0b3, 0x8a6478, 0xff7aa8, 0xffa8c6, 0xffc2d6, 0xc96bd0,
      { 0x1a0d2e, 0x3d1450, 0x7a2860, 0xc05078, 0xffa0a8 }, 0xffd0dc, 0x120813, 0xffd27a, WeatherPetals },
    // 4 - Niebla: misty teal morning
    { u"Niebla", 0x0f181d, 0x18252c, 0x121d23, 0xcfe3e8, 0xe0edf0, 0x94adb5, 0x5f7780, 0xe8a08a, 0xffc4b0, 0x6fb3c0, 0x8a9fd0,
      { 0x1b2c36, 0x2c4551, 0x4f6d7b, 0x86a3ae, 0xc9d8dc }, 0xf3e3d0, 0x0e161b, 0xf0d9a8, WeatherMist },
};
constexpr int kPaletteCount = static_cast<int>(sizeof(kPalettes) / sizeof(kPalettes[0]));

// ---------------------------------------------------------------------------------------------
// Live theme: chosen style, optional accent hue, wallpapers. Persisted next to the launcher data.
// ---------------------------------------------------------------------------------------------
struct DashTheme {
    int preset = 0;
    int hueOverride = -1;  // -1 = use the style's own accent
    int dim = 45;
    bool pixel = false;    // pixelate the user's wallpaper
    bool animate = true;   // animate the built-in scene
    QString wallpaper;                           // file name inside the backgrounds folder
    QHash<QString, QString> instanceWallpapers;  // instance id -> file name

    const Palette& pal() const { return kPalettes[qBound(0, preset, kPaletteCount - 1)]; }

    QColor withHue(const QColor& c) const
    {
        if (hueOverride < 0) {
            return c;
        }
        return QColor::fromHslF(hueOverride / 360.0, c.hslSaturationF(), c.lightnessF());
    }

    QColor accent() const { return withHue(QColor(pal().accent)); }
    QColor accentLight() const { return withHue(QColor(pal().accentLight)); }
    QColor accentDark() const
    {
        const QColor a = accent();
        return QColor::fromHslF(a.hslHueF() < 0 ? 0 : a.hslHueF(), a.hslSaturationF() * 0.6, 0.14);
    }
    QColor secondary() const { return QColor(pal().secondary); }
    QColor tertiary() const { return QColor(pal().tertiary); }
    QColor bg() const { return QColor(pal().bg); }
    QColor panelTop() const { return QColor(pal().panelTop); }
    QColor panelBottom() const { return QColor(pal().panelBottom); }
    QColor ink() const { return QColor(pal().ink); }
    QColor text() const { return QColor(pal().text); }
    QColor muted() const { return QColor(pal().muted); }
    QColor dimText() const { return QColor(pal().dim); }
    QColor onAccent() const { return bg(); }

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
        preset = qBound(0, obj.value(QStringLiteral("preset")).toInt(preset), kPaletteCount - 1);
        hueOverride = qBound(-1, obj.value(QStringLiteral("hueOverride")).toInt(hueOverride), 359);
        dim = qBound(0, obj.value(QStringLiteral("dim")).toInt(dim), 90);
        pixel = obj.value(QStringLiteral("pixel")).toBool(pixel);
        animate = obj.value(QStringLiteral("animate")).toBool(animate);
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
        obj.insert(QStringLiteral("preset"), preset);
        obj.insert(QStringLiteral("hueOverride"), hueOverride);
        obj.insert(QStringLiteral("dim"), dim);
        obj.insert(QStringLiteral("pixel"), pixel);
        obj.insert(QStringLiteral("animate"), animate);
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
* { font-family: "Cascadia Mono", "Consolas", "Lucida Console", "DejaVu Sans Mono", monospace; }
#dashRoot { background-color: @BG@; }
QLabel { background: transparent; color: @TEXT@; }
#heroName { color: @TEXT@; font-size: 22px; font-weight: bold; }
#heroSub { color: @MUTED@; font-size: 12px; }
#cardTitle { color: @MUTED@; font-size: 11px; font-weight: bold; }
#sectionTitle { color: @DIM@; font-size: 10px; font-weight: bold; }
#statusText { color: @MUTED@; font-size: 11px; }
#playtime { color: @DIM@; font-size: 11px; }
#chip {
    background-color: rgba(@INK@, 0.07);
    border: 1px solid rgba(@INK@, 0.32);
    border-radius: 0px;
    padding: 3px 8px;
    color: @TEXT@;
    font-size: 11px;
}
#badge {
    background-color: @ACC@;
    border: 1px solid @ACCL@;
    border-radius: 0px;
    padding: 1px 7px;
    color: @BG@;
    font-size: 10px;
    font-weight: bold;
}
#accBadge {
    background-color: rgba(@A@, 0.16);
    border: 2px solid @ACC@;
    border-radius: 0px;
    color: @ACCL@;
    font-size: 11px;
    font-weight: bold;
    min-width: 22px;
    max-width: 22px;
    min-height: 22px;
    max-height: 22px;
}
#divider { background: transparent; border: none; border-top: 1px dashed rgba(@INK@, 0.38); min-height: 1px; max-height: 1px; }
QProgressBar {
    background-color: rgba(0, 0, 0, 0.55);
    border: 1px solid rgba(@INK@, 0.40);
    border-radius: 0px;
    min-height: 8px;
    max-height: 8px;
    text-align: center;
    color: transparent;
}
QProgressBar::chunk { background-color: @ACC@; }
QSlider::groove:horizontal { height: 6px; border: 1px solid rgba(@INK@, 0.35); background-color: rgba(@INK@, 0.12); }
QSlider::sub-page:horizontal { background-color: @ACC@; }
QSlider::handle:horizontal {
    width: 10px; height: 10px; margin: -5px 0;
    background-color: @BG@; border: 2px solid @INK_HEX@;
}
QSlider::handle:horizontal:hover { border-color: @ACCL@; }
#hueSlider::groove:horizontal { height: 8px; border: 1px solid rgba(@INK@, 0.35); background: qlineargradient(x1:0, y1:0, x2:1, y2:0, @HUES@); }
#hueSlider::sub-page:horizontal { background: transparent; }
#hueSlider::add-page:horizontal { background: transparent; }
#hueSlider::handle:horizontal { width: 8px; height: 16px; margin: -5px 0; background-color: @BG@; border: 2px solid @INK_HEX@; }
QListView { background: transparent; border: none; outline: none; }
QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }
QScrollBar::handle:vertical { background: rgba(@INK@, 0.28); min-height: 28px; }
QScrollBar::handle:vertical:hover { background: @ACC@; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QToolTip { background-color: @PT@; color: @TEXT@; border: 1px solid @INK_HEX@; padding: 4px 8px; }
QMenu { background-color: @PT@; color: @TEXT@; border: 2px solid rgba(@INK@, 0.55); padding: 3px; }
QMenu::item { padding: 6px 26px 6px 12px; }
QMenu::item:selected { background-color: @ACC@; color: @BG@; }
QMenu::separator { height: 1px; background: rgba(@INK@, 0.25); margin: 4px 8px; }
)QSS";

QString DashTheme::styleSheet() const
{
    QString hues;
    for (int i = 0; i <= 6; ++i) {
        const QColor c = QColor::fromHslF(qMin(i * 60, 359) / 360.0, 0.75, 0.55);
        if (!hues.isEmpty()) {
            hues += QStringLiteral(", ");
        }
        hues += QStringLiteral("stop:%1 %2").arg(i / 6.0, 0, 'f', 3).arg(c.name());
    }
    QString css = QString::fromLatin1(kStyleTemplate);
    css.replace(QStringLiteral("@INK_HEX@"), ink().name());
    css.replace(QStringLiteral("@INK@"), rgb(ink()));
    css.replace(QStringLiteral("@BG@"), bg().name());
    css.replace(QStringLiteral("@PT@"), panelTop().name());
    css.replace(QStringLiteral("@TEXT@"), text().name());
    css.replace(QStringLiteral("@MUTED@"), muted().name());
    css.replace(QStringLiteral("@DIM@"), dimText().name());
    css.replace(QStringLiteral("@ACCL@"), accentLight().name());
    css.replace(QStringLiteral("@ACC@"), accent().name());
    css.replace(QStringLiteral("@A@"), rgb(accent()));
    css.replace(QStringLiteral("@HUES@"), hues);
    return css;
}

// ---------------------------------------------------------------------------------------------
// Pixel-art boxes: hard edges, one-step stair corners and optional hard shadows
// ---------------------------------------------------------------------------------------------
QPolygonF notchPoly(const QRectF& r, qreal n, qreal borderWidth)
{
    const qreal l = r.left() + borderWidth / 2;
    const qreal t = r.top() + borderWidth / 2;
    const qreal rr = r.right() - borderWidth / 2;
    const qreal b = r.bottom() - borderWidth / 2;
    QPolygonF poly;
    poly << QPointF(l + n, t) << QPointF(rr - n, t) << QPointF(rr - n, t + n) << QPointF(rr, t + n) << QPointF(rr, b - n)
         << QPointF(rr - n, b - n) << QPointF(rr - n, b) << QPointF(l + n, b) << QPointF(l + n, b - n) << QPointF(l, b - n)
         << QPointF(l, t + n) << QPointF(l + n, t + n);
    return poly;
}

void drawPixelBox(QPainter& p, const QRectF& r, qreal notch, const QBrush& fill, const QColor& border, qreal borderWidth)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setBrush(fill);
    if (borderWidth > 0 && border.alpha() > 0) {
        p.setPen(QPen(border, borderWidth, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    } else {
        p.setPen(Qt::NoPen);
    }
    p.drawPolygon(notchPoly(r, notch, borderWidth > 0 ? borderWidth : 0));
    p.restore();
}

// ---------------------------------------------------------------------------------------------
// Vector glyphs, drawn on a 24x24 grid (no image resources needed). Square caps = "technical" look.
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
    Palette,
    Torii
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
    p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.setBrush(Qt::NoBrush);

    switch (glyph) {
        case Glyph::None:
            break;
        case Glyph::Grid:
            for (int row = 0; row < 2; ++row) {
                for (int col = 0; col < 2; ++col) {
                    p.drawRect(QRectF(4 + col * 9.5, 4 + row * 9.5, 6.5, 6.5));
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
            body.addRect(QRectF(4, 9, 12, 11));
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
            p.drawRect(QRectF(8.5, 8.5, 7, 7));
            for (int i = 0; i < 8; ++i) {
                const qreal a = qDegreesToRadians(i * 45.0);
                p.drawLine(QPointF(12 + qCos(a) * 8.0, 12 + qSin(a) * 8.0), QPointF(12 + qCos(a) * 10.2, 12 + qSin(a) * 10.2));
            }
            break;
        case Glyph::Image: {
            p.drawRect(QRectF(3.5, 4.5, 17, 15));
            p.drawRect(QRectF(7.5, 8, 2.5, 2.5));
            QPolygonF hills;
            hills << QPointF(3.5, 17) << QPointF(9, 12.5) << QPointF(14, 16.5) << QPointF(16.5, 14.5) << QPointF(20.5, 18);
            p.drawPolyline(hills);
            break;
        }
        case Glyph::Camera: {
            p.drawRect(QRectF(3, 7.5, 18, 12));
            QPolygonF hump;
            hump << QPointF(8.5, 7.5) << QPointF(10, 4.8) << QPointF(14, 4.8) << QPointF(15.5, 7.5);
            p.drawPolyline(hump);
            p.drawRect(QRectF(9.2, 10.7, 5.6, 5.6));
            break;
        }
        case Glyph::Globe:
            p.drawEllipse(QPointF(12, 12), 9, 9);
            p.drawEllipse(QPointF(12, 12), 4, 9);
            p.drawLine(QPointF(3, 12), QPointF(21, 12));
            break;
        case Glyph::Terminal: {
            p.drawRect(QRectF(3, 4.5, 18, 15));
            QPolygonF chevron;
            chevron << QPointF(7, 9.5) << QPointF(10.5, 12) << QPointF(7, 14.5);
            p.drawPolyline(chevron);
            p.drawLine(QPointF(12.5, 15), QPointF(16.5, 15));
            break;
        }
        case Glyph::Gear:
            p.drawEllipse(QPointF(12, 12), 3.0, 3.0);
            p.drawRect(QRectF(6.5, 6.5, 11, 11));
            for (int i = 0; i < 4; ++i) {
                const qreal a = qDegreesToRadians(i * 90.0);
                p.drawLine(QPointF(12 + qCos(a) * 7.5, 12 + qSin(a) * 7.5), QPointF(12 + qCos(a) * 10.5, 12 + qSin(a) * 10.5));
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
            p.drawRect(QRectF(6.5, 6.5, 11, 11));
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
            p.drawRect(QRectF(3.5, 10.5, 3.5, 3.5));
            p.drawRect(QRectF(10.25, 10.5, 3.5, 3.5));
            p.drawRect(QRectF(17, 10.5, 3.5, 3.5));
            break;
        case Glyph::Palette: {
            p.drawEllipse(QPointF(12, 12), 9, 9);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawRect(QRectF(6.8, 9.3, 2.4, 2.4));
            p.drawRect(QRectF(10.8, 6.3, 2.4, 2.4));
            p.drawRect(QRectF(14.8, 9.3, 2.4, 2.4));
            p.drawRect(QRectF(8, 14, 2.4, 2.4));
            break;
        }
        case Glyph::Torii: {
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawRect(QRectF(2, 4, 20, 3));      // kasagi (top beam)
            p.drawRect(QRectF(4.5, 8.6, 15, 2));  // nuki (lower beam)
            p.drawRect(QRectF(6, 8, 2.4, 13));    // left pillar
            p.drawRect(QRectF(15.6, 8, 2.4, 13)); // right pillar
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
// Custom painted controls. Some are global (not anonymous) because Dashboard.h forward-declares them.
// =============================================================================================

class DashFrame : public QFrame {
   public:
    enum class Kind { Panel, Card, Hero };

    DashFrame(Kind kind, QWidget* parent) : QFrame(parent), m_kind(kind) {}
    void setTitle(const QString& title)
    {
        m_title = title;
        update();
    }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        const DashTheme& t = theme();
        QPainter p(this);
        const QRectF r(rect());
        switch (m_kind) {
            case Kind::Panel: {
                QLinearGradient g(r.topLeft(), r.bottomLeft());
                g.setColorAt(0, t.panelTop());
                g.setColorAt(1, t.panelBottom());
                drawPixelBox(p, r, 6, QBrush(g), withAlpha(t.ink(), 80), 2);
                break;
            }
            case Kind::Card:
                drawPixelBox(p, r, 4, withAlpha(t.ink(), 10), withAlpha(t.ink(), 75), 1);
                break;
            case Kind::Hero: {
                const QRectF box(0, 0, r.width() - 5, r.height() - 5);
                drawPixelBox(p, box.translated(5, 5), 6, QColor(0, 0, 0, 150), QColor(), 0);
                QLinearGradient g(box.topLeft(), box.bottomLeft());
                g.setColorAt(0, withAlpha(t.panelTop(), 238));
                g.setColorAt(1, withAlpha(t.panelBottom(), 238));
                drawPixelBox(p, box, 6, QBrush(g), withAlpha(t.ink(), 225), 2);
                // inverted title bar, like the boxed dialogs of old monochrome games
                p.save();
                QPainterPath clip;
                clip.addPolygon(notchPoly(box, 6, 0));
                p.setClipPath(clip);
                p.fillRect(QRectF(0, 0, box.width(), 24), t.ink());
                p.restore();
                p.setPen(t.bg());
                p.setFont(monoFont(11, true));
                p.drawText(QRectF(12, 0, box.width() - 80, 24), Qt::AlignVCenter | Qt::AlignLeft, m_title);
                for (int i = 0; i < 3; ++i) {
                    p.fillRect(QRectF(box.width() - 16 - i * 11, 9, 6, 6), t.bg());
                }
                break;
            }
        }
    }

   private:
    Kind m_kind;
    QString m_title;
};

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
                setFixedSize(304, 60);
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
                setFixedHeight(28);
                break;
        }
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(140);
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
    void setSwatchIndex(int index)
    {
        m_swatch = index;
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
            return QSize(fontMetrics().horizontalAdvance(text()) + 26, 28);
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
        p.setOpacity(isEnabled() ? 1.0 : 0.38);
        const qreal h = m_hover;
        const bool down = isDown();
        const QRectF r(rect());
        const QColor ink = t.ink();
        const QColor onAccent = t.onAccent();

        switch (m_kind) {
            case Kind::Square: {
                const QColor fill = m_active ? ink : withAlpha(ink, qRound(10 + 26 * h));
                const QColor border = m_active ? ink : withAlpha(ink, qRound(70 + 150 * h));
                drawPixelBox(p, r, 5, down ? withAlpha(ink, 6) : fill, border, 2);
                const qreal g = down ? 18 : 20;
                drawGlyph(p, m_glyph, QRectF(r.center().x() - g / 2, r.center().y() - g / 2, g, g),
                          m_active ? onAccent : mixColors(t.muted(), t.text(), h));
                break;
            }
            case Kind::Round:
            case Kind::Mini: {
                const bool accented = m_accented;
                const QColor fill = accented ? mixColors(t.accent(), t.accentLight(), h * 0.6) : withAlpha(ink, qRound(10 + 24 * h));
                const QColor border = accented ? t.accentLight() : withAlpha(ink, qRound(80 + 140 * h));
                drawPixelBox(p, r, 4, down ? withAlpha(fill, 160) : fill, border, 2);
                const qreal g = r.width() * (m_kind == Kind::Mini ? 0.52 : 0.46);
                drawGlyph(p, m_glyph, QRectF(r.center().x() - g / 2, r.center().y() - g / 2, g, g), accented ? onAccent : mixColors(t.text(), t.accentLight(), h));
                break;
            }
            case Kind::Play: {
                const QRectF box(0, 0, r.width() - 5, r.height() - 5);
                const QRectF top = down ? box.translated(4, 4) : box.translated(-h * 1.0, -h * 1.0);
                if (!down) {
                    drawPixelBox(p, box.translated(5, 5), 6, QColor(0, 0, 0, 170), QColor(), 0);
                }
                drawPixelBox(p, top, 6, mixColors(t.accent(), t.accentLight(), h * 0.7), withAlpha(ink, 235), 2);
                QFont f = monoFont(15, true);
                p.setFont(f);
                const QString label = text().toUpper();
                const int textWidth = QFontMetrics(f).horizontalAdvance(label);
                const qreal total = 20 + 12 + textWidth;
                const qreal x = top.center().x() - total / 2;
                drawGlyph(p, Glyph::Play, QRectF(x, top.center().y() - 10, 20, 20), onAccent);
                p.setPen(onAccent);
                p.drawText(QRectF(x + 32, top.top(), textWidth + 6, top.height()), Qt::AlignVCenter | Qt::AlignLeft, label);
                break;
            }
            case Kind::Bar: {
                drawPixelBox(p, r, 5, down ? withAlpha(ink, 6) : withAlpha(ink, qRound(9 + 14 * h)), withAlpha(ink, qRound(70 + 130 * h)), 2);
                const QRectF badge(r.left() + 10, r.center().y() - 14, 28, 28);
                drawPixelBox(p, badge, 3, t.accent(), t.accentLight(), 2);
                drawGlyph(p, m_glyph, QRectF(badge.center().x() - 7, badge.center().y() - 7, 14, 14), onAccent);
                p.setFont(monoFont(12, true));
                p.setPen(mixColors(t.text(), t.accentLight(), h));
                p.drawText(QRectF(r.left() + 50, r.top(), r.width() - 80, r.height()), Qt::AlignVCenter | Qt::AlignLeft, text().toUpper());
                QPolygonF arrow;
                const qreal ax = r.right() - 20 + 3 * h;
                arrow << QPointF(ax - 3, r.center().y() - 5) << QPointF(ax + 3, r.center().y()) << QPointF(ax - 3, r.center().y() + 5);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setPen(Qt::NoPen);
                p.setBrush(t.muted());
                p.drawPolygon(arrow);
                break;
            }
            case Kind::Chip: {
                const QColor fill = m_active ? t.accent() : withAlpha(ink, qRound(8 + 16 * h));
                const QColor border = m_active ? t.accentLight() : withAlpha(ink, qRound(70 + 120 * h));
                drawPixelBox(p, r, 3, down ? withAlpha(ink, 6) : fill, border, 1);
                p.setFont(monoFont(11, m_active));
                p.setPen(m_active ? onAccent : mixColors(t.text(), t.accentLight(), h));
                p.drawText(r, Qt::AlignCenter, text());
                break;
            }
            case Kind::Header: {
                if (h > 0.01) {
                    drawPixelBox(p, r, 4, withAlpha(ink, qRound(10 * h)), QColor(), 0);
                }
                const QRectF badge(r.left() + 12, r.center().y() - 14, 28, 28);
                drawPixelBox(p, badge, 3, t.accent(), t.accentLight(), 2);
                drawGlyph(p, m_glyph, QRectF(badge.center().x() - 8, badge.center().y() - 8, 16, 16), onAccent);
                p.setFont(monoFont(12, true));
                p.setPen(mixColors(t.text(), t.accentLight(), h));
                p.drawText(QRectF(r.left() + 52, r.top(), r.width() - 90, r.height()), Qt::AlignVCenter | Qt::AlignLeft, text().toUpper());
                p.save();
                p.setRenderHint(QPainter::Antialiasing, true);
                p.translate(r.right() - 22, r.center().y());
                p.rotate(180.0 * m_expand);
                QPolygonF tri;
                tri << QPointF(-5, -3) << QPointF(5, -3) << QPointF(0, 3);
                p.setPen(Qt::NoPen);
                p.setBrush(t.muted());
                p.drawPolygon(tri);
                p.restore();
                break;
            }
            case Kind::Swatch: {
                const auto& pal = kPalettes[qBound(0, m_swatch, kPaletteCount - 1)];
                const bool current = theme().preset == m_swatch;
                const QRectF box = r.adjusted(3 - h * 1.5, 3 - h * 1.5, -3 + h * 1.5, -3 + h * 1.5);
                if (current) {
                    drawPixelBox(p, r.adjusted(1, 1, -1, -1), 3, QColor(), t.text(), 2);
                }
                // two-tone chip: accent over background, so the style is recognisable at a glance
                drawPixelBox(p, box, 2, QColor(pal.bg), QColor(pal.ink), 1);
                p.fillRect(QRectF(box.left() + 2, box.top() + 2, box.width() - 4, (box.height() - 4) / 2 + 1), QColor(pal.accent));
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
    int m_swatch = 0;
    QVariantAnimation* m_anim = nullptr;
};

class DashSwitch : public QAbstractButton {
   public:
    explicit DashSwitch(QWidget* parent = nullptr) : QAbstractButton(parent)
    {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(46, 24);
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(150);
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
    QSize sizeHint() const override { return QSize(46, 24); }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        const DashTheme& t = theme();
        QPainter p(this);
        const QRectF r(rect());
        drawPixelBox(p, r, 3, mixColors(withAlpha(t.ink(), 14), t.accent(), m_pos), mixColors(withAlpha(t.ink(), 110), t.accentLight(), m_pos), 2);
        const qreal d = r.height() - 10;
        const qreal x = r.left() + 5 + (r.width() - d - 10) * m_pos;
        p.fillRect(QRectF(qRound(x), r.top() + 5, d, d), mixColors(t.muted(), t.onAccent(), m_pos));
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
        const qreal d = 34;
        const qreal step = 26;
        const qreal top = (height() - d) / 2;
        const int count = static_cast<int>(m_faces.size());

        for (int i = count - 1; i >= 0; --i) {
            const QRectF box(2 + i * step, top, d, d);
            const bool isDefault = i == m_defaultIndex;
            drawPixelBox(p, box.adjusted(-2, -2, 2, 2), 3, t.panelBottom(), QColor(), 0);
            drawPixelBox(p, box, 3, QColor(0x26, 0x26, 0x2e), isDefault ? t.accentLight() : withAlpha(t.ink(), 120), 2);
            if (!m_faces[i].isNull()) {
                p.save();
                p.setRenderHint(QPainter::SmoothPixmapTransform, false);
                p.drawPixmap(box.adjusted(3, 3, -3, -3).toRect(), m_faces[i]);
                p.restore();
            } else if (!m_names[i].isEmpty()) {
                p.setFont(monoFont(14, true));
                p.setPen(t.text());
                p.drawText(box, Qt::AlignCenter, m_names[i].left(1).toUpper());
            }
        }

        const QRectF plus(2 + count * step + (count > 0 ? 6 : 0), top, d, d);
        p.save();
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(withAlpha(t.ink(), 130), 1, Qt::DashLine));
        p.drawRect(plus.adjusted(0.5, 0.5, -0.5, -0.5));
        p.restore();
        drawGlyph(p, Glyph::Plus, QRectF(plus.center().x() - 8, plus.center().y() - 8, 16, 16), t.muted());
        if (count == 0) {
            p.setFont(monoFont(11, false));
            p.setPen(t.muted());
            p.drawText(QRectF(plus.right() + 10, top, width() - plus.right() - 10, d), Qt::AlignVCenter | Qt::AlignLeft,
                       QStringLiteral(u"A\u00d1ADIR CUENTA"));
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
        setFixedSize(112, 112);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(160);
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
        const qreal lift = -qRound(3 * m_hover);  // stepped little hop on hover
        const QRectF box(8, 12 + lift, 92, 92);
        drawPixelBox(p, box.translated(5, 5 - lift), 6, QColor(0, 0, 0, 140), QColor(), 0);
        drawPixelBox(p, box, 6, withAlpha(t.accent(), 34), mixColors(t.ink(), t.accentLight(), m_hover), 2);
        if (!m_pixmap.isNull()) {
            p.setRenderHint(QPainter::SmoothPixmapTransform, false);
            p.drawPixmap(QRectF(box.center().x() - 30, box.center().y() - 30, 60, 60), m_pixmap, QRectF(m_pixmap.rect()));
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

// The "hanko" seal that stands in for a name: a red stamp with a torii gate.
class DashBrand : public QWidget {
   public:
    explicit DashBrand(QWidget* parent = nullptr) : QWidget(parent) { setFixedSize(46, 46); }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        const DashTheme& t = theme();
        QPainter p(this);
        const QRectF r(rect());
        drawPixelBox(p, r.adjusted(0, 0, -3, -3).translated(3, 3), 5, QColor(0, 0, 0, 130), QColor(), 0);
        drawPixelBox(p, r.adjusted(0, 0, -3, -3), 5, t.accent(), withAlpha(t.ink(), 230), 2);
        drawGlyph(p, Glyph::Torii, QRectF(9, 9, 23, 23), t.bg());
    }
};

class DashWallpaper : public QFrame {
   public:
    explicit DashWallpaper(QWidget* parent = nullptr) : QFrame(parent)
    {
        setObjectName(QStringLiteral("centerPanel"));
        m_timer = new QTimer(this);
        m_timer->setInterval(110);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            if (isVisible() && !hasWallpaper() && theme().animate) {
                ++m_tick;
                update();
            }
        });
        m_timer->start();
    }

    bool hasWallpaper() const { return m_movie || !m_still.isNull(); }

    void invalidateScene()
    {
        m_sceneKey.clear();
        update();
    }

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
        const QRectF r(rect());
        QPainterPath clip;
        clip.addPolygon(notchPoly(r, 6, 0));
        p.setClipPath(clip);

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
            if (t.pixel) {
                const int lw = qMax(1, static_cast<int>(r.width() / kScale));
                const int lh = qMax(1, static_cast<int>(r.height() / kScale));
                const QImage small = frame.copy(source.toRect()).toImage().scaled(QSize(lw, lh), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                p.setRenderHint(QPainter::SmoothPixmapTransform, false);
                p.drawImage(QRect(0, 0, lw * kScale, lh * kScale), small);
            } else {
                p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                p.drawPixmap(r, frame, source);
            }
            p.fillRect(r, QColor(0, 0, 0, qRound(t.dim * 2.55)));
            QLinearGradient vignette(r.topLeft(), r.bottomLeft());
            vignette.setColorAt(0, QColor(0, 0, 0, 50));
            vignette.setColorAt(0.5, QColor(0, 0, 0, 0));
            vignette.setColorAt(1, QColor(0, 0, 0, 120));
            p.fillRect(r, vignette);
        } else {
            const int lw = qMax(8, (rect().width() + kScale - 1) / kScale);
            const int lh = qMax(8, (rect().height() + kScale - 1) / kScale);
            const QString key = QStringLiteral("%1x%2:%3").arg(lw).arg(lh).arg(t.preset);
            if (key != m_sceneKey) {
                buildScene(lw, lh);
                m_sceneKey = key;
            }
            p.setRenderHint(QPainter::SmoothPixmapTransform, false);
            p.drawImage(QRect(0, 0, lw * kScale, lh * kScale), m_scene);
            drawOverlay(p, lw, lh);
            for (int y = 0; y < lh * kScale; y += kScale) {
                p.fillRect(QRect(0, y + kScale - 1, lw * kScale, 1), QColor(0, 0, 0, 34));
            }
        }

        p.setClipping(false);
        drawPixelBox(p, r, 6, QBrush(Qt::NoBrush), withAlpha(t.ink(), 80), 2);
    }

   private:
    static constexpr int kScale = 3;

    void buildScene(int lw, int lh)
    {
        const DashTheme& t = theme();
        const Palette& pal = t.pal();
        m_scene = QImage(lw, lh, QImage::Format_ARGB32);
        m_stars.clear();
        m_windows.clear();
        m_horizon = qRound(lh * 0.70);
        static const int bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };

        auto skyAt = [&](int x, int y) {
            const qreal pos = qMin<qreal>(3.999, static_cast<qreal>(y) / m_horizon * 4.0);
            const int i = static_cast<int>(pos);
            const qreal f = pos - i;
            const bool upper = f * 16.0 > bayer[y & 3][x & 3];
            return QColor(pal.sky[upper ? i + 1 : i]);
        };

        const QColor ground(pal.skyline);
        for (int y = 0; y < lh; ++y) {
            for (int x = 0; x < lw; ++x) {
                m_scene.setPixelColor(x, y, y < m_horizon ? skyAt(x, y) : ground);
            }
        }

        // striped sun, half hidden behind the skyline
        const int cx = static_cast<int>(lw * 0.50);
        const int r = static_cast<int>(qMin(lw, lh) * 0.19);
        const int cy = static_cast<int>(lh * 0.25);
        const QColor sun(pal.sun);
        for (int y = cy - r; y <= cy + r; ++y) {
            for (int x = cx - r; x <= cx + r; ++x) {
                if (x < 0 || y < 0 || x >= lw || y >= m_horizon) {
                    continue;
                }
                if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > r * r) {
                    continue;
                }
                const int rel = y - (cy - r / 3);
                bool cut = false;
                if (rel > 0) {
                    const int thickness = qMin(4, 1 + rel / qMax(1, r / 3));
                    cut = (rel % 6) < thickness;
                }
                m_scene.setPixelColor(x, y, cut ? skyAt(x, y) : sun);
            }
        }

        // skyline
        quint32 seed = 0x9e3779b9u;
        auto rnd = [&seed]() {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            return seed;
        };
        const int towerX = static_cast<int>(lw * 0.12);
        // far skyline: lighter silhouettes behind the main ones, for depth
        {
            const QColor far = mixColors(QColor(pal.sky[4]), QColor(pal.skyline), 0.86);
            int fx = 0;
            while (fx < lw) {
                const int fw = 5 + static_cast<int>(rnd() % 9);
                const int fh = static_cast<int>(lh * 0.10) + static_cast<int>(rnd() % static_cast<quint32>(qMax(2, static_cast<int>(lh * 0.14))));
                for (int by = m_horizon - fh; by < m_horizon; ++by) {
                    for (int bx = fx; bx < qMin(fx + fw, lw); ++bx) {
                        m_scene.setPixelColor(bx, by, far);
                    }
                }
                fx += fw;
            }
        }
        int x = 0;
        while (x < lw) {
            const int w = 4 + static_cast<int>(rnd() % 7);
            const int h = static_cast<int>(lh * 0.05) + static_cast<int>(rnd() % static_cast<quint32>(qMax(2, static_cast<int>(lh * 0.17))));
            const int top = m_horizon - h;
            const bool nearTower = qAbs(x + w / 2 - towerX) < 9;
            for (int by = top; by < m_horizon; ++by) {
                for (int bx = x; bx < qMin(x + w, lw); ++bx) {
                    if (!nearTower) {
                        m_scene.setPixelColor(bx, by, ground);
                    }
                }
            }
            if (!nearTower) {
                for (int wy = top + 2; wy < m_horizon - 1; wy += 3) {
                    for (int wx = x + 1; wx < x + w - 1 && wx < lw; wx += 2) {
                        if (rnd() % 100 < 22) {
                            m_scene.setPixelColor(wx, wy, QColor(pal.window));
                            m_windows.append(QPoint(wx, wy));
                        }
                    }
                }
            }
            x += w + ((rnd() % 3) == 0 ? 1 : 0);
        }

        // Tokyo Tower
        const int towerH = static_cast<int>(lh * 0.44);
        const int topY = m_horizon - towerH;
        QPainter ip(&m_scene);
        ip.setRenderHint(QPainter::Antialiasing, false);
        ip.setPen(ground);
        ip.setBrush(ground);
        QPolygon body;
        body << QPoint(towerX - 9, m_horizon) << QPoint(towerX - 3, m_horizon - static_cast<int>(towerH * 0.42))
             << QPoint(towerX - 2, m_horizon - static_cast<int>(towerH * 0.72)) << QPoint(towerX - 1, topY + 6) << QPoint(towerX, topY)
             << QPoint(towerX + 1, topY + 6) << QPoint(towerX + 2, m_horizon - static_cast<int>(towerH * 0.72))
             << QPoint(towerX + 3, m_horizon - static_cast<int>(towerH * 0.42)) << QPoint(towerX + 9, m_horizon);
        ip.drawPolygon(body);
        ip.drawRect(towerX - 5, m_horizon - static_cast<int>(towerH * 0.42) - 1, 11, 2);
        ip.drawRect(towerX - 3, m_horizon - static_cast<int>(towerH * 0.72), 7, 1);
        ip.end();
        for (int sy = topY + 8; sy < m_horizon - 4; sy += 6) {
            if (towerX >= 0 && towerX < lw) {
                m_scene.setPixelColor(towerX, sy, QColor(pal.accent));
            }
        }
        m_beacon = QPoint(towerX, topY - 1);

        // stars
        for (int i = 0; i < 30; ++i) {
            m_stars.append(QPoint(static_cast<int>(rnd() % static_cast<quint32>(lw)), static_cast<int>(rnd() % static_cast<quint32>(qMax(2, static_cast<int>(m_horizon * 0.55))))));
        }
    }

    void drawOverlay(QPainter& p, int lw, int lh)
    {
        const DashTheme& t = theme();
        const Palette& pal = t.pal();
        const int tk = t.animate ? m_tick : 0;
        auto px = [&](int x, int y, int w, int h, const QColor& c) { p.fillRect(QRect(x * kScale, y * kScale, w * kScale, h * kScale), c); };

        for (int i = 0; i < m_stars.size(); ++i) {
            const int phase = (tk / 2 + i * 5) % 20;
            const int alpha = phase < 2 ? 255 : (phase < 10 ? 170 : 90);
            px(m_stars[i].x(), m_stars[i].y(), 1, 1, withAlpha(QColor(pal.ink), alpha));
        }
        for (int i = 0; i < m_windows.size(); ++i) {
            if (((tk / 9) + i * 7) % 13 == 0) {
                px(m_windows[i].x(), m_windows[i].y(), 1, 1, QColor(pal.skyline));
            }
        }
        if ((tk / 6) % 2 == 0) {
            px(m_beacon.x(), m_beacon.y(), 1, 2, QColor(pal.accent));
        }

        switch (pal.weather) {
            case WeatherRain:
                for (int i = 0; i < 44; ++i) {
                    const int x = (i * 53) % lw;
                    const int y = ((i * 29) % m_horizon + tk * 3) % m_horizon;
                    px(x, y, 1, 3, withAlpha(t.secondary(), 170));
                }
                break;
            case WeatherPetals:
                for (int i = 0; i < 24; ++i) {
                    const int x = (i * 47 + tk / 2 + static_cast<int>(4 * qSin((tk + i * 9) / 10.0)) + lw) % lw;
                    const int y = (i * 31 + tk) % (lh + 6);
                    px(x, y, 2, 1, withAlpha(t.accent(), 220));
                    px(x + 1, y + 1, 1, 1, withAlpha(t.accentLight(), 200));
                }
                break;
            case WeatherMist:
                for (int i = 0; i < 5; ++i) {
                    const int x = ((i * 61 + tk / 3) % (lw + 40)) - 20;
                    px(x, static_cast<int>(m_horizon * (0.25 + 0.12 * i)), 16 + i * 4, 2, withAlpha(QColor(pal.ink), 32));
                }
                px(0, m_horizon - 8, lw, 6, withAlpha(QColor(pal.ink), 14));
                break;
            default:
                break;
        }
    }

    QString m_path;
    QMovie* m_movie = nullptr;
    QPixmap m_still;
    QImage m_scene;
    QString m_sceneKey;
    QVector<QPoint> m_stars;
    QVector<QPoint> m_windows;
    QPoint m_beacon;
    int m_horizon = 0;
    QTimer* m_timer = nullptr;
    int m_tick = 0;
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
        const QRectF r = QRectF(option.rect).adjusted(2, 3, -2, -3);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hot = option.state & QStyle::State_MouseOver;

        if (selected) {
            drawPixelBox(*p, r, 4, withAlpha(t.accent(), 46), t.accent(), 2);
            p->fillRect(QRectF(r.left() + 2, r.top() + 6, 3, r.height() - 12), t.accentLight());
        } else {
            drawPixelBox(*p, r, 4, withAlpha(t.ink(), hot ? 16 : 7), withAlpha(t.ink(), hot ? 110 : 45), 1);
        }

        const QRectF badge(r.left() + 12, r.center().y() - 18, 36, 36);
        drawPixelBox(*p, badge, 3, withAlpha(t.bg(), 200), selected ? t.accentLight() : withAlpha(t.ink(), 110), 2);
        const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        if (!icon.isNull()) {
            const QPixmap pm = icon.pixmap(QSize(24, 24));
            p->setRenderHint(QPainter::SmoothPixmapTransform, false);
            p->drawPixmap(QRectF(badge.center().x() - 12, badge.center().y() - 12, 24, 24), pm, QRectF(pm.rect()));
        }

        auto* inst = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
        const bool running = inst && inst->isRunning();

        const qreal textLeft = badge.right() + 12;
        const qreal pillWidth = running ? 74 : 0;
        const qreal textWidth = r.right() - textLeft - 12 - (running ? pillWidth + 6 : 0);

        QFont titleFont = monoFont(13, true);
        p->setFont(titleFont);
        p->setPen(selected ? t.accentLight() : t.text());
        const QString name = QFontMetrics(titleFont).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, static_cast<int>(textWidth));
        p->drawText(QRectF(textLeft, r.top() + 9, textWidth, 20), Qt::AlignVCenter | Qt::AlignLeft, name);

        QFont subFont = monoFont(11, false);
        p->setFont(subFont);
        p->setPen(t.dimText());
        QString sub = describeInstance(inst);
        if (sub.isEmpty()) {
            sub = index.data(InstanceList::GroupRole).toString();
        }
        p->drawText(QRectF(textLeft, r.top() + 30, textWidth, 18), Qt::AlignVCenter | Qt::AlignLeft,
                    QFontMetrics(subFont).elidedText(sub, Qt::ElideRight, static_cast<int>(textWidth)));

        if (running) {
            const QRectF pill(r.right() - pillWidth - 10, r.center().y() - 10, pillWidth, 20);
            drawPixelBox(*p, pill, 2, t.accent(), QColor(), 0);
            p->setPen(t.onAccent());
            p->setFont(monoFont(10, true));
            p->drawText(pill, Qt::AlignCenter, QStringLiteral("EN JUEGO"));
        }
        p->restore();
    }
};

QLabel* makeLabel(const QString& text, const char* objectName, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QString::fromLatin1(objectName));
    return label;
}

QFrame* makeDivider(QWidget* parent)
{
    auto* frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("divider"));
    return frame;
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
    auto* sidebar = new DashFrame(DashFrame::Kind::Panel, this);
    sidebar->setFixedWidth(76);
    auto* side = new QVBoxLayout(sidebar);
    side->setContentsMargins(14, 14, 14, 14);
    side->setSpacing(10);

    side->addWidget(new DashBrand(sidebar), 0, Qt::AlignHCenter);
    side->addWidget(makeDivider(sidebar));

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
    side->addWidget(makeDivider(sidebar));

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
    pl->setContentsMargins(22, 20, 22, 16);
    pl->setSpacing(10);

    auto* top = new QHBoxLayout();
    m_countChip = makeLabel(QString(), "chip", m_centerPanel);
    top->addWidget(m_countChip);
    top->addStretch(1);
    pl->addLayout(top);
    pl->addStretch(1);

    // hero card: a boxed dialog with an inverted title bar
    auto* hero = new DashFrame(DashFrame::Kind::Hero, m_centerPanel);
    hero->setTitle(QStringLiteral("> INSTANCIA"));
    hero->setMinimumWidth(450);
    hero->setMaximumWidth(570);
    auto* hl = new QVBoxLayout(hero);
    hl->setContentsMargins(30, 36, 35, 30);
    hl->setSpacing(4);

    m_heroContent = new QWidget(hero);
    auto* hc = new QVBoxLayout(m_heroContent);
    hc->setContentsMargins(0, 0, 0, 0);
    hc->setSpacing(2);
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
    hl->addWidget(m_heroContent);
    hl->addSpacing(12);

    auto* playRow = new QHBoxLayout();
    playRow->setSpacing(10);
    playRow->addStretch(1);
    m_playButton = new DashButton(Glyph::Play, DashButton::Kind::Play, hero);
    if (a.launch) {
        m_playButton->setDefaultAction(a.launch);
    }
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
    connect(m_heroAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        if (m_heroFade) {
            m_heroFade->setOpacity(value.toReal());
        }
    });
    connect(m_heroAnim, &QVariantAnimation::finished, this, [this]() {
        if (m_heroFade && m_heroFade->opacity() >= 0.999) {
            m_heroContent->setGraphicsEffect(nullptr);  // effects are expensive: drop it once the fade is over
            m_heroFade = nullptr;
        }
    });

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
    auto* card1 = new DashFrame(DashFrame::Kind::Card, this);
    card1->setFixedHeight(136);
    auto* c1 = new QVBoxLayout(card1);
    c1->setContentsMargins(18, 14, 18, 12);
    c1->setSpacing(10);
    auto* c1head = new QHBoxLayout();
    c1head->addWidget(makeLabel(QStringLiteral(u"\u25a0 DATOS DE LA INSTANCIA"), "cardTitle", card1));
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
    c1->addWidget(makeDivider(card1));
    auto* c1foot = new QHBoxLayout();
    c1foot->addWidget(makeLabel(QStringLiteral("TIEMPO JUGADO"), "statusText", card1));
    c1foot->addStretch(1);
    m_cardPlayed = makeLabel(QString(), "statusText", card1);
    c1foot->addWidget(m_cardPlayed);
    c1->addLayout(c1foot);
    center->addWidget(card1);
    root->addLayout(center, 1);

    // ------------------------------------------------------------------ right panel
    auto* rightPanel = new DashFrame(DashFrame::Kind::Panel, this);
    rightPanel->setFixedWidth(344);
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
    rl->addWidget(makeDivider(rightPanel));

    // appearance drawer
    auto* drawer = new DashFrame(DashFrame::Kind::Card, rightPanel);
    auto* dl = new QVBoxLayout(drawer);
    dl->setContentsMargins(0, 0, 0, 0);
    dl->setSpacing(0);
    auto* drawerHeader = new DashButton(Glyph::Palette, DashButton::Kind::Header, drawer);
    drawerHeader->setText(QStringLiteral("Apariencia"));
    dl->addWidget(drawerHeader);

    m_drawerBody = new QWidget(drawer);
    auto* bl = new QVBoxLayout(m_drawerBody);
    bl->setContentsMargins(16, 2, 16, 16);
    bl->setSpacing(8);

    bl->addWidget(makeLabel(QStringLiteral("ESTILO"), "sectionTitle", m_drawerBody));
    auto* styleGrid = new QVBoxLayout();
    styleGrid->setSpacing(6);
    QHBoxLayout* styleRow = nullptr;
    for (int i = 0; i < kPaletteCount; ++i) {
        if (i % 3 == 0) {
            styleRow = new QHBoxLayout();
            styleRow->setSpacing(6);
            styleGrid->addLayout(styleRow);
        }
        auto* chip = new DashButton(Glyph::None, DashButton::Kind::Chip, m_drawerBody);
        chip->setText(QString::fromUtf16(kPalettes[i].name));
        chip->setToolTip(QString::fromUtf16(kPalettes[i].name));
        connect(chip, &QToolButton::clicked, this, [this, i]() {
            theme().preset = i;
            theme().hueOverride = -1;
            syncThemeControls();
            scheduleThemeApply();
        });
        styleRow->addWidget(chip);
        m_styleChips.append(chip);
    }
    styleRow->addStretch(1);
    bl->addLayout(styleGrid);

    bl->addSpacing(2);
    bl->addWidget(makeLabel(QStringLiteral("ACENTO"), "sectionTitle", m_drawerBody));
    auto* hueRow = new QHBoxLayout();
    hueRow->setSpacing(8);
    m_hueSlider = new QSlider(Qt::Horizontal, m_drawerBody);
    m_hueSlider->setObjectName(QStringLiteral("hueSlider"));
    m_hueSlider->setRange(0, 359);
    m_hueSlider->setFixedHeight(24);
    hueRow->addWidget(m_hueSlider, 1);
    auto* autoChip = new DashButton(Glyph::None, DashButton::Kind::Chip, m_drawerBody);
    autoChip->setText(QStringLiteral("Auto"));
    autoChip->setToolTip(QStringLiteral("Volver al color original del estilo"));
    hueRow->addWidget(autoChip);
    bl->addLayout(hueRow);
    connect(autoChip, &QToolButton::clicked, this, [this]() {
        theme().hueOverride = -1;
        syncThemeControls();
        scheduleThemeApply();
    });
    connect(m_hueSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_syncingTheme) {
            return;
        }
        theme().hueOverride = value;
        scheduleThemeApply();
    });

    bl->addSpacing(2);
    bl->addWidget(makeLabel(QStringLiteral("FONDO"), "sectionTitle", m_drawerBody));
    auto* wallRow = new QHBoxLayout();
    wallRow->setSpacing(8);
    auto* chooseButton = new DashButton(Glyph::None, DashButton::Kind::Chip, m_drawerBody);
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
    dimRow->addWidget(makeLabel(QStringLiteral("Oscurecer"), "statusText", m_drawerBody));
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

    auto addSwitchRow = [&](DashSwitch*& target, const QString& label) {
        auto* row = new QHBoxLayout();
        target = new DashSwitch(m_drawerBody);
        row->addWidget(target);
        row->addWidget(makeLabel(label, "statusText", m_drawerBody));
        row->addStretch(1);
        bl->addLayout(row);
    };
    addSwitchRow(m_onlyThisInstance, QStringLiteral("Solo para esta instancia"));
    addSwitchRow(m_pixelSwitch, QStringLiteral("Filtro pixel en mi fondo"));
    addSwitchRow(m_animSwitch, QStringLiteral("Animar el paisaje"));
    connect(m_onlyThisInstance, &QAbstractButton::toggled, this, [this]() { syncThemeControls(); });
    connect(m_pixelSwitch, &QAbstractButton::toggled, this, [this](bool on) {
        if (m_syncingTheme) {
            return;
        }
        theme().pixel = on;
        scheduleThemeApply();
    });
    connect(m_animSwitch, &QAbstractButton::toggled, this, [this](bool on) {
        if (m_syncingTheme) {
            return;
        }
        theme().animate = on;
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
    m_countChip->setText(QStringLiteral("<span style='color:%3'>\u25a0</span>&nbsp;&nbsp;%1 %2")
                             .arg(n)
                             .arg(n == 1 ? QStringLiteral("INSTANCIA") : QStringLiteral("INSTANCIAS"))
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
    m_centerPanel->invalidateScene();
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
    m_hueSlider->setValue(t.hueOverride >= 0 ? t.hueOverride : qMax(0, static_cast<int>(qRound(t.accent().hslHueF() * 360.0)) % 360));
    m_dimSlider->setValue(t.dim);
    m_pixelSwitch->setChecked(t.pixel);
    m_animSwitch->setChecked(t.animate);
    m_syncingTheme = false;
    for (int i = 0; i < m_styleChips.size(); ++i) {
        m_styleChips[i]->setActive(i == t.preset);
    }

    QString text;
    const QString own = t.instanceWallpapers.value(m_lastInstanceId);
    if (m_onlyThisInstance->isChecked()) {
        text = own.isEmpty() ? QStringLiteral("Esta instancia usa el fondo general") : QStringLiteral("Esta instancia: ") + stripStamp(own);
    } else {
        text = t.wallpaper.isEmpty() ? QStringLiteral("Sin fondo propio (paisaje del estilo)") : QStringLiteral("General: ") + stripStamp(t.wallpaper);
    }
    m_wallpaperName->setText(text);
    // findChildren<> needs a Q_OBJECT class on newer Qt versions, so refresh the drawer through plain QWidget
    const auto drawerWidgets = m_drawerBody->findChildren<QWidget*>();
    for (QWidget* widget : drawerWidgets) {
        widget->update();
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

    const QString dotVersion = QStringLiteral("<span style='color:%1'>\u25a0</span>&nbsp;&nbsp;%2");
    const QString c1 = t.secondary().name();
    const QString c2 = t.accentLight().name();
    const QString c3 = t.tertiary().name();

    if (!has) {
        m_heroName->setText(QStringLiteral("Sin instancia seleccionada"));
        m_heroSub->setText(QStringLiteral("Elige una instancia de la lista"));
        m_heroIcon->setPixmap(glyphPixmap(Glyph::Cube, t.accentLight(), 56, dpr));
        m_chipVersion->setText(dotVersion.arg(c1, QStringLiteral("\u2014")));
        m_chipLoader->setText(dotVersion.arg(c2, QStringLiteral("\u2014")));
        m_chipRam->setText(dotVersion.arg(c3, QStringLiteral("\u2014")));
        m_cardBadge->setText(QStringLiteral(u"SIN SELECCI\u00d3N"));
        m_cardPlayed->setText(QStringLiteral("\u2014"));
        m_statusText->setText(QStringLiteral("> sin instancia seleccionada"));
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
        m_chipVersion->setText(dotVersion.arg(c1, info.version.isEmpty() ? QStringLiteral("Minecraft") : info.version));
        m_chipLoader->setText(dotVersion.arg(c2, info.loader.isEmpty() ? QStringLiteral("Vanilla") : info.loader));
        m_chipRam->setText(dotVersion.arg(c3, QStringLiteral("%1 MiB").arg(inst->settings()->get(QStringLiteral("MaxMemAlloc")).toInt())));
        const qint64 played = inst->totalTimePlayed();
        m_cardPlayed->setText(played > 0 ? Time::prettifyDuration(played) : QStringLiteral(u"A\u00daN SIN JUGAR"));

        if (inst->isRunning()) {
            m_cardBadge->setText(QStringLiteral("EN JUEGO"));
            m_statusText->setText(QStringLiteral("> en ejecuci\u00f3n: ") + inst->name());
            m_activity->setRange(0, 0);
        } else {
            m_cardBadge->setText(QStringLiteral("LISTO"));
            m_statusText->setText(QStringLiteral("> ") + inst->getStatusbarDescription());
            m_activity->setRange(0, 1);
            m_activity->setValue(0);
        }
    }

    if (instanceChanged) {
        applyWallpaper();
        syncThemeControls();
        m_heroAnim->stop();
        m_heroFade = new QGraphicsOpacityEffect(m_heroContent);
        m_heroFade->setOpacity(0.0);
        m_heroContent->setGraphicsEffect(m_heroFade);
        m_heroAnim->start();
    }

    const int total = APPLICATION->instances()->getTotalPlayTime();
    m_playtime->setText(total > 0 ? QStringLiteral("TOTAL: %1").arg(Time::prettifyDuration(total).toUpper()) : QString());

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
