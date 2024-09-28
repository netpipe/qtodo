#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QDateEdit>
#include <QDateTimeEdit> // Include QDateTimeEdit
#include <QCalendarWidget>
#include <QtSql>
#include <QDebug>
#include <QDateTime>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QMessageBox>
#include <QCloseEvent>
//#include <QtMultimedia/QMediaPlayer>
#include <QSound>

class QCloseEvent;

class TaskManager {
public:
    TaskManager() {
        db = QSqlDatabase::addDatabase("QSQLITE");
#ifndef __APPLE__
        db.setDatabaseName("todo.db");
#else
        db.setDatabaseName("/Applications/qtodo.app/Contents/MacOS/todo.db");
#endif

        if (!db.open()) {
            qDebug() << "Error: unable to open database";
            // Handle error
        }

        // Create tasks table if not exists
        QSqlQuery query;
        query.exec("CREATE TABLE IF NOT EXISTS tasks (id INTEGER PRIMARY KEY AUTOINCREMENT, task TEXT, due_date TEXT, alarm_time TEXT, alerted INTEGER DEFAULT 0)");
    }

    ~TaskManager() {
        db.close();
    }

    void addTask(const QString& taskName, const QString& dueDate, const QString& alarmTime) {
        QSqlQuery query;
        query.prepare("INSERT INTO tasks (task, due_date, alarm_time) VALUES (:task, :due_date, :alarm_time)");
        query.bindValue(":task", taskName);
        query.bindValue(":due_date", dueDate);
        query.bindValue(":alarm_time", alarmTime);
        query.exec();
    }

    void updateTaskDueDate(int taskId, const QString& dueDate) {
        QSqlQuery query;
        query.prepare("UPDATE tasks SET due_date = :due_date WHERE id = :id");
        query.bindValue(":due_date", dueDate);
        query.bindValue(":id", taskId);
        query.exec();
    }

    void updateTaskName(int taskId, const QString& taskName) {
        QSqlQuery query;
        query.prepare("UPDATE tasks SET task = :task WHERE id = :id");
        query.bindValue(":task", taskName);
        query.bindValue(":id", taskId);
        query.exec();
    }

    void updateTaskAlarmTime(int taskId, const QString& alarmTime) {
        QSqlQuery query;
        query.prepare("UPDATE tasks SET alarm_time = :alarm_time WHERE id = :id");
        query.bindValue(":alarm_time", alarmTime);
        query.bindValue(":id", taskId);
        query.exec();
    }

    void deleteTask(int taskId) {
        QSqlQuery query;
        query.prepare("DELETE FROM tasks WHERE id = :id");
        query.bindValue(":id", taskId);
        query.exec();
    }

    bool isTaskAlerted(int taskId) {
        QSqlQuery query;
        query.prepare("SELECT alerted FROM tasks WHERE id = :id");
        query.bindValue(":id", taskId);
        query.exec();
        if (query.next()) {
            return query.value(0).toBool();
        }
        return false;
    }

    void setTaskAlerted(int taskId, bool alerted) {
        QSqlQuery query;
        query.prepare("UPDATE tasks SET alerted = :alerted WHERE id = :id");
        query.bindValue(":alerted", alerted);
        query.bindValue(":id", taskId);
        query.exec();
    }

    // Implement alarm functionality here if needed
private:
    QSqlDatabase db;
};


class MainWindow : public QMainWindow {
    Q_OBJECT
protected:
    void closeEvent(QCloseEvent *event) override {
        if (trayIcon && trayIcon->isVisible()) {
            hide();
            event->ignore();
        } else {
            event->accept();
        }
    }
public:



    MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("ToDo Task Manager");

        taskManager = new TaskManager();

        QWidget *centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        QVBoxLayout *mainLayout = new QVBoxLayout;
        centralWidget->setLayout(mainLayout);

        taskList = new QListWidget(this);
        mainLayout->addWidget(taskList);

        QHBoxLayout *buttonLayout = new QHBoxLayout;
        mainLayout->addLayout(buttonLayout);

        addButton = new QPushButton("Add Task", this);
        connect(addButton, &QPushButton::clicked, this, &MainWindow::addTask);
        buttonLayout->addWidget(addButton);

        updateButton = new QPushButton("Update Task", this);
        connect(updateButton, &QPushButton::clicked, this, &MainWindow::updateTask);
        buttonLayout->addWidget(updateButton);

        deleteButton = new QPushButton("Delete Task", this);
        connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deleteTask);
        buttonLayout->addWidget(deleteButton);

        taskNameEdit = new QLineEdit(this);
        taskNameEdit->setPlaceholderText("Task Name");
        mainLayout->addWidget(taskNameEdit);

        QHBoxLayout *dateLayout = new QHBoxLayout;
        mainLayout->addLayout(dateLayout);

        dueDateEdit = new QDateEdit(this);
        dueDateEdit->setCalendarPopup(true);
        dueDateEdit->setDate(QDate::currentDate());
        dateLayout->addWidget(dueDateEdit);

        // Use QDateTimeEdit instead of QTimeEdit
        dateTimeEdit = new QTimeEdit(this);
        //dateTimeEdit->setDisplayFormat("hh:mm AP");
        dateTimeEdit->setDateTime(QDateTime::currentDateTime());
        dateLayout->addWidget(dateTimeEdit);

        // Populate task list
        updateTaskList();

        createTrayIcon();

        connect(taskList, &QListWidget::itemDoubleClicked, this, &MainWindow::editTask);
        // Timer to check for due tasks
        QTimer *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &MainWindow::checkDueTasks);
        timer->start(60000); // Check every minute (adjust as needed)
    }

    ~MainWindow() {
        delete taskManager;
    }

private slots:
    void addTask() {
        QString taskName = taskNameEdit->text();
        QString dueDate = dueDateEdit->date().toString("yyyy-MM-dd");
        QString alarmTime = dateTimeEdit->dateTime().toString("hh:mm");
        if (!taskName.isEmpty()) {
            taskManager->addTask(taskName, dueDate, alarmTime);
            updateTaskList();
        }
    }

    void updateTask() {
        QString taskName = taskNameEdit->text();
        QString dueDate = dueDateEdit->date().toString("yyyy-MM-dd");
        QString alarmTime = dateTimeEdit->dateTime().toString("hh:mm");
        QListWidgetItem *selectedItem = taskList->currentItem();
        if (selectedItem) {
            int taskId = selectedItem->data(Qt::UserRole).toInt();
            taskManager->updateTaskName(taskId, taskName);
            taskManager->updateTaskDueDate(taskId, dueDate);
            taskManager->updateTaskAlarmTime(taskId, alarmTime);
            updateTaskList();
        }
    }

    void deleteTask() {
        QListWidgetItem *selectedItem = taskList->currentItem();
        if (selectedItem) {
            int taskId = selectedItem->data(Qt::UserRole).toInt();
            //QMessageBox::StandardButton confirmDelete = QMessageBox::question(this, "Confirm Delete", "Are you sure you want to delete this task?", QMessageBox::Yes | QMessageBox::No);
           // if (confirmDelete == QMessageBox::Yes) {
                taskManager->deleteTask(taskId);
                updateTask();
          //  }
        }
    }

    void editTask(QListWidgetItem *item) {
        if (item) {
            QString taskName = item->text().split(" - ").first();
            QString dueDate = item->text().split(" - ").last().split(" ").at(2);
            QString alarmTime = item->text().split(" - ").last().split(" ").at(3);

            taskNameEdit->setText(taskName);
            dueDateEdit->setDate(QDate::fromString(dueDate, "yyyy-MM-dd"));
            dateTimeEdit->setDateTime(QDateTime::fromString(dueDate + " " + alarmTime, "yyyy-MM-dd hh:mm"));
        }
    }

    void updateTaskList() {
        qApp->setQuitOnLastWindowClosed(false);
        taskList->clear();
        QSqlQuery query("SELECT id, task, due_date, alarm_time, alerted FROM tasks");
        while (query.next()) {
            int taskId = query.value(0).toInt();
            QString taskName = query.value(1).toString();
            QString dueDate = query.value(2).toString();
            QString alarmTime = query.value(3).toString();
            bool alerted = query.value(4).toBool();

            QListWidgetItem *item = new QListWidgetItem(taskName + " - Due Date: " + dueDate + " " + alarmTime);
            item->setData(Qt::UserRole, taskId);

            QDateTime dueDateTime = QDateTime::fromString(dueDate + " " + alarmTime, "yyyy-MM-dd hh:mm");
            if (dueDateTime.date() == QDate::currentDate() && alerted) {
                item->setForeground(Qt::red);
               // QMessageBox::StandardButton confirmDelete = QMessageBox::question(this, "Delete Alerted Task", "Do you want to delete the alerted task: " + taskName + "?", QMessageBox::Yes | QMessageBox::No);
               // if (confirmDelete == QMessageBox::Yes) {
               //     taskManager->deleteTask(taskId);
              //  } else {
                   // taskManager->setTaskAlerted(taskId, false);
              //  }
            }
            taskList->addItem(item);
          // }
        }
        qApp->setQuitOnLastWindowClosed(true);
    }

    void createTrayIcon() {
        trayIcon = new QSystemTrayIcon(this);
        QIcon icon = QIcon(":/qtodo.png");
        trayIcon->setIcon(icon);
        trayIcon->setVisible(true);

        connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::toggleWindow);

        // Create context menu
        QMenu *trayMenu = new QMenu(this);
        QAction *showWindowAction = new QAction("Show Window", this);
        connect(showWindowAction, &QAction::triggered, this, &MainWindow::showWindow);
        trayMenu->addAction(showWindowAction);

        QAction *quitAction = new QAction("Quit", this);
        connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
        trayMenu->addAction(quitAction);

        trayIcon->setContextMenu(trayMenu);
    }

    void toggleWindow(QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            if (isVisible()) {
                hide();
            } else {
                show();
            }
        }
    }

    void showWindow() {
        show();
    }


    void checkDueTasks() {
          QSqlQuery query("SELECT id, task, due_date, alarm_time, alerted FROM tasks");
          while (query.next()) {
              int taskId = query.value(0).toInt();
              QString taskName = query.value(1).toString();
              QString dueDate = query.value(2).toString();
              QString alarmTime = query.value(3).toString();
              bool alerted = query.value(4).toBool();

              QDateTime dueDateTime = QDateTime::fromString(dueDate + " " + alarmTime, "yyyy-MM-dd hh:mm");
              QDateTime currentDateTime = QDateTime::currentDateTime();

              if (!alerted && currentDateTime >= dueDateTime) {
                  // Task is due and not alerted
               //  QSound::play( "test.wav");
                 QSound *sound = new QSound("test.wav");
                 sound->setLoops(100);
                 sound->play();

                  QListWidgetItem *item = findTaskItem(taskId);
                  if (item) {
                      item->setForeground(Qt::green);
                      QMessageBox::StandardButton confirmAlert = QMessageBox::question(this, "Task Alert", "Task " + taskName + " is due. Do you want to mark it as alerted?", QMessageBox::Yes | QMessageBox::No);
                      if (confirmAlert == QMessageBox::Yes) {
                          item->setForeground(Qt::red);
                          taskManager->setTaskAlerted(taskId, true);
                           sound->stop();
                      }else   {sound->stop();}
                  }
              }
          }
      }

      QListWidgetItem* findTaskItem(int taskId) {
          for (int i = 0; i < taskList->count(); ++i) {
              QListWidgetItem *item = taskList->item(i);
              if (item->data(Qt::UserRole).toInt() == taskId) {
                  return item;
              }
          }
          return nullptr;
      }
private:
    TaskManager *taskManager;
    QListWidget *taskList;
    QPushButton *addButton;
    QPushButton *updateButton;
    QPushButton *deleteButton;
    QLineEdit *taskNameEdit;
    QDateEdit *dueDateEdit;
    QTimeEdit *dateTimeEdit; // Change QTimeEdit to QDateTimeEdit
    QSystemTrayIcon *trayIcon;
};

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    MainWindow w;
    w.show();

    return a.exec();
}

#include "main.moc"
