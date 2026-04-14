#include <QApplication>
#include "mainwindow.h"
#include "databasemanager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Инициализация базы данных
    if (DatabaseManager::instance().connectToDatabase()) {
        DatabaseManager::instance().populateInitialData(); 
        
        // Импорт рас и других данных из JSON, если файлы существуют
        DatabaseManager::instance().importRacesFromJson("races_dndsu.json");
        DatabaseManager::instance().importClassesFromJson("classes_dndsu.json");
    }

    MainWindow w;
    w.show();

    return app.exec();
}
