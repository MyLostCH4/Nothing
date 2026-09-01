#pragma once

#include <QMainWindow>
#include <QDate>
#include <QMap>
#include <QPointer>
#include <QStringList>
#include <QVector>

#include <functional>
#include <memory>

class QButtonGroup;
class QLabel;
class QStackedWidget;
class QTableWidget;
class QWidget;
class DataLineChart;
class ExpensePieChart;
class DailyMetricChart;
class DataStore;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    QWidget *createSidebar();
    QWidget *createOverviewPage();
    QWidget *createMoneyPage(const QString &title, const QStringList &categories, bool income);
    QWidget *createRecurringPage();
    QWidget *createHuabeiPage();
    QWidget *createVaultPage();
    QWidget *createBodyPage();
    QWidget *createResearchPage();
    void exportAllData();
    void importAllData();
    void clearAllData();
    void refreshOverview();
    void refreshDailyCharts();
    void rebuildLiquidHistory();
    void refreshChartsFromStoredState();
    void refreshBodyLatestLabels();
    void refreshWorkSummary(const QString &section);
    void loadStoredState();
    void openWorkNoteEditor(QTableWidget *table, const QString &section, int row);
    bool saveWorkNote(QTableWidget *table, const QString &section, qint64 recordId,
                      const QString &text);
    void enableRowDeletion(QTableWidget *table, const QString &storageTable);
    bool deleteStoredRecord(const QString &storageTable, qint64 recordId);
    void removeRecordFromTable(QTableWidget *table, qint64 recordId);
    bool runStorageTransaction(const std::function<bool()> &action);

    QStackedWidget *m_pages = nullptr;
    QButtonGroup *m_navigation = nullptr;
    QLabel *m_overviewWeight = nullptr;
    QLabel *m_overviewIncome = nullptr;
    QLabel *m_overviewExpense = nullptr;
    QLabel *m_overviewBalance = nullptr;
    QLabel *m_overviewHuabei = nullptr;
    QLabel *m_huabeiPageUsed = nullptr;
    QLabel *m_huabeiPageAvailable = nullptr;
    QLabel *m_vaultPageBalance = nullptr;
    QMap<QString, QLabel *> m_workOverviewLabels;
    QTableWidget *m_expenseTable = nullptr;
    QTableWidget *m_huabeiTable = nullptr;
    DataLineChart *m_liquidChart = nullptr;
    DataLineChart *m_vaultChart = nullptr;
    DataLineChart *m_weightChart = nullptr;
    ExpensePieChart *m_expensePie = nullptr;
    DailyMetricChart *m_activityChart = nullptr;
    DailyMetricChart *m_workHoursChart = nullptr;
    std::unique_ptr<DataStore> m_store;
    double m_totalIncome = 0.0;
    double m_totalExpense = 0.0;
    double m_liquidFunds = 0.0;
    double m_huabeiDebt = 0.0;
    double m_vaultBalance = 0.0;
    double m_latestWeight = 0.0;
    double m_previousWeight = 0.0;
    QMap<QString, double> m_expenseCategories;
    QMap<QDate, double> m_storedLiquidHistory;
    QMap<QDate, double> m_storedVaultHistory;
    QMap<QDate, double> m_storedWeightHistory;
    int m_overviewRangeDays = 30;
    QMap<QString, QString> m_latestWorkSummaries;
    QMap<qint64, QPointer<QWidget>> m_openWorkNoteEditors;
};

QMainWindow *createMainWindow();
