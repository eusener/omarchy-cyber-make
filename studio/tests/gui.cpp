#include "../src/backend.h"
#include "../src/desktoptheme.h"

#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

class StudioGuiTest : public QObject {
    Q_OBJECT

private slots:
    void editPreviewAndNavigate();
};

static QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems())
        if (auto *item = findItem(child, name))
            return item;
    return nullptr;
}

void StudioGuiTest::editPreviewAndNavigate()
{
    const Desktop desktop;
    const auto languagePath = desktop.home + "/.config/omarchy/language";
    QVERIFY(QDir().mkpath(QFileInfo(languagePath).absolutePath()));
    writeFile(languagePath, "pt-BR\n");

    StudioTranslator translator(desktop);
    QCoreApplication::installTranslator(&translator);
    DesktopTheme theme(desktop, translator);
    Backend backend(qEnvironmentVariable("CYBER_MAKE_STUDIO_TEST_THEME"));
    QVERIFY2(backend.ready(), qPrintable(backend.status()));
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("studio", &backend);
    engine.rootContext()->setContextProperty("desktopTheme", &theme);
    connect(&theme, &DesktopTheme::languageChanged, &engine, &QQmlApplicationEngine::retranslate);
    engine.load(QUrl("qrc:/Main.qml"));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    const auto screenshot = qEnvironmentVariable("CYBER_MAKE_STUDIO_SCREENSHOT");
    if (!screenshot.isEmpty()) {
        QTest::qWait(450);
        const auto image = window->grabWindow();
        QVERIFY(!image.isNull());
        QVERIFY(image.save(screenshot));
    }

    const auto item = [window](const QString &name) { return findItem(window->contentItem(), name); };
    QTRY_VERIFY(item("editor_palette.accent"));
    auto *accent = item("editor_palette.accent");
    auto *apply = item("applyTheme");
    QVERIFY(apply);
    QCOMPARE(apply->property("text").toString(), "Aplicar tema");

    auto edit = [&](QQuickItem *item, const QString &text) {
        item->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        QGuiApplication::clipboard()->setText(text);
        QTest::keyClick(window, Qt::Key_V, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_Return);
    };
    edit(accent, "inválido");
    QTRY_VERIFY(!apply->isEnabled());
    QCOMPARE(window->property("invalidCount").toInt(), 1);
    edit(accent, "#27dfb2");
    QTRY_VERIFY(apply->isEnabled());
    QCOMPARE(backend.value("palette.accent").toString(), "#27dfb2");
    QVERIFY(backend.dirty());
    backend.undo();
    QCOMPARE(backend.value("palette.accent").toString(), "#a4a7ff");
    backend.redo();
    QCOMPARE(accent->property("text").toString(), "#27dfb2");

    auto *windows = item("navigate_windows");
    QVERIFY(windows);
    const auto nav = windows->mapToScene(QPointF(windows->width() / 2, windows->height() / 2));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, nav.toPoint());
    QTRY_COMPARE(window->property("category").toString(), "windows");
    auto *rounding = item("editor_windows.decoration.rounding");
    QVERIFY(rounding);
    edit(rounding, "18");
    QCOMPARE(backend.value("windows.decoration.rounding").toInt(), 18);

    window->resize(730, 570);
    QTRY_VERIFY(!window->property("wide").toBool());
    QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);
    auto *search = item("search");
    QVERIFY(search);
    QTRY_VERIFY(search->hasActiveFocus());
    QGuiApplication::clipboard()->setText("desfoque");
    QTest::keyClick(window, Qt::Key_V, Qt::ControlModifier);
    QTRY_VERIFY(!item("editor_windows.decoration.rounding"));
    QVERIFY(item("editor_windows.decoration.blur.size"));

    writeFile(languagePath, "en\n");
    QTRY_COMPARE(apply->property("text").toString(), "Apply theme");
    QCoreApplication::removeTranslator(&translator);
}

int main(int argc, char **argv)
{
    QTemporaryDir home;
    if (!home.isValid())
        return 1;
    qputenv("HOME", home.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", home.filePath(".config").toUtf8());
    qputenv("XDG_STATE_HOME", home.filePath(".local/state").toUtf8());
    qputenv("XDG_RUNTIME_DIR", home.path().toUtf8());
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QT_FORCE_STDERR_LOGGING", "1");
    qunsetenv("QT_QPA_PLATFORMTHEME");
    qunsetenv("HYPRLAND_INSTANCE_SIGNATURE");
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Material");
    StudioGuiTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "gui.moc"
