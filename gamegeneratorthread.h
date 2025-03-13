#ifndef GAMEGENERATORTHREAD_H
#define GAMEGENERATORTHREAD_H

#include <QThread>
#include <QDateTime>
#include "databasemanager.h"
#include "gamegenerator.h"

class GameGeneratorThread : public QThread {
    Q_OBJECT
public:
    GameGeneratorThread(DatabaseManager* dbManager, int gameCount,
                        const QDateTime &startDate, const QDateTime &endDate,
                        int playersPerTeam, QObject *parent = nullptr);
    ~GameGeneratorThread() override;

protected:
    void run() override;

signals:
    void progressUpdate(int value);
    void timeElapsed(int msec);
    void finished();

private:
    DatabaseManager* m_dbManager;
    int m_gameCount;
    QDateTime m_startDate;
    QDateTime m_endDate;
    int m_playersPerTeam;
};

#endif // GAMEGENERATORTHREAD_H
