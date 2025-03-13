#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "databasemanager.h"
#include "gamegenerator.h"
#include "QFileDialog"
#include "QStandardItemModel"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButton_2_clicked();
    void showTimeElapsed(int msec);

    void on_pushButton_clicked();

    void on_comboBox_currentIndexChanged(int index);

    void onRowDoubleClicked(const QModelIndex &index);

    void onAnalyzeRatingDistributionClicked();

private:
    Ui::MainWindow *ui;
    DatabaseManager dbManager;
    GameGenerator gameGen;

};
#endif // MAINWINDOW_H
