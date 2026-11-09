#include "main_window.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QString::fromWCharArray(L"Zero 0.0.2-beta (Cross-Platform)"));
    resize(1120, 700);

    m_scanner = ZeroCore::IAppScanner::CreatePlatformScanner();
    m_procEngine = ZeroCore::IProcessEngine::CreatePlatformEngine();
    m_cleaner = ZeroCore::ILeftoverCleaner::CreatePlatformCleaner();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::onUpdateProcesses);

    setupUi();
    applyDarkTheme();

    onSwitchToDesktopApps();
}

void MainWindow::setupUi() {
    QIcon icon("src/app.png");
    if (icon.isNull()) icon = QIcon("app.png");
    if (icon.isNull()) icon = QIcon(QCoreApplication::applicationDirPath() + "/app.png");
    if (icon.isNull()) icon = QIcon(QCoreApplication::applicationDirPath() + "/../Resources/app.png");
    if (!icon.isNull()) setWindowIcon(icon);

    auto *central = new QWidget(this);
    auto *vbox = new QVBoxLayout(central);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    // Table
    m_table = new QTableWidget(this);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &MainWindow::onTableDoubleClicked);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &MainWindow::onContextMenu);
    connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked, this, &MainWindow::onHeaderClicked);
    vbox->addWidget(m_table, 1);

    // Bottom Bar
    auto *bottomBar = new QWidget(this);
    bottomBar->setFixedHeight(38);
    auto *bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(14, 0, 14, 0);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #8C8C8C; font-size: 13px;");
    bottomLayout->addWidget(m_statusLabel, 1);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QString::fromWCharArray(L"Type to find a program"));
    m_searchEdit->setFixedWidth(280);
    m_searchEdit->setFixedHeight(26);
    m_searchEdit->setStyleSheet(
        "QLineEdit { background: #262626; color: #E0E0E0; border: 1px solid #343434; border-radius: 13px; padding: 0 12px; font-size: 13px; }"
        "QLineEdit:focus { border: 1px solid #E0562E; }"
    );
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    bottomLayout->addWidget(m_searchEdit, 0);

    vbox->addWidget(bottomBar, 0);
    setCentralWidget(central);

    // Menus
    auto *mb = menuBar();

    // File
    auto *menuFile = mb->addMenu(tr("&File"));
    menuFile->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);

    // Action
    auto *menuAction = mb->addMenu(tr("&Action"));
    menuAction->addAction(tr("&Find..."), [this]() {
        if (m_searchEdit) {
            m_searchEdit->setFocus();
            m_searchEdit->selectAll();
        }
    }, QKeySequence::Find);
    menuAction->addAction(tr("&Refresh"), this, &MainWindow::onRefresh, QKeySequence(Qt::CTRL | Qt::Key_R));

    // Processes
    auto *menuProcesses = mb->addMenu(tr("&Processes"));
    menuProcesses->addAction(tr("&View Processes"), this, &MainWindow::onSwitchToProcesses, QKeySequence(Qt::CTRL | Qt::Key_3));
    menuProcesses->addAction(tr("&End Task"), this, &MainWindow::onEndTask, QKeySequence(Qt::Key_Delete));

    // View
    auto *menuView = mb->addMenu(tr("&View"));
    m_actionDesktop = menuView->addAction(tr("&Desktop Apps"), this, &MainWindow::onSwitchToDesktopApps, QKeySequence(Qt::CTRL | Qt::Key_1));
    m_actionDesktop->setCheckable(true);
    m_actionStore = menuView->addAction(tr("&System / Store Apps"), this, &MainWindow::onSwitchToStoreApps, QKeySequence(Qt::CTRL | Qt::Key_2));
    m_actionStore->setCheckable(true);
    m_actionProcesses = menuView->addAction(tr("&Processes"), this, &MainWindow::onSwitchToProcesses, QKeySequence(Qt::CTRL | Qt::Key_3));
    m_actionProcesses->setCheckable(true);

    // Settings
    auto *menuSettings = mb->addMenu(tr("&Settings"));
    m_actionDark = menuSettings->addAction(tr("&Dark Mode"), [this]() { applyDarkTheme(); });
    m_actionDark->setCheckable(true);
    m_actionDark->setChecked(true);
    m_actionLight = menuSettings->addAction(tr("&Light Mode"), [this]() { applyLightTheme(); });
    m_actionLight->setCheckable(true);
    m_actionLight->setChecked(false);

    menuSettings->addSeparator();

    m_actionCompact = menuSettings->addAction(tr("&Compact Cards (Small)"), [this]() { setRowSizeMode(0); });
    m_actionCompact->setCheckable(true);
    m_actionCompact->setChecked(true);
    m_actionNormal = menuSettings->addAction(tr("&Normal Cards (Medium)"), [this]() { setRowSizeMode(1); });
    m_actionNormal->setCheckable(true);
    m_actionNormal->setChecked(false);
    m_actionLarge = menuSettings->addAction(tr("&Large Cards (Expanded)"), [this]() { setRowSizeMode(2); });
    m_actionLarge->setCheckable(true);

    // Help
    auto *menuHelp = mb->addMenu(tr("&Help"));
    menuHelp->addAction(tr("&Check for Updates..."), [this]() {
        QDesktopServices::openUrl(QUrl("https://github.com/hamzabellouch/zero/releases"));
    });
    menuHelp->addSeparator();
    menuHelp->addAction(tr("&About Zero..."), [this]() {
        QMessageBox::about(this, tr("About Zero"), tr("Zero 0.0.2-beta\n\nCross-Platform Standalone Uninstaller and Task Manager.\nC++ & Qt."));
    });
}

void MainWindow::applyDarkTheme() {
    m_isDarkMode = true;
    if (m_actionDark) m_actionDark->setChecked(true);
    if (m_actionLight) m_actionLight->setChecked(false);

    qApp->setStyle("Fusion");
    QPalette p;
    p.setColor(QPalette::Window, QColor(26, 26, 26));
    p.setColor(QPalette::WindowText, QColor(224, 224, 224));
    p.setColor(QPalette::Base, QColor(25, 25, 25));
    p.setColor(QPalette::AlternateBase, QColor(33, 33, 33));
    p.setColor(QPalette::Text, QColor(224, 224, 224));
    p.setColor(QPalette::Highlight, QColor(0, 120, 215));
    p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    qApp->setPalette(p);

    m_table->setStyleSheet(
        "QTableWidget { background: #191919; alternate-background-color: #212121; color: #E0E0E0; border: none; selection-background-color: #0078D7; selection-color: #FFFFFF; font-size: 13px; }"
        "QHeaderView::section { background: #1A1A1A; color: #C8C8C8; border: none; border-bottom: 1px solid #2D2D2D; border-right: 1px solid #2D2D2D; padding: 6px 12px; font-weight: 600; font-size: 12px; }"
    );
    if (m_searchEdit) {
        m_searchEdit->setStyleSheet(
            "QLineEdit { background: #262626; color: #E0E0E0; border: 1px solid #343434; border-radius: 13px; padding: 0 12px; font-size: 13px; }"
            "QLineEdit:focus { border: 1px solid #0078D7; }"
        );
    }
}

void MainWindow::applyLightTheme() {
    m_isDarkMode = false;
    if (m_actionDark) m_actionDark->setChecked(false);
    if (m_actionLight) m_actionLight->setChecked(true);

    qApp->setStyle("Fusion");
    QPalette p;
    p.setColor(QPalette::Window, QColor(242, 244, 247));
    p.setColor(QPalette::WindowText, QColor(32, 32, 32));
    p.setColor(QPalette::Base, QColor(255, 255, 255));
    p.setColor(QPalette::AlternateBase, QColor(246, 248, 250));
    p.setColor(QPalette::Text, QColor(32, 32, 32));
    p.setColor(QPalette::Highlight, QColor(0, 120, 215));
    p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    qApp->setPalette(p);

    m_table->setStyleSheet(
        "QTableWidget { background: #FFFFFF; alternate-background-color: #F6F8FA; color: #202020; border: none; selection-background-color: #0078D7; selection-color: #FFFFFF; font-size: 13px; }"
        "QHeaderView::section { background: #F2F4F7; color: #404040; border: none; border-bottom: 1px solid #DADEFF; border-right: 1px solid #DADEFF; padding: 6px 12px; font-weight: 600; font-size: 12px; }"
    );
    if (m_searchEdit) {
        m_searchEdit->setStyleSheet(
            "QLineEdit { background: #FFFFFF; color: #202020; border: 1px solid #D0D0D0; border-radius: 13px; padding: 0 12px; font-size: 13px; }"
            "QLineEdit:focus { border: 1px solid #0078D7; }"
        );
    }
}

void MainWindow::setRowSizeMode(int mode) {
    m_rowSizeMode = mode;
    if (m_actionCompact) m_actionCompact->setChecked(mode == 0);
    if (m_actionNormal) m_actionNormal->setChecked(mode == 1);
    if (m_actionLarge) m_actionLarge->setChecked(mode == 2);

    int rowH = 32;
    if (mode == 0) rowH = 25;
    else if (mode == 2) rowH = 46;

    m_table->verticalHeader()->setDefaultSectionSize(rowH);
}

void MainWindow::onSwitchToDesktopApps() {
    m_currentMode = ViewMode::DesktopApps;
    m_timer->stop();
    m_actionDesktop->setChecked(true);
    m_actionStore->setChecked(false);
    m_actionProcesses->setChecked(false);
    m_searchEdit->setPlaceholderText(QString::fromWCharArray(L"Type to find a program"));
    loadApps(false);
}

void MainWindow::onSwitchToStoreApps() {
    m_currentMode = ViewMode::StoreApps;
    m_timer->stop();
    m_actionDesktop->setChecked(false);
    m_actionStore->setChecked(true);
    m_actionProcesses->setChecked(false);
    m_searchEdit->setPlaceholderText(QString::fromWCharArray(L"Type to find a package"));
    loadApps(true);
}

void MainWindow::onSwitchToProcesses() {
    m_currentMode = ViewMode::Processes;
    m_actionDesktop->setChecked(false);
    m_actionStore->setChecked(false);
    m_actionProcesses->setChecked(true);
    m_searchEdit->setPlaceholderText(QString::fromWCharArray(L"Type to find a process"));
    loadProcesses();
    m_timer->start(1500);
}

void MainWindow::sortApps() {
    std::sort(m_apps.begin(), m_apps.end(), [this](const ZeroCore::AppInfo &a, const ZeroCore::AppInfo &b) {
        if (m_appSortColumn == 0) {
            int cmp = _wcsicmp(a.displayName.c_str(), b.displayName.c_str());
            return m_appSortAscending ? (cmp < 0) : (cmp > 0);
        }
        if (m_appSortColumn == 1) {
            if (a.estimatedSizeBytes != b.estimatedSizeBytes) {
                if (a.estimatedSizeBytes == 0) return false;
                if (b.estimatedSizeBytes == 0) return true;
                return m_appSortAscending ? (a.estimatedSizeBytes < b.estimatedSizeBytes) : (a.estimatedSizeBytes > b.estimatedSizeBytes);
            }
            return _wcsicmp(a.displayName.c_str(), b.displayName.c_str()) < 0;
        }
        if (m_appSortColumn == 2) {
            if (a.installDateSortKey != b.installDateSortKey) {
                if (a.installDateSortKey == 0) return false;
                if (b.installDateSortKey == 0) return true;
                return m_appSortAscending ? (a.installDateSortKey < b.installDateSortKey) : (a.installDateSortKey > b.installDateSortKey);
            }
            return _wcsicmp(a.displayName.c_str(), b.displayName.c_str()) < 0;
        }
        return false;
    });
}

void MainWindow::onHeaderClicked(int logicalIndex) {
    if (m_currentMode == ViewMode::Processes) return;

    if (logicalIndex == 0) {
        if (m_appSortColumn == 0) {
            m_appSortAscending = !m_appSortAscending;
        } else {
            m_appSortColumn = 0;
            m_appSortAscending = true;
        }
        sortApps();
        loadApps(m_currentMode == ViewMode::StoreApps);
        return;
    }

    if (logicalIndex == 1) {
        QMenu menu(this);
        auto *actDesc = menu.addAction(tr("Sort Largest to Smallest"));
        auto *actAsc = menu.addAction(tr("Sort Smallest to Largest"));
        actDesc->setCheckable(true);
        actAsc->setCheckable(true);
        if (m_appSortColumn == 1) {
            if (!m_appSortAscending) actDesc->setChecked(true);
            else actAsc->setChecked(true);
        }
        QPoint pos = QCursor::pos();
        QAction *chosen = menu.exec(pos);
        if (chosen == actDesc) {
            m_appSortColumn = 1;
            m_appSortAscending = false;
            sortApps();
            loadApps(m_currentMode == ViewMode::StoreApps);
        } else if (chosen == actAsc) {
            m_appSortColumn = 1;
            m_appSortAscending = true;
            sortApps();
            loadApps(m_currentMode == ViewMode::StoreApps);
        }
        return;
    }

    if (logicalIndex == 2) {
        QMenu menu(this);
        auto *actDesc = menu.addAction(tr("Sort Newest to Oldest"));
        auto *actAsc = menu.addAction(tr("Sort Oldest to Newest"));
        actDesc->setCheckable(true);
        actAsc->setCheckable(true);
        if (m_appSortColumn == 2) {
            if (!m_appSortAscending) actDesc->setChecked(true);
            else actAsc->setChecked(true);
        }
        QPoint pos = QCursor::pos();
        QAction *chosen = menu.exec(pos);
        if (chosen == actDesc) {
            m_appSortColumn = 2;
            m_appSortAscending = false;
            sortApps();
            loadApps(m_currentMode == ViewMode::StoreApps);
        } else if (chosen == actAsc) {
            m_appSortColumn = 2;
            m_appSortAscending = true;
            sortApps();
            loadApps(m_currentMode == ViewMode::StoreApps);
        }
        return;
    }
}

void MainWindow::loadApps(bool store) {
    if (!m_scanner) return;
    m_apps = store ? m_scanner->EnumerateStorePackages() : m_scanner->EnumerateDesktopApps();
    sortApps();

    m_table->clear();
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({ tr("Program Name"), tr("Size"), tr("Installed On") });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    m_table->setRowCount(static_cast<int>(m_apps.size()));
    for (size_t i = 0; i < m_apps.size(); ++i) {
        const auto &app = m_apps[i];
        auto *item0 = new QTableWidgetItem(QString::fromStdWString(app.displayName));
        auto *item1 = new QTableWidgetItem(QString::fromStdWString(app.sizeFormatted.empty() ? L"-" : app.sizeFormatted));
        auto *item2 = new QTableWidgetItem(QString::fromStdWString(app.version.empty() ? app.installDate : app.version));
        item1->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        item2->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(static_cast<int>(i), 0, item0);
        m_table->setItem(static_cast<int>(i), 1, item1);
        m_table->setItem(static_cast<int>(i), 2, item2);
    }

    onFilterChanged(m_searchEdit->text());
    updateStatusBarText();
}

void MainWindow::loadProcesses() {
    if (!m_procEngine) return;
    m_procEngine->UpdateSnapshot();
    m_procs = m_procEngine->GetProcesses();
    m_sysStats = m_procEngine->GetSystemStats();

    m_table->clear();
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({ tr("Name"), tr("CPU"), tr("Memory"), tr("Disk"), tr("Network") });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c <= 4; ++c) {
        m_table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }

    m_table->setRowCount(static_cast<int>(m_procs.size()));
    for (size_t i = 0; i < m_procs.size(); ++i) {
        const auto &proc = m_procs[i];
        QString name = QString::fromStdWString(proc.name);
        if (proc.instanceCount > 1) {
            name += QString(" (%1)").arg(proc.instanceCount);
        }
        auto *item0 = new QTableWidgetItem(name);
        auto *item1 = new QTableWidgetItem(QString::fromStdWString(proc.cpuText));
        auto *item2 = new QTableWidgetItem(QString::fromStdWString(proc.memoryText));
        auto *item3 = new QTableWidgetItem(QString::fromStdWString(proc.diskText));
        auto *item4 = new QTableWidgetItem(QString::fromStdWString(proc.networkText));

        for (int c = 1; c <= 4; ++c) {
            auto *it = (c == 1) ? item1 : ((c == 2) ? item2 : ((c == 3) ? item3 : item4));
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }

        m_table->setItem(static_cast<int>(i), 0, item0);
        m_table->setItem(static_cast<int>(i), 1, item1);
        m_table->setItem(static_cast<int>(i), 2, item2);
        m_table->setItem(static_cast<int>(i), 3, item3);
        m_table->setItem(static_cast<int>(i), 4, item4);
    }

    onFilterChanged(m_searchEdit->text());
    updateStatusBarText();
}

void MainWindow::onUpdateProcesses() {
    if (m_currentMode != ViewMode::Processes || !m_procEngine) return;
    m_procEngine->UpdateSnapshot();
    m_procs = m_procEngine->GetProcesses();
    m_sysStats = m_procEngine->GetSystemStats();

    if (m_table->rowCount() != static_cast<int>(m_procs.size())) {
        loadProcesses();
        return;
    }

    for (size_t i = 0; i < m_procs.size(); ++i) {
        const auto &proc = m_procs[i];
        if (auto *it = m_table->item(static_cast<int>(i), 1)) it->setText(QString::fromStdWString(proc.cpuText));
        if (auto *it = m_table->item(static_cast<int>(i), 2)) it->setText(QString::fromStdWString(proc.memoryText));
        if (auto *it = m_table->item(static_cast<int>(i), 3)) it->setText(QString::fromStdWString(proc.diskText));
        if (auto *it = m_table->item(static_cast<int>(i), 4)) it->setText(QString::fromStdWString(proc.networkText));
    }
    updateStatusBarText();
}

void MainWindow::onFilterChanged(const QString &text) {
    QString query = text.trimmed().toLower();
    for (int r = 0; r < m_table->rowCount(); ++r) {
        bool match = false;
        if (query.isEmpty()) {
            match = true;
        } else {
            auto *item = m_table->item(r, 0);
            if (item && item->text().toLower().contains(query)) {
                match = true;
            }
        }
        m_table->setRowHidden(r, !match);
    }
    updateStatusBarText();
}

void MainWindow::updateStatusBarText() {
    int visibleCount = 0;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (!m_table->isRowHidden(r)) visibleCount++;
    }

    if (m_currentMode == ViewMode::Processes) {
        m_statusLabel->setText(tr("%1 processes (CPU: %2, Memory: %3 / %4)")
            .arg(visibleCount)
            .arg(QString::fromStdWString(ZeroCore::IProcessEngine::FormatPercent(m_sysStats.totalCpuPercent)))
            .arg(QString::fromStdWString(ZeroCore::IProcessEngine::FormatMemory(m_sysStats.usedPhysMemBytes)))
            .arg(QString::fromStdWString(ZeroCore::IProcessEngine::FormatMemory(m_sysStats.totalPhysMemBytes)))
        );
    } else {
        m_statusLabel->setText(tr("%1 programs").arg(visibleCount));
    }
}

void MainWindow::onRefresh() {
    if (m_currentMode == ViewMode::Processes) {
        loadProcesses();
    } else {
        loadApps(m_currentMode == ViewMode::StoreApps);
    }
}

void MainWindow::onEndTask() {
    int row = m_table->currentRow();
    if (row < 0 || row >= static_cast<int>(m_procs.size())) return;
    const auto &proc = m_procs[row];
    if (QMessageBox::question(this, tr("End Task"), tr("Are you sure you want to terminate %1?").arg(QString::fromStdWString(proc.name))) == QMessageBox::Yes) {
        m_procEngine->TerminateProcessGroup(proc);
        loadProcesses();
    }
}

void MainWindow::onTableDoubleClicked(int row, int) {
    if (m_currentMode == ViewMode::Processes) {
        // Open file folder if path available
        if (row >= 0 && row < static_cast<int>(m_procs.size())) {
            QString path = QString::fromStdWString(m_procs[row].fullPath);
            if (!path.isEmpty()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            }
        }
    } else {
        if (row >= 0 && row < static_cast<int>(m_apps.size())) {
            const auto &app = m_apps[row];
            if (QMessageBox::question(this, tr("Uninstall"), tr("Run uninstaller for %1?").arg(QString::fromStdWString(app.displayName))) == QMessageBox::Yes) {
                std::string err;
                m_scanner->LaunchUninstall(app, err);
                onRefresh();
            }
        }
    }
}

void MainWindow::onContextMenu(const QPoint &pos) {
    QMenu menu(this);
    if (m_currentMode == ViewMode::Processes) {
        menu.addAction(tr("&End Task\tDel"), this, &MainWindow::onEndTask);
        menu.addSeparator();
        menu.addAction(tr("&Refresh\tCtrl+R"), this, &MainWindow::onRefresh);
    } else {
        menu.addAction(tr("&Uninstall"), [this]() {
            onTableDoubleClicked(m_table->currentRow(), 0);
        });
        menu.addSeparator();
        menu.addAction(tr("&Refresh\tCtrl+R"), this, &MainWindow::onRefresh);
    }
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}
