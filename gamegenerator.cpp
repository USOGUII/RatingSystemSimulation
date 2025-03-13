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

// Исправленный метод для работы с прогресс-баром и корректного подсчета побед
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
        qDebug() << "Not enough players for games. Need at least" << (playersPerTeam * 2) << "players, but have" << totalPlayers;
        return false;
    }

    // Получаем данные игроков для подбора
    QVector<PlayerData> allPlayers = m_dbManager->getPlayersForMatching();

    qint64 totalSecondsInRange = startDate.secsTo(endDate);
    if (totalSecondsInRange <= 0) {
        qDebug() << "End date must be after start date";
        return false;
    }

    // Вычисляем интервал времени между играми
    qint64 timeIntervalBetweenGames = totalSecondsInRange / gameCount;

    // Начальное время для первой игры
    QDateTime currentGameTime = startDate;

    m_dbManager->database().transaction();
    for (int i = 0; i < gameCount; ++i) {
        // Вместо случайного смещения используем последовательное увеличение времени
        // Добавляем небольшую случайность (до 30 минут) к интервалу, чтобы время не было строго равномерным
        qint64 randomExtraOffset = m_random.bounded(1800); // до 30 минут в секундах

        // Переходим к следующему времени игры (после первой игры)
        if (i > 0) {
            currentGameTime = currentGameTime.addSecs(timeIntervalBetweenGames + randomExtraOffset);
        }

        // Проверяем, не вышли ли за пределы допустимого диапазона
        if (currentGameTime > endDate) {
            currentGameTime = endDate;
        }

        // Копируем список игроков для текущей игры
        QVector<PlayerData> availablePlayers = allPlayers;

        // Подбираем игроков с близким уровнем навыка
        QVector<PlayerData> selectedPlayers = selectBalancedPlayers(playersPerTeam * 2, availablePlayers);

        if (selectedPlayers.size() < playersPerTeam * 2) {
            qDebug() << "Not enough players available for a balanced game";
            m_dbManager->database().rollback();
            return false;
        }

        // Распределяем игроков по командам
        QVector<PlayerData> team1Players;
        QVector<PlayerData> team2Players;

        distributePlayers(selectedPlayers, team1Players, team2Players);

        // Рассчитываем вероятность победы на основе уровня навыка
        double team1SkillSum = 0, team2SkillSum = 0;

        for (const auto& player : team1Players) {
            team1SkillSum += player.skillLevel;
        }

        for (const auto& player : team2Players) {
            team2SkillSum += player.skillLevel;
        }

        // Вероятность победы первой команды, сильно зависящая от уровня скилла
        double team1WinProb = team1SkillSum / (team1SkillSum + team2SkillSum);
        team1WinProb = pow(team1WinProb, 2); // Усиливаем влияние разницы в скилле

        // Определяем победителя на основе вероятности
        double randomValue = m_random.generateDouble();
        QString winnerTeam;
        int team1Score, team2Score;

        if (randomValue < team1WinProb) {
            winnerTeam = "team1";
            team1Score = 5 + m_random.bounded(6); // 5-10
            team2Score = m_random.bounded(5);     // 0-4
        } else {
            winnerTeam = "team2";
            team2Score = 5 + m_random.bounded(6); // 5-10
            team1Score = m_random.bounded(5);     // 0-4
        }

        // Добавляем игру в БД
        QSqlQuery gameQuery(m_dbManager->database());
        gameQuery.prepare("INSERT INTO games (game_date, team1_score, team2_score, winner_team) "
                          "VALUES (:gameDate, :team1Score, :team2Score, :winnerTeam)");
        gameQuery.bindValue(":gameDate", currentGameTime);
        gameQuery.bindValue(":team1Score", team1Score);
        gameQuery.bindValue(":team2Score", team2Score);
        gameQuery.bindValue(":winnerTeam", winnerTeam);

        if (!gameQuery.exec()) {
            qDebug() << "Error creating game:" << gameQuery.lastError().text();
            m_dbManager->database().rollback();
            return false;
        }

        int gameId = gameQuery.lastInsertId().toInt();


        // Добавляем игроков в игру (просто участие)
        for (const auto& player : team1Players) {
            QSqlQuery participationQuery(m_dbManager->database());
            participationQuery.prepare("INSERT INTO game_participation (game_id, player_id, team) "
                                       "VALUES (:gameId, :playerId, 'team1')");
            participationQuery.bindValue(":gameId", gameId);
            participationQuery.bindValue(":playerId", player.playerId);

            if (!participationQuery.exec()) {
                qDebug() << "Error adding player to game:" << participationQuery.lastError().text();
                m_dbManager->database().rollback();
                return false;
            }
        }

        for (const auto& player : team2Players) {
            QSqlQuery participationQuery(m_dbManager->database());
            participationQuery.prepare("INSERT INTO game_participation (game_id, player_id, team) "
                                       "VALUES (:gameId, :playerId, 'team2')");
            participationQuery.bindValue(":gameId", gameId);
            participationQuery.bindValue(":playerId", player.playerId);

            if (!participationQuery.exec()) {
                qDebug() << "Error adding player to game:" << participationQuery.lastError().text();
                m_dbManager->database().rollback();
                return false;
            }
        }

        // Обновляем рейтинги по системе Glicko и заполняем rating_change
        updatePlayerRatings(gameId);

        // Обновляем статистику побед и общего числа игр
        for (const auto& player : team1Players) {
            QSqlQuery updateStatsQuery(m_dbManager->database());
            updateStatsQuery.prepare("UPDATE players SET total_matches = total_matches + 1, "
                                     "wins = CASE WHEN :isWinner THEN wins + 1 ELSE wins END, "
                                     "win_rate = CASE WHEN (total_matches + 1) > 0 THEN "
                                     "(CASE WHEN :isWinner THEN (wins + 1) ELSE wins END) * 100.0 / (total_matches + 1) "
                                     "ELSE 0 END "
                                     "WHERE player_id = :playerId");
            updateStatsQuery.bindValue(":isWinner", winnerTeam == "team1");
            updateStatsQuery.bindValue(":playerId", player.playerId);

            if (!updateStatsQuery.exec()) {
                qDebug() << "Error updating player stats:" << updateStatsQuery.lastError().text();
            }

            // Также обновляем total_matches в ratings
            QSqlQuery updateRatingMatchesQuery(m_dbManager->database());
            updateRatingMatchesQuery.prepare("UPDATE ratings SET total_matches = total_matches + 1 "
                                             "WHERE player_id = :playerId");
            updateRatingMatchesQuery.bindValue(":playerId", player.playerId);

            if (!updateRatingMatchesQuery.exec()) {
                qDebug() << "Error updating rating total matches:" << updateRatingMatchesQuery.lastError().text();
            }
        }

        for (const auto& player : team2Players) {
            QSqlQuery updateStatsQuery(m_dbManager->database());
            updateStatsQuery.prepare("UPDATE players SET total_matches = total_matches + 1, "
                                     "wins = CASE WHEN :isWinner THEN wins + 1 ELSE wins END, "
                                     "win_rate = CASE WHEN (total_matches + 1) > 0 THEN "
                                     "(CASE WHEN :isWinner THEN (wins + 1) ELSE wins END) * 100.0 / (total_matches + 1) "
                                     "ELSE 0 END "
                                     "WHERE player_id = :playerId");
            updateStatsQuery.bindValue(":isWinner", winnerTeam == "team2");
            updateStatsQuery.bindValue(":playerId", player.playerId);

            if (!updateStatsQuery.exec()) {
                qDebug() << "Error updating player stats:" << updateStatsQuery.lastError().text();
            }

            QSqlQuery updateRatingMatchesQuery(m_dbManager->database());
            updateRatingMatchesQuery.prepare("UPDATE ratings SET total_matches = total_matches + 1 "
                                             "WHERE player_id = :playerId");
            updateRatingMatchesQuery.bindValue(":playerId", player.playerId);

            if (!updateRatingMatchesQuery.exec()) {
                qDebug() << "Error updating rating total matches:" << updateRatingMatchesQuery.lastError().text();
            }
        }

        // Обновляем локальный список игроков для следующих игр
        refreshPlayerData(allPlayers);

        // Обновляем прогресс-бар
        if (progressObject) {
            if (auto thread = qobject_cast<GameGeneratorThread*>(progressObject)) {
                emit thread->progressUpdate(i + 1);
            } else {
                emit progressUpdate(i + 1);
            }
        }
    }

    return m_dbManager->database().commit();
}
// Выбрать игроков с близким уровнем навыка
QVector<PlayerData> GameGenerator::selectBalancedPlayers(int count, QVector<PlayerData> &availablePlayers)
{
    QVector<PlayerData> selectedPlayers;

    // Если недостаточно игроков, вернуть всех доступных
    if (availablePlayers.size() <= count) {
        selectedPlayers = availablePlayers;
        availablePlayers.clear();
        return selectedPlayers;
    }

    // Сортируем игроков по рейтингу
    std::sort(availablePlayers.begin(), availablePlayers.end(),
              [](const PlayerData& a, const PlayerData& b) { return a.rating < b.rating; });

    // Выбираем случайную начальную позицию в пределах диапазона рейтингов
    // чтобы подобрать игроков с близкими рейтингами
    int maxStartPos = availablePlayers.size() - count;
    int startPos = m_random.bounded(maxStartPos + 1);

    // Выбираем последовательно игроков с близкими рейтингами
    for (int i = 0; i < count; ++i) {
        selectedPlayers.append(availablePlayers[startPos + i]);
    }

    // Удаляем выбранных игроков из доступных
    for (int i = count - 1; i >= 0; --i) {
        availablePlayers.removeAt(startPos + i);
    }

    return selectedPlayers;
}

// Распределение игроков по командам с учетом скилла
void GameGenerator::distributePlayers(QVector<PlayerData>& selectedPlayers, QVector<PlayerData>& team1, QVector<PlayerData>& team2)
{
    // Сортируем игроков по рейтингу от высшего к низшему
    std::sort(selectedPlayers.begin(), selectedPlayers.end(),
              [](const PlayerData& a, const PlayerData& b) { return a.rating > b.rating; });

    int teamSize = selectedPlayers.size() / 2;

    // Распределяем игроков по схеме "змейка" для баланса рейтингов
    // 1,4,5,8,... в team1 и 2,3,6,7,... в team2
    for (int i = 0; i < selectedPlayers.size(); ++i) {
        if (i % 4 == 0 || i % 4 == 3) {
            team1.append(selectedPlayers[i]);
        } else {
            team2.append(selectedPlayers[i]);
        }
    }
}

// Расчет изменения рейтинга
QVector<double> GameGenerator::calculateRatingChanges(const QVector<PlayerData>& team,
                                                      const QVector<PlayerData>& opponents,
                                                      bool isWinner)
{
    QVector<double> changes;

    for (const auto& player : team) {
        // Собираем данные о противниках
        QVector<double> opponentRatings;
        QVector<double> opponentRDs;
        QVector<bool> outcomes;

        for (const auto& opponent : opponents) {
            opponentRatings.append(opponent.rating);
            opponentRDs.append(opponent.rd);
            outcomes.append(isWinner);
        }

        // Копируем рейтинг и RD для расчета
        double newRating = player.rating;
        double newRD = player.rd;

        // Рассчитываем новый рейтинг по системе Glicko
        m_ratingSystem.updateRating(newRating, newRD, opponentRatings, opponentRDs, outcomes);

        // Изменение рейтинга
        double change = newRating - player.rating;

        // Ограничиваем для безопасности
        if (change > 50.0) change = 50.0;
        if (change < -50.0) change = -50.0;

        changes.append(change);
    }

    return changes;
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

    // Получаем всех участников игры с их текущими рейтингами и RD
    QSqlQuery participantsQuery(m_dbManager->database());
    participantsQuery.prepare("SELECT gp.player_id, gp.team, r.glicko_rating, r.rd "
                              "FROM game_participation gp "
                              "JOIN ratings r ON gp.player_id = r.player_id "
                              "WHERE gp.game_id = :gameId");
    participantsQuery.bindValue(":gameId", gameId);

    if (!participantsQuery.exec()) {
        qDebug() << "Error retrieving game participants:" << participantsQuery.lastError().text();
        return;
    }

    struct PlayerGlickoData {
        int playerId;
        QString team;
        double rating;
        double rd;
    };

    QVector<PlayerGlickoData> team1Players;
    QVector<PlayerGlickoData> team2Players;

    while (participantsQuery.next()) {
        PlayerGlickoData player;
        player.playerId = participantsQuery.value(0).toInt();
        player.team = participantsQuery.value(1).toString();
        player.rating = participantsQuery.value(2).toDouble();
        player.rd = participantsQuery.value(3).toDouble();

        if (player.team == "team1") {
            team1Players.append(player);
        } else {
            team2Players.append(player);
        }
    }

    // Обновляем рейтинги игроков по системе Glicko
    for (PlayerGlickoData& player : team1Players) {
        // Данные о противниках
        QVector<double> opponentRatings;
        QVector<double> opponentRDs;
        QVector<bool> outcomes;

        for (const PlayerGlickoData& opponent : team2Players) {
            opponentRatings.append(opponent.rating);
            opponentRDs.append(opponent.rd);
            outcomes.append(winnerTeam == "team1"); // true если игрок выиграл
        }

        // Сохраняем старый рейтинг
        double oldRating = player.rating;

        // Обновляем рейтинг по системе Glicko
        m_ratingSystem.updateRating(player.rating, player.rd, opponentRatings, opponentRDs, outcomes);

        // Вычисляем изменение рейтинга
        double ratingChange = player.rating - oldRating;

        // Обновляем данные в базе
        QSqlQuery updateQuery(m_dbManager->database());
        updateQuery.prepare("UPDATE ratings SET glicko_rating = :rating, rd = :rd "
                            "WHERE player_id = :playerId");
        updateQuery.bindValue(":rating", player.rating);
        updateQuery.bindValue(":rd", player.rd);
        updateQuery.bindValue(":playerId", player.playerId);

        if (!updateQuery.exec()) {
            qDebug() << "Error updating rating:" << updateQuery.lastError().text();
        }

        // Обновляем рейтинг в таблице players
        QSqlQuery updatePlayerQuery(m_dbManager->database());
        updatePlayerQuery.prepare("UPDATE players SET glicko_rating = :rating "
                                  "WHERE player_id = :playerId");
        updatePlayerQuery.bindValue(":rating", player.rating);
        updatePlayerQuery.bindValue(":playerId", player.playerId);

        if (!updatePlayerQuery.exec()) {
            qDebug() << "Error updating player rating:" << updatePlayerQuery.lastError().text();
        }

        // Обновляем rating_change в game_participation
        QSqlQuery updateParticipationQuery(m_dbManager->database());
        updateParticipationQuery.prepare("UPDATE game_participation SET rating_change = :ratingChange "
                                         "WHERE game_id = :gameId AND player_id = :playerId");
        updateParticipationQuery.bindValue(":ratingChange", ratingChange);
        updateParticipationQuery.bindValue(":gameId", gameId);
        updateParticipationQuery.bindValue(":playerId", player.playerId);

        if (!updateParticipationQuery.exec()) {
            qDebug() << "Error updating rating change:" << updateParticipationQuery.lastError().text();
        }
    }

    // Аналогично для второй команды
    for (PlayerGlickoData& player : team2Players) {
        QVector<double> opponentRatings;
        QVector<double> opponentRDs;
        QVector<bool> outcomes;

        for (const PlayerGlickoData& opponent : team1Players) {
            opponentRatings.append(opponent.rating);
            opponentRDs.append(opponent.rd);
            outcomes.append(winnerTeam == "team2"); // true если игрок выиграл
        }

        double oldRating = player.rating;
        m_ratingSystem.updateRating(player.rating, player.rd, opponentRatings, opponentRDs, outcomes);
        double ratingChange = player.rating - oldRating;

        QSqlQuery updateQuery(m_dbManager->database());
        updateQuery.prepare("UPDATE ratings SET glicko_rating = :rating, rd = :rd "
                            "WHERE player_id = :playerId");
        updateQuery.bindValue(":rating", player.rating);
        updateQuery.bindValue(":rd", player.rd);
        updateQuery.bindValue(":playerId", player.playerId);

        if (!updateQuery.exec()) {
            qDebug() << "Error updating rating:" << updateQuery.lastError().text();
        }

        QSqlQuery updatePlayerQuery(m_dbManager->database());
        updatePlayerQuery.prepare("UPDATE players SET glicko_rating = :rating "
                                  "WHERE player_id = :playerId");
        updatePlayerQuery.bindValue(":rating", player.rating);
        updatePlayerQuery.bindValue(":playerId", player.playerId);

        if (!updatePlayerQuery.exec()) {
            qDebug() << "Error updating player rating:" << updatePlayerQuery.lastError().text();
        }

        QSqlQuery updateParticipationQuery(m_dbManager->database());
        updateParticipationQuery.prepare("UPDATE game_participation SET rating_change = :ratingChange "
                                         "WHERE game_id = :gameId AND player_id = :playerId");
        updateParticipationQuery.bindValue(":ratingChange", ratingChange);
        updateParticipationQuery.bindValue(":gameId", gameId);
        updateParticipationQuery.bindValue(":playerId", player.playerId);

        if (!updateParticipationQuery.exec()) {
            qDebug() << "Error updating rating change:" << updateParticipationQuery.lastError().text();
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

// Генерирует игроков с указанным уровнем навыка
bool GameGenerator::generatePlayers(int count, int skillLevel)
{
    if (skillLevel < 1 || skillLevel > 4) {
        qDebug() << "Invalid skill level (must be 1-4)";
        return false;
    }

    if (!m_dbManager->database().transaction()) {
        qDebug() << "Failed to start transaction for player generation";
        return false;
    }

    for (int i = 0; i < count; ++i) {
        QString nickname = QString("Player%1_Skill%2").arg(i + 1).arg(skillLevel);

        // Фиксированный начальный рейтинг 1000
        double initialRating = 1000.0;

        QSqlQuery playerQuery(m_dbManager->database());
        playerQuery.prepare("INSERT INTO players (nickname, glicko_rating, skill_level, wins, total_matches, win_rate) "
                            "VALUES (:nickname, :rating, :skill, 0, 0, 0.0)");
        playerQuery.bindValue(":nickname", nickname);
        playerQuery.bindValue(":rating", initialRating);
        playerQuery.bindValue(":skill", skillLevel);

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

// Генерирует заданное количество игроков каждого уровня навыка
bool GameGenerator::generatePlayersBySkill(int lowSkillCount, int mediumSkillCount,
                                           int aboveAverageSkillCount, int highSkillCount)
{
    bool success = true;

    if (lowSkillCount > 0) {
        success = success && generatePlayers(lowSkillCount, 1); // Низкий уровень
    }

    if (mediumSkillCount > 0) {
        success = success && generatePlayers(mediumSkillCount, 2); // Средний уровень
    }

    if (aboveAverageSkillCount > 0) {
        success = success && generatePlayers(aboveAverageSkillCount, 3); // Выше среднего
    }

    if (highSkillCount > 0) {
        success = success && generatePlayers(highSkillCount, 4); // Высокий уровень
    }

    return success;
}

// Стандартный метод выбора случайных игроков (для совместимости)
QVector<PlayerData> GameGenerator::selectRandomPlayers(int count, QVector<PlayerData> &availablePlayers)
{
    QVector<PlayerData> selectedPlayers;

    for (int i = 0; i < count && !availablePlayers.isEmpty(); ++i) {
        int randomIndex = m_random.bounded(availablePlayers.size());
        selectedPlayers.append(availablePlayers.takeAt(randomIndex));
    }

    return selectedPlayers;
}

// Новый вспомогательный метод для обновления данных игроков
void GameGenerator::refreshPlayerData(QVector<PlayerData>& players)
{
    QSqlQuery query(m_dbManager->database());
    query.prepare("SELECT p.player_id, p.glicko_rating, r.rd, p.total_matches, p.wins, p.win_rate, p.skill_level "
                  "FROM players p JOIN ratings r ON p.player_id = r.player_id");

    if (!query.exec()) {
        qDebug() << "Error refreshing player data:" << query.lastError().text();
        return;
    }

    // Создаем временную карту для быстрого доступа к игрокам
    QMap<int, int> playerIdToIndex;
    for (int i = 0; i < players.size(); ++i) {
        playerIdToIndex[players[i].playerId] = i;
    }

    while (query.next()) {
        int playerId = query.value(0).toInt();

        if (playerIdToIndex.contains(playerId)) {
            int idx = playerIdToIndex[playerId];

            players[idx].rating = query.value(1).toDouble();
            players[idx].rd = query.value(2).toDouble();
            players[idx].totalMatches = query.value(3).toInt();
            players[idx].wins = query.value(4).toInt();
            players[idx].winRate = query.value(5).toDouble();
            players[idx].skillLevel = query.value(6).toInt();
        }
    }
}
