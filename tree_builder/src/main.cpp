#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "dpt_model.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    // Check for DPT file argument
    if (argc < 2) {
        qDebug() << "Usage:" << argv[0] << "<path-to-dpt-file>";
        qDebug() << "Example:" << argv[0] << "../resources/test_graphs/passthrough.dpt";
        return 1;
    }

    QString dptFilePath = QString::fromLocal8Bit(argv[1]);

    QQmlApplicationEngine engine;

    // Register DPTModel for QML
    DPTModel dptModel;
    engine.rootContext()->setContextProperty("dptModel", &dptModel);

    // Load main QML
    const QUrl url(u"qrc:/TreeBuilder/qml/main.qml"_qs);
    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    // Load DPT file from command line argument
    qDebug() << "Loading DPT file:" << dptFilePath;
    if (!dptModel.loadDPT(dptFilePath)) {
        qDebug() << "Failed to load DPT file:" << dptFilePath;
        qDebug() << "Error:" << dptModel.errorMessage();
        return 1;
    }

    return app.exec();
}
