#include "datastore.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>

namespace {

constexpr auto connectionName = "PersonalDataManagerSqlite";

QDate advanceRecurringDate(const QDate &date, const QString &cycle)
{
    if (cycle == QStringLiteral("每季度"))
        return date.addMonths(3);
    if (cycle == QStringLiteral("每年"))
        return date.addYears(1);
    return date.addMonths(1);
}

QDate firstFutureRecurringDate(QDate date, const QString &cycle, const QDate &today)
{
    while (date.isValid() && date <= today)
        date = advanceRecurringDate(date, cycle);
    return date;
}

}

DataStore::DataStore()
    : m_connectionName(QString::fromLatin1(connectionName))
{
}

DataStore::~DataStore()
{
    if (m_database.isValid())
        m_database.close();
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool DataStore::initialize()
{
    QString dataFolder = qEnvironmentVariable("NOTHING_DATA_DIR");
    if (dataFolder.isEmpty()) {
#ifdef NOTHING_DEBUG_BUILD
        dataFolder = QStringLiteral("D:/MyQt/PersonalDataManager/dev-data");
#else
        dataFolder = QStringLiteral("D:/MyQt/PersonalDataManager/data");
#endif
    }
    if ((!qEnvironmentVariableIsEmpty("NOTHING_DATA_DIR") || QDir(QStringLiteral("D:/")).exists())
        && QDir().mkpath(dataFolder)) {
        // Use the explicitly requested location or the normal D: installation folder.
    } else {
        dataFolder = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
                         .filePath(QStringLiteral("PersonalDataManager/data"));
    }
    if (dataFolder.isEmpty() || !QDir().mkpath(dataFolder)) {
        setError(QStringLiteral("无法创建数据库目录。"));
        return false;
    }

    m_databasePath = QDir(dataFolder).filePath(QStringLiteral("personal_data.db"));
    if (QSqlDatabase::contains(m_connectionName))
        QSqlDatabase::removeDatabase(m_connectionName);
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_databasePath);
    if (!m_database.open()) {
        setError(QStringLiteral("无法打开 SQLite 数据库：%1").arg(m_database.lastError().text()));
        return false;
    }

    execute(QStringLiteral("PRAGMA foreign_keys = ON"));
    execute(QStringLiteral("PRAGMA journal_mode = WAL"));

    const bool hasExistingData = scalar(QStringLiteral(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='app_state'")) > 0.0;
    const bool hasMetadata = scalar(QStringLiteral(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='app_meta'")) > 0.0;
    const int schemaVersion = hasMetadata
        ? static_cast<int>(scalar(QStringLiteral(
              "SELECT CAST(value AS INTEGER) FROM app_meta WHERE key='schema_version'"), {}, 0.0))
        : 0;
    if (hasExistingData && schemaVersion < 4) {
        const QString backupFolder = QDir(dataFolder).filePath(QStringLiteral("backups"));
        if (!QDir().mkpath(backupFolder)) {
            setError(QStringLiteral("升级前无法创建自动备份目录。"));
            return false;
        }
        const QString backupPath = QDir(backupFolder).filePath(
            QStringLiteral("pre_upgrade_v%1_to_v4_%2.nothingdata")
                .arg(schemaVersion)
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"))));
        if (!execute(QStringLiteral("VACUUM INTO ?"), {QDir::toNativeSeparators(backupPath)})) {
            setError(QStringLiteral("数据库升级前自动备份失败：%1").arg(m_lastError));
            return false;
        }
    }
    if (!createSchema())
        return false;
    if (schemaVersion < 4) {
        if (!begin() || !removeLegacyDemoData()
            || !execute(QStringLiteral(
                "UPDATE huabei SET expense_id = (SELECT id FROM expense "
                "WHERE payment='花呗' AND expense.date=huabei.date AND expense.amount=huabei.amount "
                "AND expense.category=huabei.category AND expense.note=huabei.note "
                "ORDER BY expense.id DESC LIMIT 1) "
                "WHERE operation='新增欠款' AND expense_id IS NULL"))
            || !execute(QStringLiteral(
                "INSERT INTO app_meta(key, value) VALUES('schema_version', '4') "
                "ON CONFLICT(key) DO UPDATE SET value = excluded.value"))
            || !commit()) {
            rollback();
            return false;
        }
    }
    if (tableIsEmpty(QStringLiteral("app_state")) && !seedInitialData())
        return false;

    m_ready = true;
    m_lastError.clear();
    return true;
}

bool DataStore::isReady() const
{
    return m_ready;
}

QString DataStore::databasePath() const
{
    return m_databasePath;
}

QString DataStore::lastError() const
{
    return m_lastError;
}

bool DataStore::begin()
{
    if (!m_database.transaction()) {
        setError(m_database.lastError().text());
        return false;
    }
    return true;
}

bool DataStore::commit()
{
    if (!m_database.commit()) {
        setError(m_database.lastError().text());
        return false;
    }
    return true;
}

void DataStore::rollback()
{
    m_database.rollback();
}

bool DataStore::execute(const QString &sql, const QVariantList &values)
{
    QSqlQuery query(m_database);
    if (!query.prepare(sql)) {
        setError(query.lastError().text());
        return false;
    }
    for (const QVariant &value : values)
        query.addBindValue(value);
    if (!query.exec()) {
        setError(query.lastError().text());
        return false;
    }
    return true;
}

QVector<QVariantList> DataStore::queryRows(const QString &sql, const QVariantList &values)
{
    QVector<QVariantList> rows;
    QSqlQuery query(m_database);
    if (!query.prepare(sql)) {
        setError(query.lastError().text());
        return rows;
    }
    for (const QVariant &value : values)
        query.addBindValue(value);
    if (!query.exec()) {
        setError(query.lastError().text());
        return rows;
    }
    while (query.next()) {
        QVariantList row;
        for (int column = 0; column < query.record().count(); ++column)
            row.append(query.value(column));
        rows.append(row);
    }
    return rows;
}

double DataStore::scalar(const QString &sql, const QVariantList &values, double fallback)
{
    const auto rows = queryRows(sql, values);
    if (rows.isEmpty() || rows.first().isEmpty() || rows.first().first().isNull())
        return fallback;
    return rows.first().first().toDouble();
}

qint64 DataStore::lastInsertId()
{
    return static_cast<qint64>(scalar(QStringLiteral("SELECT last_insert_rowid()")));
}

double DataStore::state(const QString &key, double fallback)
{
    return scalar(QStringLiteral("SELECT value FROM app_state WHERE key = ?"), {key}, fallback);
}

bool DataStore::setState(const QString &key, double value)
{
    return execute(QStringLiteral(
                       "INSERT INTO app_state(key, value) VALUES(?, ?) "
                       "ON CONFLICT(key) DO UPDATE SET value = excluded.value"),
                   {key, value});
}

bool DataStore::appendSnapshot(const QString &series, double value, const QString &recordedAt)
{
    return execute(QStringLiteral(
                       "INSERT INTO chart_snapshots(series, recorded_at, value) VALUES(?, ?, ?)"),
                   {series, recordedAt, value});
}

QVector<double> DataStore::snapshots(const QString &series)
{
    QVector<double> values;
    const auto rows = queryRows(
        QStringLiteral("SELECT value FROM chart_snapshots WHERE series = ? ORDER BY id ASC"), {series});
    for (const auto &row : rows)
        values.append(row.first().toDouble());
    return values;
}

int DataStore::processRecurringExpenses(const QDate &today)
{
    if (!m_ready || !today.isValid())
        return 0;

    const auto rows = queryRows(QStringLiteral(
        "SELECT id, name, amount, cycle, start_date, next_due_date "
        "FROM recurring WHERE active = 1 ORDER BY id ASC"));
    if (!begin())
        return -1;

    double liquidFunds = state(QStringLiteral("liquid_funds"));
    int chargedCount = 0;
    for (const auto &row : rows) {
        const int recurringId = row.at(0).toInt();
        const QString name = row.at(1).toString();
        const double amount = row.at(2).toDouble();
        const QString cycle = row.at(3).toString();
        const QDate startDate = QDate::fromString(row.at(4).toString(), Qt::ISODate);
        QDate dueDate = QDate::fromString(row.at(5).toString(), Qt::ISODate);

        // Legacy rows did not store a next charge date. Initialize them to the
        // first future occurrence so upgrading never creates surprise back-payments.
        if (!dueDate.isValid()) {
            dueDate = firstFutureRecurringDate(startDate, cycle, today);
            if (!dueDate.isValid()
                || !execute(QStringLiteral("UPDATE recurring SET next_due_date = ? WHERE id = ?"),
                            {dueDate.toString(Qt::ISODate), recurringId})) {
                rollback();
                return -1;
            }
            continue;
        }

        while (dueDate <= today) {
            if (!execute(QStringLiteral(
                    "INSERT INTO recurring_charges(recurring_id, due_date, amount) VALUES(?, ?, ?)"),
                    {recurringId, dueDate.toString(Qt::ISODate), amount})
                || !execute(QStringLiteral(
                    "INSERT INTO expense(date, amount, category, note, payment) "
                    "VALUES(?, ?, '周期消费', ?, '流动资金')"),
                    {dueDate.toString(Qt::ISODate), amount, name})) {
                rollback();
                return -1;
            }
            liquidFunds -= amount;
            if (!appendSnapshot(QStringLiteral("liquid"), liquidFunds,
                                dueDate.toString(QStringLiteral("yyyy-MM-dd 12:00:00")))) {
                rollback();
                return -1;
            }
            ++chargedCount;
            dueDate = advanceRecurringDate(dueDate, cycle);
        }

        if (!execute(QStringLiteral("UPDATE recurring SET next_due_date = ? WHERE id = ?"),
                     {dueDate.toString(Qt::ISODate), recurringId})) {
            rollback();
            return -1;
        }
    }

    if (chargedCount > 0 && !setState(QStringLiteral("liquid_funds"), liquidFunds)) {
        rollback();
        return -1;
    }
    if (!commit()) {
        rollback();
        return -1;
    }
    return chargedCount;
}

bool DataStore::exportBackup(const QString &filePath)
{
    if (!m_ready || filePath.isEmpty())
        return false;
    QFile::remove(filePath);
    return execute(QStringLiteral("VACUUM INTO ?"), {QDir::toNativeSeparators(filePath)});
}

bool DataStore::importBackup(const QString &filePath)
{
    if (!m_ready || !QFile::exists(filePath)) {
        setError(QStringLiteral("备份文件不存在。"));
        return false;
    }
    execute(QStringLiteral("DETACH DATABASE nothing_import"));
    if (!execute(QStringLiteral("ATTACH DATABASE ? AS nothing_import"),
                 {QDir::toNativeSeparators(filePath)}))
        return false;

    const auto sourceRows = queryRows(QStringLiteral(
        "SELECT name FROM nothing_import.sqlite_master WHERE type = 'table'"));
    QStringList sourceTables;
    for (const auto &row : sourceRows)
        sourceTables.append(row.first().toString());
    if (!sourceTables.contains(QStringLiteral("app_state"))) {
        execute(QStringLiteral("DETACH DATABASE nothing_import"));
        setError(QStringLiteral("这不是有效的 Nothing 数据备份。"));
        return false;
    }

    const QStringList deleteOrder{
        QStringLiteral("recurring_charges"), QStringLiteral("income"), QStringLiteral("expense"),
        QStringLiteral("recurring"), QStringLiteral("huabei"), QStringLiteral("vault"),
        QStringLiteral("body_records"), QStringLiteral("work_notes"), QStringLiteral("work_hours"),
        QStringLiteral("chart_snapshots"), QStringLiteral("app_state")};
    const QStringList copyOrder{
        QStringLiteral("app_state"), QStringLiteral("income"), QStringLiteral("expense"),
        QStringLiteral("recurring"), QStringLiteral("recurring_charges"), QStringLiteral("huabei"),
        QStringLiteral("vault"), QStringLiteral("body_records"), QStringLiteral("work_notes"),
        QStringLiteral("work_hours"), QStringLiteral("chart_snapshots")};

    if (!begin()) {
        execute(QStringLiteral("DETACH DATABASE nothing_import"));
        return false;
    }
    for (const QString &table : deleteOrder) {
        if (!execute(QStringLiteral("DELETE FROM main.%1").arg(table))) {
            rollback();
            execute(QStringLiteral("DETACH DATABASE nothing_import"));
            return false;
        }
    }

    for (const QString &table : copyOrder) {
        if (!sourceTables.contains(table))
            continue;
        QStringList destinationColumns;
        for (const auto &row : queryRows(QStringLiteral("PRAGMA main.table_info(%1)").arg(table)))
            destinationColumns.append(row.at(1).toString());
        QStringList sourceColumns;
        for (const auto &row : queryRows(QStringLiteral("PRAGMA nothing_import.table_info(%1)").arg(table)))
            sourceColumns.append(row.at(1).toString());
        QStringList commonColumns;
        for (const QString &column : destinationColumns) {
            if (sourceColumns.contains(column))
                commonColumns.append(QStringLiteral("\"%1\"").arg(column));
        }
        if (commonColumns.isEmpty())
            continue;
        const QString columns = commonColumns.join(QStringLiteral(", "));
        if (!execute(QStringLiteral(
                "INSERT INTO main.%1 (%2) SELECT %2 FROM nothing_import.%1")
                         .arg(table, columns))) {
            rollback();
            execute(QStringLiteral("DETACH DATABASE nothing_import"));
            return false;
        }
    }

    if (!removeLegacyDemoData()
        || !setState(QStringLiteral("liquid_funds"), state(QStringLiteral("liquid_funds"), 0.0))
        || !setState(QStringLiteral("huabei_debt"), state(QStringLiteral("huabei_debt"), 0.0))
        || !setState(QStringLiteral("vault_balance"), state(QStringLiteral("vault_balance"), 0.0))
        || !commit()) {
        rollback();
        execute(QStringLiteral("DETACH DATABASE nothing_import"));
        return false;
    }
    if (!execute(QStringLiteral("DETACH DATABASE nothing_import")))
        return false;
    return true;
}

bool DataStore::clearAll()
{
    if (!begin())
        return false;
    const QStringList tables{
        QStringLiteral("income"), QStringLiteral("expense"), QStringLiteral("recurring_charges"),
        QStringLiteral("recurring"),
        QStringLiteral("huabei"), QStringLiteral("vault"), QStringLiteral("body_records"),
        QStringLiteral("work_notes"), QStringLiteral("work_hours"), QStringLiteral("chart_snapshots")};
    for (const QString &table : tables) {
        if (!execute(QStringLiteral("DELETE FROM %1").arg(table))) {
            rollback();
            return false;
        }
    }
    if (!setState(QStringLiteral("liquid_funds"), 0.0)
        || !setState(QStringLiteral("huabei_debt"), 0.0)
        || !setState(QStringLiteral("vault_balance"), 0.0)) {
        rollback();
        return false;
    }
    if (!commit()) {
        rollback();
        return false;
    }
    return true;
}

bool DataStore::createSchema()
{
    const QStringList statements{
        QStringLiteral("CREATE TABLE IF NOT EXISTS app_state("
                       "key TEXT PRIMARY KEY, value REAL NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS app_meta("
                       "key TEXT PRIMARY KEY, value TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS income("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, date TEXT NOT NULL, amount REAL NOT NULL, "
                       "category TEXT NOT NULL, note TEXT NOT NULL DEFAULT '')"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS expense("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, date TEXT NOT NULL, amount REAL NOT NULL, "
                       "category TEXT NOT NULL, note TEXT NOT NULL DEFAULT '', payment TEXT NOT NULL DEFAULT '流动资金')"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS recurring("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, amount REAL NOT NULL, "
                       "cycle TEXT NOT NULL, start_date TEXT NOT NULL, "
                       "next_due_date TEXT NOT NULL DEFAULT '', active INTEGER NOT NULL DEFAULT 1)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS recurring_charges("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, recurring_id INTEGER NOT NULL, "
                       "due_date TEXT NOT NULL, amount REAL NOT NULL, "
                       "UNIQUE(recurring_id, due_date))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS huabei("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, date TEXT NOT NULL, operation TEXT NOT NULL, "
                       "amount REAL NOT NULL, category TEXT NOT NULL DEFAULT '', note TEXT NOT NULL DEFAULT '', "
                       "expense_id INTEGER)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS vault("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, date TEXT NOT NULL, operation TEXT NOT NULL, "
                       "amount REAL NOT NULL, note TEXT NOT NULL DEFAULT '')"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS body_records("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, date TEXT NOT NULL, project TEXT NOT NULL, "
                       "data_text TEXT NOT NULL, numeric_value REAL, note TEXT NOT NULL DEFAULT '')"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS work_notes("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, recorded_at TEXT NOT NULL, section TEXT NOT NULL, "
                       "idea TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS work_hours("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, work_date TEXT NOT NULL UNIQUE, "
                       "morning_start TEXT NOT NULL, morning_end TEXT NOT NULL, "
                       "afternoon_start TEXT NOT NULL, afternoon_end TEXT NOT NULL, "
                       "evening_start TEXT NOT NULL, evening_end TEXT NOT NULL, "
                       "total_hours REAL NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS chart_snapshots("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, series TEXT NOT NULL, recorded_at TEXT NOT NULL, "
                       "value REAL NOT NULL)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_expense_date ON expense(date)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_body_project ON body_records(project, id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_snapshots_series ON chart_snapshots(series, id)")};
    for (const QString &statement : statements) {
        if (!execute(statement))
            return false;
    }

    // Forward-only migration for databases created by earlier releases.
    if (!hasColumn(QStringLiteral("recurring"), QStringLiteral("next_due_date"))
        && !execute(QStringLiteral(
            "ALTER TABLE recurring ADD COLUMN next_due_date TEXT NOT NULL DEFAULT ''")))
        return false;
    if (!hasColumn(QStringLiteral("recurring"), QStringLiteral("active"))
        && !execute(QStringLiteral(
            "ALTER TABLE recurring ADD COLUMN active INTEGER NOT NULL DEFAULT 1")))
        return false;
    if (!hasColumn(QStringLiteral("huabei"), QStringLiteral("expense_id"))
        && !execute(QStringLiteral("ALTER TABLE huabei ADD COLUMN expense_id INTEGER")))
        return false;
    return true;
}

bool DataStore::hasColumn(const QString &tableName, const QString &columnName)
{
    const auto rows = queryRows(QStringLiteral("PRAGMA table_info(%1)").arg(tableName));
    for (const auto &row : rows) {
        if (row.size() > 1 && row.at(1).toString() == columnName)
            return true;
    }
    return false;
}

bool DataStore::removeLegacyDemoData()
{
    const double demoRows = scalar(QStringLiteral(
        "SELECT "
        "(SELECT COUNT(*) FROM income WHERE date='2025-05-10' AND amount=8500 "
        " AND category='工资' AND note='五月工资') + "
        "(SELECT COUNT(*) FROM recurring WHERE name='房租与水电' AND amount=1650 "
        " AND cycle='每月' AND start_date='2025-05-01') + "
        "(SELECT COUNT(*) FROM body_records WHERE date='2025-05-31' AND project='体重' "
        " AND numeric_value=61.5) + "
        "(SELECT COUNT(*) FROM work_notes WHERE recorded_at='2025-05-31 16:20' "
        " AND section='试验' AND idea='优化蛋白质纯化流程')"));
    if (demoRows <= 0.0)
        return true;

    const QStringList cleanupStatements{
        QStringLiteral("DELETE FROM income WHERE "
                       "(date='2025-05-10' AND amount=8500 AND category='工资' AND note='五月工资') OR "
                       "(date='2025-05-18' AND amount=460 AND category='报销' AND note='试验材料报销')"),
        QStringLiteral("DELETE FROM expense WHERE note='五月累计' AND date BETWEEN '2025-05-01' AND '2025-05-31' "
                       "AND category IN ('简餐','下馆子','下厨','交通','运动补给','零食','医疗支出','购物','日用品','试验垫付')"),
        QStringLiteral("DELETE FROM recurring WHERE "
                       "(name='房租与水电' AND amount=1650 AND cycle='每月' AND start_date='2025-05-01') OR "
                       "(name='互联网数字服务' AND amount=148 AND cycle='每月' AND start_date='2025-05-08') OR "
                       "(name='宽带' AND amount=1200 AND cycle='每年' AND start_date='2025-03-01')"),
        QStringLiteral("DELETE FROM huabei WHERE "
                       "(date='2025-05-01' AND operation='期初欠款' AND amount=968.5 AND note='期初余额') OR "
                       "(date='2025-05-28' AND operation='新增欠款' AND amount=268 AND category='日用品' AND note='生活用品')"),
        QStringLiteral("DELETE FROM vault WHERE "
                       "(date='2025-05-17' AND operation='支出' AND amount=1200 AND note='应急维修') OR "
                       "(date='2025-05-31' AND operation='存入' AND amount=4200 AND note='五月结余手动转入')"),
        QStringLiteral("DELETE FROM body_records WHERE "
                       "(date='2025-05-24' AND project='体重' AND numeric_value=61.8) OR "
                       "(date='2025-05-30' AND project='羽球' AND numeric_value=90) OR "
                       "(date='2025-05-31' AND project='体重' AND numeric_value=61.5)"),
        QStringLiteral("DELETE FROM work_notes WHERE "
                       "(recorded_at='2025-05-30 10:15' AND section='理论' AND idea='重新梳理 mTOR 信号通路文献') OR "
                       "(recorded_at='2025-05-31 16:20' AND section='试验' AND idea='优化蛋白质纯化流程') OR "
                       "(recorded_at='2025-05-31 19:10' AND section='小组工作' AND idea='整理下周分工与试验排期')"),
        QStringLiteral("DELETE FROM chart_snapshots WHERE series IN ('liquid','vault')")};
    for (const QString &statement : cleanupStatements) {
        if (!execute(statement))
            return false;
    }

    return recalculateDerivedState();
}

bool DataStore::recalculateDerivedState()
{
    const double liquidFunds = scalar(QStringLiteral(
        "SELECT COALESCE((SELECT SUM(amount) FROM income),0) "
        "- COALESCE((SELECT SUM(amount) FROM expense WHERE payment='流动资金'),0) "
        "- COALESCE((SELECT SUM(amount) FROM huabei WHERE operation='还款'),0) "
        "- COALESCE((SELECT SUM(amount) FROM vault WHERE operation='存入'),0)"));
    const double vaultBalance = scalar(QStringLiteral(
        "SELECT COALESCE(SUM(CASE WHEN operation='存入' THEN amount ELSE -amount END),0) FROM vault"));
    double huabeiDebt = 0.0;
    const auto huabeiRows = queryRows(QStringLiteral(
        "SELECT operation, amount FROM huabei ORDER BY id ASC"));
    for (const auto &row : huabeiRows) {
        const QString operation = row.at(0).toString();
        const double amount = row.at(1).toDouble();
        if (operation == QStringLiteral("欠款校准"))
            huabeiDebt = amount;
        else if (operation == QStringLiteral("还款"))
            huabeiDebt -= amount;
        else
            huabeiDebt += amount;
    }
    return setState(QStringLiteral("liquid_funds"), liquidFunds)
        && setState(QStringLiteral("vault_balance"), vaultBalance)
        && setState(QStringLiteral("huabei_debt"), std::max(0.0, huabeiDebt));
}

bool DataStore::seedInitialData()
{
    if (!begin())
        return false;
    if (!setState(QStringLiteral("liquid_funds"), 0.0)
        || !setState(QStringLiteral("huabei_debt"), 0.0)
        || !setState(QStringLiteral("vault_balance"), 0.0)
        || !commit()) {
        rollback();
        return false;
    }
    return true;
}

bool DataStore::tableIsEmpty(const QString &tableName)
{
    return scalar(QStringLiteral("SELECT COUNT(*) FROM %1").arg(tableName)) == 0.0;
}

void DataStore::setError(const QString &message)
{
    m_lastError = message;
}
