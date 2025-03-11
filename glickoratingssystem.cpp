#include "glickoratingssystem.h"

GlickoRatingSystem::GlickoRatingSystem(QObject *parent)
    : QObject(parent)
{
}

double GlickoRatingSystem::calculateExpectedOutcome(double rating1, double rd1, double rating2, double rd2)
{
    double g = 1.0 / sqrt(1.0 + 3.0 * q * q * (rd1 * rd1 + rd2 * rd2) / (PI * PI));
    double E = 1.0 / (1.0 + pow(10.0, -g * (rating1 - rating2) / 400.0));
    return E;
}

double GlickoRatingSystem::calculateNewRD(double rd, int timeElapsed)
{
    // Time elapsed is in days
    double newRD = sqrt(rd * rd + c * c * timeElapsed);
    return std::min(newRD, 350.0); // Cap RD at 350
}

void GlickoRatingSystem::updateRating(double &rating, double &rd,
                                      const QVector<double> &opponentRatings,
                                      const QVector<double> &opponentRDs,
                                      const QVector<bool> &outcomes)
{
    if (opponentRatings.isEmpty()) {
        // If no games played, just update RD for time decay
        rd = calculateNewRD(rd, 1);
        return;
    }

    double d2 = 0.0;
    double sumExpectedResults = 0.0;
    double sumActualResults = 0.0;

    for (int i = 0; i < opponentRatings.size(); ++i) {
        double g = 1.0 / sqrt(1.0 + 3.0 * q * q * opponentRDs[i] * opponentRDs[i] / (PI * PI));
        double E = calculateExpectedOutcome(rating, rd, opponentRatings[i], opponentRDs[i]);
        double S = outcomes[i] ? 1.0 : 0.0; // Win = 1, Loss = 0

        sumExpectedResults += g * E;
        sumActualResults += g * S;
        d2 += g * g * E * (1.0 - E);
    }

    d2 = 1.0 / (q * q * d2);

    // Calculate new rating
    double ratingChange = q / (1.0/rd/rd + 1.0/d2) * (sumActualResults - sumExpectedResults);
    rating += ratingChange;

    // Calculate new RD
    rd = sqrt(1.0 / (1.0/rd/rd + 1.0/d2));
}
