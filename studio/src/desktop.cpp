#include "desktop.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QTemporaryDir>
#include <QUuid>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>

static const QString slug = "cyber-make-personal";
static const QByteArray loader = R"(
-- Cyber Make Studio: apply the personal theme after Omarchy's appearance toggles.
do
  local current = os.getenv("HOME") .. "/.local/state/omarchy/current/"
  local theme = io.open(current .. "theme.name", "r")
  if theme then
    local name = theme:read("*l")
    theme:close()
    if name == "cyber-make-personal" then
      local path = current .. "theme/cyber-make-appearance.lua"
      local appearance = io.open(path, "r")
      if appearance then
        appearance:close()
        dofile(path)
      end
    end
  end
end
-- End Cyber Make Studio.
)";

Desktop::Desktop()
    : home(QDir::homePath()),
      variant(home + "/.config/omarchy/themes/" + slug),
      revisions(home + "/.local/state/cyber-make-studio/revisions"),
      state(home + "/.local/state/omarchy/current"),
      hyprlandConfig(qEnvironmentVariable("XDG_CONFIG_HOME", home + "/.config") + "/hypr/hyprland.lua")
{
}

QString Desktop::activeTheme() const
{
    const auto file = state + "/theme.name";
    return QFileInfo::exists(file) ? QString::fromUtf8(readFile(file)).trimmed() : QString{};
}

QString Desktop::revision() const
{
    const QFileInfo link(variant);
    if (!link.exists() && !link.isSymLink())
        return {};
    const auto target = link.symLinkTarget();
    const QFileInfo folder(target);
    if (!link.isSymLink() || !folder.isDir() || folder.isSymLink() ||
        folder.absolutePath() != revisions || !folder.fileName().startsWith("revision-") ||
        !QFileInfo::exists(target + "/.studio.json"))
        throw ProfileError(message("Another theme already uses %1. Move it before applying.").arg(variant));
    return target;
}

QString Desktop::defaultSource() const
{
    const auto installed = home + "/.config/omarchy/themes/cyber-make";
    if (QFileInfo::exists(installed + "/colors.toml"))
        return installed;
    return "/usr/share/cyber-make-studio/theme";
}

static void pointThemeAt(const QString &link, const QString &revision)
{
    if (revision.isEmpty()) {
        if (!QFile::remove(link))
            throw ProfileError(message("Could not remove %1.").arg(link));
        return;
    }
    const auto temp = QFile::encodeName(link + "." + QUuid::createUuid().toString(QUuid::Id128));
    if (::symlink(QFile::encodeName(revision).constData(), temp.constData()) != 0)
        throw ProfileError(message("Could not link %1: %2").arg(link, QString::fromLocal8Bit(strerror(errno))));
    if (::rename(temp.constData(), QFile::encodeName(link).constData()) != 0) {
        const auto error = QString::fromLocal8Bit(strerror(errno));
        ::unlink(temp.constData());
        throw ProfileError(message("Could not link %1: %2").arg(link, error));
    }
}

static void checkConfigErrors(const QString &before)
{
    const auto errors = run("hyprctl", {"configerrors"});
    QStringList added;
    const auto original = before.split('\n', Qt::SkipEmptyParts);
    for (const auto &error : errors.split('\n', Qt::SkipEmptyParts))
        if (!original.contains(error))
            added.append(error);
    if (!added.isEmpty())
        throw ProfileError(message("Hyprland reported: %1").arg(added.join('\n')));
}

static void setTheme(const QString &name)
{
    // Reapplying a theme through Omarchy normally cycles its wallpaper.
    // Studio applies appearance first and chooses the wallpaper explicitly.
    run("env", {"OMARCHY_THEME_SKIP_BACKGROUND=1", "omarchy", "theme", "set", name}, 40000);
}

static void setWallpaper(const QString &state, const QString &path)
{
    if (path.isEmpty())
        return;
    if (QFileInfo(state + "/background").symLinkTarget() == path) {
        // A new revision can replace an image without changing its public path.
        // The shell caches that path, so invalidate the decoded image as well.
        run("omarchy-shell", {"-q", "background", "setInstant", path});
    } else {
        run("omarchy", {"theme", "bg", "set", path});
    }
}

void exportTheme(const QString &source, const QString &destination, const Catalog &catalog,
                 const QVariantMap &profile)
{
    const QFileInfo output(QDir::cleanPath(destination));
    if (output.exists() || output.isSymLink())
        throw ProfileError(message("Export path already exists: %1").arg(destination));
    QTemporaryDir pending(output.absolutePath() + "/.cyber-make-export-XXXXXX");
    if (!pending.isValid())
        throw ProfileError(message("Could not create a temporary directory."));
    buildTheme(source, pending.path(), catalog, profile);
    if (!QDir().rename(pending.path(), output.absoluteFilePath()))
        throw ProfileError(message("Could not create %1.").arg(destination));
}

void Desktop::apply(const QString &source, const Catalog &catalog, const QVariantMap &profile,
                    const QString &expectedRevision) const
{
    if (!QDir().mkpath(revisions) || !QDir().mkpath(QFileInfo(variant).absolutePath()))
        throw ProfileError(message("Could not create the personal theme directory."));
    QLockFile lock(revisions + "/apply.lock");
    lock.setStaleLockTime(0);
    if (!lock.tryLock())
        throw ProfileError(message("Cyber Make Studio is applying a theme in another window."));
    const QString previous = revision();
    if (previous != expectedRevision)
        throw ProfileError(message("The saved theme changed in another window. Reload it and try again."));

    const auto originalTheme = activeTheme();
    if (originalTheme.isEmpty())
        throw ProfileError(message("Omarchy did not report an active theme."));
    const auto originalErrors = run("hyprctl", {"configerrors"});
    const auto originalWallpaper = QFileInfo(state + "/background").symLinkTarget();

    QTemporaryDir pending(revisions + "/revision-XXXXXX");
    if (!pending.isValid())
        throw ProfileError(message("Could not create a temporary directory."));
    buildTheme(source, pending.path(), catalog, profile);

    const auto currentBackgrounds = state + "/theme/backgrounds/";
    const auto available = [&](const QString &path) {
        if (path.startsWith(currentBackgrounds))
            return QFileInfo::exists(pending.filePath("backgrounds/" + QFileInfo(path).fileName()));
        return QFileInfo(path).isFile();
    };
    auto wallpaper = profile.value("identity.wallpaper").toString();
    if (!wallpaper.isEmpty()) {
        wallpaper.prepend(currentBackgrounds);
    } else {
        if ((originalTheme == slug || originalTheme == "cyber-make") && available(originalWallpaper))
            wallpaper = originalWallpaper;
        const auto memory = QFileInfo(state).absolutePath() + "/theme-backgrounds/" + slug;
        if (wallpaper.isEmpty() && QFileInfo::exists(memory)) {
            const auto remembered = QString::fromUtf8(readFile(memory)).trimmed();
            if (available(remembered))
                wallpaper = remembered;
        }
        if (wallpaper.isEmpty() && !catalog.wallpapers.isEmpty()) {
            const auto first = catalog.wallpapers.cbegin();
            wallpaper = first.value().startsWith(personalWallpaperFolder() + "/")
                            ? first.value() : currentBackgrounds + first.key();
        }
    }

    QByteArray originalConfig;
    QByteArray amendedConfig;
    if (hasAppearance(profile)) {
        const auto config = QFileInfo(hyprlandConfig);
        if (!config.isFile() || config.isSymLink())
            throw ProfileError(message("Expected a regular Hyprland Lua config at %1.").arg(hyprlandConfig));
        originalConfig = readFile(hyprlandConfig);
        if (!originalConfig.contains(loader)) {
            if (originalConfig.contains("-- Cyber Make Studio:"))
                throw ProfileError(message("The Cyber Make Studio loader in %1 was edited.").arg(hyprlandConfig));
            amendedConfig = originalConfig + '\n' + loader;
        }
    }

    bool published = false;
    bool configChanged = false;
    try {
        if (!amendedConfig.isEmpty()) {
            const auto backup = hyprlandConfig + ".cyber-make-" + QUuid::createUuid().toString(QUuid::Id128) + ".bak";
            if (!QFile::copy(hyprlandConfig, backup) || readFile(hyprlandConfig) != originalConfig)
                throw ProfileError(message("Could not back up %1 or it changed while editing.").arg(hyprlandConfig));
            writeFile(hyprlandConfig, amendedConfig);
            configChanged = true;
        }
        pointThemeAt(variant, pending.path());
        published = true;
        setTheme(slug);
        if (activeTheme() != slug)
            throw ProfileError(message("Omarchy did not activate the personal theme."));
        setWallpaper(state, wallpaper);
        run("hyprctl", {"reload"});
        checkConfigErrors(originalErrors);
    } catch (const ProfileError &failure) {
        try {
            if (published) {
                if (revision() != pending.path())
                    throw ProfileError(message("The personal theme changed during application."));
                pointThemeAt(variant, previous);
            }
            if (configChanged) {
                if (readFile(hyprlandConfig) != amendedConfig)
                    throw ProfileError(message("The Hyprland configuration changed during application."));
                writeFile(hyprlandConfig, originalConfig);
            }
            if (published && activeTheme() == slug) {
                setTheme(originalTheme);
                if (!originalWallpaper.isEmpty()) {
                    setWallpaper(state, originalWallpaper);
                } else if (QFileInfo(state + "/background").isSymLink() &&
                           !QFile::remove(state + "/background")) {
                    throw ProfileError(message("Could not remove %1.").arg(state + "/background"));
                }
            }
            if (published || configChanged) {
                run("hyprctl", {"reload"});
                checkConfigErrors(originalErrors);
            }
        } catch (const ProfileError &rollback) {
            // If the active link still points here, keep its files available.
            if (QFileInfo(variant).symLinkTarget() == pending.path())
                pending.setAutoRemove(false);
            throw ProfileError(message("%1\nRecovery also failed: %2").arg(failure.message, rollback.message));
        }
        throw failure;
    }

    pending.setAutoRemove(false);
    if (!previous.isEmpty())
        QDir(previous).removeRecursively();
}
