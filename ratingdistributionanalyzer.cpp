#include "RatingDistributionAnalyzer.h"

#include <QtMath>
#include <QDebug>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QBarCategoryAxis>
#include <QValueAxis>
#include <algorithm>

RatingDistributionAnalyzer::RatingDistributionAnalyzer(QObject *parent) : QObject(parent) {}

void RatingDistributionAnalyzer::setDataFromTable(QTableWidget *table) {
    m_ratingData.clear();
    // Предполагаем, что в первом столбце рейтинг, во втором - количество игр
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, 0) && table->item(row, 1)) {
            int rating = table->item(row, 0)->text().toInt();
            int games = table->item(row, 1)->text().toInt();
            m_ratingData[rating] = games;
        }
    }
}

void RatingDistributionAnalyzer::addData(int rating, int gamesCount) {
    m_ratingData[rating] = gamesCount;
}

void RatingDistributionAnalyzer::clearData() {
    m_ratingData.clear();
}

QMap<QString, double> RatingDistributionAnalyzer::calculateStatistics() {
    QMap<QString, double> stats;
    if (m_ratingData.isEmpty()) {
        qWarning() << "Нет данных для анализа";
        return stats;
    }

    // Вычислить среднее значение рейтинга, взвешенное по количеству игр
    double totalRatingPoints = 0;
    int totalGames = 0;
    for (auto it = m_ratingData.constBegin(); it != m_ratingData.constEnd(); ++it) {
        totalRatingPoints += it.key() * it.value();
        totalGames += it.value();
    }

    double meanRating = totalRatingPoints / totalGames;
    stats["Средний рейтинг"] = meanRating;

    // Вычислить стандартное отклонение
    double variance = 0;
    for (auto it = m_ratingData.constBegin(); it != m_ratingData.constEnd(); ++it) {
        variance += pow(it.key() - meanRating, 2) * it.value();
    }

    variance /= totalGames;
    double stdDeviation = qSqrt(variance);
    stats["Стандартное отклонение"] = stdDeviation;

    // Вычислить медиану рейтинга
    QVector<int> allRatings;
    for (auto it = m_ratingData.constBegin(); it != m_ratingData.constEnd(); ++it) {
        for (int i = 0; i < it.value(); ++i) {
            allRatings.append(it.key());
        }
    }

    std::sort(allRatings.begin(), allRatings.end());
    int medianIndex = allRatings.size() / 2;
    if (allRatings.size() % 2 == 0 && allRatings.size() > 0) {
        stats["Медиана"] = (allRatings[medianIndex - 1] + allRatings[medianIndex]) / 2.0;
    } else if (allRatings.size() > 0) {
        stats["Медиана"] = allRatings[medianIndex];
    }

    // Вычислить асимметрию (skewness)
    double skewness = 0;
    for (auto it = m_ratingData.constBegin(); it != m_ratingData.constEnd(); ++it) {
        skewness += pow((it.key() - meanRating) / stdDeviation, 3) * it.value();
    }

    skewness /= totalGames;
    stats["Асимметрия"] = skewness;

    // Вычислить эксцесс (kurtosis)
    double kurtosis = 0;
    for (auto it = m_ratingData.constBegin(); it != m_ratingData.constEnd(); ++it) {
        kurtosis += pow((it.key() - meanRating) / stdDeviation, 4) * it.value();
    }

    kurtosis = kurtosis / totalGames - 3; // Excess kurtosis (нормальное распределение = 0)
    stats["Эксцесс"] = kurtosis;

    return stats;
}

double RatingDistributionAnalyzer::checkNormalDistribution() {
    // Упрощенная реализация проверки на нормальность
    // В реальном приложении используйте специализированные библиотеки статистики
    QMap<QString, double> stats = calculateStatistics();
    // Если асимметрия близка к 0 и эксцесс близок к 0, это признаки нормального распределения
    double skewnessValue = qAbs(stats["Асимметрия"]);
    double kurtosisValue = qAbs(stats["Эксцесс"]);

    // Простая эвристика для оценки нормальности
    double normalityScore = 1.0 - (skewnessValue / 3.0) - (kurtosisValue / 6.0);
    normalityScore = qBound(0.0, normalityScore, 1.0);
    return normalityScore;
}

QChartView* RatingDistributionAnalyzer::createDistributionChart() {
    // Создаем серию для гистограммы
    QBarSeries *series = new QBarSeries();
    QBarSet *ratingSet = new QBarSet("Количество игр");
    QStringList categories;

    // Создаем диапазоны рейтинга для лучшей визуализации
    int minRating = m_ratingData.isEmpty() ? 0 : m_ratingData.firstKey();
    int maxRating = m_ratingData.isEmpty() ? 0 : m_ratingData.lastKey();

    // Определение размера интервала
    int intervalSize = qMax(1, (maxRating - minRating) / 20);

    // Группируем данные по диапазонам рейтинга
    QMap<int, int> groupedData;
    for (auto it = m_ratingData.constBegin(); it != m_ratingData.constEnd(); ++it) {
        int intervalStart = (it.key() / intervalSize) * intervalSize;
        // В данном случае m_ratingData содержит рейтинг -> количество игр
        // Итак, it.value() - это количество игроков с данным рейтингом
        groupedData[intervalStart] += it.value();
    }

    // Заполняем данные для графика
    for (auto it = groupedData.constBegin(); it != groupedData.constEnd(); ++it) {
        // Добавляем метку оси X - это рейтинг
        categories << QString("%1").arg(it.key());
        // Добавляем значение - это количество игроков с данным рейтингом
        *ratingSet << it.value();
    }

    series->append(ratingSet);

    // Создаем график
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Распределение рейтинга игроков");
    chart->setAnimationOptions(QChart::SeriesAnimations);

    // Настраиваем ось X (рейтинг)
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    axisX->setTitleText("Рейтинг");
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    // Настраиваем ось Y (количество игроков)
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Количество игр");
    // Устанавливаем диапазон оси Y от 0 до максимального количества игроков (с небольшим запасом)
    int maxPlayers = 0;
    for (auto value : groupedData.values()) {
        maxPlayers = qMax(maxPlayers, value);
    }
    axisY->setRange(0, maxPlayers * 1.1); // +10% для лучшего отображения

    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    // Настраиваем легенду
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    // Создаем представление графика
    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    return chartView;
}

QString RatingDistributionAnalyzer::analyzeDistributionFairness() {
    QString result;
    if (m_ratingData.isEmpty()) {
        return "Нет данных для анализа";
    }

    QMap<QString, double> stats = calculateStatistics();
    double normalityScore = checkNormalDistribution();

    result += "Результаты анализа распределения:\n\n";
    result += QString("Средний рейтинг: %1\n").arg(stats["Средний рейтинг"]);
    result += QString("Медиана: %1\n").arg(stats["Медиана"]);
    result += QString("Стандартное отклонение: %1\n").arg(stats["Стандартное отклонение"]);
    result += QString("Асимметрия: %1\n").arg(stats["Асимметрия"]);
    result += QString("Эксцесс: %1\n").arg(stats["Эксцесс"]);
    result += QString("Оценка нормальности: %1%\n\n").arg(normalityScore * 100.0, 0, 'f', 2);

    // Анализ асимметрии
    if (qAbs(stats["Асимметрия"]) > 0.5) {
        if (stats["Асимметрия"] > 0) {
            result += "Положительная асимметрия указывает на наличие длинного хвоста в сторону высоких рейтингов.\n";
        } else {
            result += "Отрицательная асимметрия указывает на наличие длинного хвоста в сторону низких рейтингов.\n";
        }
    }

    // Анализ эксцесса
    if (qAbs(stats["Эксцесс"]) > 1.0) {
        if (stats["Эксцесс"] > 0) {
            result += "Высокий эксцесс свидетельствует о большем количестве экстремальных значений, чем ожидается при нормальном распределении.\n";
        } else {
            result += "Низкий эксцесс свидетельствует о меньшем количестве экстремальных значений, чем ожидается при нормальном распределении.\n";
        }
    }

    // Общий анализ нормальности
    if (normalityScore > 0.8) {
        result += "Распределение рейтинга близко к нормальному, что свидетельствует о справедливой системе.\n";
    } else if (normalityScore > 0.5) {
        result += "Распределение имеет умеренные отклонения от нормального, но остается приемлемым.\n";
    } else {
        result += "Значительные отклонения от нормального распределения могут указывать на проблемы в системе.\n";
    }

    return result;
}

