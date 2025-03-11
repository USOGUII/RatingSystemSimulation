#include "gameinfowindow.h"

GameInfoWindow::GameInfoWindow(int gameId, DatabaseManager* dbManager, QWidget *parent)
    : QDialog(parent), ui(new Ui::GameInfoWindow), gameId(gameId), dbManager(dbManager) {
    ui->setupUi(this);
    loadGameInfo();
    setWindowTitle("Информация об игре");
}


GameInfoWindow::~GameInfoWindow() {
    delete ui;
}

void GameInfoWindow::loadGameInfo() {
    QSqlDatabase db = dbManager->database();
    QSqlQuery query(db);

    // Получаем основную информацию об игре
    query.prepare("SELECT team1_score, team2_score, winner_team FROM games WHERE game_id = :gameId");
    query.bindValue(":gameId", gameId);

    if (!query.exec()) {
        qDebug() << "Error retrieving game info:" << query.lastError().text();
        return;
    }

    if (query.next()) {
        int team1Score = query.value(0).toInt();
        int team2Score = query.value(1).toInt();
        QString winnerTeam = query.value(2).toString();

        //  Получаем ID команд из таблицы game_participation
        QString team1Squad = getTeamSquad(gameId, "team1");
        QString team2Squad = getTeamSquad(gameId, "team2");
        // Отображаем информацию в UI
        ui->team1SquadLabel->setText("Состав команды 1:\n" + team1Squad);
        ui->team2SquadLabel->setText("Состав команды 2:\n" + team2Squad);
        ui->winnerLabel->setText("Победитель: " + winnerTeam);

        // Формируем строку со счетом
        QString scoreText = QString::number(team1Score) + " - " + QString::number(team2Score);
        ui->team1ScoreLabel->setText("Счет: " + scoreText);
    }
    ui->gameIDLabel->setText("ID игры: " + QString::number(gameId));
}
QString GameInfoWindow::getTeamSquad(int gameId, const QString& team) {
    QSqlDatabase db = dbManager->database();
    QSqlQuery query(db);
    QString squad;

    // Запрос для получения игроков команды, участвовавших в данной игре
    query.prepare("SELECT p.nickname FROM players p JOIN game_participation gp ON p.player_id = gp.player_id WHERE gp.game_id = :gameId AND gp.team = :team");
    query.bindValue(":gameId", gameId);
    query.bindValue(":team", team);

    if (!query.exec()) {
        qDebug() << "Error retrieving team squad:" << query.lastError().text();
        return "Ошибка получения состава";
    }

    while (query.next()) {
        squad += query.value(0).toString() + "\n";
    }

    return squad;
}
