#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QSqlRecord>

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(const QString &dbPath, QObject *parent = nullptr);
    ~DatabaseManager();

    bool isOpen() const;
    bool initialize();

    // Add player
    bool addPlayer(const QString &nickname, double glickoRating = 1000.0);

    // Add game
    bool addGame(int team1Score, int team2Score, const QString &winnerTeam);

    // Add player to game
    bool addPlayerToGame(int gameId, int playerId, const QString &team);

    // Get player by nickname
    int getPlayerId(const QString &nickname);

    // Update player rating
    bool updatePlayerRating(int playerId, double glickoRating, double rd, int totalMatches);

    //Export to Json
    bool exportToJson(const QString& filePath);

    //Import from Json
    bool importFromJson(const QString& filePath);

    //Получить таблицу с играми
    QVector<QVector<QString>> getGamesTable();

    //Получить твблицу с игроками
    QVector<QVector<QString>> getPlayersWithRatings();

    QSqlDatabase& database() { return m_db; }

private:
    bool createTables();
    bool executeQuery(const QString &query);

    QSqlDatabase m_db;
    QString m_dbPath;
};

#endif // DATABASEMANAGER_H
