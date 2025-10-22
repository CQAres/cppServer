#include "mainwindow.h"

#include <QApplication>


int main(int argc, char *argv[]) {

    // 初始化Qt application
    QApplication app(argc, argv);
    // 创建客户端窗口
    MainWindow window;
    window.show();
    return app.exec();
}
