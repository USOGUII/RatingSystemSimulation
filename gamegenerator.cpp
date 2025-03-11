#include "gamegenerator.h"
#include "gamegeneratorthread.h"
#include <QDebug>
#include <QSqlError>
#include <QVector>
#include <algorithm>
#include <QProgressDialog>
#include <QCoreApplication>

GameGenerator::GameGenerator(DatabaseManager *dbManager, QObject *parent)
    : QObject(parent), m_dbManager(dbManager), m_random(*QRandomGenerator::global())
{
}

bool GameGenerator::generateGames(int gameCount, const QDateTime &startDate,
                                  const QDateTime &endDate, int playersPerTeam,
                                  QObject* progressObject)
{
    QSqlQuery playerQuery(m_dbManager->database());
    if (!playerQuery.exec("SELECT COUNT(*) FROM players")) {
        qDebug() << "Error counting players:" << playerQuery.lastError().text();
        return false;
    }

    playerQuery.next();
    int totalPlayers = playerQuery.value(0).toInt();
    if (totalPlayers < playersPerTeam * 2) {
        qDebug() << "Not enough players for games. Generating more players...";
        generatePlayers(playersPerTeam * 2);
    }
    QVector<int> allPlayerIds;
    QSqlQuery allPlayersQuery(m_dbManager->database());
    if (!allPlayersQuery.exec("SELECT player_id FROM players")) {
        qDebug() << "Error retrieving players:" << allPlayersQuery.lastError().text();
        return false;
    }
    while (allPlayersQuery.next()) {
        allPlayerIds.append(allPlayersQuery.value(0).toInt());
    }
    qint64 totalSecondsInRange = startDate.secsTo(endDate);
    if (totalSecondsInRange <= 0) {
        qDebug() << "End date must be after start date";
        return false;
    }
    m_dbManager->database().transaction();
    for (int i = 0; i < gameCount; ++i) {
        qint64 randomOffset = m_random.bounded(static_cast<quint32>(totalSecondsInRange));
        QDateTime gameTime = startDate.addSecs(randomOffset);
        QVector<int> availablePlayers = allPlayerIds;
        QVector<int> team1Players = selectRandomPlayers(playersPerTeam, availablePlayers);
        QVector<int> team2Players = selectRandomPlayers(playersPerTeam, availablePlayers);
        int team1Score = m_random.bounded(0, 11);
        int team2Score = m_random.bounded(0, 11);
        while (team1Score == team2Score) {
            team2Score = m_random.bounded(0, 11);
        }
        QString winnerTeam = (team1Score > team2Score) ? "team1" : "team2";
        QSqlQuery gameQuery(m_dbManager->database());
        gameQuery.prepare("INSERT INTO games (game_date, team1_score, team2_score, winner_team) "
                          "VALUES (:gameDate, :team1Score, :team2Score, :winnerTeam)");
        gameQuery.bindValue(":gameDate", gameTime);
        gameQuery.bindValue(":team1Score", team1Score);
        gameQuery.bindValue(":team2Score", team2Score);
        gameQuery.bindValue(":winnerTeam", winnerTeam);
        if (!gameQuery.exec()) {
            qDebug() << "Error creating game:" << gameQuery.lastError().text();
            m_dbManager->database().rollback();
            return false;
        }
        int gameId = gameQuery.lastInsertId().toInt();
        for (int playerId : team1Players) {
            QSqlQuery participationQuery(m_dbManager->database());
            participationQuery.prepare("INSERT INTO game_participation (game_id, player_id, team) "
                                       "VALUES (:gameId, :playerId, 'team1')");
            participationQuery.bindValue(":gameId", gameId);
            participationQuery.bindValue(":playerId", playerId);
            if (!participationQuery.exec()) {
                qDebug() << "Error adding player to game:" << participationQuery.lastError().text();
                m_dbManager->database().rollback();
                return false;
            }
        }
        for (int playerId : team2Players) {
            QSqlQuery participationQuery(m_dbManager->database());
            participationQuery.prepare("INSERT INTO game_participation (game_id, player_id, team) "
                                       "VALUES (:gameId, :playerId, 'team2')");
            participationQuery.bindValue(":gameId", gameId);
            participationQuery.bindValue(":playerId", playerId);
            if (!participationQuery.exec()) {
                qDebug() << "Error adding player to game:" << participationQuery.lastError().text();
                m_dbManager->database().rollback();
                return false;
            }
        }
        updatePlayerRatings(gameId);

        if(progressObject){
            emit static_cast<GameGeneratorThread*>(progressObject)->progressUpdate(i + 1);
        }
    }
    return m_dbManager->database().commit();
}


QVector<int> GameGenerator::selectRandomPlayers(int count, QVector<int> &availablePlayers)
{
    QVector<int> selectedPlayers;
    for (int i = 0; i < count && !availablePlayers.isEmpty(); ++i) {
        int randomIndex = m_random.bounded(availablePlayers.size());
        selectedPlayers.append(availablePlayers.takeAt(randomIndex));
    }
    return selectedPlayers;
}

void GameGenerator::updatePlayerRatings(int gameId)
{
    QSqlQuery gameQuery(m_dbManager->database());
    gameQuery.prepare("SELECT winner_team FROM games WHERE game_id = :gameId");
    gameQuery.bindValue(":gameId", gameId);
    if (!gameQuery.exec() || !gameQuery.next()) {
        qDebug() << "Error retrieving game details:" << gameQuery.lastError().text();
        return;
    }
    QString winnerTeam = gameQuery.value(0).toString();
    QSqlQuery participantsQuery(m_dbManager->database());
    participantsQuery.prepare("SELECT player_id, team FROM game_participation WHERE game_id = :gameId");
    participantsQuery.bindValue(":gameId", gameId);
    if (!participantsQuery.exec()) {
        qDebug() << "Error retrieving game participants:" << participantsQuery.lastError().text();
        return;
    }
    QVector<int> team1Players;
    QVector<int> team2Players;
    while (participantsQuery.next()) {
        int playerId = participantsQuery.value(0).toInt();
        QString team = participantsQuery.value(1).toString();
        if (team == "team1") {
            team1Players.append(playerId);
        } else {
            team2Players.append(playerId);
        }
    }
    for (int playerId : team1Players) {
        QSqlQuery ratingQuery(m_dbManager->database());
        ratingQuery.prepare("SELECT glicko_rating, rd, total_matches FROM ratings WHERE player_id = :playerId");
        ratingQuery.bindValue(":playerId", playerId);
        if (!ratingQuery.exec() || !ratingQuery.next()) {
            qDebug() << "Error retrieving player rating:" << ratingQuery.lastError().text();
            continue;
        }
        double rating = ratingQuery.value(0).toDouble();
        double rd = ratingQuery.value(1).toDouble();
        int totalMatches = ratingQuery.value(2).toInt() + 1;
        QVector<double> opponentRatings;
        QVector<double> opponentRDs;
        QVector<bool> outcomes;
        for (int opponentId : team2Players) {
            QSqlQuery opponentQuery(m_dbManager->database());
            opponentQuery.prepare("SELECT glicko_rating, rd FROM ratings WHERE player_id = :opponentId");
            opponentQuery.bindValue(":opponentId", opponentId);
            if (!opponentQuery.exec() || !opponentQuery.next()) {
                qDebug() << "Error retrieving opponent rating:" << opponentQuery.lastError().text();
                continue;
            }
            double opponentRating = opponentQuery.value(0).toDouble();
            double opponentRD = opponentQuery.value(1).toDouble();
            opponentRatings.append(opponentRating);
            opponentRDs.append(opponentRD);
            outcomes.append(winnerTeam == "team1");
        }
        m_ratingSystem.updateRating(rating, rd, opponentRatings, opponentRDs, outcomes);
        QSqlQuery updateQuery(m_dbManager->database());
        updateQuery.prepare("UPDATE ratings SET glicko_rating = :rating, rd = :rd, "
                            "total_matches = :totalMatches WHERE player_id = :playerId");
        updateQuery.bindValue(":rating", rating);
        updateQuery.bindValue(":rd", rd);
        updateQuery.bindValue(":totalMatches", totalMatches);
        updateQuery.bindValue(":playerId", playerId);
        if (!updateQuery.exec()) {
            qDebug() << "Error updating player rating:" << updateQuery.lastError().text();
            continue;
        }
        QSqlQuery updatePlayerQuery(m_dbManager->database());
        updatePlayerQuery.prepare("UPDATE players SET glicko_rating = :rating WHERE player_id = :playerId");
        updatePlayerQuery.bindValue(":rating", rating);
        updatePlayerQuery.bindValue(":playerId", playerId);
        if (!updatePlayerQuery.exec()) {
            qDebug() << "Error updating player rating in players table:" << updatePlayerQuery.lastError().text();
        }
    }
    for (int playerId : team2Players) {
        QSqlQuery ratingQuery(m_dbManager->database());
        ratingQuery.prepare("SELECT glicko_rating, rd, total_matches FROM ratings WHERE player_id = :playerId");
        ratingQuery.bindValue(":playerId", playerId);
        if (!ratingQuery.exec() || !ratingQuery.next()) {
            qDebug() << "Error retrieving player rating:" << ratingQuery.lastError().text();
            continue;
        }
        double rating = ratingQuery.value(0).toDouble();
        double rd = ratingQuery.value(1).toDouble();
        int totalMatches = ratingQuery.value(2).toInt() + 1;
        QVector<double> opponentRatings;
        QVector<double> opponentRDs;
        QVector<bool> outcomes;
        for (int opponentId : team1Players) {
            QSqlQuery opponentQuery(m_dbManager->database());
            opponentQuery.prepare("SELECT glicko_rating, rd FROM ratings WHERE player_id = :opponentId");
            opponentQuery.bindValue(":opponentId", opponentId);
            if (!opponentQuery.exec() || !opponentQuery.next()) {
                qDebug() << "Error retrieving opponent rating:" << opponentQuery.lastError().text();
                continue;
            }
            double opponentRating = opponentQuery.value(0).toDouble();
            double opponentRD = opponentQuery.value(1).toDouble();
            opponentRatings.append(opponentRating);
            opponentRDs.append(opponentRD);
            outcomes.append(winnerTeam == "team2");
        }
        m_ratingSystem.updateRating(rating, rd, opponentRatings, opponentRDs, outcomes);
        QSqlQuery updateQuery(m_dbManager->database());
        updateQuery.prepare("UPDATE ratings SET glicko_rating = :rating, rd = :rd, "
                            "total_matches = :totalMatches WHERE player_id = :playerId");
        updateQuery.bindValue(":rating", rating);
        updateQuery.bindValue(":rd", rd);
        updateQuery.bindValue(":totalMatches", totalMatches);
        updateQuery.bindValue(":playerId", playerId);
        if (!updateQuery.exec()) {
            qDebug() << "Error updating player rating:" << updateQuery.lastError().text();
            continue;
        }
        QSqlQuery updatePlayerQuery(m_dbManager->database());
        updatePlayerQuery.prepare("UPDATE players SET glicko_rating = :rating WHERE player_id = :playerId");
        updatePlayerQuery.bindValue(":rating", rating);
        updatePlayerQuery.bindValue(":playerId", playerId);
        if (!updatePlayerQuery.exec()) {
            qDebug() << "Error updating player rating in players table:" << updatePlayerQuery.lastError().text();
        }
    }
}

bool GameGenerator::clearDatabase() {
    // Генерируем уникальное имя файла для резервной копии
    QString backupFilePath = "backup_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss") + ".json";

    if (!m_dbManager->exportToJson(backupFilePath)) {
        qDebug() << "Failed to export database to JSON before clearing.";
        return false;
    }

    // Очистка базы данных
    QSqlQuery query(m_dbManager->database());

    if (!m_dbManager->database().transaction()) {
        qDebug() << "Failed to start transaction for database clearing";
        return false;
    }

    QStringList tableNames = {"game_participation", "games", "ratings", "players"};
    for (const QString &tableName : tableNames) {
        if (!query.exec("DELETE FROM " + tableName)) {
            qDebug() << "Error clearing table" << tableName << ":" << query.lastError().text();
            m_dbManager->database().rollback();
            return false;
        }
        if (!query.exec("DELETE FROM sqlite_sequence WHERE name='" + tableName + "'")) {
            qDebug() << "Error resetting auto-increment for" << tableName << ":" << query.lastError().text();
            m_dbManager->database().rollback();
            return false;
        }
    }
    return m_dbManager->database().commit();
}

bool GameGenerator::generatePlayers(int count)
{
    if (!m_dbManager->database().transaction()) {
        qDebug() << "Failed to start transaction for player generation";
        return false;
    }
    for (int i = 0; i < count; ++i) {
        QString nickname = QString("Player%1").arg(i + 1);
        double initialRating = 1000.0;
        QSqlQuery playerQuery(m_dbManager->database());
        playerQuery.prepare("INSERT INTO players (nickname, glicko_rating) VALUES (:nickname, :rating)");
        playerQuery.bindValue(":nickname", nickname);
        playerQuery.bindValue(":rating", initialRating);
        if (!playerQuery.exec()) {
            if (playerQuery.lastError().text().contains("UNIQUE constraint failed")) {
                continue;
            }
            qDebug() << "Error creating player:" << playerQuery.lastError().text();
            m_dbManager->database().rollback();
            return false;
        }
        int playerId = playerQuery.lastInsertId().toInt();
        QSqlQuery ratingQuery(m_dbManager->database());
        ratingQuery.prepare("INSERT INTO ratings (player_id, glicko_rating, rd, total_matches) "
                            "VALUES (:playerId, :rating, 350.0, 0)");
        ratingQuery.bindValue(":playerId", playerId);
        ratingQuery.bindValue(":rating", initialRating);
        if (!ratingQuery.exec()) {
            qDebug() << "Error creating player rating:" << ratingQuery.lastError().text();
            m_dbManager->database().rollback();
            return false;
        }
    }
    return m_dbManager->database().commit();
}
