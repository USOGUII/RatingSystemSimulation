// gamegeneratorthread.h
#ifndef GAMEGENERATOR_THREAD_H
#define GAMEGENERATOR_THREAD_H

#include <QThread>
#include "databasemanager.h"
#include "gamegenerator.h"
#include <QDateTime>

class GameGeneratorThread : public QThread {
    Q_OBJECT

public:
    GameGeneratorThread(DatabaseManager* dbManager, int gameCount, const QDateTime &startDate,
                        const QDateTime &endDate, int playersPerTeam, QObject *parent = nullptr);
    ~GameGeneratorThread() override;

signals:
    void progressUpdate(int value);
    void finished();
    void timeElapsed(int msec);

protected:
    void run() override;

private:
    DatabaseManager* m_dbManager;
    int m_gameCount;
    QDateTime m_startDate;
    QDateTime m_endDate;
    int m_playersPerTeam;
};

#endif // GAMEGENERATOR_THREAD_H
