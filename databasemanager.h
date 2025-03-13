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

// Структура для хранения данных о игроке и его рейтинге
struct PlayerData {
    int playerId;
    QString nickname;
    double rating;
    double rd;
    int skillLevel;
    int totalMatches;
    int wins;
    double winRate;
};

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(const QString &dbPath, QObject *parent = nullptr);
    ~DatabaseManager();

    bool isOpen() const;
    bool initialize();

    // Add player with skill level
    bool addPlayer(const QString &nickname, int skillLevel = 1, double glickoRating = 1000.0);

    // Add game
    bool addGame(int team1Score, int team2Score, const QString &winnerTeam);

    // Add player to game with rating change
    bool addPlayerToGame(int gameId, int playerId, const QString &team, double ratingChange = 0.0);

    // Get player by nickname
    int getPlayerId(const QString &nickname);

    // Update player rating and winrate
    bool updatePlayerRating(int playerId, double glickoRating, double rd, int totalMatches, int wins);

    // Export to Json
    bool exportToJson(const QString& filePath);

    // Import from Json
    bool importFromJson(const QString& filePath);

    // Get games table
    QVector<QVector<QString>> getGamesTable();

    // Get players with ratings and winrate
    QVector<QVector<QString>> getPlayersWithRatings();

    // Get rating data for statistics
    QMap<int, int> getRatingData();

    // Get player data for matching
    QVector<PlayerData> getPlayersForMatching();

    // Get game details with rating changes
    QVector<QVector<QString>> getGameDetails(int gameId);

    // Update player win stats
    bool updatePlayerWinStats(int playerId, bool isWin);

    QSqlDatabase& database() { return m_db; }

private:
    bool createTables();
    bool executeQuery(const QString &query);

    QSqlDatabase m_db;
    QString m_dbPath;
};

#endif // DATABASEMANAGER_H
