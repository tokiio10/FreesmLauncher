// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Emerald Glass dashboard - a full visual replacement for the main window contents.
 *
 *  This widget only *presents* things: every button is bound to an existing QAction of the
 *  main window, and the instance list shares the model and selection of the original view.
 *  No launcher logic lives here.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QAbstractItemModel>
#include <QAction>
#include <QItemSelectionModel>
#include <QList>
#include <QUrl>
#include <QWidget>

#include <functional>

class BaseInstance;
class QDragEnterEvent;
class QDropEvent;
class QFrame;
class QLabel;
class QListView;
class QShowEvent;
class QProgressBar;
class QSlider;

class AvatarStrip;
class SwitchToggle;

/** The existing main window actions the dashboard is allowed to trigger. */
struct DashboardActions {
    QAction* launch = nullptr;
    QAction* kill = nullptr;
    QAction* edit = nullptr;
    QAction* settings = nullptr;
    QAction* addInstance = nullptr;
    QAction* folders = nullptr;   // carries the folders menu
    QAction* help = nullptr;      // carries the help menu
    QAction* accounts = nullptr;  // carries the accounts menu
    QAction* checkUpdate = nullptr;
    QAction* moreNews = nullptr;
    QAction* viewLog = nullptr;
};

class Dashboard : public QWidget {
    Q_OBJECT
   public:
    Dashboard(QAbstractItemModel* model, QItemSelectionModel* selection, const DashboardActions& actions, QWidget* parent = nullptr);

    /** How the dashboard finds out which instance is selected. */
    void setSelectedInstanceGetter(std::function<BaseInstance*()> getter);

    /** Re-reads the selected instance and updates the hero card, chips and running state. */
    void refresh();

    /** Starts inline renaming of the current instance in the list. */
    void editCurrent();

   signals:
    void instanceActivated(const QModelIndex& index);
    void instanceContextMenuRequested(const QPoint& globalPos);
    void urlsDropped(QList<QUrl> urls);

   protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void showEvent(QShowEvent* event) override;

   private:
    void buildUi(const DashboardActions& actions);
    void openInstancePage(const QString& page);
    void syncMemorySlider();
    void updateInstanceCount();

    QAbstractItemModel* m_model = nullptr;
    QItemSelectionModel* m_selection = nullptr;
    std::function<BaseInstance*()> m_selected;
    DashboardActions m_actions;

    QListView* m_list = nullptr;
    QLabel* m_countChip = nullptr;
    QLabel* m_heroName = nullptr;
    QLabel* m_heroSub = nullptr;
    QLabel* m_heroIcon = nullptr;
    QLabel* m_statusText = nullptr;
    QLabel* m_playtime = nullptr;
    QProgressBar* m_activity = nullptr;
    QLabel* m_chipVersion = nullptr;
    QLabel* m_chipLoader = nullptr;
    QLabel* m_chipRam = nullptr;
    QLabel* m_cardBadge = nullptr;
    QLabel* m_cardPlayed = nullptr;
    QLabel* m_accountBadge = nullptr;
    QLabel* m_ramValue = nullptr;
    QLabel* m_ramMin = nullptr;
    QLabel* m_ramMax = nullptr;
    QSlider* m_ramSlider = nullptr;
    SwitchToggle* m_consoleToggle = nullptr;
    AvatarStrip* m_avatars = nullptr;
    QList<QWidget*> m_pageButtons;
    bool m_syncingSlider = false;
};
