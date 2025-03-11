#ifndef GAMEINFOWINDOW_H
#define GAMEINFOWINDOW_H

#include <QDialog>
#include "ui_gameinfowindow.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include "databasemanager.h"  // Include DatabaseManager

class GameInfoWindow : public QDialog {
    Q_OBJECT

public:
    explicit GameInfoWindow(int gameId, DatabaseManager* dbManager, QWidget *parent = nullptr);
    ~GameInfoWindow();

private:
    Ui::GameInfoWindow *ui;
    int gameId;
    DatabaseManager* dbManager;

    void loadGameInfo();
    QString getTeamSquad(int teamId, const QString& team);
};

#endif // GAMEINFOWINDOW_H
