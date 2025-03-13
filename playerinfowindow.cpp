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

    query.prepare("SELECT p.nickname, r.glicko_rating, r.rd, p.win_rate FROM players p JOIN ratings r ON p.player_id = r.player_id WHERE p.player_id = :playerId");
    query.bindValue(":playerId", playerId);

    if (!query.exec()) {
        qDebug() << "Error retrieving player info:" << query.lastError().text() << "Query:" << query.lastQuery();
        return;
    }

    if (query.next()) {
        QString nickname = query.value(0).toString();
        int rating = qRound(query.value(1).toDouble()); // Округляем до целого
        int rd = qRound(query.value(2).toDouble());     // Округляем до целого
        QString winrate = QString::number(query.value(3).toDouble(), 'f', 1) + "%"; // Процент побед

        ui->playerLabel->setText(nickname);
        ui->rdLabel_2->setText("Рейтинг: " + QString::number(rating));
        ui->raitingLabel->setText("RD: " + QString::number(rd));
        ui->winrateLabel->setText("Winrate: " + winrate);
    } else {
        qDebug() << "Player not found with ID:" << playerId;
    }
}



void PlayerInfoWindow::loadGameHistory() {
    QSqlDatabase db = dbManager->database();
    QSqlQuery query(db);

    query.prepare("SELECT gp.game_id, g.team1_score, g.team2_score, g.game_date, "
                  "CASE WHEN gp.team = g.winner_team THEN 'Победа' ELSE 'Поражение' END AS result, "
                  "gp.rating_change " // Добавляем изменение рейтинга
                  "FROM game_participation gp "
                  "JOIN games g ON gp.game_id = g.game_id "
                  "WHERE gp.player_id = :playerId "
                  "ORDER BY gp.game_id DESC"); // Сортировка по ID игры по убыванию
    query.bindValue(":playerId", playerId);

    if (!query.exec()) {
        qDebug() << "Error retrieving game history:" << query.lastError().text();
        return;
    }

    QStandardItemModel* model = new QStandardItemModel(0, 6, this);
    model->setHorizontalHeaderLabels(QStringList()
                                     << "ID игры"
                                     << "Счет команды 1"
                                     << "Счет команды 2"
                                     << "Дата игры"
                                     << "Результат"
                                     << "Δ Рейтинга"); // Новый заголовок

    while (query.next()) {
        QList<QStandardItem*> items;
        // Основные данные
        items << new QStandardItem(query.value(0).toString())
              << new QStandardItem(query.value(1).toString())
              << new QStandardItem(query.value(2).toString())
              << new QStandardItem(query.value(3).toDateTime().toString("dd.MM.yyyy HH:mm"));

        // Результат с цветовым оформлением
        QStandardItem* resultItem = new QStandardItem(query.value(4).toString());
        resultItem->setForeground(query.value(4).toString() == "Победа"
                                      ? QBrush(Qt::darkGreen)
                                      : QBrush(Qt::red));

        // Изменение рейтинга с форматированием
        double ratingChange = query.value(5).toDouble();
        QStandardItem* changeItem = new QStandardItem();
        changeItem->setData(ratingChange, Qt::DisplayRole);
        changeItem->setText(QString("%1%2")
                                .arg(ratingChange > 0 ? "+" : "")
                                .arg(ratingChange, 0, 'f', 1));

        changeItem->setForeground(ratingChange >= 0
                                      ? QBrush(Qt::darkGreen)
                                      : QBrush(Qt::red));

        items << resultItem << changeItem;
        model->appendRow(items);
    }

    ui->tableView->setModel(model);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
}



void PlayerInfoWindow::onRowDoubleClicked(const QModelIndex &index) {
    int gameId = index.sibling(index.row(), 0).data().toInt();
    GameInfoWindow* gameInfo = new GameInfoWindow(gameId, dbManager, this);
    gameInfo->show();
}
