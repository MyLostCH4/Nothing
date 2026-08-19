#include "mainwindow.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QFont>
#include <QIcon>
#include <QTimer>

#include <memory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app_icon.png")));
    app.setApplicationName(QStringLiteral("Nothing"));
    app.setOrganizationName(QStringLiteral("MyQt"));
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));

    // Keep popup controls readable even when Windows itself uses a dark theme.
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette lightPalette;
    lightPalette.setColor(QPalette::Window, QColor(QStringLiteral("#ffffff")));
    lightPalette.setColor(QPalette::WindowText, QColor(QStringLiteral("#080808")));
    lightPalette.setColor(QPalette::Base, QColor(QStringLiteral("#ffffff")));
    lightPalette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#f5f6f8")));
    lightPalette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#ffffff")));
    lightPalette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#080808")));
    lightPalette.setColor(QPalette::Text, QColor(QStringLiteral("#080808")));
    lightPalette.setColor(QPalette::Button, QColor(QStringLiteral("#ffffff")));
    lightPalette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#080808")));
    lightPalette.setColor(QPalette::Highlight, QColor(QStringLiteral("#dce8fa")));
    lightPalette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#080808")));
    lightPalette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#555b63")));
    app.setPalette(lightPalette);

    std::unique_ptr<QMainWindow> window(createMainWindow());
    window->show();

    if (app.arguments().contains(QStringLiteral("--smoke-test")))
        QTimer::singleShot(300, &app, &QCoreApplication::quit);

    return app.exec();
}
