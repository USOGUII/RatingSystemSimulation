// gamegeneratorthread.cpp
#include "gamegeneratorthread.h"
#include <QDebug>

GameGeneratorThread::GameGeneratorThread(DatabaseManager* dbManager, int gameCount,
                                         const QDateTime &startDate, const QDateTime &endDate,
                                         int playersPerTeam, QObject *parent)
    : QThread(parent), m_dbManager(dbManager), m_gameCount(gameCount),
    m_startDate(startDate), m_endDate(endDate), m_playersPerTeam(playersPerTeam) {}

GameGeneratorThread::~GameGeneratorThread() {}

void GameGeneratorThread::run() {
    qint64 start = QDateTime::currentMSecsSinceEpoch();
    GameGenerator gameGen(m_dbManager, this);
    bool success = gameGen.generateGames(m_gameCount, m_startDate, m_endDate, m_playersPerTeam, this);
    if (!success) {
        qDebug() << "Game generation failed in thread!";
    }
    qint64 end = QDateTime::currentMSecsSinceEpoch();
    int duration = end - start;
    emit timeElapsed(duration);
    emit finished();
}
