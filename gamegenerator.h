#ifndef GAMEGENERATOR_H
#define GAMEGENERATOR_H

#include <QObject>
#include "databasemanager.h"
#include <QRandomGenerator>
#include <QDateTime>
#include <QVector>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include "glickoratingssystem.h"

class GameGenerator : public QObject
{
    Q_OBJECT
public:
    explicit GameGenerator(DatabaseManager *dbManager, QObject *parent = nullptr);

    bool generateGames(int gameCount, const QDateTime &startDate,
                       const QDateTime &endDate, int playersPerTeam, QObject* progressObject = nullptr);
    bool clearDatabase();
    bool generatePlayers(int count);

signals:
    void progressUpdate(int value);

private:
    DatabaseManager *m_dbManager;
    QRandomGenerator m_random;
    QVector<int> selectRandomPlayers(int count, QVector<int> &availablePlayers);
    void updatePlayerRatings(int gameId);
    GlickoRatingSystem m_ratingSystem;
};

#endif // GAMEGENERATOR_H
