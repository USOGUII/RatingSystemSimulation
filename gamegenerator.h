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

    // Генерировать игры с учетом уровня навыка
    bool generateGames(int gameCount, const QDateTime &startDate,
                       const QDateTime &endDate, int playersPerTeam, QObject* progressObject = nullptr);

    // Очистить базу данных
    bool clearDatabase();

    // Генерировать игроков с заданным уровнем навыка
    bool generatePlayers(int count, int skillLevel = 1);

    // Генерирование игроков разных уровней навыка
    bool generatePlayersBySkill(int lowSkillCount, int mediumSkillCount,
                                int aboveAverageSkillCount, int highSkillCount);

    void refreshPlayerData(QVector<PlayerData>& players);

signals:
    // Сигнал для обновления прогресса
    void progressUpdate(int value);

private:
    DatabaseManager *m_dbManager;
    QRandomGenerator m_random;

    // Выбор случайных игроков из доступных
    QVector<PlayerData> selectRandomPlayers(int count, QVector<PlayerData> &availablePlayers);

    // Подбор игроков с близким уровнем навыка
    QVector<PlayerData> selectBalancedPlayers(int count, QVector<PlayerData> &availablePlayers);

    // Обновление рейтингов игроков после игры
    void updatePlayerRatings(int gameId);

    // Расчет изменения рейтинга для игроков команды
    QVector<double> calculateRatingChanges(const QVector<PlayerData>& team,
                                           const QVector<PlayerData>& opponents,
                                           bool isWinner);

    // Распределить игроков по командам с балансировкой
    void distributePlayers(QVector<PlayerData>& selectedPlayers,
                           QVector<PlayerData>& team1,
                           QVector<PlayerData>& team2);

    GlickoRatingSystem m_ratingSystem;
};

#endif // GAMEGENERATOR_H
