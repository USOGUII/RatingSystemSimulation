// importdatabasethread.cpp
#include "importdatabasethread.h"
#include <QDebug>

ImportDatabaseThread::ImportDatabaseThread(DatabaseManager* dbManager, const QString& filePath, QProgressDialog* progressDialog, QObject *parent)
    : QThread(parent), m_dbManager(dbManager), m_filePath(filePath), m_progressDialog(progressDialog), m_success(false) {}

ImportDatabaseThread::~ImportDatabaseThread() {}

bool ImportDatabaseThread::success() const {
    return m_success;
}

void ImportDatabaseThread::run() {
    m_success = m_dbManager->importFromJson(m_filePath);
    emit finished();
}
