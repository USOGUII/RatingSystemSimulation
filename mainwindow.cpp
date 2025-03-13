#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "QMessageBox"
#include <QProgressDialog>
#include "gamegeneratorthread.h" // Include the header file for your thread
#include "gameinfowindow.h"
#include "importdatabasethread.h"
#include "playerinfowindow.h"
#include "ratingdistributionanalyzer.h"

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
    connect(ui->analyzeButton, &QPushButton::clicked, this, &MainWindow::onAnalyzeRatingDistributionClicked);
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

    ui->analysisTextEdit->clear();
    ui->meanRatingLabel->setText("Средний рейтинг: ");
    ui->medianRatingLabel->setText("Медиана: ");
    ui->stdDevLabel->setText("Стандартное отклонение: ");

    if (!gameGen.clearDatabase()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось очистить базу данных!");
        return;
    }

    // Генерация игроков разных уровней навыка
    if (!gameGen.generatePlayersBySkill(
            ui->playerCountBox->value() / 4,  // низкий уровень
            ui->playerCountBox->value() / 4,  // средний уровень
            ui->playerCountBox->value() / 4,  // выше среднего
            ui->playerCountBox->value() / 4)) // высокий уровень
    {
        QMessageBox::critical(this, "Ошибка", "Не удалось сгенерировать игроков!");
        return;
    }

    int gameCount = ui->gameCountBox->value();

    // Отключаем кнопку на время генерации
    ui->pushButton_2->setEnabled(false);

    // Создаем прогресс-диалог
    QProgressDialog* progressDialog = new QProgressDialog("Идет генерация игр...", "Отмена", 0, gameCount, this);
    progressDialog->setStyleSheet("background-color: #2f2f2f; color: white;");
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setAutoClose(true);
    progressDialog->setAutoReset(true);
    progressDialog->setValue(0);
    progressDialog->setFixedSize(progressDialog->size());
    progressDialog->show();

    // Создаем и запускаем поток
    GameGeneratorThread* thread = new GameGeneratorThread(&dbManager, gameCount, startDate, endDate, playersInTeam, this);

    // Подключаем сигналы
    connect(thread, &GameGeneratorThread::timeElapsed, this, &MainWindow::showTimeElapsed);

    // При обновлении прогресса обновляем прогресс-диалог
    connect(thread, &GameGeneratorThread::progressUpdate, progressDialog, &QProgressDialog::setValue);

    // Когда поток завершится, выполняем очистку
    connect(thread, &GameGeneratorThread::finished, this, [=]() {
        progressDialog->setValue(gameCount); // Показываем 100% завершение
        progressDialog->close();
        delete progressDialog;  // Важно: освобождаем память диалога
        thread->deleteLater();  // Важно: удаляем поток после его завершения
        ui->pushButton_2->setEnabled(true); // Включаем кнопку

        // Обновляем интерфейс после генерации
        int currentIndex = ui->comboBox->currentIndex();
        ui->comboBox->setCurrentIndex(currentIndex == 0 ? 1 : 0);
        ui->comboBox->setCurrentIndex(currentIndex);
    });

    // Если пользователь отменяет операцию, останавливаем поток
    connect(progressDialog, &QProgressDialog::canceled, thread, [=]() {
        thread->terminate(); // Можно использовать quit() и wait() для более безопасной остановки
        thread->deleteLater();
        ui->pushButton_2->setEnabled(true);
    });

    thread->start();  // Запускаем поток
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
        ui->analysisTextEdit->clear();
        ui->meanRatingLabel->setText("Средний рейтинг: ");
        ui->medianRatingLabel->setText("Медиана: ");
        ui->stdDevLabel->setText("Стандартное отклонение: ");

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

        model->setHorizontalHeaderLabels(QStringList() << "Никнейм" << "Рейтинг" << "RD" << "Общее количество игр" << "Уровень игры" << "Количество побед" << "Winrate");

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

void MainWindow::onAnalyzeRatingDistributionClicked() {
    // Получаем данные через метод DatabaseManager
    QMap<int, int> ratingData = dbManager.getRatingData();

    if (ratingData.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Нет данных для анализа");
        return;
    }

    // Создаем и настраиваем анализатор
    RatingDistributionAnalyzer analyzer;
    for (auto it = ratingData.constBegin(); it != ratingData.constEnd(); ++it) {
        analyzer.addData(it.key(), it.value());
    }

    // Получаем статистику
    QMap<QString, double> stats = analyzer.calculateStatistics();

    // Обновляем UI с статистикой
    ui->meanRatingLabel->setText("Средний рейтинг: " + QString::number(stats["Средний рейтинг"], 'f', 1));
    ui->medianRatingLabel->setText("Медиана: " + QString::number(stats["Медиана"], 'f', 1));
    ui->stdDevLabel->setText("Стандартное отклонение: " + QString::number(stats["Стандартное отклонение"], 'f', 1));

    // Создаем и отображаем график
    QChartView* chartView = analyzer.createDistributionChart();
    if (chartView) {
        QMainWindow* chartWindow = new QMainWindow(this);
        chartWindow->setCentralWidget(chartView);
        chartWindow->resize(800, 600);
        chartWindow->show();
    }

    // Анализ справедливости
    QString analysis = analyzer.analyzeDistributionFairness();
    ui->analysisTextEdit->setPlainText(analysis);
}
