#ifndef GLICKORATINGSYSTEM_H
#define GLICKORATINGSYSTEM_H

#include <QObject>
#include <QVector>

class GlickoRatingSystem : public QObject
{
    Q_OBJECT

public:
    explicit GlickoRatingSystem(QObject *parent = nullptr);

    // Core Glicko functions
    double calculateExpectedOutcome(double rating1, double rd1, double rating2, double rd2);
    double calculateNewRD(double rd, int timeElapsed);
    void updateRating(double &rating, double &rd, const QVector<double> &opponentRatings,
                      const QVector<double> &opponentRDs, const QVector<bool> &outcomes);

private:
    // Constants for Glicko system
    const double q = 0.00575646273; // ln(10)/400
    const double c = 34.6;         // Rating change constant
    const double PI = 3.14159265358979323846;
};

#endif // GLICKORATINGSYSTEM_H
