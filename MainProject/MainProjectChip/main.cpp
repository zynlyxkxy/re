#include <QApplication>
#include <QWidget>
#include <QLibrary>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Chip_UI");

#ifdef GUI_DEBUG
    QLibrary lib("libUIRmd");
#else
    QLibrary lib("libUIRm");
#endif
    if (!lib.load()) {
        qCritical("Failed to load libUIRm: %s", qPrintable(lib.errorString()));
        return -1;
    }

    using GetWidgetFunc = QWidget* (*)(const char*, const char*);
    auto UIRm_GetWidgetWithLib = (GetWidgetFunc)lib.resolve("UIRm_GetWidgetWithLib");
    if (!UIRm_GetWidgetWithLib) {
        qCritical("Failed to resolve UIRm_GetWidgetWithLib");
        return -1;
    }

#ifdef GUI_DEBUG
    auto* view = UIRm_GetWidgetWithLib("rm", "libUIRE_CHIPd");
#else
    auto* view = UIRm_GetWidgetWithLib("rm", "libUIRE_CHIP");
#endif
    view->resize(1280, 820);
    view->setMinimumSize(1024, 600);
    view->show();

    return app.exec();
}
