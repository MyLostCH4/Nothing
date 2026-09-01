#include "mainwindow.h"

#include "datastore.h"
#include "trendchart.h"
#include "worknoteeditor.h"

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QProcess>
#include <QPixmap>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QTime>
#include <QTimeEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QLabel *label(const QString &text, const char *objectName = nullptr)
{
    auto *result = new QLabel(text);
    if (objectName)
        result->setObjectName(QString::fromLatin1(objectName));
    return result;
}

QFrame *card()
{
    auto *result = new QFrame;
    result->setObjectName(QStringLiteral("card"));
    return result;
}

QWidget *metricWidget(const QString &name, const QString &value)
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(18, 12, 18, 12);
    layout->setSpacing(6);

    auto *nameLabel = label(name, "metricLabel");
    auto *valueLabel = label(value, "metricValue");
    nameLabel->setAlignment(Qt::AlignCenter);
    valueLabel->setAlignment(Qt::AlignCenter);
    layout->addStretch();
    layout->addWidget(nameLabel);
    layout->addWidget(valueLabel);
    layout->addStretch();
    return widget;
}

void prepareTable(QTableWidget *table)
{
    table->setAlternatingRowColors(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setShowGrid(true);
}

void prependRow(QTableWidget *table, const QStringList &values, qint64 recordId = 0)
{
    table->insertRow(0);
    auto *recordItem = new QTableWidgetItem;
    recordItem->setData(Qt::UserRole, recordId);
    table->setVerticalHeaderItem(0, recordItem);
    for (int column = 0; column < values.size(); ++column)
        table->setItem(0, column, new QTableWidgetItem(values.at(column)));
}

qint64 tableRecordId(QTableWidget *table, int row)
{
    const auto *item = table->verticalHeaderItem(row);
    return item ? item->data(Qt::UserRole).toLongLong() : 0;
}

QWidget *pageHeader(const QString &title)
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(label(title, "pageTitle"));
    return widget;
}

QDateEdit *dateEditor()
{
    auto *editor = new QDateEdit(QDate::currentDate());
    editor->setCalendarPopup(true);
    editor->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    return editor;
}

QDoubleSpinBox *moneyEditor()
{
    auto *editor = new QDoubleSpinBox;
    editor->setRange(0.0, 100000000.0);
    editor->setDecimals(2);
    editor->setPrefix(QStringLiteral("¥ "));
    editor->setSingleStep(10.0);
    return editor;
}

QString csvCell(QString value)
{
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    if (value.contains(QLatin1Char(',')) || value.contains(QLatin1Char('"'))
        || value.contains(QLatin1Char('\n')) || value.contains(QLatin1Char('\r'))) {
        value = QLatin1Char('"') + value + QLatin1Char('"');
    }
    return value;
}

QString moneyText(double value)
{
    return QStringLiteral("¥ %1").arg(value, 0, 'f', 2);
}

QString storedText(QString value)
{
    return value.isEmpty() ? QStringLiteral("") : value;
}

QDate nextRecurringDate(QDate date, const QString &cycle, const QDate &today)
{
    while (date.isValid() && date <= today) {
        if (cycle == QStringLiteral("每季度"))
            date = date.addMonths(3);
        else if (cycle == QStringLiteral("每年"))
            date = date.addYears(1);
        else
            date = date.addMonths(1);
    }
    return date;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Nothing"));
    resize(1420, 880);
    setMinimumSize(1120, 720);

    m_store = std::make_unique<DataStore>();
    if (m_store->initialize()) {
        QFile::remove(m_store->databasePath() + QStringLiteral(".error.log"));
        m_store->processRecurringExpenses(QDate::currentDate());
        loadStoredState();
    } else {
        QFile errorLog(m_store->databasePath() + QStringLiteral(".error.log"));
        if (errorLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&errorLog);
            stream << QDateTime::currentDateTime().toString(Qt::ISODate) << '\n'
                   << m_store->lastError() << '\n';
        }
        QMessageBox::critical(this, QStringLiteral("数据库初始化失败"),
                              QStringLiteral("程序无法启用数据持久化：\n%1")
                                  .arg(m_store->lastError()));
    }

    auto *central = new QWidget;
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_pages = new QStackedWidget;
    m_pages->addWidget(createOverviewPage());
    m_pages->addWidget(createMoneyPage(QStringLiteral("收入记录"),
                                       {QStringLiteral("工资"), QStringLiteral("奖金"),
                                        QStringLiteral("报销"), QStringLiteral("其他")}, true));
    m_pages->addWidget(createMoneyPage(QStringLiteral("消费记录"),
                                       {QStringLiteral("简餐"), QStringLiteral("下馆子"),
                                        QStringLiteral("下厨"), QStringLiteral("交通"),
                                        QStringLiteral("运动补给"), QStringLiteral("零食"),
                                        QStringLiteral("医疗支出"), QStringLiteral("购物"),
                                        QStringLiteral("日用品"), QStringLiteral("试验垫付")}, false));
    m_pages->addWidget(createRecurringPage());
    m_pages->addWidget(createHuabeiPage());
    m_pages->addWidget(createVaultPage());
    m_pages->addWidget(createBodyPage());
    m_pages->addWidget(createResearchPage());
    refreshOverview();

    root->addWidget(createSidebar());
    root->addWidget(m_pages, 1);
    setCentralWidget(central);

    auto *recurringTimer = new QTimer(this);
    recurringTimer->setInterval(60 * 60 * 1000);
    connect(recurringTimer, &QTimer::timeout, this, [this] {
        if (!m_store || !m_store->isReady())
            return;
        const int processed = m_store->processRecurringExpenses(QDate::currentDate());
        if (processed <= 0)
            return;
        loadStoredState();
        if (m_expensePie)
            m_expensePie->setCategoryValues(m_expenseCategories);
        if (m_expenseTable) {
            const auto rows = m_store->queryRows(QStringLiteral(
                "SELECT id, date, amount, category, note FROM expense ORDER BY id DESC LIMIT ?"),
                {processed});
            for (int i = rows.size() - 1; i >= 0; --i) {
                const auto &row = rows.at(i);
                prependRow(m_expenseTable,
                           {row.at(1).toString(), moneyText(row.at(2).toDouble()),
                            row.at(3).toString(), row.at(4).toString()}, row.at(0).toLongLong());
            }
        }
        refreshOverview();
    });
    recurringTimer->start();

    qApp->setStyleSheet(QStringLiteral(R"(
        * {
            color: #080808;
            font-family: "Microsoft YaHei UI";
        }
        QMainWindow, QWidget#contentPage, QStackedWidget {
            background: #ffffff;
        }
        QWidget#sidebar {
            background: #fbfbfc;
            border-right: 1px solid #d9dde3;
        }
        QLabel#brand {
            font-size: 17px;
            font-weight: 700;
            padding: 8px 12px 18px 16px;
        }
        QPushButton#navButton {
            background: transparent;
            border: none;
            border-left: 3px solid transparent;
            border-radius: 0;
            padding: 12px 16px;
            text-align: left;
            font-size: 14px;
        }
        QPushButton#navButton:hover {
            background: #f1f3f6;
        }
        QPushButton#navButton:checked {
            background: #f3f6fb;
            border-left: 3px solid #165fd0;
            font-weight: 700;
        }
        QLabel#pageTitle {
            font-size: 25px;
            font-weight: 800;
        }
        QLabel#subtitle {
            color: #111111;
            font-size: 11px;
        }
        QLabel#sectionTitle {
            font-size: 16px;
            font-weight: 700;
        }
        QLabel#metricLabel {
            font-size: 12px;
            font-weight: 700;
        }
        QLabel#metricValue {
            font-size: 22px;
            font-weight: 800;
        }
        QLabel#bodyName {
            font-size: 13px;
            font-weight: 700;
        }
        QLabel#bodyValue {
            font-size: 26px;
            font-weight: 800;
        }
        QLabel#bodyChange {
            font-size: 14px;
            font-weight: 700;
        }
        QFrame#card {
            background: #ffffff;
            border: 1px solid #d6dae0;
            border-radius: 4px;
        }
        QFrame#separator {
            background: #d6dae0;
            border: none;
        }
        QDateEdit, QDateTimeEdit, QDoubleSpinBox, QComboBox, QLineEdit {
            background: #ffffff;
            border: 1px solid #cbd0d8;
            border-radius: 3px;
            padding: 7px 9px;
            min-height: 20px;
        }
        QComboBox QAbstractItemView {
            background-color: #ffffff;
            color: #080808;
            border: 1px solid #aeb5bf;
            selection-background-color: #dce8fa;
            selection-color: #080808;
            outline: 0;
        }
        QComboBox QAbstractItemView::item {
            min-height: 28px;
            padding: 3px 8px;
        }
        QComboBox QAbstractItemView::item:hover {
            background-color: #edf3fc;
            color: #080808;
        }
        QCalendarWidget {
            background-color: #ffffff;
            color: #080808;
        }
        QCalendarWidget QWidget#qt_calendar_navigationbar {
            background-color: #f3f5f7;
            border-bottom: 1px solid #d6dae0;
        }
        QCalendarWidget QToolButton {
            background-color: #f3f5f7;
            color: #080808;
            border: none;
            border-radius: 2px;
            padding: 5px;
            font-weight: 700;
        }
        QCalendarWidget QToolButton:hover {
            background-color: #e4ebf5;
        }
        QCalendarWidget QMenu {
            background-color: #ffffff;
            color: #080808;
            border: 1px solid #aeb5bf;
        }
        QCalendarWidget QMenu::item:selected {
            background-color: #dce8fa;
            color: #080808;
        }
        QCalendarWidget QSpinBox {
            background-color: #ffffff;
            color: #080808;
            selection-background-color: #dce8fa;
            selection-color: #080808;
        }
        QCalendarWidget QAbstractItemView:enabled {
            background-color: #ffffff;
            color: #080808;
            selection-background-color: #dce8fa;
            selection-color: #080808;
            outline: 0;
        }
        QPushButton#primaryButton {
            background: #eaf1fc;
            border: 1px solid #165fd0;
            border-radius: 3px;
            padding: 8px 18px;
            font-weight: 700;
        }
        QPushButton#primaryButton:hover {
            background: #dbe8fb;
        }
        QPushButton#utilityButton {
            background: transparent;
            border: 1px solid #cbd0d8;
            border-radius: 3px;
            padding: 8px 12px;
            margin-left: 14px;
            margin-right: 14px;
            text-align: center;
            font-weight: 700;
        }
        QPushButton#utilityButton:hover {
            background: #eef1f5;
        }
        QTableWidget {
            background: #ffffff;
            alternate-background-color: #ffffff;
            border: 1px solid #d6dae0;
            border-radius: 3px;
            gridline-color: #e1e4e8;
        }
        QHeaderView::section {
            background: #f5f6f8;
            color: #080808;
            border: none;
            border-right: 1px solid #d6dae0;
            border-bottom: 1px solid #d6dae0;
            padding: 8px;
            font-weight: 700;
        }
        QTableWidget::item {
            padding: 7px;
        }
    )"));
}

MainWindow::~MainWindow() = default;

QMainWindow *createMainWindow()
{
    return new MainWindow;
}

QWidget *MainWindow::createSidebar()
{
    auto *sidebar = new QWidget;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(210);
    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(0, 20, 0, 20);
    layout->setSpacing(3);

    layout->addWidget(label(QStringLiteral("Nothing"), "brand"));

    m_navigation = new QButtonGroup(this);
    m_navigation->setExclusive(true);
    const QStringList items{
        QStringLiteral("概览"), QStringLiteral("收入记录"), QStringLiteral("消费记录"),
        QStringLiteral("周期消费"), QStringLiteral("花呗"), QStringLiteral("金库"),
        QStringLiteral("身体记录"), QStringLiteral("work")};

    for (int index = 0; index < items.size(); ++index) {
        auto *button = new QPushButton(items.at(index));
        button->setObjectName(QStringLiteral("navButton"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedHeight(48);
        m_navigation->addButton(button, index);
        layout->addWidget(button);
        if (index == 0)
            button->setChecked(true);
    }
    layout->addSpacing(35);
    auto *avatar = new QLabel;
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(176, 176);

    // Compose the backdrop and transparent icon into one pixmap.  Drawing them
    // separately through QLabel style sheets can hide the foreground pixmap on
    // some Windows/Qt style combinations.
    QPixmap avatarImage(176, 176);
    avatarImage.fill(Qt::transparent);
    {
        QPainter painter(&avatarImage);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor(QStringLiteral("#3b424c")), 2));
        painter.setBrush(QColor(QStringLiteral("#20242a")));
        painter.drawEllipse(QRectF(2, 2, 172, 172));

        const QPixmap icon(QStringLiteral(":/icons/app_icon.png"));
        if (!icon.isNull()) {
            const QPixmap scaledIcon = icon.scaled(
                156, 156, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            const QPoint topLeft((avatarImage.width() - scaledIcon.width()) / 2,
                                 (avatarImage.height() - scaledIcon.height()) / 2);
            painter.drawPixmap(topLeft, scaledIcon);
        }
    }
    avatar->setPixmap(avatarImage);
    layout->addWidget(avatar, 0, Qt::AlignHCenter);
    layout->addStretch();

    auto *utilitySeparator = new QFrame;
    utilitySeparator->setObjectName(QStringLiteral("separator"));
    utilitySeparator->setFixedHeight(1);
    layout->addWidget(utilitySeparator);

    auto *exportButton = new QPushButton(QStringLiteral("导出数据"));
    exportButton->setObjectName(QStringLiteral("utilityButton"));
    exportButton->setCursor(Qt::PointingHandCursor);
    auto *importButton = new QPushButton(QStringLiteral("导入备份"));
    importButton->setObjectName(QStringLiteral("utilityButton"));
    importButton->setCursor(Qt::PointingHandCursor);
    auto *clearButton = new QPushButton(QStringLiteral("清除现有数据"));
    clearButton->setObjectName(QStringLiteral("utilityButton"));
    clearButton->setCursor(Qt::PointingHandCursor);
    layout->addWidget(exportButton);
    layout->addWidget(importButton);
    layout->addWidget(clearButton);

    connect(m_navigation, &QButtonGroup::idClicked, m_pages, &QStackedWidget::setCurrentIndex);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportAllData);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::importAllData);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::clearAllData);
    return sidebar;
}

QWidget *MainWindow::createOverviewPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("contentPage"));
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(12);

    auto *header = new QHBoxLayout;
    header->addWidget(label(QStringLiteral("个人数据概览"), "pageTitle"));
    header->addStretch();
    header->addWidget(label(QStringLiteral("显示范围"), "metricLabel"));
    auto *range = new QComboBox;
    range->addItem(QStringLiteral("近 1 周"), 7);
    range->addItem(QStringLiteral("近 2 周"), 14);
    range->addItem(QStringLiteral("近 1 个月"), 30);
    range->addItem(QStringLiteral("近 2 个月"), 60);
    range->addItem(QStringLiteral("近 3 个月"), 90);
    range->setCurrentIndex(2);
    range->setMinimumWidth(130);
    header->addWidget(range);
    root->addLayout(header);

    auto *summary = card();
    summary->setFixedHeight(102);
    auto *summaryLayout = new QHBoxLayout(summary);
    summaryLayout->setContentsMargins(0, 0, 0, 0);
    summaryLayout->setSpacing(0);
    const QList<QPair<QString, QString>> metrics{
        {QStringLiteral("体重"), QStringLiteral("—")},
        {QStringLiteral("本月收入"), QStringLiteral("¥ 0.00")},
        {QStringLiteral("本月支出"), QStringLiteral("¥ 0.00")},
        {QStringLiteral("本月结余"), QStringLiteral("¥ 0.00")},
        {QStringLiteral("花呗欠款"), QStringLiteral("¥ 0.00")}
    };
    for (int i = 0; i < metrics.size(); ++i) {
        if (i > 0) {
            auto *separator = new QFrame;
            separator->setObjectName(QStringLiteral("separator"));
            separator->setFixedWidth(1);
            separator->setMaximumHeight(54);
            summaryLayout->addWidget(separator);
        }
        auto *metricPanel = metricWidget(metrics.at(i).first, metrics.at(i).second);
        if (auto *valueLabel = metricPanel->findChild<QLabel *>(QStringLiteral("metricValue"))) {
            valueLabel->setProperty("clearableMetric", true);
            switch (i) {
            case 0: m_overviewWeight = valueLabel; break;
            case 1: m_overviewIncome = valueLabel; break;
            case 2: m_overviewExpense = valueLabel; break;
            case 3: m_overviewBalance = valueLabel; break;
            case 4: m_overviewHuabei = valueLabel; break;
            default: break;
            }
        }
        summaryLayout->addWidget(metricPanel, 1);
    }
    root->addWidget(summary);

    m_liquidChart = new DataLineChart(DataLineChart::Series::LiquidFunds);
    m_vaultChart = new DataLineChart(DataLineChart::Series::VaultFunds);
    m_weightChart = new DataLineChart(DataLineChart::Series::Weight);
    m_expensePie = new ExpensePieChart;
    m_activityChart = new DailyMetricChart(DailyMetricChart::Style::Bars,
                                           QStringLiteral("运动时间（分钟）"));
    m_workHoursChart = new DailyMetricChart(DailyMetricChart::Style::Line,
                                            QStringLiteral("工作时长（小时）"));
    const auto restoreChart = [](DataLineChart *chart, double currentValue,
                                 const QMap<QDate, double> &storedHistory) {
        chart->clearData();
        if (storedHistory.isEmpty()) {
            chart->setCurrentValue(currentValue);
            return;
        }
        chart->setDatedValues(storedHistory);
    };
    restoreChart(m_liquidChart, m_liquidFunds, m_storedLiquidHistory);
    restoreChart(m_vaultChart, m_vaultBalance, m_storedVaultHistory);
    restoreChart(m_weightChart, m_latestWeight, m_storedWeightHistory);
    m_expensePie->setCategoryValues(m_expenseCategories);
    refreshDailyCharts();

    auto makeChartCard = [](const QString &title, QWidget *chart) {
        auto *frame = card();
        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(14, 10, 10, 8);
        layout->setSpacing(2);
        layout->addWidget(label(title, "sectionTitle"));
        layout->addWidget(chart, 1);
        return frame;
    };

    auto *charts = new QGridLayout;
    charts->setContentsMargins(0, 0, 0, 0);
    charts->setHorizontalSpacing(12);
    charts->setVerticalSpacing(12);
    charts->addWidget(makeChartCard(QStringLiteral("总流动资金（不含金库）"), m_liquidChart), 0, 0);
    charts->addWidget(makeChartCard(QStringLiteral("金库资金"), m_vaultChart), 0, 1);
    charts->addWidget(makeChartCard(QStringLiteral("体重"), m_weightChart), 1, 0);
    charts->addWidget(makeChartCard(QStringLiteral("本月支出分布"), m_expensePie), 1, 1);
    charts->addWidget(makeChartCard(QStringLiteral("运动情况"), m_activityChart), 2, 0);
    charts->addWidget(makeChartCard(QStringLiteral("工作时长曲线"), m_workHoursChart), 2, 1);
    charts->setColumnStretch(0, 1);
    charts->setColumnStretch(1, 1);
    charts->setRowStretch(0, 1);
    charts->setRowStretch(1, 1);
    charts->setRowStretch(2, 1);
    root->addLayout(charts, 1);

    const auto updateRanges = [this, range] {
        const int days = range->currentData().toInt();
        m_liquidChart->setRangeDays(days);
        m_vaultChart->setRangeDays(days);
        m_weightChart->setRangeDays(days);
        m_overviewRangeDays = days;
        refreshDailyCharts();
    };
    connect(range, &QComboBox::currentIndexChanged, page, updateRanges);
    updateRanges();

    auto *workBrief = card();
    workBrief->setFixedHeight(68);
    auto *workLayout = new QHBoxLayout(workBrief);
    workLayout->setContentsMargins(16, 6, 16, 6);
    workLayout->setSpacing(12);
    workLayout->addWidget(label(QStringLiteral("work"), "sectionTitle"));
    const QStringList workSections{QStringLiteral("理论"), QStringLiteral("试验"),
                                   QStringLiteral("小组工作")};
    for (int i = 0; i < workSections.size(); ++i) {
        if (i > 0) {
            auto *separator = new QFrame;
            separator->setObjectName(QStringLiteral("separator"));
            separator->setFixedWidth(1);
            separator->setMaximumHeight(42);
            workLayout->addWidget(separator);
        }
        auto *section = new QWidget;
        section->setMinimumWidth(0);
        section->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        auto *sectionLayout = new QHBoxLayout(section);
        sectionLayout->setContentsMargins(8, 2, 8, 2);
        sectionLayout->setSpacing(10);
        auto *sectionName = label(workSections.at(i), "sectionTitle");
        sectionName->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        sectionLayout->addWidget(sectionName);
        auto *summary = label(m_latestWorkSummaries.value(workSections.at(i),
                                                          QStringLiteral("暂无记录")));
        summary->setProperty("clearableResearch", true);
        summary->setWordWrap(false);
        summary->setMinimumWidth(0);
        summary->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        m_workOverviewLabels[workSections.at(i)] = summary;
        sectionLayout->addWidget(summary, 1);
        workLayout->addWidget(section, 1);
    }
    root->addWidget(workBrief);

    return page;
}

QWidget *MainWindow::createMoneyPage(const QString &title, const QStringList &categories, bool income)
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("contentPage"));
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(30, 24, 30, 24);
    root->setSpacing(18);
    root->addWidget(pageHeader(title));

    auto *form = card();
    auto *formLayout = new QHBoxLayout(form);
    formLayout->setContentsMargins(16, 14, 16, 14);
    formLayout->setSpacing(12);
    auto *date = dateEditor();
    auto *amount = moneyEditor();
    auto *category = new QComboBox;
    category->addItems(categories);
    auto *note = new QLineEdit;
    note->setPlaceholderText(QStringLiteral("备注"));
    auto *add = new QPushButton(QStringLiteral("添加记录"));
    add->setObjectName(QStringLiteral("primaryButton"));
    formLayout->addWidget(date);
    formLayout->addWidget(amount);
    formLayout->addWidget(category);
    formLayout->addWidget(note, 1);
    formLayout->addWidget(add);
    root->addWidget(form);

    auto *table = new QTableWidget(0, 4);
    m_huabeiTable = table;
    table->setProperty("exportName", title);
    table->setHorizontalHeaderLabels({QStringLiteral("日期"), QStringLiteral("金额"),
                                      QStringLiteral("分类"), QStringLiteral("备注")});
    prepareTable(table);
    if (!income)
        m_expenseTable = table;
    const QString sourceTable = income ? QStringLiteral("income") : QStringLiteral("expense");
    enableRowDeletion(table, sourceTable);
    if (m_store && m_store->isReady()) {
        const auto rows = m_store->queryRows(
            QStringLiteral("SELECT id, date, amount, category, note FROM %1 ORDER BY id ASC")
                .arg(sourceTable));
        for (const auto &row : rows) {
            prependRow(table, {row.at(1).toString(), moneyText(row.at(2).toDouble()),
                               row.at(3).toString(), row.at(4).toString()}, row.at(0).toLongLong());
        }
    }
    root->addWidget(table, 1);

    connect(add, &QPushButton::clicked, page, [this, date, amount, category, note, table, income] {
        if (amount->value() <= 0.0)
            return;
        const double addedAmount = amount->value();
        const QString dateText = date->date().toString(QStringLiteral("yyyy-MM-dd"));
        const QString categoryText = category->currentText();
        const QString noteText = storedText(note->text().trimmed());
        const double newLiquidFunds = m_liquidFunds + (income ? addedAmount : -addedAmount);
        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        qint64 recordId = 0;
        if (!runStorageTransaction([this, income, dateText, addedAmount, categoryText, noteText,
                                    newLiquidFunds, timestamp, &recordId] {
                const QString sql = income
                    ? QStringLiteral("INSERT INTO income(date, amount, category, note) VALUES(?, ?, ?, ?)")
                    : QStringLiteral("INSERT INTO expense(date, amount, category, note, payment) "
                                     "VALUES(?, ?, ?, ?, '流动资金')");
                if (!m_store->execute(sql, {dateText, addedAmount, categoryText, noteText}))
                    return false;
                recordId = m_store->lastInsertId();
                return m_store->setState(QStringLiteral("liquid_funds"), newLiquidFunds)
                    && m_store->appendSnapshot(QStringLiteral("liquid"), newLiquidFunds, timestamp);
            }))
            return;

        prependRow(table, {dateText, moneyText(addedAmount), categoryText, noteText}, recordId);
        if (income) {
            m_totalIncome += addedAmount;
            m_liquidFunds += addedAmount;
        } else {
            m_totalExpense += addedAmount;
            m_liquidFunds -= addedAmount;
            m_expenseCategories[categoryText] += addedAmount;
            m_expensePie->setCategoryValues(m_expenseCategories);
        }
        rebuildLiquidHistory();
        refreshOverview();
        amount->setValue(0.0);
        note->clear();
    });
    return page;
}

QWidget *MainWindow::createRecurringPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("contentPage"));
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(30, 24, 30, 24);
    root->setSpacing(18);
    root->addWidget(pageHeader(QStringLiteral("周期消费")));

    auto *form = card();
    auto *layout = new QHBoxLayout(form);
    layout->setContentsMargins(16, 14, 16, 14);
    auto *name = new QLineEdit;
    name->setPlaceholderText(QStringLiteral("项目名称"));
    auto *amount = moneyEditor();
    auto *cycle = new QComboBox;
    cycle->addItems({QStringLiteral("每月"), QStringLiteral("每季度"), QStringLiteral("每年")});
    auto *date = dateEditor();
    auto *add = new QPushButton(QStringLiteral("添加项目"));
    add->setObjectName(QStringLiteral("primaryButton"));
    layout->addWidget(name, 1);
    layout->addWidget(amount);
    layout->addWidget(cycle);
    layout->addWidget(date);
    layout->addWidget(add);
    root->addWidget(form);

    auto *table = new QTableWidget(0, 5);
    table->setProperty("exportName", QStringLiteral("周期消费"));
    table->setHorizontalHeaderLabels({QStringLiteral("项目"), QStringLiteral("金额"),
                                      QStringLiteral("周期"), QStringLiteral("开始日期"),
                                      QStringLiteral("下次扣款")});
    prepareTable(table);
    enableRowDeletion(table, QStringLiteral("recurring"));
    if (m_store && m_store->isReady()) {
        const auto rows = m_store->queryRows(QStringLiteral(
            "SELECT id, name, amount, cycle, start_date, next_due_date FROM recurring ORDER BY id ASC"));
        for (const auto &row : rows) {
            prependRow(table, {row.at(1).toString(), moneyText(row.at(2).toDouble()),
                               row.at(3).toString(), row.at(4).toString(),
                               row.at(5).toString()}, row.at(0).toLongLong());
        }
    }
    root->addWidget(table, 1);

    connect(add, &QPushButton::clicked, page, [this, name, amount, cycle, date, table] {
        if (name->text().trimmed().isEmpty() || amount->value() <= 0.0)
            return;
        const QString nameText = name->text().trimmed();
        const double amountValue = amount->value();
        const QString cycleText = cycle->currentText();
        const QString dateText = date->date().toString(QStringLiteral("yyyy-MM-dd"));
        const QString nextDateText = nextRecurringDate(date->date(), cycleText, QDate::currentDate())
                                         .toString(QStringLiteral("yyyy-MM-dd"));
        qint64 recordId = 0;
        if (!runStorageTransaction([this, nameText, amountValue, cycleText, dateText, nextDateText,
                                    &recordId] {
                return m_store->execute(QStringLiteral(
                    "INSERT INTO recurring(name, amount, cycle, start_date, next_due_date) "
                    "VALUES(?, ?, ?, ?, ?)"),
                    {nameText, amountValue, cycleText, dateText, nextDateText})
                    && (recordId = m_store->lastInsertId()) > 0;
            }))
            return;
        prependRow(table, {nameText, moneyText(amountValue), cycleText, dateText, nextDateText}, recordId);
        name->clear();
        amount->setValue(0.0);
    });
    return page;
}

QWidget *MainWindow::createHuabeiPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("contentPage"));
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(30, 24, 30, 24);
    root->setSpacing(18);
    root->addWidget(pageHeader(QStringLiteral("花呗")));

    auto *summary = card();
    summary->setFixedHeight(120);
    auto *summaryLayout = new QHBoxLayout(summary);
    summaryLayout->setContentsMargins(0, 0, 0, 0);
    auto *usedMetric = metricWidget(QStringLiteral("当前欠款"), moneyText(m_huabeiDebt));
    m_huabeiPageUsed = usedMetric->findChild<QLabel *>(QStringLiteral("metricValue"));
    summaryLayout->addWidget(usedMetric, 1);
    summaryLayout->addWidget(metricWidget(QStringLiteral("总额度"), QStringLiteral("¥ 3,000.00")), 1);
    auto *availableMetric = metricWidget(QStringLiteral("可用额度"), moneyText(3000.0 - m_huabeiDebt));
    m_huabeiPageAvailable = availableMetric->findChild<QLabel *>(QStringLiteral("metricValue"));
    summaryLayout->addWidget(availableMetric, 1);
    root->addWidget(summary);

    auto *forms = new QWidget;
    auto *formsLayout = new QGridLayout(forms);
    formsLayout->setContentsMargins(0, 0, 0, 0);
    formsLayout->setSpacing(14);

    auto *debtForm = card();
    auto *debtLayout = new QVBoxLayout(debtForm);
    debtLayout->setContentsMargins(16, 12, 16, 14);
    debtLayout->setSpacing(9);
    debtLayout->addWidget(label(QStringLiteral("新增欠款"), "sectionTitle"));
    auto *debtInputs = new QHBoxLayout;
    auto *debtDate = dateEditor();
    auto *debtAmount = moneyEditor();
    auto *debtCategory = new QComboBox;
    debtCategory->addItems({QStringLiteral("简餐"), QStringLiteral("下馆子"),
                            QStringLiteral("下厨"), QStringLiteral("交通"),
                            QStringLiteral("运动补给"), QStringLiteral("零食"),
                            QStringLiteral("医疗支出"), QStringLiteral("购物"),
                            QStringLiteral("日用品"), QStringLiteral("试验垫付")});
    auto *debtNote = new QLineEdit;
    debtNote->setPlaceholderText(QStringLiteral("用途或备注"));
    auto *addDebt = new QPushButton(QStringLiteral("记入欠款"));
    addDebt->setObjectName(QStringLiteral("primaryButton"));
    debtInputs->addWidget(debtDate);
    debtInputs->addWidget(debtAmount);
    debtInputs->addWidget(debtCategory);
    debtInputs->addWidget(debtNote, 1);
    debtInputs->addWidget(addDebt);
    debtLayout->addLayout(debtInputs);
    formsLayout->addWidget(debtForm, 0, 0);

    auto *repaymentForm = card();
    auto *repaymentLayout = new QVBoxLayout(repaymentForm);
    repaymentLayout->setContentsMargins(16, 12, 16, 14);
    repaymentLayout->setSpacing(9);
    repaymentLayout->addWidget(label(QStringLiteral("还款"), "sectionTitle"));
    auto *repaymentInputs = new QHBoxLayout;
    auto *repaymentDate = dateEditor();
    auto *repaymentAmount = moneyEditor();
    auto *repaymentNote = new QLineEdit;
    repaymentNote->setPlaceholderText(QStringLiteral("备注（可选）"));
    auto *repay = new QPushButton(QStringLiteral("确认还款"));
    repay->setObjectName(QStringLiteral("primaryButton"));
    repaymentInputs->addWidget(repaymentDate);
    repaymentInputs->addWidget(repaymentAmount);
    repaymentInputs->addWidget(repaymentNote, 1);
    repaymentInputs->addWidget(repay);
    repaymentLayout->addLayout(repaymentInputs);
    formsLayout->addWidget(repaymentForm, 0, 1);

    auto *resetForm = card();
    auto *resetLayout = new QVBoxLayout(resetForm);
    resetLayout->setContentsMargins(16, 12, 16, 14);
    resetLayout->setSpacing(9);
    resetLayout->addWidget(label(QStringLiteral("重新输入欠款"), "sectionTitle"));
    auto *resetInputs = new QHBoxLayout;
    auto *resetDate = dateEditor();
    auto *resetAmount = moneyEditor();
    resetAmount->setRange(0.0, 3000.0);
    auto *resetNote = new QLineEdit;
    resetNote->setPlaceholderText(QStringLiteral("用于校准当前欠款，不计入消费"));
    auto *resetDebt = new QPushButton(QStringLiteral("确认校准"));
    resetDebt->setObjectName(QStringLiteral("primaryButton"));
    resetInputs->addWidget(resetDate);
    resetInputs->addWidget(resetAmount);
    resetInputs->addWidget(resetNote, 1);
    resetInputs->addWidget(resetDebt);
    resetLayout->addLayout(resetInputs);
    formsLayout->addWidget(resetForm, 1, 0, 1, 2);
    root->addWidget(forms);

    auto *table = new QTableWidget(0, 4);
    table->setProperty("exportName", QStringLiteral("花呗"));
    table->setHorizontalHeaderLabels({QStringLiteral("日期"), QStringLiteral("操作"),
                                      QStringLiteral("金额"), QStringLiteral("备注")});
    prepareTable(table);
    enableRowDeletion(table, QStringLiteral("huabei"));
    if (m_store && m_store->isReady()) {
        const auto rows = m_store->queryRows(QStringLiteral(
            "SELECT id, date, operation, amount, category, note FROM huabei ORDER BY id ASC"));
        for (const auto &row : rows) {
            const QString categoryText = row.at(4).toString();
            const QString noteText = row.at(5).toString();
            const QString details = categoryText.isEmpty()
                ? noteText
                : (noteText.isEmpty() ? categoryText
                                      : QStringLiteral("%1 · %2").arg(categoryText, noteText));
            prependRow(table, {row.at(1).toString(), row.at(2).toString(),
                               moneyText(row.at(3).toDouble()), details}, row.at(0).toLongLong());
        }
    }
    root->addWidget(table, 1);

    connect(addDebt, &QPushButton::clicked, page,
            [this, debtDate, debtAmount, debtCategory, debtNote, table] {
        if (debtAmount->value() <= 0.0)
            return;
        const double addedAmount = debtAmount->value();
        if (m_huabeiDebt + addedAmount > 3000.0) {
            QMessageBox::warning(this, QStringLiteral("超过额度"),
                                 QStringLiteral("本次记录会使花呗欠款超过总额度。"));
            return;
        }
        const QString dateText = debtDate->date().toString(QStringLiteral("yyyy-MM-dd"));
        const QString noteText = storedText(debtNote->text().trimmed());
        const QString categoryText = debtCategory->currentText();
        const double newDebt = m_huabeiDebt + addedAmount;
        qint64 expenseId = 0;
        qint64 huabeiId = 0;
        if (!runStorageTransaction([this, dateText, addedAmount, categoryText, noteText, newDebt,
                                    &expenseId, &huabeiId] {
                if (!m_store->execute(QStringLiteral(
                           "INSERT INTO expense(date, amount, category, note, payment) VALUES(?, ?, ?, ?, '花呗')"),
                           {dateText, addedAmount, categoryText, noteText}))
                    return false;
                expenseId = m_store->lastInsertId();
                if (!m_store->execute(QStringLiteral(
                        "INSERT INTO huabei(date, operation, amount, category, note, expense_id) "
                        "VALUES(?, '新增欠款', ?, ?, ?, ?)"),
                        {dateText, addedAmount, categoryText, noteText, expenseId}))
                    return false;
                huabeiId = m_store->lastInsertId();
                return m_store->setState(QStringLiteral("huabei_debt"), newDebt);
            }))
            return;
        prependRow(table, {dateText,
                           QStringLiteral("新增欠款"), moneyText(addedAmount),
                           noteText.isEmpty() ? categoryText
                                              : QStringLiteral("%1 · %2").arg(categoryText, noteText)}, huabeiId);
        m_huabeiDebt += addedAmount;
        m_totalExpense += addedAmount;
        m_expenseCategories[categoryText] += addedAmount;
        if (m_expensePie)
            m_expensePie->setCategoryValues(m_expenseCategories);
        if (m_expenseTable) {
            prependRow(m_expenseTable, {dateText, moneyText(addedAmount),
                                        categoryText,
                                        noteText.isEmpty()
                                            ? QStringLiteral("花呗")
                                            : QStringLiteral("花呗 · %1").arg(noteText)}, expenseId);
        }
        refreshOverview();
        debtAmount->setValue(0.0);
        debtNote->clear();
    });

    connect(resetDebt, &QPushButton::clicked, page,
            [this, resetDate, resetAmount, resetNote, table] {
        const double newDebt = resetAmount->value();
        const QString dateText = resetDate->date().toString(QStringLiteral("yyyy-MM-dd"));
        const QString noteText = storedText(resetNote->text().trimmed());
        qint64 recordId = 0;
        if (!runStorageTransaction([this, dateText, newDebt, noteText, &recordId] {
                if (!m_store->execute(QStringLiteral(
                           "INSERT INTO huabei(date, operation, amount, category, note) "
                           "VALUES(?, '欠款校准', ?, '', ?)"),
                           {dateText, newDebt, noteText}))
                    return false;
                recordId = m_store->lastInsertId();
                return m_store->setState(QStringLiteral("huabei_debt"), newDebt);
            }))
            return;
        m_huabeiDebt = newDebt;
        prependRow(table, {dateText, QStringLiteral("欠款校准"), moneyText(newDebt),
                           noteText.isEmpty() ? QStringLiteral("重新输入当前欠款") : noteText}, recordId);
        refreshOverview();
        resetNote->clear();
    });

    connect(repay, &QPushButton::clicked, page,
            [this, repaymentDate, repaymentAmount, repaymentNote, table] {
        if (repaymentAmount->value() <= 0.0)
            return;
        const double paidAmount = repaymentAmount->value();
        if (paidAmount > m_huabeiDebt) {
            QMessageBox::warning(this, QStringLiteral("还款金额有误"),
                                 QStringLiteral("还款金额不能大于当前花呗欠款。"));
            return;
        }
        if (paidAmount > m_liquidFunds) {
            QMessageBox::warning(this, QStringLiteral("流动资金不足"),
                                 QStringLiteral("当前流动资金不足以完成这笔还款。"));
            return;
        }
        const QString repaymentDateText = repaymentDate->date().toString(QStringLiteral("yyyy-MM-dd"));
        const QString repaymentNoteText = storedText(repaymentNote->text().trimmed());
        const double newDebt = m_huabeiDebt - paidAmount;
        const double newLiquidFunds = m_liquidFunds - paidAmount;
        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        qint64 recordId = 0;
        if (!runStorageTransaction([this, repaymentDateText, paidAmount, repaymentNoteText,
                                    newDebt, newLiquidFunds, timestamp, &recordId] {
                if (!m_store->execute(QStringLiteral(
                           "INSERT INTO huabei(date, operation, amount, category, note) VALUES(?, '还款', ?, '', ?)"),
                           {repaymentDateText, paidAmount, repaymentNoteText}))
                    return false;
                recordId = m_store->lastInsertId();
                return m_store->setState(QStringLiteral("huabei_debt"), newDebt)
                    && m_store->setState(QStringLiteral("liquid_funds"), newLiquidFunds)
                    && m_store->appendSnapshot(QStringLiteral("liquid"), newLiquidFunds, timestamp);
            }))
            return;
        m_huabeiDebt -= paidAmount;
        m_liquidFunds -= paidAmount;
        prependRow(table, {repaymentDateText,
                           QStringLiteral("还款"), moneyText(paidAmount),
                           repaymentNoteText}, recordId);
        rebuildLiquidHistory();
        refreshOverview();
        repaymentAmount->setValue(0.0);
        repaymentNote->clear();
    });
    return page;
}

QWidget *MainWindow::createVaultPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("contentPage"));
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(30, 24, 30, 24);
    root->setSpacing(18);
    root->addWidget(pageHeader(QStringLiteral("金库")));

    auto *summary = card();
    summary->setFixedHeight(118);
    auto *summaryLayout = new QVBoxLayout(summary);
    summaryLayout->setContentsMargins(24, 16, 24, 16);
    auto *balanceName = label(QStringLiteral("当前金库资金"), "metricLabel");
    auto *balanceValue = label(QStringLiteral("¥ 0.00"), "metricValue");
    m_vaultPageBalance = balanceValue;
    balanceValue->setProperty("clearableVaultBalance", true);
    balanceValue->setProperty("vaultBalance", 0.0);
    summaryLayout->addWidget(balanceName);
    summaryLayout->addWidget(balanceValue);
    root->addWidget(summary);

    auto *form = card();
    auto *formLayout = new QHBoxLayout(form);
    formLayout->setContentsMargins(16, 14, 16, 14);
    formLayout->setSpacing(12);
    auto *date = dateEditor();
    auto *type = new QComboBox;
    type->addItems({QStringLiteral("存入"), QStringLiteral("支出")});
    auto *amount = moneyEditor();
    auto *note = new QLineEdit;
    note->setPlaceholderText(QStringLiteral("备注（可选）"));
    auto *add = new QPushButton(QStringLiteral("添加记录"));
    add->setObjectName(QStringLiteral("primaryButton"));
    formLayout->addWidget(date);
    formLayout->addWidget(type);
    formLayout->addWidget(amount);
    formLayout->addWidget(note, 1);
    formLayout->addWidget(add);
    root->addWidget(form);

    auto *table = new QTableWidget(0, 4);
    table->setProperty("exportName", QStringLiteral("金库"));
    table->setHorizontalHeaderLabels({QStringLiteral("日期"), QStringLiteral("操作"),
                                      QStringLiteral("金额"), QStringLiteral("备注")});
    prepareTable(table);
    enableRowDeletion(table, QStringLiteral("vault"));
    if (m_store && m_store->isReady()) {
        const auto rows = m_store->queryRows(QStringLiteral(
            "SELECT id, date, operation, amount, note FROM vault ORDER BY id ASC"));
        for (const auto &row : rows) {
            prependRow(table, {row.at(1).toString(), row.at(2).toString(),
                               moneyText(row.at(3).toDouble()), row.at(4).toString()},
                       row.at(0).toLongLong());
        }
    }
    root->addWidget(table, 1);

    connect(add, &QPushButton::clicked, page,
            [this, date, type, amount, note, table, balanceValue] {
        if (amount->value() <= 0.0)
            return;
        const double changedAmount = amount->value();
        const bool deposit = type->currentIndex() == 0;
        if (deposit && changedAmount > m_liquidFunds)
            return;
        if (!deposit && changedAmount > m_vaultBalance)
            return;
        const QString dateText = date->date().toString(QStringLiteral("yyyy-MM-dd"));
        const QString operationText = type->currentText();
        const QString noteText = storedText(note->text().trimmed());
        const double newVaultBalance = m_vaultBalance + (deposit ? changedAmount : -changedAmount);
        const double newLiquidFunds = m_liquidFunds - (deposit ? changedAmount : 0.0);
        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        qint64 recordId = 0;
        if (!runStorageTransaction([this, dateText, operationText, changedAmount, noteText,
                                    newVaultBalance, newLiquidFunds, deposit, timestamp, &recordId] {
                if (!m_store->execute(QStringLiteral(
                        "INSERT INTO vault(date, operation, amount, note) VALUES(?, ?, ?, ?)"),
                        {dateText, operationText, changedAmount, noteText}))
                    return false;
                recordId = m_store->lastInsertId();
                if (!m_store->setState(QStringLiteral("vault_balance"), newVaultBalance)
                    || !m_store->appendSnapshot(QStringLiteral("vault"), newVaultBalance, timestamp))
                    return false;
                if (deposit) {
                    return m_store->setState(QStringLiteral("liquid_funds"), newLiquidFunds)
                        && m_store->appendSnapshot(QStringLiteral("liquid"), newLiquidFunds, timestamp);
                }
                return true;
            }))
            return;
        m_vaultBalance = newVaultBalance;
        if (deposit) {
            m_liquidFunds = newLiquidFunds;
            rebuildLiquidHistory();
        }
        balanceValue->setProperty("vaultBalance", m_vaultBalance);
        balanceValue->setText(moneyText(m_vaultBalance));
        prependRow(table, {dateText, operationText, moneyText(changedAmount), noteText}, recordId);
        m_vaultChart->appendValue(m_vaultBalance, QDate::fromString(dateText, Qt::ISODate));
        refreshOverview();
        amount->setValue(0.0);
        note->clear();
    });
    return page;
}

QWidget *MainWindow::createBodyPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("contentPage"));
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(30, 24, 30, 24);
    root->setSpacing(18);
    root->addWidget(pageHeader(QStringLiteral("身体记录")));

    auto *table = new QTableWidget(0, 4);
    table->setProperty("exportName", QStringLiteral("身体记录"));
    table->setHorizontalHeaderLabels({QStringLiteral("日期"), QStringLiteral("项目"),
                                      QStringLiteral("数据"), QStringLiteral("备注")});
    prepareTable(table);
    enableRowDeletion(table, QStringLiteral("body_records"));
    if (m_store && m_store->isReady()) {
        const auto rows = m_store->queryRows(QStringLiteral(
            "SELECT id, date, project, data_text, note FROM body_records ORDER BY id ASC"));
        for (const auto &row : rows) {
            prependRow(table, {row.at(1).toString(), row.at(2).toString(),
                               row.at(3).toString(), row.at(4).toString()}, row.at(0).toLongLong());
        }
    }

    const auto latestBodyText = [this](const QString &project, const QString &fallback) {
        if (!m_store || !m_store->isReady())
            return fallback;
        const auto rows = m_store->queryRows(QStringLiteral(
            "SELECT data_text FROM body_records WHERE project = ? ORDER BY id DESC LIMIT 1"), {project});
        return rows.isEmpty() ? fallback : rows.first().first().toString();
    };

    auto *columns = new QWidget;
    auto *columnsLayout = new QHBoxLayout(columns);
    columnsLayout->setContentsMargins(0, 0, 0, 0);
    columnsLayout->setSpacing(10);

    auto *weightCard = card();
    auto *weightLayout = new QVBoxLayout(weightCard);
    weightLayout->setContentsMargins(12, 12, 12, 12);
    weightLayout->setSpacing(8);
    weightLayout->addWidget(label(QStringLiteral("体重"), "sectionTitle"));
    auto *latestWeight = label(m_latestWeight > 0.0
                                   ? QStringLiteral("当前 %1 kg").arg(m_latestWeight, 0, 'f', 1)
                                   : QStringLiteral("暂无记录"),
                               "metricLabel");
    latestWeight->setProperty("bodyProject", QStringLiteral("体重"));
    weightLayout->addWidget(latestWeight);
    auto *weightDate = dateEditor();
    auto *weightValue = new QDoubleSpinBox;
    weightValue->setRange(20.0, 250.0);
    weightValue->setDecimals(1);
    weightValue->setSuffix(QStringLiteral(" kg"));
    weightValue->setValue(75.0);
    auto *addWeight = new QPushButton(QStringLiteral("记录"));
    addWeight->setObjectName(QStringLiteral("primaryButton"));
    weightLayout->addWidget(weightDate);
    weightLayout->addWidget(weightValue);
    weightLayout->addWidget(addWeight);
    columnsLayout->addWidget(weightCard, 1);

    connect(addWeight, &QPushButton::clicked, page,
            [this, weightDate, weightValue, latestWeight, table] {
        const double newWeight = weightValue->value();
        const double change = m_latestWeight > 0.0 ? newWeight - m_latestWeight : 0.0;
        const QString changeText = QStringLiteral("较上次 %1%2 kg")
                               .arg(change >= 0.0 ? QStringLiteral("+") : QString())
                               .arg(change, 0, 'f', 1);
        const QString dateText = weightDate->date().toString(QStringLiteral("yyyy-MM-dd"));
        const QString dataText = QStringLiteral("%1 kg").arg(newWeight, 0, 'f', 1);
        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        qint64 recordId = 0;
        if (!runStorageTransaction([this, dateText, dataText, newWeight, changeText, timestamp,
                                    &recordId] {
                if (!m_store->execute(QStringLiteral(
                           "INSERT INTO body_records(date, project, data_text, numeric_value, note) "
                           "VALUES(?, '体重', ?, ?, ?)"),
                           {dateText, dataText, newWeight, changeText}))
                    return false;
                recordId = m_store->lastInsertId();
                return m_store->appendSnapshot(QStringLiteral("weight"), newWeight, timestamp);
            }))
            return;
        m_previousWeight = m_latestWeight;
        m_latestWeight = newWeight;
        prependRow(table, {dateText, QStringLiteral("体重"), dataText, changeText}, recordId);
        latestWeight->setText(QStringLiteral("当前 %1 kg").arg(newWeight, 0, 'f', 1));
        m_weightChart->appendValue(m_latestWeight, QDate::fromString(dateText, Qt::ISODate));
        refreshOverview();
    });

    auto addNumericColumn = [this, page, table, columnsLayout](const QString &name,
                                                         const QString &unit,
                                                         double maximum,
                                                         double initialValue,
                                                         const QString &latestText) {
        auto *column = card();
        auto *layout = new QVBoxLayout(column);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(8);
        layout->addWidget(label(name, "sectionTitle"));
        auto *latest = label(latestText, "metricLabel");
        latest->setProperty("bodyProject", name);
        layout->addWidget(latest);
        auto *date = dateEditor();
        auto *value = new QDoubleSpinBox;
        value->setRange(0.0, maximum);
        value->setDecimals(unit == QStringLiteral("km") ? 1 : 0);
        value->setSuffix(QStringLiteral(" ") + unit);
        value->setValue(initialValue);
        auto *add = new QPushButton(QStringLiteral("记录"));
        add->setObjectName(QStringLiteral("primaryButton"));
        layout->addWidget(date);
        layout->addWidget(value);
        layout->addWidget(add);
        columnsLayout->addWidget(column, 1);
        connect(add, &QPushButton::clicked, page, [this, date, value, latest, table, name, unit] {
            if (value->value() <= 0.0)
                return;
            const int decimals = unit == QStringLiteral("km") ? 1 : 0;
            const QString data = QStringLiteral("%1 %2").arg(value->value(), 0, 'f', decimals).arg(unit);
            const QString dateText = date->date().toString(QStringLiteral("yyyy-MM-dd"));
            const double numericValue = value->value();
            qint64 recordId = 0;
            if (!runStorageTransaction([this, dateText, name, data, numericValue, &recordId] {
                    if (!m_store->execute(QStringLiteral(
                               "INSERT INTO body_records(date, project, data_text, numeric_value, note) "
                               "VALUES(?, ?, ?, ?, '')"),
                               {dateText, name, data, numericValue}))
                        return false;
                    recordId = m_store->lastInsertId();
                    return true;
                }))
                return;
            prependRow(table, {dateText, name, data, QString()}, recordId);
            latest->setText(QStringLiteral("最近 %1").arg(data));
            refreshDailyCharts();
        });
    };

    addNumericColumn(QStringLiteral("羽球"), QStringLiteral("分钟"), 600.0, 90.0,
                     QStringLiteral("最近 %1").arg(latestBodyText(QStringLiteral("羽球"), QStringLiteral("暂无记录"))));
    addNumericColumn(QStringLiteral("骑行"), QStringLiteral("分钟"), 1440.0, 60.0,
                     QStringLiteral("最近 %1").arg(latestBodyText(QStringLiteral("骑行"), QStringLiteral("暂无记录"))));
    addNumericColumn(QStringLiteral("健身"), QStringLiteral("分钟"), 600.0, 55.0,
                     QStringLiteral("最近 %1").arg(latestBodyText(QStringLiteral("健身"), QStringLiteral("暂无记录"))));
    addNumericColumn(QStringLiteral("网球"), QStringLiteral("分钟"), 600.0, 60.0,
                     QStringLiteral("最近 %1").arg(latestBodyText(QStringLiteral("网球"), QStringLiteral("暂无记录"))));

    auto *otherCard = card();
    auto *otherLayout = new QVBoxLayout(otherCard);
    otherLayout->setContentsMargins(12, 12, 12, 12);
    otherLayout->setSpacing(8);
    otherLayout->addWidget(label(QStringLiteral("其他"), "sectionTitle"));
    auto *latestOther = label(QStringLiteral("最近 %1").arg(
                                  latestBodyText(QStringLiteral("其他"), QStringLiteral("暂无记录"))),
                              "metricLabel");
    latestOther->setProperty("bodyProject", QStringLiteral("其他"));
    otherLayout->addWidget(latestOther);
    auto *otherDate = dateEditor();
    auto *otherText = new QLineEdit;
    otherText->setPlaceholderText(QStringLiteral("项目 + 数据"));
    auto *addOther = new QPushButton(QStringLiteral("记录"));
    addOther->setObjectName(QStringLiteral("primaryButton"));
    otherLayout->addWidget(otherDate);
    otherLayout->addWidget(otherText);
    otherLayout->addWidget(addOther);
    columnsLayout->addWidget(otherCard, 1);
    connect(addOther, &QPushButton::clicked, page,
            [this, otherDate, otherText, latestOther, table] {
        const QString text = otherText->text().trimmed();
        if (text.isEmpty())
            return;
        const QString dateText = otherDate->date().toString(QStringLiteral("yyyy-MM-dd"));
        qint64 recordId = 0;
        if (!runStorageTransaction([this, dateText, text, &recordId] {
                if (!m_store->execute(QStringLiteral(
                           "INSERT INTO body_records(date, project, data_text, numeric_value, note) "
                           "VALUES(?, '其他', ?, NULL, '')"),
                           {dateText, text}))
                    return false;
                recordId = m_store->lastInsertId();
                return true;
            }))
            return;
        prependRow(table, {dateText, QStringLiteral("其他"), text, QString()}, recordId);
        latestOther->setText(QStringLiteral("最近 %1").arg(text));
        otherText->clear();
    });

    root->addWidget(columns);
    root->addWidget(table, 1);
    return page;
}

QWidget *MainWindow::createResearchPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("contentPage"));
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(30, 24, 30, 24);
    root->setSpacing(18);
    root->addWidget(pageHeader(QStringLiteral("work")));

    auto *hoursCard = card();
    auto *hoursLayout = new QVBoxLayout(hoursCard);
    hoursLayout->setContentsMargins(16, 12, 16, 12);
    hoursLayout->setSpacing(8);
    hoursLayout->addWidget(label(QStringLiteral("每日工时"), "sectionTitle"));

    auto *hoursInputs = new QHBoxLayout;
    hoursInputs->setSpacing(8);
    auto *workDate = dateEditor();
    auto timeEditor = [](const QTime &time) {
        auto *editor = new QTimeEdit(time);
        editor->setDisplayFormat(QStringLiteral("HH:mm"));
        editor->setMinimumWidth(72);
        return editor;
    };
    auto *morningStart = timeEditor(QTime(8, 30));
    auto *morningEnd = timeEditor(QTime(12, 0));
    auto *afternoonStart = timeEditor(QTime(13, 30));
    auto *afternoonEnd = timeEditor(QTime(17, 30));
    auto *eveningStart = timeEditor(QTime(19, 0));
    auto *eveningEnd = timeEditor(QTime(19, 0));
    auto timePair = [](const QString &name, QTimeEdit *start, QTimeEdit *end) {
        auto *widget = new QWidget;
        auto *layout = new QVBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(3);
        auto *caption = label(name, "metricLabel");
        caption->setAlignment(Qt::AlignCenter);
        layout->addWidget(caption);
        auto *row = new QHBoxLayout;
        row->setSpacing(4);
        row->addWidget(start);
        row->addWidget(label(QStringLiteral("—")));
        row->addWidget(end);
        layout->addLayout(row);
        return widget;
    };
    auto *totalLabel = label(QStringLiteral("合计 7.5 小时"), "sectionTitle");
    totalLabel->setMinimumWidth(96);
    totalLabel->setAlignment(Qt::AlignCenter);
    auto *recordHours = new QPushButton(QStringLiteral("记录工时"));
    recordHours->setObjectName(QStringLiteral("primaryButton"));
    hoursInputs->addWidget(workDate);
    hoursInputs->addWidget(timePair(QStringLiteral("上午"), morningStart, morningEnd));
    hoursInputs->addWidget(timePair(QStringLiteral("下午"), afternoonStart, afternoonEnd));
    hoursInputs->addWidget(timePair(QStringLiteral("晚上"), eveningStart, eveningEnd));
    hoursInputs->addStretch();
    hoursInputs->addWidget(totalLabel);
    hoursInputs->addWidget(recordHours);
    hoursLayout->addLayout(hoursInputs);

    auto *hoursTable = new QTableWidget(0, 5);
    hoursTable->setProperty("exportName", QStringLiteral("work_工时"));
    hoursTable->setHorizontalHeaderLabels({QStringLiteral("日期"), QStringLiteral("上午"),
                                           QStringLiteral("下午"), QStringLiteral("晚上"),
                                           QStringLiteral("总工时")});
    prepareTable(hoursTable);
    enableRowDeletion(hoursTable, QStringLiteral("work_hours"));
    hoursTable->setFixedHeight(112);
    if (m_store && m_store->isReady()) {
        const auto rows = m_store->queryRows(QStringLiteral(
            "SELECT id, work_date, morning_start, morning_end, afternoon_start, afternoon_end, "
            "evening_start, evening_end, total_hours FROM work_hours ORDER BY work_date ASC"));
        for (const auto &row : rows) {
            prependRow(hoursTable,
                       {row.at(1).toString(),
                        QStringLiteral("%1—%2").arg(row.at(2).toString(), row.at(3).toString()),
                        QStringLiteral("%1—%2").arg(row.at(4).toString(), row.at(5).toString()),
                        QStringLiteral("%1—%2").arg(row.at(6).toString(), row.at(7).toString()),
                        QStringLiteral("%1 小时").arg(row.at(8).toDouble(), 0, 'f', 2)},
                       row.at(0).toLongLong());
        }
    }
    hoursLayout->addWidget(hoursTable);
    root->addWidget(hoursCard);

    const auto intervalHours = [](const QTime &start, const QTime &end) {
        return end > start ? start.secsTo(end) / 3600.0 : 0.0;
    };
    const auto updateTotal = [=] {
        const double total = intervalHours(morningStart->time(), morningEnd->time())
            + intervalHours(afternoonStart->time(), afternoonEnd->time())
            + intervalHours(eveningStart->time(), eveningEnd->time());
        totalLabel->setText(QStringLiteral("合计 %1 小时").arg(total, 0, 'f', 2));
    };
    for (auto *editor : {morningStart, morningEnd, afternoonStart, afternoonEnd,
                         eveningStart, eveningEnd}) {
        connect(editor, &QTimeEdit::timeChanged, page, updateTotal);
    }

    connect(recordHours, &QPushButton::clicked, page, [=] {
        if (morningEnd->time() < morningStart->time()
            || afternoonEnd->time() < afternoonStart->time()
            || eveningEnd->time() < eveningStart->time()) {
            QMessageBox::warning(this, QStringLiteral("时间有误"),
                                 QStringLiteral("每个时段的下班时间不能早于上班时间。"));
            return;
        }
        const double total = intervalHours(morningStart->time(), morningEnd->time())
            + intervalHours(afternoonStart->time(), afternoonEnd->time())
            + intervalHours(eveningStart->time(), eveningEnd->time());
        const QString dateText = workDate->date().toString(QStringLiteral("yyyy-MM-dd"));
        const QString ms = morningStart->time().toString(QStringLiteral("HH:mm"));
        const QString me = morningEnd->time().toString(QStringLiteral("HH:mm"));
        const QString as = afternoonStart->time().toString(QStringLiteral("HH:mm"));
        const QString ae = afternoonEnd->time().toString(QStringLiteral("HH:mm"));
        const QString es = eveningStart->time().toString(QStringLiteral("HH:mm"));
        const QString ee = eveningEnd->time().toString(QStringLiteral("HH:mm"));

        const bool dateAlreadyRecorded = m_store && m_store->isReady()
            && m_store->scalar(QStringLiteral(
                   "SELECT COUNT(*) FROM work_hours WHERE work_date = ?"),
                   {dateText}) > 0.0;
        if (dateAlreadyRecorded) {
            const auto answer = QMessageBox::question(
                this, QStringLiteral("确认覆盖工时记录"),
                QStringLiteral("%1 已经存在工时记录。\n\n"
                               "继续后，原记录将被当前输入的时间覆盖。是否继续？")
                    .arg(dateText),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                workDate->setFocus();
                workDate->selectAll();
                return;
            }
        }

        if (!runStorageTransaction([=] {
                return m_store->execute(QStringLiteral(
                    "INSERT INTO work_hours(work_date, morning_start, morning_end, afternoon_start, "
                    "afternoon_end, evening_start, evening_end, total_hours) VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
                    "ON CONFLICT(work_date) DO UPDATE SET morning_start=excluded.morning_start, "
                    "morning_end=excluded.morning_end, afternoon_start=excluded.afternoon_start, "
                    "afternoon_end=excluded.afternoon_end, evening_start=excluded.evening_start, "
                    "evening_end=excluded.evening_end, total_hours=excluded.total_hours"),
                    {dateText, ms, me, as, ae, es, ee, total});
            }))
            return;

        const qint64 recordId = static_cast<qint64>(m_store->scalar(
            QStringLiteral("SELECT id FROM work_hours WHERE work_date = ?"), {dateText}));

        int existingRow = -1;
        for (int row = 0; row < hoursTable->rowCount(); ++row) {
            if (hoursTable->item(row, 0) && hoursTable->item(row, 0)->text() == dateText) {
                existingRow = row;
                break;
            }
        }
        const QStringList values{dateText, QStringLiteral("%1—%2").arg(ms, me),
                                 QStringLiteral("%1—%2").arg(as, ae),
                                 QStringLiteral("%1—%2").arg(es, ee),
                                 QStringLiteral("%1 小时").arg(total, 0, 'f', 2)};
        if (existingRow >= 0) {
            for (int column = 0; column < values.size(); ++column)
                hoursTable->setItem(existingRow, column, new QTableWidgetItem(values.at(column)));
            auto *recordItem = new QTableWidgetItem;
            recordItem->setData(Qt::UserRole, recordId);
            hoursTable->setVerticalHeaderItem(existingRow, recordItem);
        } else {
            prependRow(hoursTable, values, recordId);
        }
        refreshDailyCharts();
    });

    auto *columns = new QWidget;
    auto *columnsLayout = new QHBoxLayout(columns);
    columnsLayout->setContentsMargins(0, 0, 0, 0);
    columnsLayout->setSpacing(14);

    auto createWorkColumn = [this, page, columnsLayout](const QString &name) {
        auto *column = card();
        auto *layout = new QVBoxLayout(column);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(10);
        layout->addWidget(label(name, "sectionTitle"));

        auto *dateTime = new QDateTimeEdit(QDateTime::currentDateTime());
        dateTime->setCalendarPopup(true);
        dateTime->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
        auto *idea = new QLineEdit;
        idea->setPlaceholderText(QStringLiteral("想法 / idea"));
        auto *add = new QPushButton(QStringLiteral("记录"));
        add->setObjectName(QStringLiteral("primaryButton"));
        layout->addWidget(dateTime);
        layout->addWidget(idea);
        layout->addWidget(add);

        auto *table = new QTableWidget(0, 2);
        table->setProperty("exportName", QStringLiteral("work_%1").arg(name));
        table->setHorizontalHeaderLabels({QStringLiteral("时间"), QStringLiteral("想法 / idea")});
        prepareTable(table);
        enableRowDeletion(table, QStringLiteral("work_notes"));
        if (m_store && m_store->isReady()) {
            const auto rows = m_store->queryRows(QStringLiteral(
                "SELECT id, recorded_at, idea FROM work_notes WHERE section = ? ORDER BY id ASC"), {name});
            for (const auto &row : rows)
                prependRow(table, {row.at(1).toString(), row.at(2).toString()}, row.at(0).toLongLong());
        }
        table->setToolTip(QStringLiteral("双击任意记录可打开详细编辑窗口"));
        connect(table, &QTableWidget::cellDoubleClicked, page,
                [this, table, name](int row, int) {
                    openWorkNoteEditor(table, name, row);
                });
        layout->addWidget(table, 1);
        columnsLayout->addWidget(column, 1);

        connect(add, &QPushButton::clicked, page, [this, dateTime, idea, table, name] {
            const QString ideaText = idea->text().trimmed();
            if (ideaText.isEmpty())
                return;
            const QString recordedAt = dateTime->dateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
            qint64 recordId = 0;
            if (!runStorageTransaction([this, recordedAt, name, ideaText, &recordId] {
                    if (!m_store->execute(QStringLiteral(
                               "INSERT INTO work_notes(recorded_at, section, idea) VALUES(?, ?, ?)"),
                               {recordedAt, name, ideaText}))
                        return false;
                    recordId = m_store->lastInsertId();
                    return true;
                }))
                return;
            prependRow(table, {recordedAt, ideaText}, recordId);
            refreshWorkSummary(name);
            idea->clear();
        });
    };

    createWorkColumn(QStringLiteral("理论"));
    createWorkColumn(QStringLiteral("试验"));
    createWorkColumn(QStringLiteral("小组工作"));

    root->addWidget(columns, 1);
    return page;
}

void MainWindow::openWorkNoteEditor(QTableWidget *table, const QString &section, int row)
{
    if (!table || !m_store || !m_store->isReady())
        return;

    const qint64 recordId = tableRecordId(table, row);
    if (recordId <= 0)
        return;

    if (QWidget *existing = m_openWorkNoteEditors.value(recordId).data()) {
        existing->show();
        existing->raise();
        existing->activateWindow();
        return;
    }

    const auto rows = m_store->queryRows(QStringLiteral(
        "SELECT recorded_at, idea FROM work_notes WHERE id = ? AND section = ? LIMIT 1"),
        {recordId, section});
    if (rows.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无法打开"),
                             QStringLiteral("这条 work 记录已不存在。"));
        return;
    }

    const QString recordedAt = rows.first().at(0).toString();
    const QString text = rows.first().at(1).toString();
    auto *editor = new WorkNoteEditor(
        recordId, section, recordedAt, text,
        [this, table, section](qint64 id, const QString &updatedText) {
            return saveWorkNote(table, section, id, updatedText);
        },
        this);
    m_openWorkNoteEditors[recordId] = editor;
    editor->show();
}

bool MainWindow::saveWorkNote(QTableWidget *table, const QString &section,
                              qint64 recordId, const QString &text)
{
    if (!m_store || !m_store->isReady() || recordId <= 0)
        return false;
    if (!m_store->execute(QStringLiteral("UPDATE work_notes SET idea = ? WHERE id = ?"),
                          {text, recordId}))
        return false;

    if (table) {
        for (int row = 0; row < table->rowCount(); ++row) {
            if (tableRecordId(table, row) != recordId)
                continue;
            if (!table->item(row, 1))
                table->setItem(row, 1, new QTableWidgetItem);
            table->item(row, 1)->setText(text);
            table->item(row, 1)->setToolTip(text);
            break;
        }
    }
    refreshWorkSummary(section);
    return true;
}

void MainWindow::refreshWorkSummary(const QString &section)
{
    if (!m_store || !m_store->isReady())
        return;
    const auto rows = m_store->queryRows(QStringLiteral(
        "SELECT recorded_at, idea FROM work_notes WHERE section = ? ORDER BY id DESC LIMIT 1"),
        {section});
    const QString summary = rows.isEmpty()
        ? QStringLiteral("暂无记录")
        : QStringLiteral("%1  %2")
              .arg(rows.first().at(0).toString().left(10).mid(5),
                   rows.first().at(1).toString());
    m_latestWorkSummaries[section] = summary;
    if (m_workOverviewLabels.value(section))
        m_workOverviewLabels.value(section)->setText(summary);
}

void MainWindow::loadStoredState()
{
    if (!m_store || !m_store->isReady())
        return;

    m_totalIncome = m_store->scalar(QStringLiteral("SELECT COALESCE(SUM(amount), 0) FROM income"));
    m_totalExpense = m_store->scalar(QStringLiteral("SELECT COALESCE(SUM(amount), 0) FROM expense"));
    m_liquidFunds = m_store->state(QStringLiteral("liquid_funds"));
    m_huabeiDebt = m_store->state(QStringLiteral("huabei_debt"));
    m_vaultBalance = m_store->state(QStringLiteral("vault_balance"));

    m_expenseCategories.clear();
    const auto categoryRows = m_store->queryRows(QStringLiteral(
        "SELECT category, SUM(amount) FROM expense GROUP BY category ORDER BY category"));
    for (const auto &row : categoryRows)
        m_expenseCategories[row.at(0).toString()] = row.at(1).toDouble();

    const auto weights = m_store->queryRows(QStringLiteral(
        "SELECT numeric_value FROM body_records WHERE project = '体重' "
        "ORDER BY id DESC LIMIT 2"));
    m_latestWeight = weights.isEmpty() ? 0.0 : weights.at(0).at(0).toDouble();
    m_previousWeight = weights.size() < 2 ? 0.0 : weights.at(1).at(0).toDouble();

    m_latestWorkSummaries.clear();
    for (const QString &section : {QStringLiteral("理论"), QStringLiteral("试验"),
                                   QStringLiteral("小组工作")}) {
        const auto work = m_store->queryRows(QStringLiteral(
            "SELECT recorded_at, idea FROM work_notes WHERE section = ? ORDER BY id DESC LIMIT 1"),
            {section});
        m_latestWorkSummaries[section] = work.isEmpty()
            ? QStringLiteral("暂无记录")
            : QStringLiteral("%1  %2")
                  .arg(work.first().at(0).toString().left(10).mid(5),
                       work.first().at(1).toString());
    }

    const auto loadSnapshotHistory = [this](const QString &series) {
        QMap<QDate, double> history;
        const auto rows = m_store->queryRows(QStringLiteral(
            "SELECT recorded_at, value FROM chart_snapshots WHERE series = ? ORDER BY id ASC"),
            {series});
        for (const auto &row : rows) {
            const QDate date = QDate::fromString(row.at(0).toString().left(10), Qt::ISODate);
            if (date.isValid())
                history[date] = row.at(1).toDouble();
        }
        return history;
    };
    rebuildLiquidHistory();
    m_storedVaultHistory = loadSnapshotHistory(QStringLiteral("vault"));
    m_storedWeightHistory.clear();
    const auto weightHistory = m_store->queryRows(QStringLiteral(
        "SELECT date, numeric_value FROM body_records WHERE project='体重' ORDER BY id ASC"));
    for (const auto &row : weightHistory) {
        const QDate date = QDate::fromString(row.at(0).toString(), Qt::ISODate);
        if (date.isValid())
            m_storedWeightHistory[date] = row.at(1).toDouble();
    }
}

void MainWindow::rebuildLiquidHistory()
{
    m_storedLiquidHistory.clear();
    if (!m_store || !m_store->isReady())
        return;

    // Build one end-of-day balance per transaction date. This deliberately
    // ignores chart_snapshots: snapshots describe entry order, which differs
    // from transaction order when the user backfills an earlier date.
    const auto rows = m_store->queryRows(QStringLiteral(
        "SELECT event_date, SUM(delta) FROM ("
        "SELECT date AS event_date, amount AS delta FROM income "
        "UNION ALL "
        "SELECT date AS event_date, -amount AS delta FROM expense WHERE payment='流动资金' "
        "UNION ALL "
        "SELECT date AS event_date, -amount AS delta FROM huabei WHERE operation='还款' "
        "UNION ALL "
        "SELECT date AS event_date, -amount AS delta FROM vault WHERE operation='存入'"
        ") GROUP BY event_date ORDER BY event_date"));

    double ledgerNet = 0.0;
    for (const auto &row : rows) {
        if (QDate::fromString(row.at(0).toString(), Qt::ISODate).isValid())
            ledgerNet += row.at(1).toDouble();
    }

    // Preserve a possible opening balance from an older/imported database that
    // predates the current transaction ledger, then apply every dated change.
    double runningBalance = m_liquidFunds - ledgerNet;
    for (const auto &row : rows) {
        const QDate date = QDate::fromString(row.at(0).toString(), Qt::ISODate);
        if (!date.isValid())
            continue;
        runningBalance += row.at(1).toDouble();
        m_storedLiquidHistory[date] = runningBalance;
    }

    if (!m_liquidChart)
        return;
    m_liquidChart->clearData();
    if (m_storedLiquidHistory.isEmpty())
        m_liquidChart->setCurrentValue(m_liquidFunds);
    else
        m_liquidChart->setDatedValues(m_storedLiquidHistory);
}

bool MainWindow::runStorageTransaction(const std::function<bool()> &action)
{
    if (!m_store || !m_store->isReady()) {
        QMessageBox::warning(this, QStringLiteral("无法保存"),
                             QStringLiteral("SQLite 数据库当前不可用，记录未添加。"));
        return false;
    }
    if (!m_store->begin()) {
        QMessageBox::warning(this, QStringLiteral("无法保存"), m_store->lastError());
        return false;
    }
    if (!action() || !m_store->commit()) {
        m_store->rollback();
        QMessageBox::warning(this, QStringLiteral("保存失败"), m_store->lastError());
        return false;
    }
    return true;
}

void MainWindow::enableRowDeletion(QTableWidget *table, const QString &storageTable)
{
    table->setProperty("storageTable", storageTable);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table, &QTableWidget::customContextMenuRequested, this,
            [this, table, storageTable](const QPoint &position) {
        const QModelIndex index = table->indexAt(position);
        if (!index.isValid())
            return;
        const int row = index.row();
        table->selectRow(row);
        const qint64 recordId = tableRecordId(table, row);
        if (recordId <= 0)
            return;

        QMenu menu(table);
        QAction *deleteAction = menu.addAction(QStringLiteral("删除选中记录"));
        if (menu.exec(table->viewport()->mapToGlobal(position)) != deleteAction)
            return;
        const QString recordName = table->item(row, 0) ? table->item(row, 0)->text()
                                                       : QStringLiteral("该记录");
        if (QMessageBox::question(
                this, QStringLiteral("确认删除"),
                QStringLiteral("确定删除“%1”这条记录吗？\n删除后相关余额和图表会重新计算。")
                    .arg(recordName),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        if (!deleteStoredRecord(storageTable, recordId))
            return;
        table->removeRow(row);
    });
}

void MainWindow::removeRecordFromTable(QTableWidget *table, qint64 recordId)
{
    if (!table || recordId <= 0)
        return;
    for (int row = 0; row < table->rowCount(); ++row) {
        if (tableRecordId(table, row) == recordId) {
            table->removeRow(row);
            return;
        }
    }
}

bool MainWindow::deleteStoredRecord(const QString &storageTable, qint64 recordId)
{
    const QStringList allowedTables{
        QStringLiteral("income"), QStringLiteral("expense"), QStringLiteral("recurring"),
        QStringLiteral("huabei"), QStringLiteral("vault"), QStringLiteral("body_records"),
        QStringLiteral("work_notes"), QStringLiteral("work_hours")};
    if (!allowedTables.contains(storageTable) || recordId <= 0)
        return false;

    qint64 linkedExpenseId = 0;
    qint64 linkedHuabeiId = 0;
    if (storageTable == QStringLiteral("huabei")) {
        const auto rows = m_store->queryRows(QStringLiteral(
            "SELECT expense_id FROM huabei WHERE id = ?"), {recordId});
        if (!rows.isEmpty() && !rows.first().first().isNull())
            linkedExpenseId = rows.first().first().toLongLong();
    } else if (storageTable == QStringLiteral("expense")) {
        const auto rows = m_store->queryRows(QStringLiteral(
            "SELECT id FROM huabei WHERE expense_id = ? LIMIT 1"), {recordId});
        if (!rows.isEmpty())
            linkedHuabeiId = rows.first().first().toLongLong();
    }

    const bool financial = storageTable == QStringLiteral("income")
        || storageTable == QStringLiteral("expense")
        || storageTable == QStringLiteral("huabei")
        || storageTable == QStringLiteral("vault");
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!runStorageTransaction([this, storageTable, recordId, linkedExpenseId,
                                linkedHuabeiId, financial, timestamp] {
            if (storageTable == QStringLiteral("recurring")
                && !m_store->execute(QStringLiteral(
                    "DELETE FROM recurring_charges WHERE recurring_id = ?"), {recordId}))
                return false;
            if (linkedExpenseId > 0
                && !m_store->execute(QStringLiteral("DELETE FROM expense WHERE id = ?"),
                                     {linkedExpenseId}))
                return false;
            if (linkedHuabeiId > 0
                && !m_store->execute(QStringLiteral("DELETE FROM huabei WHERE id = ?"),
                                     {linkedHuabeiId}))
                return false;
            if (!m_store->execute(QStringLiteral("DELETE FROM %1 WHERE id = ?").arg(storageTable),
                                  {recordId}))
                return false;
            if (!financial)
                return true;
            if (!m_store->recalculateDerivedState()
                || !m_store->execute(QStringLiteral(
                    "DELETE FROM chart_snapshots WHERE series IN ('liquid','vault')")))
                return false;
            return m_store->appendSnapshot(QStringLiteral("liquid"),
                                           m_store->state(QStringLiteral("liquid_funds")), timestamp)
                && m_store->appendSnapshot(QStringLiteral("vault"),
                                           m_store->state(QStringLiteral("vault_balance")), timestamp);
        }))
        return false;

    if (linkedExpenseId > 0)
        removeRecordFromTable(m_expenseTable, linkedExpenseId);
    if (linkedHuabeiId > 0)
        removeRecordFromTable(m_huabeiTable, linkedHuabeiId);

    loadStoredState();
    refreshOverview();
    refreshChartsFromStoredState();
    if (m_expensePie)
        m_expensePie->setCategoryValues(m_expenseCategories);
    refreshDailyCharts();
    refreshBodyLatestLabels();
    for (auto it = m_workOverviewLabels.begin(); it != m_workOverviewLabels.end(); ++it)
        it.value()->setText(m_latestWorkSummaries.value(it.key(), QStringLiteral("暂无记录")));
    return true;
}

void MainWindow::refreshChartsFromStoredState()
{
    const auto restore = [](DataLineChart *chart, const QMap<QDate, double> &history,
                            double currentValue) {
        if (!chart)
            return;
        chart->clearData();
        if (history.isEmpty())
            chart->setCurrentValue(currentValue);
        else
            chart->setDatedValues(history);
    };
    restore(m_liquidChart, m_storedLiquidHistory, m_liquidFunds);
    restore(m_vaultChart, m_storedVaultHistory, m_vaultBalance);
    restore(m_weightChart, m_storedWeightHistory, m_latestWeight);
}

void MainWindow::refreshBodyLatestLabels()
{
    if (!m_pages || !m_store || !m_store->isReady())
        return;
    const auto labels = m_pages->findChildren<QLabel *>();
    for (QLabel *valueLabel : labels) {
        const QString project = valueLabel->property("bodyProject").toString();
        if (project.isEmpty())
            continue;
        const auto rows = m_store->queryRows(QStringLiteral(
            "SELECT data_text FROM body_records WHERE project = ? ORDER BY id DESC LIMIT 1"),
            {project});
        if (rows.isEmpty())
            valueLabel->setText(QStringLiteral("暂无记录"));
        else if (project == QStringLiteral("体重"))
            valueLabel->setText(QStringLiteral("当前 %1").arg(rows.first().first().toString()));
        else
            valueLabel->setText(QStringLiteral("最近 %1").arg(rows.first().first().toString()));
    }
}

void MainWindow::refreshOverview()
{
    if (m_overviewWeight)
        m_overviewWeight->setText(m_latestWeight > 0.0
                                      ? QStringLiteral("%1 kg").arg(m_latestWeight, 0, 'f', 1)
                                      : QStringLiteral("—"));
    if (m_overviewIncome)
        m_overviewIncome->setText(moneyText(m_totalIncome));
    if (m_overviewExpense)
        m_overviewExpense->setText(moneyText(m_totalExpense));
    if (m_overviewBalance)
        m_overviewBalance->setText(moneyText(m_totalIncome - m_totalExpense));
    if (m_overviewHuabei)
        m_overviewHuabei->setText(moneyText(m_huabeiDebt));
    if (m_huabeiPageUsed)
        m_huabeiPageUsed->setText(moneyText(m_huabeiDebt));
    if (m_huabeiPageAvailable)
        m_huabeiPageAvailable->setText(moneyText(m_huabeiDebt < 3000.0 ? 3000.0 - m_huabeiDebt : 0.0));
    if (m_vaultPageBalance) {
        m_vaultPageBalance->setProperty("vaultBalance", m_vaultBalance);
        m_vaultPageBalance->setText(moneyText(m_vaultBalance));
    }
}

void MainWindow::refreshDailyCharts()
{
    if (!m_store || !m_store->isReady())
        return;

    if (m_activityChart) {
        QMap<QDate, QMap<QString, double>> activityValues;
        const auto rows = m_store->queryRows(QStringLiteral(
            "SELECT date, project, SUM(numeric_value) FROM body_records "
            "WHERE project IN ('羽球', '骑行', '健身', '网球') "
            "AND numeric_value IS NOT NULL GROUP BY date, project ORDER BY date"));
        for (const auto &row : rows) {
            const QDate date = QDate::fromString(row.at(0).toString(), Qt::ISODate);
            if (date.isValid())
                activityValues[date][row.at(1).toString()] = row.at(2).toDouble();
        }
        m_activityChart->setStackedDailyValues(
            activityValues,
            {QStringLiteral("羽球"), QStringLiteral("骑行"),
             QStringLiteral("健身"), QStringLiteral("网球")},
            m_overviewRangeDays);
    }

    if (m_workHoursChart) {
        QMap<QDate, double> workValues;
        const auto rows = m_store->queryRows(QStringLiteral(
            "SELECT work_date, total_hours FROM work_hours ORDER BY work_date"));
        for (const auto &row : rows) {
            const QDate date = QDate::fromString(row.at(0).toString(), Qt::ISODate);
            if (date.isValid())
                workValues[date] = row.at(1).toDouble();
        }
        m_workHoursChart->setDailyValues(workValues, m_overviewRangeDays);
    }
}

void MainWindow::exportAllData()
{
    const QString selectedFolder = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择数据导出位置"), QDir::homePath());
    if (selectedFolder.isEmpty())
        return;

    QDir root(selectedFolder);
    const QString folderName = QStringLiteral("个人数据导出_%1")
                                   .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    if (!root.mkpath(folderName)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法创建导出文件夹。"));
        return;
    }

    QDir output(root.filePath(folderName));
    int exportedFiles = 0;
    QStringList failedModules;
    const auto tables = m_pages->findChildren<QTableWidget *>();
    for (auto *table : tables) {
        const QString moduleName = table->property("exportName").toString();
        if (moduleName.isEmpty())
            continue;

        QFile file(output.filePath(moduleName + QStringLiteral(".csv")));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            failedModules.append(moduleName);
            continue;
        }

        file.write("\xEF\xBB\xBF");
        QTextStream stream(&file);
        QStringList headerCells;
        for (int column = 0; column < table->columnCount(); ++column) {
            const auto *header = table->horizontalHeaderItem(column);
            headerCells.append(csvCell(header ? header->text() : QString()));
        }
        stream << headerCells.join(QLatin1Char(',')) << '\n';

        for (int row = 0; row < table->rowCount(); ++row) {
            QStringList cells;
            for (int column = 0; column < table->columnCount(); ++column) {
                const auto *item = table->item(row, column);
                cells.append(csvCell(item ? item->text() : QString()));
            }
            stream << cells.join(QLatin1Char(',')) << '\n';
        }
        ++exportedFiles;
    }

    const QString backupName = QStringLiteral("Nothing_%1.nothingdata")
                                   .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString backupPath = output.filePath(backupName);
    const bool backupCreated = m_store && m_store->isReady() && m_store->exportBackup(backupPath);
    if (!backupCreated)
        failedModules.append(QStringLiteral("可迁移数据备份"));

    if (!failedModules.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("部分导出失败"),
                             QStringLiteral("以下栏目未能导出：%1").arg(failedModules.join(QStringLiteral("、"))));
        return;
    }

    QMessageBox::information(this, QStringLiteral("导出完成"),
                             QStringLiteral("已导出 %1 个 CSV 文件和 1 个可迁移备份：\n%2\n\n"
                                            "以后升级软件时，使用“导入备份”选择 %3 即可。")
                                 .arg(exportedFiles)
                                 .arg(output.absolutePath(), backupName));
}

void MainWindow::importAllData()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 Nothing 数据备份"), QDir::homePath(),
        QStringLiteral("Nothing 数据备份 (*.nothingdata);;所有文件 (*.*)"));
    if (filePath.isEmpty())
        return;

    const auto answer = QMessageBox::question(
        this, QStringLiteral("确认导入备份"),
        QStringLiteral("导入会替换当前软件中的全部记录。\n"
                       "建议先导出当前数据。是否继续？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    if (!m_store || !m_store->isReady() || !m_store->importBackup(filePath)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
                             m_store ? m_store->lastError()
                                     : QStringLiteral("SQLite 数据库不可用。"));
        return;
    }

    QMessageBox::information(this, QStringLiteral("导入完成"),
                             QStringLiteral("数据已导入，软件将自动重新启动。"));
    const QString applicationPath = QCoreApplication::applicationFilePath();
    QProcess::startDetached(applicationPath, {});
    qApp->quit();
}

void MainWindow::clearAllData()
{
    const auto answer = QMessageBox::question(
        this, QStringLiteral("确认清除数据"),
        QStringLiteral("将清空所有栏目记录、概览指标和图表。\n此操作无法撤销，是否继续？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    if (!m_store || !m_store->isReady() || !m_store->clearAll()) {
        QMessageBox::warning(this, QStringLiteral("清除失败"),
                             m_store ? m_store->lastError()
                                     : QStringLiteral("SQLite 数据库不可用。"));
        return;
    }

    const auto tables = m_pages->findChildren<QTableWidget *>();
    for (auto *table : tables)
        table->setRowCount(0);

    m_totalIncome = 0.0;
    m_totalExpense = 0.0;
    m_liquidFunds = 0.0;
    m_huabeiDebt = 0.0;
    m_vaultBalance = 0.0;
    m_latestWeight = 0.0;
    m_previousWeight = 0.0;
    m_expenseCategories.clear();

    const auto widgets = m_pages->findChildren<QWidget *>();
    for (auto *widget : widgets) {
        if (auto *lineChart = dynamic_cast<DataLineChart *>(widget))
            lineChart->clearData();
        if (auto *pieChart = dynamic_cast<ExpensePieChart *>(widget))
            pieChart->clearData();
        if (auto *dailyChart = dynamic_cast<DailyMetricChart *>(widget))
            dailyChart->clearData();
    }

    const auto labels = m_pages->findChildren<QLabel *>();
    for (auto *valueLabel : labels) {
        if (valueLabel->property("clearableMetric").toBool())
            valueLabel->setText(QStringLiteral("—"));
        if (valueLabel->property("clearableVaultBalance").toBool()) {
            valueLabel->setProperty("vaultBalance", 0.0);
            valueLabel->setText(QStringLiteral("¥ 0.00"));
        }
        if (valueLabel->property("clearableResearch").toBool())
            valueLabel->setText(QStringLiteral("暂无记录"));
    }
    m_latestWorkSummaries.clear();
    for (auto it = m_workOverviewLabels.begin(); it != m_workOverviewLabels.end(); ++it)
        it.value()->setText(QStringLiteral("暂无记录"));
    refreshOverview();

    QMessageBox::information(this, QStringLiteral("清除完成"), QStringLiteral("当前数据已清空。"));
}
