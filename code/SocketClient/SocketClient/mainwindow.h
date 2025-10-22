#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSendClicked();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    QTcpSocket *socket;
    QLineEdit *inputLineEdit;
    QTextEdit *displayTextEdit;
    QPushButton *sendButton;
    QByteArray readBuffer;
    qint64 expectedReadSize = 0;
};

#endif // MAINWINDOW_H
