#include "desktoptheme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>

StudioTranslator::StudioTranslator(const Desktop &desktop)
    : m_path(qEnvironmentVariable("XDG_CONFIG_HOME", desktop.home + "/.config") + "/omarchy/language")
{
    refresh();
}

void StudioTranslator::refresh()
{
    QFile file(m_path);
    QString language = QLocale::system().bcp47Name();
    if (file.open(QIODevice::ReadOnly))
        language = QString::fromUtf8(file.readAll()).trimmed();
    if (language == m_language)
        return;
    m_language = language;
    QVariantMap strings;
    for (const auto &code : {language, language.section('-', 0, 0)}) {
        QFile translations(":/i18n/" + code + ".json");
        if (translations.open(QIODevice::ReadOnly)) {
            strings = QJsonDocument::fromJson(translations.readAll()).object().toVariantMap();
            break;
        }
    }
    {
        QWriteLocker lock(&m_lock);
        m_strings = std::move(strings);
    }
    QLocale::setDefault(QLocale(language));
}

QString StudioTranslator::translate(const char *, const char *sourceText, const char *, int) const
{
    QReadLocker lock(&m_lock);
    return m_strings.value(QString::fromUtf8(sourceText)).toString();
}

DesktopTheme::DesktopTheme(Desktop desktop, StudioTranslator &translator)
    : m_desktop(std::move(desktop)), m_translator(translator)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &DesktopTheme::refresh);
    m_queryTimeout.setSingleShot(true);
    m_queryTimeout.setInterval(1500);
    connect(&m_queryTimeout, &QTimer::timeout, &m_hyprland, &QProcess::kill);
    connect(&m_hyprland, &QProcess::finished, this, [this] {
        m_queryTimeout.stop();
        const auto info = QJsonDocument::fromJson(m_hyprland.readAllStandardOutput()).object();
        m_hyprland.readAllStandardError();
        if (info.contains("int")) {
            m_radius = qBound(0, info.value("int").toInt(), 18);
            emit changed();
        }
    });
    refresh();
}

DesktopTheme::~DesktopTheme()
{
    if (m_hyprland.state() != QProcess::NotRunning) {
        m_hyprland.kill();
        m_hyprland.waitForFinished();
    }
}

void DesktopTheme::refresh()
{
    const QString palette = m_desktop.state + "/theme/colors.toml";
    const QString config = qEnvironmentVariable("XDG_CONFIG_HOME", m_desktop.home + "/.config");
    const QStringList paths{m_desktop.state, m_desktop.state + "/theme", palette, config + "/omarchy",
                            config + "/omarchy/language", config + "/hypr", m_desktop.hyprlandConfig};
    for (const auto &path : paths)
        if (QFileInfo::exists(path) && !m_watcher.files().contains(path) && !m_watcher.directories().contains(path))
            m_watcher.addPath(path);

    const auto oldLanguage = m_translator.language();
    m_translator.refresh();
    if (oldLanguage != m_translator.language())
        emit languageChanged();

    QFile file(palette);
    QColor accent("#a4a7ff");
    if (file.open(QIODevice::ReadOnly)) {
        static const QRegularExpression pattern(R"rx(^\s*accent\s*=\s*["'](#[a-fA-F0-9]{6})["'])rx",
                                                 QRegularExpression::MultilineOption);
        const auto match = pattern.match(QString::fromUtf8(file.readAll()));
        if (match.hasMatch())
            accent = QColor(match.captured(1));
    }
    m_accent = accent;
    if (!qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE") && m_hyprland.state() == QProcess::NotRunning) {
        m_hyprland.start("hyprctl", {"-j", "getoption", "decoration:rounding"});
        m_queryTimeout.start();
    }
    emit changed();
}
