// importdatabasethread.h
#ifndef IMPORTDATABASETHREAD_H
#define IMPORTDATABASETHREAD_H

#include <QThread>
#include "databasemanager.h"
#include <QProgressDialog>

class ImportDatabaseThread : public QThread {
    Q_OBJECT

public:
    ImportDatabaseThread(DatabaseManager* dbManager, const QString& filePath, QProgressDialog* progressDialog, QObject *parent = nullptr);
    ~ImportDatabaseThread() override;

    bool success() const;

signals:
    void progressUpdate(int value);
    void finished();

protected:
    void run() override;

private:
    DatabaseManager* m_dbManager;
    QString m_filePath;
    QProgressDialog* m_progressDialog;
    bool m_success;
};

#endif // IMPORTDATABASETHREAD_H
