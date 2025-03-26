#include "RatingDistributionAnalyzer.h"

#include <QtMath>
#include <QDebug>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QBarCategoryAxis>
#include <QValueAxis>
#include <QLineSeries>
#include <QScatterSeries>
#include <QMainWindow>
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
    chart->setTitle("Распределение рейтинга по количеству игр");
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

// Новый метод для создания графика распределения рейтинга по количеству игроков
QChartView* RatingDistributionAnalyzer::createPlayerDistributionChart() {
    // Получаем данные о количестве игроков для каждого рейтинга
    QMap<int, int> playersByRating;

    // Запрос к базе данных для получения распределения игроков по рейтингу
    QSqlQuery query;
    query.prepare("SELECT ROUND(glicko_rating/50)*50 as rating_group, COUNT(*) as player_count "
                  "FROM players "
                  "GROUP BY rating_group "
                  "ORDER BY rating_group");

    if (!query.exec()) {
        qDebug() << "Error getting player distribution data:" << query.lastError().text();
        return nullptr;
    }

    while (query.next()) {
        int ratingGroup = query.value(0).toInt();
        int playerCount = query.value(1).toInt();
        playersByRating[ratingGroup] = playerCount;
    }

    if (playersByRating.isEmpty()) {
        qWarning() << "Нет данных о распределении игроков по рейтингу";
        return nullptr;
    }

    // Создаем столбиковую диаграмму
    QBarSeries *series = new QBarSeries();
    QBarSet *playerSet = new QBarSet("Количество игроков");
    QStringList categories;

    // Заполняем данные для графика
    for (auto it = playersByRating.constBegin(); it != playersByRating.constEnd(); ++it) {
        categories << QString("%1").arg(it.key());
        *playerSet << it.value();
    }

    series->append(playerSet);

    // Создаем график
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Распределение рейтинга по количеству игроков");
    chart->setAnimationOptions(QChart::SeriesAnimations);

    // Настраиваем ось X (рейтинг)
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    axisX->setTitleText("Рейтинг (группы)");
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    // Настраиваем ось Y (количество игроков)
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Количество игроков");
    // Находим максимальное количество игроков
    int maxPlayers = 0;
    for (auto value : playersByRating.values()) {
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

// Новый метод для создания графика зависимости рейтинга от уровня скилла
QChartView* RatingDistributionAnalyzer::createSkillRatingChart() {
    // Запрос к базе данных для получения данных о рейтинге и уровне скилла
    QSqlQuery query;
    query.prepare("SELECT skill_level, AVG(glicko_rating) as avg_rating, "
                  "MIN(glicko_rating) as min_rating, MAX(glicko_rating) as max_rating "
                  "FROM players "
                  "GROUP BY skill_level "
                  "ORDER BY skill_level");

    if (!query.exec()) {
        qDebug() << "Error getting skill rating data:" << query.lastError().text();
        return nullptr;
    }

    QLineSeries *avgSeries = new QLineSeries();
    avgSeries->setName("Средний рейтинг");

    QLineSeries *minSeries = new QLineSeries();
    minSeries->setName("Минимальный рейтинг");

    QLineSeries *maxSeries = new QLineSeries();
    maxSeries->setName("Максимальный рейтинг");

    QStringList skillLabels;
    skillLabels << "Низкий" << "Средний" << "Выше среднего" << "Высокий";

    while (query.next()) {
        int skillLevel = query.value(0).toInt();
        double avgRating = query.value(1).toDouble();
        double minRating = query.value(2).toDouble();
        double maxRating = query.value(3).toDouble();

        avgSeries->append(skillLevel, avgRating);
        minSeries->append(skillLevel, minRating);
        maxSeries->append(skillLevel, maxRating);
    }

    // Создаем график
    QChart *chart = new QChart();
    chart->addSeries(avgSeries);
    chart->addSeries(minSeries);
    chart->addSeries(maxSeries);
    chart->setTitle("Зависимость рейтинга от уровня скилла");
    chart->setAnimationOptions(QChart::SeriesAnimations);

    // Настраиваем ось X (уровень скилла)
    QCategoryAxis *axisX = new QCategoryAxis();
    axisX->setTitleText("Уровень скилла");
    axisX->setRange(0.5, 4.5);

    for (int i = 0; i < skillLabels.size(); ++i) {
        axisX->append(skillLabels[i], i + 1);
    }

    chart->addAxis(axisX, Qt::AlignBottom);
    avgSeries->attachAxis(axisX);
    minSeries->attachAxis(axisX);
    maxSeries->attachAxis(axisX);

    // Настраиваем ось Y (рейтинг)
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Рейтинг");

    // Находим минимальное и максимальное значение рейтинга для настройки диапазона оси Y
    double minYValue = -1, maxYValue = -1;

    for (QPointF point : minSeries->points()) {
        if (minYValue == -1 || point.y() < minYValue) {
            minYValue = point.y();
        }
    }

    for (QPointF point : maxSeries->points()) {
        if (maxYValue == -1 || point.y() > maxYValue) {
            maxYValue = point.y();
        }
    }

    // Устанавливаем диапазон с запасом
    axisY->setRange(qMax(0.0, minYValue - 100), maxYValue + 100);

    chart->addAxis(axisY, Qt::AlignLeft);
    avgSeries->attachAxis(axisY);
    minSeries->attachAxis(axisY);
    maxSeries->attachAxis(axisY);

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

    // Анализ распределения по игрокам
    result += "\nАнализ распределения рейтинга по игрокам:\n";
    result += "График распределения рейтинга по количеству игроков показывает, ";

    // Запрос к БД для проверки распределения игроков по рейтингу
    QSqlQuery query;
    query.prepare("SELECT ROUND(glicko_rating/50)*50 as rating_group, COUNT(*) as player_count "
                  "FROM players "
                  "GROUP BY rating_group "
                  "ORDER BY rating_group");

    if (query.exec()) {
        // Анализируем форму распределения
        bool hasPeakAroundAverage = false;
        int totalPlayers = 0;
        int playersNearAverage = 0;

        double avgRating = stats["Средний рейтинг"];
        double avgRangeMin = avgRating - 100;
        double avgRangeMax = avgRating + 100;

        while (query.next()) {
            int ratingGroup = query.value(0).toInt();
            int playerCount = query.value(1).toInt();
            totalPlayers += playerCount;

            if (ratingGroup >= avgRangeMin && ratingGroup <= avgRangeMax) {
                playersNearAverage += playerCount;
            }
        }

        double percentageNearAverage = (totalPlayers > 0) ?
                                           ((double)playersNearAverage / totalPlayers * 100.0) : 0;

        if (percentageNearAverage > 50) {
            hasPeakAroundAverage = true;
        }

        if (hasPeakAroundAverage) {
            result += "что большинство игроков (" + QString::number(percentageNearAverage, 'f', 1) +
                      "%) находятся в центральном диапазоне рейтинга, что является признаком сбалансированной системы.\n";
        } else {
            result += "что игроки распределены достаточно равномерно, без ярко выраженного пика в центре. ";
            result += "Это может указывать на то, что система рейтинга эффективно дифференцирует игроков разного уровня навыков.\n";
        }
    }

    // Анализ зависимости рейтинга от уровня скилла
    result += "\nАнализ зависимости рейтинга от уровня скилла:\n";

    QSqlQuery skillQuery;
    skillQuery.prepare("SELECT skill_level, AVG(glicko_rating) as avg_rating "
                       "FROM players "
                       "GROUP BY skill_level "
                       "ORDER BY skill_level");

    if (skillQuery.exec()) {
        QMap<int, double> avgRatingBySkill;

        while (skillQuery.next()) {
            int skillLevel = skillQuery.value(0).toInt();
            double avgRating = skillQuery.value(1).toDouble();
            avgRatingBySkill[skillLevel] = avgRating;
        }

        if (avgRatingBySkill.size() > 1) {
            bool isMonotonic = true;
            double prevRating = -1;

            for (auto it = avgRatingBySkill.constBegin(); it != avgRatingBySkill.constEnd(); ++it) {
                if (prevRating != -1 && it.value() <= prevRating) {
                    isMonotonic = false;
                    break;
                }
                prevRating = it.value();
            }

            if (isMonotonic) {
                result += "Наблюдается чёткая зависимость: с ростом уровня скилла растёт и средний рейтинг игроков. ";
                result += "Это свидетельствует о том, что система рейтинга хорошо отражает фактический уровень навыка игрока.\n";
            } else {
                result += "Зависимость между скиллом и рейтингом не является строго монотонной. ";
                result += "Это может указывать на то, что на рейтинг влияют и другие факторы, помимо базового скилла игрока.\n";
            }

            // Анализ разброса рейтинга в пределах одного уровня скилла
            QSqlQuery dispersionQuery;
            dispersionQuery.prepare("SELECT skill_level, MAX(glicko_rating) - MIN(glicko_rating) as dispersion "
                                    "FROM players "
                                    "GROUP BY skill_level");

            if (dispersionQuery.exec()) {
                double avgDispersion = 0;
                int count = 0;

                while (dispersionQuery.next()) {
                    avgDispersion += dispersionQuery.value(1).toDouble();
                    count++;
                }

                if (count > 0) {
                    avgDispersion /= count;
                    result += QString("Средний разброс рейтинга в пределах одного уровня скилла: %1. ").arg(avgDispersion, 0, 'f', 1);

                    if (avgDispersion > 300) {
                        result += "Большой разброс указывает на то, что на рейтинг значительно влияют индивидуальные особенности игры и количество проведенных матчей.\n";
                    } else {
                        result += "Умеренный разброс свидетельствует о стабильности системы рейтинга в пределах одного уровня навыков.\n";
                    }
                }
            }
        }
    }

    return result;
}
