#include <QApplication>
#include <QDebug>
#include "App/CefApp.h"
#include "MainWindow.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (!CefHelper::Initialize(argc, argv)) {
        return 0;
    }

    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("X互评助手"));
    app.setOrganizationName(QStringLiteral("xreply"));
    app.setApplicationVersion(QStringLiteral("0.2.0"));

    qDebug() << "[INFO] X互评助手(xreply) 启动中...";

    MainWindow* window = new MainWindow();
    window->show();

    const int result = app.exec();
    delete window;
    CefHelper::Shutdown();
    return result;
}
