#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), socket(new QTcpSocket(this)) {
    // 创建控件
    inputLineEdit = new QLineEdit;
    sendButton = new QPushButton("Send");
    displayTextEdit = new QTextEdit;
    displayTextEdit->setReadOnly(true);

    // 设置布局
    QWidget *centralWidget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    layout->addWidget(displayTextEdit);

    QHBoxLayout *inputLayout = new QHBoxLayout;
    inputLayout->addWidget(inputLineEdit);
    inputLayout->addWidget(sendButton);
    layout->addLayout(inputLayout);

    setCentralWidget(centralWidget);

    // 连接信号和槽
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(inputLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendClicked);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &MainWindow::onSocketError);

    // 连接到服务器
    socket->connectToHost("127.0.0.1", 1234);
    if (!socket->waitForConnected(5000)) {
        QMessageBox::critical(this, "Error", "Failed to connect to server");
        close();
    }
}

MainWindow::~MainWindow() {
    socket->disconnectFromHost();
}

void MainWindow::onSendClicked() {
    QString text = inputLineEdit->text();
    if (text.isEmpty())
        return;

    QByteArray data = text.toUtf8();
    if (socket->write(data) == -1) {
        QMessageBox::warning(this, "Error", "Failed to send data");
        return;
    }
    expectedReadSize = data.size();
    inputLineEdit->clear();
}

void MainWindow::onReadyRead() {
    readBuffer.append(socket->readAll());

    while (expectedReadSize > 0 && readBuffer.size() >= expectedReadSize) {
        QByteArray response = readBuffer.left(expectedReadSize);
        displayTextEdit->append("Server: " + QString::fromUtf8(response));
        readBuffer = readBuffer.mid(expectedReadSize);
        expectedReadSize = 0; //
    }
}

void MainWindow::onSocketError(QAbstractSocket::SocketError error) {
    QMessageBox::critical(this, "Error", socket->errorString());
    close();
}
