#include "backend.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QtConcurrentRun>

#include <algorithm>

Backend::Backend(QString source) : m_source(std::move(source))
{
    reload();
    if (QFileInfo(m_desktop.state).isDir())
        m_watcher.addPath(m_desktop.state);
    if (QFileInfo(personalWallpaperFolder()).isDir())
        m_watcher.addPath(personalWallpaperFolder());
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path) {
        if (m_busy)
            return;
        if (path == personalWallpaperFolder()) {
            refreshWallpapers(m_catalog, m_source);
            emit catalogChanged();
            emit changed();
            return;
        }
        try {
            const bool active = m_desktop.activeTheme() == "cyber-make-personal";
            if (active != m_personalActive) {
                m_personalActive = active;
                emit changed();
            }
        } catch (const ProfileError &) {
        }
    });
    connect(&m_apply, &QFutureWatcher<QString>::finished, this, [this] {
        m_busy = false;
        m_status = m_apply.result();
        m_failed = !m_status.isEmpty();
        if (!m_failed) {
            try {
                m_revision = m_desktop.revision();
                m_personalActive = m_desktop.activeTheme() == "cyber-make-personal";
                m_saved = m_draft;
                m_status = message("Personal theme applied.");
            } catch (const ProfileError &error) {
                m_status = error.message;
                m_failed = true;
            }
        }
        emit changed();
    });
}

const Field *Backend::field(const QString &id) const
{
    const auto it = std::find_if(m_catalog.fields.cbegin(), m_catalog.fields.cend(),
                                 [&](const Field &entry) { return entry.id == id; });
    return it == m_catalog.fields.cend() ? nullptr : &*it;
}

QVariantList Backend::fields(const QString &category) const
{
    QVariantList result;
    for (const auto &entry : m_catalog.fields) {
        if (!entry.id.startsWith(category + "."))
            continue;
        const auto parts = entry.label.split(" · ");
        const auto label = parts.size() == 2 ? parts.first() + " · " + message(parts.last().toUtf8().constData())
                                             : message(entry.label.toUtf8().constData());
        result.append(QVariantMap{
            {"id", entry.id}, {"label", label},
            {"section", message(entry.section.toUtf8().constData())}, {"kind", static_cast<int>(entry.kind)},
            {"min", entry.minimum}, {"max", entry.maximum}, {"choices", entry.choices},
        });
    }
    return result;
}

QVariant Backend::value(const QString &id) const
{
    const auto *entry = field(id);
    return m_draft.value(id, entry ? entry->fallback : QVariant{});
}

bool Backend::overridden(const QString &id) const
{
    return m_draft.contains(id);
}

QUrl Backend::wallpaper() const
{
    const QString selected = value("identity.wallpaper").toString();
    const QFileInfo file(m_catalog.wallpapers.value(selected));
    static const QStringList images{"jpg", "jpeg", "png", "webp", "gif", "bmp"};
    if (file.isFile() && images.contains(file.suffix().toLower()))
        return QUrl::fromLocalFile(file.absoluteFilePath());
    return m_catalog.still.isEmpty() ? QUrl{} : QUrl::fromLocalFile(m_catalog.still);
}

QString Backend::setValue(const QString &id, const QVariant &value)
{
    if (!m_ready || m_busy)
        return message("The settings are unavailable.");
    const auto *entry = field(id);
    if (!entry)
        return message("Unknown setting: %1").arg(id);
    try {
        auto draft = m_draft;
        draft.insert(id, normalize(*entry, value));
        edit(draft);
        return {};
    } catch (const ProfileError &error) {
        return error.message;
    }
}

void Backend::edit(const QVariantMap &draft)
{
    if (!m_ready || m_busy || draft == m_draft)
        return;
    m_undo.append(m_draft);
    if (m_undo.size() > 100)
        m_undo.removeFirst();
    m_redo.clear();
    m_draft = draft;
    m_status.clear();
    m_failed = false;
    emit changed();
}

void Backend::reset(const QString &id)
{
    auto draft = m_draft;
    draft.remove(id);
    edit(draft);
}

void Backend::defaults()
{
    edit({});
}

void Backend::undo()
{
    if (!m_busy && canUndo()) {
        m_redo.append(m_draft);
        m_draft = m_undo.takeLast();
        m_status.clear();
        m_failed = false;
        emit changed();
    }
}

void Backend::redo()
{
    if (!m_busy && canRedo()) {
        m_undo.append(m_draft);
        m_draft = m_redo.takeLast();
        m_status.clear();
        m_failed = false;
        emit changed();
    }
}

void Backend::reload()
{
    if (m_busy)
        return;
    try {
        auto catalog = readCatalog(m_source);
        const auto revision = m_desktop.revision();
        const auto saved = revision.isEmpty() ? QVariantMap{} : readProfile(revision + "/.studio.json");
        QVariantMap profile;
        QStringList unavailable;
        for (auto it = saved.cbegin(); it != saved.cend(); ++it) {
            try {
                profile.insert(validateProfile(catalog, {{it.key(), it.value()}}));
            } catch (const ProfileError &) {
                unavailable.append(it.key());
            }
        }
        const bool active = m_desktop.activeTheme() == "cyber-make-personal";
        m_catalog = std::move(catalog);
        m_revision = revision;
        m_saved = unavailable.isEmpty() ? profile : saved;
        m_draft = profile;
        m_undo.clear();
        m_redo.clear();
        m_ready = true;
        m_failed = !unavailable.isEmpty();
        m_status = unavailable.isEmpty() ? QString{} :
            message("These saved settings are unavailable and will be cleared when applied: %1").arg(unavailable.join(", "));
        m_personalActive = active;
    } catch (const ProfileError &error) {
        m_failed = true;
        m_status = error.message;
    }
    emit catalogChanged();
    emit changed();
}

void Backend::openWallpaperFolder()
{
    const auto path = personalWallpaperFolder();
    if (!QDir().mkpath(path) || !QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        m_status = message("Could not open the wallpaper folder: %1").arg(path);
        m_failed = true;
        emit changed();
        return;
    }
    if (!m_watcher.directories().contains(path))
        m_watcher.addPath(path);
    refreshWallpapers(m_catalog, m_source);
    emit catalogChanged();
    emit changed();
}

void Backend::apply()
{
    if (!m_ready || m_busy)
        return;
    m_busy = true;
    m_status = message("Applying the personal theme…");
    m_failed = false;
    emit changed();
    m_apply.setFuture(QtConcurrent::run([desktop = m_desktop, source = m_source, catalog = m_catalog,
                                       profile = m_draft, revision = m_revision] {
        try {
            desktop.apply(source, catalog, profile, revision);
            return QString{};
        } catch (const ProfileError &error) {
            return error.message;
        }
    }));
}
