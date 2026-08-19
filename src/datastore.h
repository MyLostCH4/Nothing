#pragma once

#include <QSqlDatabase>
#include <QDate>
#include <QString>
#include <QVariantList>
#include <QVector>

class DataStore final
{
public:
    DataStore();
    ~DataStore();

    bool initialize();
    bool isReady() const;
    QString databasePath() const;
    QString lastError() const;

    bool begin();
    bool commit();
    void rollback();
    bool execute(const QString &sql, const QVariantList &values = {});
    QVector<QVariantList> queryRows(const QString &sql, const QVariantList &values = {});
    double scalar(const QString &sql, const QVariantList &values = {}, double fallback = 0.0);
    qint64 lastInsertId();

    double state(const QString &key, double fallback = 0.0);
    bool setState(const QString &key, double value);
    bool appendSnapshot(const QString &series, double value, const QString &recordedAt);
    QVector<double> snapshots(const QString &series);
    int processRecurringExpenses(const QDate &today);
    bool exportBackup(const QString &filePath);
    bool importBackup(const QString &filePath);
    bool recalculateDerivedState();
    bool clearAll();

private:
    bool createSchema();
    bool removeLegacyDemoData();
    bool hasColumn(const QString &tableName, const QString &columnName);
    bool seedInitialData();
    bool tableIsEmpty(const QString &tableName);
    void setError(const QString &message);

    QString m_connectionName;
    QString m_databasePath;
    QString m_lastError;
    QSqlDatabase m_database;
    bool m_ready = false;
};
