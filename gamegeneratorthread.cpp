// gamegeneratorthread.cpp
#include "gamegeneratorthread.h"
#include <QDebug>
#include <QThread>

GameGeneratorThread::GameGeneratorThread(DatabaseManager* dbManager, int gameCount,
                                         const QDateTime &startDate, const QDateTime &endDate,
                                         int playersPerTeam, QObject *parent)
    : QThread(parent), m_dbManager(dbManager), m_gameCount(gameCount),
    m_startDate(startDate), m_endDate(endDate), m_playersPerTeam(playersPerTeam) {}

GameGeneratorThread::~GameGeneratorThread() {
    // Правильное завершение потока при уничтожении объекта
    if (isRunning()) {
        quit();
        wait();
    }
}

void GameGeneratorThread::run() {
    qint64 start = QDateTime::currentMSecsSinceEpoch();

    // Создаем генератор игр в потоке
    GameGenerator gameGen(m_dbManager);

    // Соединяем сигнал прогресса из GameGenerator с сигналом в этом классе
    connect(&gameGen, &GameGenerator::progressUpdate,
            this, &GameGeneratorThread::progressUpdate, Qt::DirectConnection);

    // Запускаем генерацию игр
    bool success = gameGen.generateGames(m_gameCount, m_startDate, m_endDate, m_playersPerTeam, this);

    if (!success) {
        qDebug() << "Game generation failed in thread!";
    }

    qint64 end = QDateTime::currentMSecsSinceEpoch();
    int duration = end - start;

    emit timeElapsed(duration);
    emit finished();
}
