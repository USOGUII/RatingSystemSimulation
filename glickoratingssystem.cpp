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
    double newRD = 1/sqrt(rd * rd + c * c * timeElapsed);
    return std::min(newRD, 350.0); // Cap RD at 350
}

void GlickoRatingSystem::updateRating(double &rating, double &rd,
                                      const QVector<double> &opponentRatings,
                                      const QVector<double> &opponentRDs,
                                      const QVector<bool> &outcomes)
{
    if (opponentRatings.isEmpty()) {
        // Если игр не было, просто обновляем RD из-за прошедшего времени
        rd = calculateNewRD(rd, 1);
        return;
    }

    // Шаг 1: Вычисляем коэффициент изменчивости рейтинга
    double d2 = 0.0;
    double sumExpectedResults = 0.0;
    double sumActualResults = 0.0;

    for (int i = 0; i < opponentRatings.size(); ++i) {
        // Функция g уменьшает влияние противника с высоким RD
        double g = 1.0 / sqrt(1.0 + 3.0 * q * q * opponentRDs[i] * opponentRDs[i] / (PI * PI));

        // Ожидаемый результат
        double E = calculateExpectedOutcome(rating, rd, opponentRatings[i], opponentRDs[i]);

        // Фактический результат: 1 - победа, 0 - поражение
        double S = outcomes[i] ? 1.0 : 0.0;

        sumExpectedResults += g * E;
        sumActualResults += g * S;
        d2 += g * g * E * (1.0 - E);
    }

    // Защита от деления на ноль
    if (d2 < 0.0001) {
        d2 = 0.0001;
    }

    d2 = 1.0 / (q * q * d2);

    // Шаг 2: Вычисляем изменение рейтинга
    double ratingChange = q / (1.0/rd/rd + 1.0/d2) * (sumActualResults - sumExpectedResults);

    // Ограничиваем изменение рейтинга, чтобы не было слишком больших скачков
    // Максимальное изменение зависит от RD: чем выше RD, тем больше возможное изменение
    double maxChange = (rd / 350.0) * 50.0 + 15; // При RD=350 макс. изменение ±50, при RD=175 - ±25, и т.д.

    if (ratingChange > maxChange) {
        ratingChange = maxChange;
    } else if (ratingChange < -maxChange) {
        ratingChange = -maxChange;
    }

    // Применяем изменение рейтинга
    rating += ratingChange;

    // Шаг 3: Вычисляем новое значение RD
    rd = sqrt(1.0 / (1.0/rd/rd + 1.0/d2));

    // RD не может быть больше 350 и меньше 30
    rd = std::max(30.0, std::min(350.0, rd));
}
