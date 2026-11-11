#include "backend.h"
#include "desktoptheme.h"

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTextStream>

static int runCli(const QString &command, const QString &source, const QString &profileFile,
                  const QString &output, bool json)
{
    try {
        const Desktop desktop;
        const auto catalog = readCatalog(source);
        const auto revision = desktop.revision();
        auto path = profileFile;
        if (path.isEmpty() && !revision.isEmpty())
            path = revision + "/.studio.json";
        const auto profile = validateProfile(catalog, path.isEmpty() ? QVariantMap{} : readProfile(path));
        if (command == "export") {
            if (output.isEmpty())
                throw ProfileError(message("Provide a new directory with --output."));
            exportTheme(source, output, catalog, profile);
        } else if (command == "apply") {
            desktop.apply(source, catalog, profile, revision);
        }
        QTextStream stream(stdout);
        if (json)
            stream << QJsonDocument(QJsonObject{{"ok", true}, {"settings", catalog.fields.size()},
                                                {"overrides", profile.size()}}).toJson(QJsonDocument::Compact);
        else
            stream << message("Done.") << '\n';
        return 0;
    } catch (const ProfileError &error) {
        QTextStream stream(stderr);
        if (json)
            stream << QJsonDocument(QJsonObject{{"ok", false}, {"error", error.message}}).toJson(QJsonDocument::Compact);
        else
            stream << error.message << '\n';
        return 1;
    }
}

int main(int argc, char *argv[])
{
    bool cli = false;
    for (int i = 1; i < argc; ++i) {
        const QByteArray argument(argv[i]);
        if (argument == "check" || argument == "export" || argument == "apply"
            || argument == "--help" || argument == "--version")
            cli = true;
    }
    if (cli) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        qunsetenv("QT_QPA_PLATFORMTHEME");
    }
    QGuiApplication app(argc, argv);
    app.setApplicationName("cyber-make-studio");
    app.setApplicationDisplayName("Cyber Make Studio");
    app.setApplicationVersion("1.0.0");
    app.setDesktopFileName("cyber-make-studio");
    app.setWindowIcon(QIcon::fromTheme("cyber-make-studio"));

    QCommandLineParser parser;
    parser.setApplicationDescription("Customize the Cyber Make theme for Omarchy.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("command", "open (default), check, export, or apply", "[command]");
    const QCommandLineOption sourceOption("source", "Use this directory as the original theme.", "directory");
    const QCommandLineOption profileOption("profile", "Read overrides from a JSON profile.", "file");
    const QCommandLineOption outputOption("output", "Export to a new directory.", "directory");
    const QCommandLineOption jsonOption("json", "Write a machine-readable result.");
    parser.addOptions({sourceOption, profileOption, outputOption, jsonOption});
    parser.process(app);
    const auto arguments = parser.positionalArguments();
    const auto command = arguments.value(0, "open");
    if (arguments.size() > 1 || !QStringList{"open", "check", "export", "apply"}.contains(command))
        parser.showHelp(1);

    const Desktop desktop;
    const QString source = QDir(parser.value(sourceOption).isEmpty() ? desktop.defaultSource()
                                                                    : parser.value(sourceOption)).absolutePath();
    StudioTranslator translator(desktop);
    app.installTranslator(&translator);
    if (command != "open")
        return runCli(command, source, parser.value(profileOption), parser.value(outputOption), parser.isSet(jsonOption));

    QQuickStyle::setStyle("Material");
    DesktopTheme theme(desktop, translator);
    Backend backend(source);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("studio", &backend);
    engine.rootContext()->setContextProperty("desktopTheme", &theme);
    QObject::connect(&theme, &DesktopTheme::languageChanged, &engine, &QQmlApplicationEngine::retranslate);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
