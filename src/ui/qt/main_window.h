#pragma once

#include <QMainWindow>
#include <QTableWidget>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>
#include <QMenuBar>
#include <QMenu>
#include <QHeaderView>
#include <memory>
#include "../../platform/app_scanner.h"
#include "../../platform/process_engine.h"
#include "../../platform/leftover_cleaner.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onRefresh();
    void onFilterChanged(const QString &text);
    void onSwitchToDesktopApps();
    void onSwitchToStoreApps();
    void onSwitchToProcesses();
    void onUpdateProcesses();
    void onTableDoubleClicked(int row, int column);
    void onContextMenu(const QPoint &pos);
    void onHeaderClicked(int logicalIndex);
    void onEndTask();

private:
    enum class ViewMode { DesktopApps, StoreApps, Processes };
    ViewMode m_currentMode = ViewMode::DesktopApps;

    std::unique_ptr<ZeroCore::IAppScanner> m_scanner;
    std::unique_ptr<ZeroCore::IProcessEngine> m_procEngine;
    std::unique_ptr<ZeroCore::ILeftoverCleaner> m_cleaner;

    std::vector<ZeroCore::AppInfo> m_apps;
    std::vector<ZeroCore::ProcessInfo> m_procs;
    ZeroCore::SystemStats m_sysStats;

    QTableWidget *m_table = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTimer *m_timer = nullptr;

    QAction *m_actionDesktop = nullptr;
    QAction *m_actionStore = nullptr;
    QAction *m_actionProcesses = nullptr;
    QAction *m_actionDark = nullptr;
    QAction *m_actionLight = nullptr;
    QAction *m_actionCompact = nullptr;
    QAction *m_actionNormal = nullptr;
    QAction *m_actionLarge = nullptr;

    bool m_isDarkMode = true;
    int m_rowSizeMode = 0; // Default: Compact Cards
    int m_appSortColumn = 0;
    bool m_appSortAscending = true;

    void setupUi();
    void applyDarkTheme();
    void applyLightTheme();
    void setRowSizeMode(int mode);
    void sortApps();
    void loadApps(bool store = false);
    void loadProcesses();
    void updateTableColumns();
    void updateStatusBarText();
};
