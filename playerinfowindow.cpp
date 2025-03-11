#include "playerinfowindow.h"
#include "gameinfowindow.h"
#include "qstandarditemmodel.h"

PlayerInfoWindow::PlayerInfoWindow(int playerId, DatabaseManager* dbManager, QWidget *parent)
    : QDialog(parent), ui(new Ui::PlayerInfoWindow), playerId(playerId), dbManager(dbManager) {
    ui->setupUi(this);
    setWindowTitle("Информация об игроке");
    loadPlayerInfo();
    loadGameHistory();

    // Подключаем сигнал для двойного клика на строку
    connect(ui->tableView, &QTableView::doubleClicked, this, &PlayerInfoWindow::onRowDoubleClicked);
}

PlayerInfoWindow::~PlayerInfoWindow() {
    delete ui;
}

void PlayerInfoWindow::loadPlayerInfo() {
    QSqlDatabase db = dbManager->database();
    QSqlQuery query(db);

    query.prepare("SELECT p.nickname, r.glicko_rating, r.rd FROM players p JOIN ratings r ON p.player_id = r.player_id WHERE p.player_id = :playerId");
    query.bindValue(":playerId", playerId);

    if (!query.exec()) {
        qDebug() << "Error retrieving player info:" << query.lastError().text() << "Query:" << query.lastQuery();
        return;
    }

    if (query.next()) {
        QString nickname = query.value(0).toString();
        int rating = qRound(query.value(1).toDouble()); // Округляем до целого
        int rd = qRound(query.value(2).toDouble());     // Округляем до целого

        ui->playerLabel->setText(nickname);
        ui->rdLabel_2->setText("Рейтинг: " + QString::number(rating));
        ui->raitingLabel->setText("RD: " + QString::number(rd));
    } else {
        qDebug() << "Player not found with ID:" << playerId;
    }
}



void PlayerInfoWindow::loadGameHistory() {
    QSqlDatabase db = dbManager->database();
    QSqlQuery query(db);

    query.prepare("SELECT gp.game_id, g.team1_score, g.team2_score, g.game_date, CASE WHEN gp.team = 'team1' THEN 'Победа' ELSE 'Поражение' END AS result "
                  "FROM game_participation gp JOIN games g ON gp.game_id = g.game_id "
                  "WHERE gp.player_id = :playerId");
    query.bindValue(":playerId", playerId);

    if (!query.exec()) {
        qDebug() << "Error retrieving game history:" << query.lastError().text();
        return;
    }

    QStandardItemModel* model = new QStandardItemModel(0, 5, this);
    model->setHorizontalHeaderLabels(QStringList() << "ID игры" << "Счет команды 1" << "Счет команды 2" << "Дата игры" << "Результат");

    while (query.next()) {
        QList<QStandardItem*> items;
        items.append(new QStandardItem(query.value(0).toString()));
        items.append(new QStandardItem(query.value(1).toString()));
        items.append(new QStandardItem(query.value(2).toString()));
        items.append(new QStandardItem(query.value(3).toString()));
        items.append(new QStandardItem(query.value(4).toString()));

        model->appendRow(items);
    }

    ui->tableView->setModel(model);
}

void PlayerInfoWindow::onRowDoubleClicked(const QModelIndex &index) {
    int gameId = index.sibling(index.row(), 0).data().toInt();
    GameInfoWindow* gameInfo = new GameInfoWindow(gameId, dbManager, this);
    gameInfo->show();
}
