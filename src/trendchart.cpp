#include "trendchart.h"

#include <QDate>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

#include <algorithm>
#include <cmath>
#include <limits>

DataLineChart::DataLineChart(Series series, QWidget *parent)
    : QWidget(parent)
    , m_series(series)
{
    setMinimumSize(320, 160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

}

void DataLineChart::setRangeDays(int days)
{
    m_rangeDays = std::clamp(days, 7, 90);
    update();
}

void DataLineChart::setCurrentValue(double value)
{
    m_dailyValues[QDate::currentDate()] = value;
    m_hasData = true;
    update();
}

void DataLineChart::setDatedValues(const QMap<QDate, double> &values)
{
    m_dailyValues = values;
    m_recordedDates = QSet<QDate>(values.keyBegin(), values.keyEnd());
    m_hasData = true;
    update();
}

void DataLineChart::appendValue(double value, const QDate &date)
{
    const QDate effectiveDate = date.isValid() ? date : QDate::currentDate();
    m_dailyValues[effectiveDate] = value;
    m_recordedDates.insert(effectiveDate);
    while (!m_dailyValues.isEmpty() && m_dailyValues.firstKey() < QDate::currentDate().addDays(-365)) {
        m_recordedDates.remove(m_dailyValues.firstKey());
        m_dailyValues.erase(m_dailyValues.begin());
    }
    m_hasData = true;
    update();
}

void DataLineChart::clearData()
{
    m_dailyValues.clear();
    m_recordedDates.clear();
    m_hasData = true;
    update();
}

void DataLineChart::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), Qt::white);

    if (!m_hasData) {
        painter.setPen(QColor("#111111"));
        painter.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂无数据"));
        return;
    }

    const int left = 64;
    const int top = 24;
    const int right = 18;
    const int bottom = 42;
    const QRectF plot(left, top, width() - left - right, height() - top - bottom);
    if (plot.width() <= 0 || plot.height() <= 0)
        return;

    const int count = m_rangeDays;
    QDate endDate = QDate::currentDate();
    if (!m_dailyValues.isEmpty() && m_dailyValues.lastKey() > endDate)
        endDate = m_dailyValues.lastKey();
    const QDate startDate = endDate.addDays(-(count - 1));
    const bool isWeight = m_series == Series::Weight;
    const double missingValue = std::numeric_limits<double>::quiet_NaN();
    QVector<double> visibleValues;
    visibleValues.reserve(count);
    // Balances are continuous state and therefore carry forward. Weight is a
    // sampled measurement: missing days must remain missing, otherwise the chart
    // invents zeroes/steps and destroys the useful Y-axis range.
    double carriedValue = isWeight ? missingValue : 0.0;
    if (!isWeight) {
        auto beforeStart = m_dailyValues.lowerBound(startDate);
        if (beforeStart != m_dailyValues.begin()) {
            --beforeStart;
            carriedValue = beforeStart.value();
        }
    }
    for (int i = 0; i < count; ++i) {
        const QDate date = startDate.addDays(i);
        if (isWeight) {
            const double candidate = m_dailyValues.value(date, missingValue);
            visibleValues.append(std::isfinite(candidate) && candidate > 0.0
                                     ? candidate
                                     : missingValue);
        } else {
            if (m_dailyValues.contains(date))
                carriedValue = m_dailyValues.value(date);
            visibleValues.append(carriedValue);
        }
    }

    QVector<double> scaleValues;
    scaleValues.reserve(visibleValues.size());
    for (const double value : visibleValues) {
        if (std::isfinite(value))
            scaleValues.append(value);
    }
    if (scaleValues.isEmpty()) {
        painter.setPen(QColor("#111111"));
        painter.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂无数据"));
        return;
    }

    const double rawMin = *std::min_element(scaleValues.cbegin(), scaleValues.cend());
    const double rawMax = *std::max_element(scaleValues.cbegin(), scaleValues.cend());
    double axisMin = 0.0;
    double axisMax = 0.0;
    double weightTickStep = 0.5;
    if (isWeight) {
        // Focus the scale on the measurements in view. A 15% margin keeps the
        // line away from the frame, while a 2 kg minimum span avoids making tiny
        // sensor/rounding changes look disproportionately dramatic.
        const double dataSpan = rawMax - rawMin;
        const double targetSpan = std::max(dataSpan * 1.30, 2.0);
        const double center = (rawMin + rawMax) / 2.0;
        const double targetMin = center - targetSpan / 2.0;
        const double targetMax = center + targetSpan / 2.0;
        const auto niceCeiling = [](double value) {
            const double exponent = std::floor(std::log10(std::max(value, 1e-9)));
            const double magnitude = std::pow(10.0, exponent);
            const double fraction = value / magnitude;
            double niceFraction = 10.0;
            if (fraction <= 1.0)
                niceFraction = 1.0;
            else if (fraction <= 2.0)
                niceFraction = 2.0;
            else if (fraction <= 2.5)
                niceFraction = 2.5;
            else if (fraction <= 5.0)
                niceFraction = 5.0;
            return niceFraction * magnitude;
        };
        weightTickStep = niceCeiling(targetSpan / 4.0);
        axisMin = std::floor(targetMin / weightTickStep) * weightTickStep;
        axisMax = std::ceil(targetMax / weightTickStep) * weightTickStep;
    } else {
        const double basePadding = std::max((rawMax - rawMin) * 0.12, 250.0);
        constexpr double moneyTickStep = 500.0;
        axisMin = std::floor((rawMin - basePadding) / moneyTickStep) * moneyTickStep;
        axisMax = std::ceil((rawMax + basePadding) / moneyTickStep) * moneyTickStep;
    }
    const double minimumSpan = isWeight ? weightTickStep : 500.0;
    const double span = std::max(axisMax - axisMin, minimumSpan);

    painter.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 8));
    painter.setPen(QColor("#111111"));
    painter.drawText(QRectF(0, 0, 84, 20), Qt::AlignCenter,
                     isWeight ? QStringLiteral("体重（kg）") : QStringLiteral("金额（元）"));

    const int gridCount = isWeight
        ? std::clamp(static_cast<int>(std::lround((axisMax - axisMin) / weightTickStep)), 4, 6)
        : 4;
    for (int line = 0; line <= gridCount; ++line) {
        const qreal ratio = line / static_cast<qreal>(gridCount);
        const qreal y = plot.bottom() - ratio * plot.height();
        painter.setPen(QPen(QColor("#d9dde3"), 1, Qt::DashLine));
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.setPen(QColor("#111111"));
        painter.drawText(QRectF(2, y - 9, left - 10, 18), Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(axisMin + span * ratio, 'f', isWeight ? 1 : 0));
    }

    painter.setPen(QPen(QColor("#30343b"), 1));
    painter.drawLine(plot.bottomLeft(), plot.topLeft());
    painter.drawLine(plot.bottomLeft(), plot.bottomRight());

    const int pointDivisor = std::max(count - 1, 1);
    constexpr int xTickCount = 4;
    for (int tick = 0; tick <= xTickCount; ++tick) {
        const qreal ratio = tick / static_cast<qreal>(xTickCount);
        const qreal x = plot.left() + ratio * plot.width();
        const int offset = count > 1 ? std::round(ratio * (count - 1)) : 0;
        painter.drawLine(QPointF(x, plot.bottom()), QPointF(x, plot.bottom() + 4));
        painter.drawText(QRectF(x - 28, plot.bottom() + 8, 56, 22), Qt::AlignCenter,
                         startDate.addDays(offset).toString(QStringLiteral("MM-dd")));
    }

    QPainterPath path;
    bool pathStarted = false;
    for (int i = 0; i < count; ++i) {
        if (!std::isfinite(visibleValues.at(i)))
            continue;
        const qreal x = plot.left() + i / static_cast<qreal>(pointDivisor) * plot.width();
        const qreal y = plot.bottom() - (visibleValues.at(i) - axisMin) / span * plot.height();
        if (!pathStarted) {
            path.moveTo(x, y);
            pathStarted = true;
        } else {
            path.lineTo(x, y);
        }
    }

    QColor lineColor("#165fd0");
    if (m_series == Series::VaultFunds)
        lineColor = QColor("#5c83b8");
    else if (m_series == Series::Weight)
        lineColor = QColor("#22262c");
    painter.setPen(QPen(lineColor, 2.1));
    painter.drawPath(path);

    // Only points added through user actions are emphasized. The seeded trend
    // remains a clean line, so newly recorded observations are easy to spot.
    if (m_rangeDays <= 14) for (int i = 0; i < count; ++i) {
        const QDate date = startDate.addDays(i);
        if (!m_recordedDates.contains(date))
            continue;
        const double recordedValue = m_dailyValues.value(date, missingValue);
        if (!std::isfinite(recordedValue) || (isWeight && recordedValue <= 0.0))
            continue;
        const qreal x = plot.left() + i / static_cast<qreal>(pointDivisor) * plot.width();
        const qreal y = plot.bottom() - (visibleValues.at(i) - axisMin) / span * plot.height();
        const QPointF point(x, y);
        painter.setPen(QPen(Qt::white, 2.4));
        painter.setBrush(QColor("#e95420"));
        painter.drawEllipse(point, 5.8, 5.8);
        painter.setPen(QPen(QColor("#a72d08"), 1.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(point, 4.8, 4.8);
    }
}

ExpensePieChart::ExpensePieChart(QWidget *parent)
    : QWidget(parent)
    , m_names{QStringLiteral("餐饮"), QStringLiteral("交通"), QStringLiteral("购物"),
              QStringLiteral("日用"), QStringLiteral("娱乐"), QStringLiteral("学习"),
              QStringLiteral("其他")}
    , m_values{32.6, 18.7, 16.4, 10.8, 8.9, 6.4, 6.2}
{
    setMinimumSize(320, 160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ExpensePieChart::setCategoryValues(const QMap<QString, double> &values)
{
    m_names.clear();
    m_values.clear();
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (it.value() <= 0.0)
            continue;
        m_names.append(it.key());
        m_values.append(it.value());
    }
    m_hasData = !m_values.isEmpty();
    update();
}

void ExpensePieChart::clearData()
{
    m_names.clear();
    m_values.clear();
    m_hasData = false;
    update();
}

void ExpensePieChart::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), Qt::white);
    painter.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));

    if (!m_hasData) {
        painter.setPen(QColor("#111111"));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂无数据"));
        return;
    }

    // Vivid Ubuntu-inspired palette: orange and aubergine lead, followed by
    // strongly separated warm/cool hues for quick category recognition.
    const QVector<QColor> colors{QColor("#e95420"), QColor("#77216f"), QColor("#f5b800"),
                                 QColor("#0e8a6d"), QColor("#2675d8"), QColor("#c7162b"),
                                 QColor("#5e2750"), QColor("#65a30d"), QColor("#a347ba"),
                                 QColor("#00a6c8")};
    double total = 0.0;
    for (const double value : m_values)
        total += value;
    if (total <= 0.0)
        return;

    const int diameter = std::min(height() - 32, static_cast<int>(width() * 0.42));
    const QRectF pieRect(24, (height() - diameter) / 2.0, diameter, diameter);
    int startAngle = 90 * 16;
    for (int i = 0; i < m_values.size(); ++i) {
        const int spanAngle = -std::round(m_values.at(i) / total * 360.0 * 16.0);
        painter.setPen(QPen(Qt::white, 1.6));
        painter.setBrush(colors.at(i % colors.size()));
        painter.drawPie(pieRect, startAngle, spanAngle);
        startAngle += spanAngle;
    }

    const qreal legendX = pieRect.right() + 28;
    const qreal rowHeight = std::max(18.0, (height() - 20.0) / static_cast<qreal>(m_names.size()));
    qreal y = 14;
    for (int i = 0; i < m_names.size(); ++i) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.at(i % colors.size()));
        painter.drawRect(QRectF(legendX, y + 5, 12, 12));
        painter.setPen(QColor("#111111"));
        painter.drawText(QRectF(legendX + 20, y, width() - legendX - 24, 22),
                         Qt::AlignVCenter,
                         QStringLiteral("%1  %2%")
                             .arg(m_names.at(i))
                             .arg(m_values.at(i) / total * 100.0, 0, 'f', 1));
        y += rowHeight;
    }
}

DailyMetricChart::DailyMetricChart(Style style, const QString &axisLabel, QWidget *parent)
    : QWidget(parent)
    , m_style(style)
    , m_axisLabel(axisLabel)
{
    setMinimumSize(320, 160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void DailyMetricChart::setDailyValues(const QMap<QDate, double> &values, int days)
{
    m_values = values;
    m_stackedValues.clear();
    m_categories.clear();
    m_days = std::clamp(days, 7, 90);
    update();
}

void DailyMetricChart::setStackedDailyValues(
    const QMap<QDate, QMap<QString, double>> &values, const QStringList &categories, int days)
{
    m_values.clear();
    m_stackedValues = values;
    m_categories = categories;
    m_days = std::clamp(days, 7, 90);
    update();
}

void DailyMetricChart::clearData()
{
    m_values.clear();
    m_stackedValues.clear();
    m_categories.clear();
    update();
}

void DailyMetricChart::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), Qt::white);
    painter.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 8));

    QDate endDate = QDate::currentDate();
    if (!m_values.isEmpty() && m_values.lastKey() > endDate)
        endDate = m_values.lastKey();
    if (!m_stackedValues.isEmpty() && m_stackedValues.lastKey() > endDate)
        endDate = m_stackedValues.lastKey();
    const QDate startDate = endDate.addDays(-(m_days - 1));
    QMap<QDate, double> visible;
    for (auto it = m_values.cbegin(); it != m_values.cend(); ++it) {
        if (it.key() >= startDate && it.key() <= endDate)
            visible.insert(it.key(), it.value());
    }
    QMap<QDate, QMap<QString, double>> visibleStacked;
    for (auto it = m_stackedValues.cbegin(); it != m_stackedValues.cend(); ++it) {
        if (it.key() >= startDate && it.key() <= endDate)
            visibleStacked.insert(it.key(), it.value());
    }
    if (visible.isEmpty() && visibleStacked.isEmpty()) {
        painter.setPen(QColor("#111111"));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂无数据"));
        return;
    }

    const int left = 54;
    const int top = visibleStacked.isEmpty() ? 24 : 42;
    const int right = 16;
    const int bottom = 38;
    const QRectF plot(left, top, width() - left - right, height() - top - bottom);
    if (plot.width() <= 0 || plot.height() <= 0)
        return;

    double rawMax = 0.0;
    if (!visibleStacked.isEmpty()) {
        for (const auto &dayValues : visibleStacked) {
            double total = 0.0;
            for (double value : dayValues)
                total += value;
            rawMax = std::max(rawMax, total);
        }
    } else {
        for (double value : visible)
            rawMax = std::max(rawMax, value);
    }
    const double axisMax = m_style == Style::Bars
        ? std::max(1.0, std::ceil(rawMax))
        : std::max(1.0, std::ceil((rawMax + 0.5) * 2.0) / 2.0);

    painter.setPen(QColor("#111111"));
    painter.drawText(QRectF(0, 0, 100, 20), Qt::AlignCenter, m_axisLabel);
    const QVector<QColor> categoryColors{QColor("#e95420"), QColor("#2675d8"),
                                         QColor("#0e9f6e"), QColor("#77216f")};
    if (!visibleStacked.isEmpty()) {
        qreal legendX = 108;
        for (int i = 0; i < m_categories.size(); ++i) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(categoryColors.at(i % categoryColors.size()));
            painter.drawRect(QRectF(legendX, 6, 11, 11));
            painter.setPen(QColor("#111111"));
            painter.drawText(QRectF(legendX + 15, 0, 58, 22), Qt::AlignVCenter,
                             m_categories.at(i));
            legendX += 76;
        }
    }
    constexpr int gridCount = 4;
    for (int line = 0; line <= gridCount; ++line) {
        const qreal ratio = line / static_cast<qreal>(gridCount);
        const qreal y = plot.bottom() - ratio * plot.height();
        painter.setPen(QPen(QColor("#d9dde3"), 1, Qt::DashLine));
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.setPen(QColor("#111111"));
        painter.drawText(QRectF(0, y - 9, left - 7, 18), Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(axisMax * ratio, 'f', m_style == Style::Bars ? 0 : 1));
    }
    painter.setPen(QPen(QColor("#30343b"), 1));
    painter.drawLine(plot.bottomLeft(), plot.topLeft());
    painter.drawLine(plot.bottomLeft(), plot.bottomRight());

    constexpr int xTickCount = 4;
    for (int tick = 0; tick <= xTickCount; ++tick) {
        const qreal ratio = tick / static_cast<qreal>(xTickCount);
        const qreal x = plot.left() + ratio * plot.width();
        const int offset = std::round(ratio * (m_days - 1));
        painter.drawLine(QPointF(x, plot.bottom()), QPointF(x, plot.bottom() + 4));
        painter.drawText(QRectF(x - 28, plot.bottom() + 7, 56, 22), Qt::AlignCenter,
                         startDate.addDays(offset).toString(QStringLiteral("MM-dd")));
    }

    const qreal dayWidth = plot.width() / std::max(m_days, 1);
    if (m_style == Style::Bars) {
        for (int day = 0; day < m_days; ++day) {
            const QDate date = startDate.addDays(day);
            qreal bottomY = plot.bottom();
            if (!visibleStacked.isEmpty()) {
                const auto dayValues = visibleStacked.value(date);
                for (int i = 0; i < m_categories.size(); ++i) {
                    const double value = dayValues.value(m_categories.at(i), 0.0);
                    if (value <= 0.0)
                        continue;
                    const qreal height = value / axisMax * plot.height();
                    const QRectF bar(plot.left() + day * dayWidth + dayWidth * 0.18,
                                     bottomY - height, dayWidth * 0.64, height);
                    painter.setPen(QPen(Qt::white, 0.8));
                    painter.setBrush(categoryColors.at(i % categoryColors.size()));
                    painter.drawRect(bar);
                    bottomY -= height;
                }
            } else {
                const double value = visible.value(date, 0.0);
                if (value <= 0.0)
                    continue;
                const qreal height = value / axisMax * plot.height();
                const QRectF bar(plot.left() + day * dayWidth + dayWidth * 0.18,
                                 plot.bottom() - height, dayWidth * 0.64, height);
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor("#e95420"));
                painter.drawRoundedRect(bar, 2.5, 2.5);
            }
        }
        return;
    }

    QPainterPath path;
    QVector<QPointF> points;
    for (auto it = visible.cbegin(); it != visible.cend(); ++it) {
        const int offset = startDate.daysTo(it.key());
        const qreal x = plot.left() + offset / static_cast<qreal>(std::max(m_days - 1, 1)) * plot.width();
        const qreal y = plot.bottom() - it.value() / axisMax * plot.height();
        const QPointF point(x, y);
        points.append(point);
        points.size() == 1 ? path.moveTo(point) : path.lineTo(point);
    }
    painter.setPen(QPen(QColor("#165fd0"), 2.2));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    if (m_days <= 14) for (const QPointF &point : points) {
        painter.setPen(QPen(Qt::white, 2));
        painter.setBrush(QColor("#e95420"));
        painter.drawEllipse(point, 5.0, 5.0);
    }
}
