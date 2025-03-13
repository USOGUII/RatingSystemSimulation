#include "databasemanager.h"

DatabaseManager::DatabaseManager(const QString &dbPath, QObject *parent)
    : QObject(parent), m_dbPath(dbPath)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::isOpen() const
{
    return m_db.isOpen();
}

bool DatabaseManager::initialize()
{
    if (!m_db.open()) {
        qDebug() << "Error: Failed to connect to database:" << m_db.lastError().text();
        return false;
    }

    return createTables();
}

bool DatabaseManager::createTables()
{
    // Begin transaction
    if (!executeQuery("BEGIN TRANSACTION;")) {
        return false;
    }

    // Create tables
    QStringList tables = {
        "CREATE TABLE IF NOT EXISTS \"players\" ("
        "\"player_id\" INTEGER,"
        "\"nickname\" TEXT NOT NULL UNIQUE,"
        "\"glicko_rating\" REAL DEFAULT 1000.0,"
        "\"skill_level\" INTEGER DEFAULT 1," // Уровень навыка игрока
        "\"wins\" INTEGER DEFAULT 0," // Количество побед
        "\"total_matches\" INTEGER DEFAULT 0," // Общее количество игр
        "\"win_rate\" REAL DEFAULT 0.0," // Процент побед
        "PRIMARY KEY(\"player_id\" AUTOINCREMENT)"
        ");",

        "CREATE TABLE IF NOT EXISTS \"games\" ("
        "\"game_id\" INTEGER,"
        "\"game_date\" DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "\"team1_score\" INTEGER NOT NULL,"
        "\"team2_score\" INTEGER NOT NULL,"
        "\"winner_team\" TEXT NOT NULL CHECK(\"winner_team\" IN ('team1', 'team2')),"
        "PRIMARY KEY(\"game_id\" AUTOINCREMENT)"
        ");",

        "CREATE TABLE IF NOT EXISTS \"game_participation\" ("
        "\"participation_id\" INTEGER,"
        "\"game_id\" INTEGER NOT NULL,"
        "\"player_id\" INTEGER NOT NULL,"
        "\"team\" TEXT NOT NULL CHECK(\"team\" IN ('team1', 'team2')),"
        "\"rating_change\" REAL DEFAULT 0.0," // Изменение рейтинга после игры
        "PRIMARY KEY(\"participation_id\" AUTOINCREMENT),"
        "FOREIGN KEY(\"game_id\") REFERENCES \"games\"(\"game_id\"),"
        "FOREIGN KEY(\"player_id\") REFERENCES \"players\"(\"player_id\")"
        ");",

        "CREATE TABLE IF NOT EXISTS \"ratings\" ("
        "\"rating_id\" INTEGER,"
        "\"player_id\" INTEGER NOT NULL UNIQUE,"
        "\"glicko_rating\" REAL DEFAULT 1000.0,"
        "\"rd\" REAL DEFAULT 350.0,"
        "\"total_matches\" INTEGER DEFAULT 0,"
        "PRIMARY KEY(\"rating_id\" AUTOINCREMENT),"
        "FOREIGN KEY(\"player_id\") REFERENCES \"players\"(\"player_id\")"
        ");"
    };

    for (const QString &query : tables) {
        if (!executeQuery(query)) {
            executeQuery("ROLLBACK;");
            return false;
        }
    }

    // Commit transaction
    return executeQuery("COMMIT;");
}

bool DatabaseManager::executeQuery(const QString &query)
{
    QSqlQuery sqlQuery;
    if (!sqlQuery.exec(query)) {
        qDebug() << "Query error:" << sqlQuery.lastError().text() << "for query:" << query;
        return false;
    }
    return true;
}

bool DatabaseManager::addPlayer(const QString &nickname, int skillLevel, double glickoRating)
{
    QSqlQuery query;
    query.prepare("INSERT INTO players (nickname, glicko_rating, skill_level, wins, total_matches, win_rate) "
                  "VALUES (:nickname, :rating, :skillLevel, 0, 0, 0.0)");
    query.bindValue(":nickname", nickname);
    query.bindValue(":rating", glickoRating);
    query.bindValue(":skillLevel", skillLevel);

    if (!query.exec()) {
        qDebug() << "Error adding player:" << query.lastError().text();
        return false;
    }

    int playerId = query.lastInsertId().toInt();

    // Also add an entry to the ratings table
    QSqlQuery ratingQuery;
    ratingQuery.prepare("INSERT INTO ratings (player_id, glicko_rating) VALUES (:playerId, :rating)");
    ratingQuery.bindValue(":playerId", playerId);
    ratingQuery.bindValue(":rating", glickoRating);

    if (!ratingQuery.exec()) {
        qDebug() << "Error adding rating:" << ratingQuery.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::addGame(int team1Score, int team2Score, const QString &winnerTeam)
{
    QSqlQuery query;
    query.prepare("INSERT INTO games (team1_score, team2_score, winner_team) "
                  "VALUES (:team1Score, :team2Score, :winnerTeam)");
    query.bindValue(":team1Score", team1Score);
    query.bindValue(":team2Score", team2Score);
    query.bindValue(":winnerTeam", winnerTeam);

    if (!query.exec()) {
        qDebug() << "Error adding game:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::addPlayerToGame(int gameId, int playerId, const QString &team, double ratingChange)
{
    QSqlQuery query;
    query.prepare("INSERT INTO game_participation (game_id, player_id, team, rating_change) "
                  "VALUES (:gameId, :playerId, :team, :ratingChange)");
    query.bindValue(":gameId", gameId);
    query.bindValue(":playerId", playerId);
    query.bindValue(":team", team);
    query.bindValue(":ratingChange", ratingChange);

    if (!query.exec()) {
        qDebug() << "Error adding player to game:" << query.lastError().text();
        return false;
    }

    // Не обновляем здесь статистику игроков, это будет делаться централизованно
    // при обработке всего игрового процесса для предотвращения проблемы
    // с подсчетом побед

    return true;
}

int DatabaseManager::getPlayerId(const QString &nickname)
{
    QSqlQuery query;
    query.prepare("SELECT player_id FROM players WHERE nickname = :nickname");
    query.bindValue(":nickname", nickname);

    if (!query.exec() || !query.next()) {
        qDebug() << "Error retrieving player:" << query.lastError().text();
        return -1;
    }

    return query.value(0).toInt();
}

bool DatabaseManager::updatePlayerRating(int playerId, double glickoRating, double rd, int totalMatches, int wins)
{
    QSqlQuery playerQuery;
    // Рассчитываем winRate
    double winRate = (totalMatches > 0) ? (static_cast<double>(wins) / totalMatches * 100.0) : 0.0;

    playerQuery.prepare("UPDATE players SET glicko_rating = :rating, total_matches = :matches, "
                        "wins = :wins, win_rate = :winRate WHERE player_id = :playerId");
    playerQuery.bindValue(":rating", glickoRating);
    playerQuery.bindValue(":matches", totalMatches);
    playerQuery.bindValue(":wins", wins);
    playerQuery.bindValue(":winRate", winRate);
    playerQuery.bindValue(":playerId", playerId);

    if (!playerQuery.exec()) {
        qDebug() << "Error updating player rating:" << playerQuery.lastError().text();
        return false;
    }

    QSqlQuery ratingQuery;
    ratingQuery.prepare("UPDATE ratings SET glicko_rating = :rating, rd = :rd, total_matches = :matches "
                        "WHERE player_id = :playerId");
    ratingQuery.bindValue(":rating", glickoRating);
    ratingQuery.bindValue(":rd", rd);
    ratingQuery.bindValue(":matches", totalMatches);
    ratingQuery.bindValue(":playerId", playerId);

    if (!ratingQuery.exec()) {
        qDebug() << "Error updating rating details:" << ratingQuery.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::exportToJson(const QString& filePath) {
    QSqlDatabase db = database();
    QJsonObject databaseObject;

    QStringList tableNames;
    tableNames << "players" << "ratings" << "games" << "game_participation";

    for (const QString& tableName : tableNames) {
        QSqlQuery query(db);
        if (!query.exec("SELECT * FROM " + tableName)) {
            qDebug() << "Error querying table" << tableName << ":" << query.lastError().text();
            return false;
        }

        QJsonArray tableArray;
        while (query.next()) {
            QJsonObject rowObject;
            QSqlRecord record = query.record();
            for (int i = 0; i < record.count(); ++i) {
                rowObject[record.fieldName(i)] = QJsonValue::fromVariant(record.value(i));
            }
            tableArray.append(rowObject);
        }
        databaseObject[tableName] = tableArray;
    }

    QJsonDocument jsonDoc(databaseObject);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Failed to open file for writing:" << filePath;
        return false;
    }

    file.write(jsonDoc.toJson());
    file.close();

    return true;
}

bool DatabaseManager::importFromJson(const QString& filePath) {
    QSqlDatabase db = database();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file for reading:" << filePath;
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
    if (jsonDoc.isNull()) {
        qDebug() << "Failed to parse JSON data.";
        return false;
    }

    QJsonObject databaseObject = jsonDoc.object();

    QStringList tableNames;
    tableNames << "players" << "ratings" << "games" << "game_participation";

    for (const QString& tableName : tableNames) {
        if (!databaseObject.contains(tableName)) {
            qDebug() << "JSON data does not contain table:" << tableName;
            return false;
        }

        QJsonArray tableArray = databaseObject[tableName].toArray();
        for (int i = 0; i < tableArray.size(); ++i) {
            QJsonObject rowObject = tableArray[i].toObject();

            // Build the INSERT query
            QStringList keys = rowObject.keys();
            QString insertQuery = "INSERT INTO " + tableName + " (";
            QString valuesQuery = "VALUES (";

            for (int j = 0; j < keys.size(); ++j) {
                insertQuery += keys[j];
                valuesQuery += ":" + keys[j];

                if (j < keys.size() - 1) {
                    insertQuery += ", ";
                    valuesQuery += ", ";
                }
            }

            insertQuery += ") ";
            valuesQuery += ")";

            QSqlQuery query(db);
            query.prepare(insertQuery + valuesQuery);

            for (const QString& key : keys) {
                query.bindValue(":" + key, rowObject[key].toVariant());
            }

            if (!query.exec()) {
                qDebug() << "Error inserting into table" << tableName << ":" << query.lastError().text();
                return false;
            }
        }
    }

    return true;
}

QVector<QVector<QString>> DatabaseManager::getGamesTable() {
    QSqlDatabase db = database();
    QSqlQuery query(db);
    if (!query.exec("SELECT game_id, game_date, team1_score, team2_score, winner_team FROM games")) {
        qDebug() << "Error retrieving games table with ID:" << query.lastError().text();
        return QVector<QVector<QString>>();
    }

    QVector<QVector<QString>> result;
    while (query.next()) {
        QSqlRecord record = query.record();
        QVector<QString> row;
        for (int i = 0; i < record.count(); ++i) {
            row.append(record.value(i).toString());
        }
        result.append(row);
    }
    return result;
}

QVector<QVector<QString>> DatabaseManager::getPlayersWithRatings() {
    QSqlDatabase db = database();
    QSqlQuery query(db);
    if (!query.exec("SELECT p.nickname, p.glicko_rating, r.rd, p.total_matches, p.skill_level, p.wins, p.win_rate "
                    "FROM players p JOIN ratings r ON p.player_id = r.player_id "
                    "ORDER BY p.glicko_rating DESC")) {
        qDebug() << "Error retrieving players with ratings:" << query.lastError().text();
        return QVector<QVector<QString>>();
    }

    QVector<QVector<QString>> result;
    while (query.next()) {
        QSqlRecord record = query.record();
        QVector<QString> row;

        row.append(record.value(0).toString()); // Никнейм
        row.append(QString::number(qRound(record.value(1).toDouble()))); // Рейтинг
        row.append(QString::number(qRound(record.value(2).toDouble()))); // RD
        row.append(record.value(3).toString()); // Общее количество игр

        // Текстовое представление уровня навыка
        int skillLevel = record.value(4).toInt();
        QString skillText;
        switch (skillLevel) {
        case 1: skillText = "Низкий"; break;
        case 2: skillText = "Средний"; break;
        case 3: skillText = "Выше среднего"; break;
        case 4: skillText = "Высокий"; break;
        default: skillText = "Неизвестный";
        }
        row.append(skillText); // Уровень навыка

        row.append(record.value(5).toString()); // Количество побед
        row.append(QString::number(record.value(6).toDouble(), 'f', 1) + "%"); // Процент побед

        result.append(row);
    }
    return result;
}

QMap<int, int> DatabaseManager::getRatingData() {
    QMap<int, int> ratingData;
    QSqlDatabase db = database();
    QSqlQuery query(db);

    // Запрос для получения рейтинга и количества игр для каждого игрока
    query.prepare("SELECT r.glicko_rating, p.total_matches FROM ratings r JOIN players p ON r.player_id = p.player_id");

    if (!query.exec()) {
        qDebug() << "Error retrieving rating data:" << query.lastError().text();
        return ratingData; // Возвращаем пустую карту в случае ошибки
    }

    while (query.next()) {
        int rating = qRound(query.value(0).toDouble()); // Округляем рейтинг до целого числа
        int totalMatches = query.value(1).toInt();
        ratingData[rating] = totalMatches;
    }

    return ratingData;
}

QVector<PlayerData> DatabaseManager::getPlayersForMatching() {
    QSqlDatabase db = database();
    QSqlQuery query(db);

    if (!query.exec("SELECT p.player_id, p.nickname, p.glicko_rating, r.rd, p.skill_level, p.total_matches, p.wins, p.win_rate "
                    "FROM players p JOIN ratings r ON p.player_id = r.player_id")) {
        qDebug() << "Error retrieving players for matching:" << query.lastError().text();
        return QVector<PlayerData>();
    }

    QVector<PlayerData> result;
    while (query.next()) {
        PlayerData player;
        player.playerId = query.value(0).toInt();
        player.nickname = query.value(1).toString();
        player.rating = query.value(2).toDouble();
        player.rd = query.value(3).toDouble();
        player.skillLevel = query.value(4).toInt();
        player.totalMatches = query.value(5).toInt();
        player.wins = query.value(6).toInt();
        player.winRate = query.value(7).toDouble();

        result.append(player);
    }

    return result;
}

QVector<QVector<QString>> DatabaseManager::getGameDetails(int gameId) {
    QSqlDatabase db = database();
    QSqlQuery query(db);

    query.prepare("SELECT p.nickname, gp.team, p.glicko_rating, gp.rating_change, p.skill_level, p.win_rate "
                  "FROM game_participation gp "
                  "JOIN players p ON gp.player_id = p.player_id "
                  "WHERE gp.game_id = :gameId "
                  "ORDER BY gp.team, p.nickname");
    query.bindValue(":gameId", gameId);

    if (!query.exec()) {
        qDebug() << "Error retrieving game details:" << query.lastError().text();
        return QVector<QVector<QString>>();
    }

    QVector<QVector<QString>> result;
    while (query.next()) {
        QVector<QString> row;
        row.append(query.value(0).toString()); // Никнейм
        row.append(query.value(1).toString()); // Команда
        row.append(QString::number(qRound(query.value(2).toDouble()))); // Рейтинг

        // Форматируем изменение рейтинга
        double ratingChange = query.value(3).toDouble();
        QString ratingChangeStr = QString::number(ratingChange, 'f', 1);
        if (ratingChange > 0) ratingChangeStr = "+" + ratingChangeStr;
        row.append(ratingChangeStr); // Изменение рейтинга

        // Уровень навыка
        int skillLevel = query.value(4).toInt();
        QString skillText;
        switch (skillLevel) {
        case 1: skillText = "Низкий"; break;
        case 2: skillText = "Средний"; break;
        case 3: skillText = "Выше среднего"; break;
        case 4: skillText = "Высокий"; break;
        default: skillText = "Неизвестный";
        }
        row.append(skillText);

        // Винрейт
        row.append(QString::number(query.value(5).toDouble(), 'f', 1) + "%");

        result.append(row);
    }

    return result;
}

bool DatabaseManager::updatePlayerWinStats(int playerId, bool isWin) {
    QSqlQuery query;

    if (isWin) {
        query.prepare("UPDATE players SET wins = wins + 1, "
                      "win_rate = CASE WHEN total_matches > 0 THEN (wins * 100.0 / total_matches) ELSE 0 END "
                      "WHERE player_id = :playerId");
    } else {
        query.prepare("UPDATE players SET "
                      "win_rate = CASE WHEN total_matches > 0 THEN (wins * 100.0 / total_matches) ELSE 0 END "
                      "WHERE player_id = :playerId");
    }

    query.bindValue(":playerId", playerId);

    if (!query.exec()) {
        qDebug() << "Error updating player win stats:" << query.lastError().text();
        return false;
    }

    return true;
}
