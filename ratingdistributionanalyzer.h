#ifndef RATINGDISTRIBUTIONANALYZER_H
#define RATINGDISTRIBUTIONANALYZER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QPair>
#include <QTableWidget>
#include <QtMath>
#include <QDebug>
#include <QChart>
#include <QBarSeries>
#include <QBarSet>
#include <QBarCategoryAxis>
#include <QValueAxis>
#include <QChartView>


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
    QString analyzeDistributionFairness();

private:
    QMap<int, int> m_ratingData;
};

#endif // RATINGDISTRIBUTIONANALYZER_H
