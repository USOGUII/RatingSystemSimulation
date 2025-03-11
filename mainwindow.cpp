#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "QMessageBox"
#include <QProgressDialog>
#include "gamegeneratorthread.h" // Include the header file for your thread
#include "gameinfowindow.h"
#include "importdatabasethread.h"
#include "playerinfowindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , dbManager("game_stats.db")  // Инициализируем dbManager здесь
    , gameGen(&dbManager)
{
    ui->setupUi(this);
    setWindowTitle("Симуляция рейтинговой системы");
    if (!dbManager.initialize()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось инициализировать базу данных!");
    }
    QStringList options = QStringList() << "Список игр" << "Игроки";
    ui->comboBox->addItems(options);
    connect(ui->tableView, &QTableView::doubleClicked, this, &MainWindow::onRowDoubleClicked);
   /*
    // Print top 5 players by rating
    QSqlQuery topPlayersQuery(dbManager.database());
    if (topPlayersQuery.exec("SELECT p.nickname, r.glicko_rating, r.total_matches "
                             "FROM players p JOIN ratings r ON p.player_id = r.player_id "
                             "ORDER BY r.glicko_rating DESC LIMIT 5")) {
        qDebug() << "Top 5 players by rating:";
        while (topPlayersQuery.next()) {
            QString nickname = topPlayersQuery.value(0).toString();
            double rating = topPlayersQuery.value(1).toDouble();
            int matches = topPlayersQuery.value(2).toInt();
            qDebug() << nickname << "- Rating:" << rating << "- Matches:" << matches;
        }
    }*/
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_2_clicked()
{
    QDateTime startDate = ui->startDateBox->dateTime();
    QDateTime endDate = ui->endDateBox->dateTime();

    if (startDate > endDate) {
        QMessageBox::critical(this, "Ошибка", "Дата начала не может быть позже даты окончания!");
        return;
    }

    int playerCount = ui->playerCountBox->value();
    int playersInTeam = ui->playersInTeamBox->value();

    if (playersInTeam * 2 >= playerCount) {
        QMessageBox::critical(this, "Ошибка", "Количество игроков в команде должно быть меньше половины общего количества игроков!");
        return;
    }

    if (!gameGen.clearDatabase()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось очистить базу данных!");
        return;
    }

    if (!gameGen.generatePlayers(ui->playerCountBox->value())) {
        QMessageBox::critical(this, "Ошибка", "Не удалось сгенерировать игроков!");
        return;
    }
    /*
    if (!gameGen.generateGames(ui->gameCountBox->value(), ui->startDateBox->dateTime(), ui->endDateBox->dateTime(), ui->playersInTeamBox->value())) {
        QMessageBox::critical(this, "Ошибка", "Ошибка в генерации игр");
        return;
    }*/
        int gameCount = ui->gameCountBox->value();

        // Disable button while generating to prevent multiple clicks
        ui->pushButton_2->setEnabled(false);

        //Create a progress dialog
        QProgressDialog* progressDialog = new QProgressDialog("Идет генерация игр...", "Отмена", 0, gameCount, this);
        progressDialog->setStyleSheet("background-color: #2f2f2f; color: white;");
        progressDialog->setWindowModality(Qt::WindowModal);
        progressDialog->setAutoClose(true);
        progressDialog->setAutoReset(true);
        progressDialog->setValue(0);
        progressDialog->setFixedSize(progressDialog->size()); // Установить фиксированный размер
        progressDialog->show();

        // Создаем и запускаем поток
        GameGeneratorThread* thread = new GameGeneratorThread(&dbManager, gameCount, startDate, endDate, playersInTeam, this);

        // Connect the signals:
        connect(thread, &GameGeneratorThread::timeElapsed, this, &MainWindow::showTimeElapsed);

        // When thread updates progress, update the progress dialog.
        connect(thread, &GameGeneratorThread::progressUpdate, progressDialog, &QProgressDialog::setValue);

        //When the job is done in gameGeneratorThread, clean up
        connect(thread, &GameGeneratorThread::finished, this, [=]() {
            progressDialog->setValue(gameCount); // Ensure 100% completion is shown.
            progressDialog->close();
            delete progressDialog;  // Important:  Clean up dialog.
            thread->deleteLater();    //Important: Delete thread *after* it's finished.
            ui->pushButton_2->setEnabled(true); // Re-enable the button.
        });


        thread->start();  // Start the thread.
}

void MainWindow::showTimeElapsed(int msec) {
    int currentIndex = ui->comboBox->currentIndex();
    ui->comboBox->setCurrentIndex(1);
    ui->comboBox->setCurrentIndex(0);
    ui->comboBox->setCurrentIndex(currentIndex);

    QDialog *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFixedSize(300, 150);
    dialog->setStyleSheet("background-color: #2f2f2f; color: white;");

    int minutes = msec / 60000;
    int seconds = (msec % 60000) / 1000;
    int milliseconds = msec % 1000;

    QString timeText = QString("Генерация заняла %1 мин. %2 с. %3 мс").arg(minutes).arg(seconds).arg(milliseconds);

    QLabel *label = new QLabel(timeText);
    label->setStyleSheet("color: white;");

    QPushButton *okButton = new QPushButton("OK");
    okButton->setStyleSheet(R"(
        QPushButton {
            background-color: #333;
            color: white;
            border: 1px solid #555;
        }
        QPushButton:hover {
            background-color: red;
            color: white;
        }
    )");

    QVBoxLayout *vLayout = new QVBoxLayout;
    vLayout->addWidget(label);
    vLayout->addWidget(okButton);
    vLayout->setContentsMargins(20, 20, 20, 20);

    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->addStretch();
    hLayout->addLayout(vLayout);
    hLayout->addStretch();

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addStretch();
    mainLayout->addLayout(hLayout);
    mainLayout->addStretch();

    dialog->setLayout(mainLayout);

    connect(okButton, &QPushButton::clicked, dialog, &QDialog::accept);

    dialog->setLayout(mainLayout);

    dialog->exec();
}

void MainWindow::on_pushButton_clicked() {


    QString filePath = QFileDialog::getOpenFileName(this, "Выбрать файл базы данных", "", "JSON-файлы (*.json)");
    if (!filePath.isEmpty()) {
        if (!gameGen.clearDatabase()) {
            QMessageBox::critical(this, "Ошибка", "Не удалось очистить базу данных!");
            return;
        }
        QProgressDialog* progressDialog = new QProgressDialog("Импорт базы данных...", "Отмена", 0, 100, this);
        progressDialog->setStyleSheet("background-color: #2f2f2f; color: white;");
        progressDialog->setWindowModality(Qt::WindowModal);
        progressDialog->setAutoClose(true);
        progressDialog->setAutoReset(true);
        progressDialog->setValue(0);
        progressDialog->show();

        // Создаем поток для импорта базы данных
        ImportDatabaseThread* thread = new ImportDatabaseThread(&dbManager, filePath, progressDialog, this);

        // Подключаем сигналы
        connect(thread, &ImportDatabaseThread::progressUpdate, progressDialog, &QProgressDialog::setValue);
        connect(thread, &ImportDatabaseThread::finished, this, [=]() {
            progressDialog->setValue(100); // Установить прогресс в 100%
            progressDialog->close();
            delete progressDialog; // Удалить прогресс-бар
            thread->deleteLater(); // Удалить поток

            if (thread->success()) {
                QMessageBox::information(this, "База данных загружена", "База данных была успешно загружена из JSON.");
                int currentIndex = ui->comboBox->currentIndex();
                ui->comboBox->setCurrentIndex(1);
                ui->comboBox->setCurrentIndex(0);
                ui->comboBox->setCurrentIndex(currentIndex);
            } else {
                QMessageBox::critical(this, "База данных не загружена", "Ошибка при загрузке БД.");
                int currentIndex = ui->comboBox->currentIndex();
                ui->comboBox->setCurrentIndex(1);
                ui->comboBox->setCurrentIndex(0);
                ui->comboBox->setCurrentIndex(currentIndex);
            }
        });

        thread->start(); // Запустить поток
    }
}



void MainWindow::on_comboBox_currentIndexChanged(int index) {
    QStandardItemModel* model = new QStandardItemModel(this);

    if (index == 0) { // Список игр
        QVector<QVector<QString>> gamesData = dbManager.getGamesTable();

        model->setHorizontalHeaderLabels(QStringList() << "ID игры" << "Дата игры" << "Счёт команды 1" << "Счёт команды 2" << "Победившая команда");

        for (const auto& row : gamesData) {
            QList<QStandardItem*> items;
            for (const QString& value : row) {
                items.append(new QStandardItem(value));
            }
            model->appendRow(items);
        }
    } else if (index == 1) { // Игроки
        QVector<QVector<QString>> playersData = dbManager.getPlayersWithRatings();

        model->setHorizontalHeaderLabels(QStringList() << "Никнейм" << "Рейтинг" << "RD" << "Общее количество игр");

        for (const auto& row : playersData) {
            QList<QStandardItem*> items;
            for (const QString& value : row) {
                items.append(new QStandardItem(value));
            }
            model->appendRow(items);
        }
    }
    ui->tableView->setModel(model);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void MainWindow::onRowDoubleClicked(const QModelIndex &index) {
    int currentIndex = ui->comboBox->currentIndex();

    if (currentIndex == 0) { // Список игр
        int gameId = index.sibling(index.row(), 0).data().toInt(); // ID игры в первом столбце
        GameInfoWindow* gameInfo = new GameInfoWindow(gameId, &dbManager, this); // Pass the dbManager
        gameInfo->show();
    } else if (currentIndex == 1) { // Игроки
        int playerId = dbManager.getPlayerId(index.sibling(index.row(), 0).data().toString()); // ID игрока по никнейму
        PlayerInfoWindow* playerInfo = new PlayerInfoWindow(playerId, &dbManager, this);
        playerInfo->show();
    }
}
