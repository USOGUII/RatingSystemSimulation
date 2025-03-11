#ifndef PLAYERINFOWINDOW_H
#define PLAYERINFOWINDOW_H

#include <QDialog>
#include "ui_playerinfowindow.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include "databasemanager.h"

class PlayerInfoWindow : public QDialog {
    Q_OBJECT

public:
    explicit PlayerInfoWindow(int playerId, DatabaseManager* dbManager, QWidget *parent = nullptr);
    ~PlayerInfoWindow();

private:
    Ui::PlayerInfoWindow *ui;
    int playerId;
    DatabaseManager* dbManager;

    void loadPlayerInfo();
    void loadGameHistory();

private slots:
    void onRowDoubleClicked(const QModelIndex &index);
};

#endif // PLAYERINFOWINDOW_H
