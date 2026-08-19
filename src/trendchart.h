#pragma once

#include <QDate>
#include <QMap>
#include <QSet>
#include <QVector>
#include <QWidget>

class DataLineChart final : public QWidget
{
public:
    enum class Series {
        LiquidFunds,
        VaultFunds,
        Weight
    };

    explicit DataLineChart(Series series, QWidget *parent = nullptr);
    void setRangeDays(int days);
    void setCurrentValue(double value);
    void setDatedValues(const QMap<QDate, double> &values);
    void appendValue(double value, const QDate &date = QDate::currentDate());
    void clearData();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Series m_series;
    QMap<QDate, double> m_dailyValues;
    QSet<QDate> m_recordedDates;
    int m_rangeDays = 30;
    bool m_hasData = true;
};

class ExpensePieChart final : public QWidget
{
public:
    explicit ExpensePieChart(QWidget *parent = nullptr);
    void setCategoryValues(const QMap<QString, double> &values);
    void clearData();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QStringList m_names;
    QVector<double> m_values;
    bool m_hasData = true;
};

class DailyMetricChart final : public QWidget
{
public:
    enum class Style { Bars, Line };

    explicit DailyMetricChart(Style style, const QString &axisLabel, QWidget *parent = nullptr);
    void setDailyValues(const QMap<QDate, double> &values, int days);
    void setStackedDailyValues(const QMap<QDate, QMap<QString, double>> &values,
                               const QStringList &categories, int days);
    void clearData();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Style m_style;
    QString m_axisLabel;
    QMap<QDate, double> m_values;
    QMap<QDate, QMap<QString, double>> m_stackedValues;
    QStringList m_categories;
    int m_days = 14;
};
