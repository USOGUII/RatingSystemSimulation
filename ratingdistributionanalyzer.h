#ifndef RATINGDISTRIBUTIONANALYZER_H
#define RATINGDISTRIBUTIONANALYZER_H

#include <QObject>
#include <QMap>
#include <QTableWidget>
#include <QChartView>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCategoryAxis>

    class RatingDistributionAnalyzer : public QObject
{
    Q_OBJECT
public:
    explicit RatingDistributionAnalyzer(QObject *parent = nullptr);

    void setDataFromTable(QTableWidget *table);
    void addData(int rating, int gamesCount);
    void clearData();

    QMap<QString, double> calculateStatistics();
    double checkNormalDistribution();

    QChartView* createDistributionChart();
    QChartView* createPlayerDistributionChart();  // Новый метод для графика распределения по игрокам
    QChartView* createSkillRatingChart();        // Новый метод для графика зависимости рейтинга от скилла

    QString analyzeDistributionFairness();

private:
    QMap<int, int> m_ratingData;  // Рейтинг -> количество игр
};

#endif // RATINGDISTRIBUTIONANALYZER_H
