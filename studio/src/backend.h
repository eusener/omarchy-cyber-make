#pragma once

#include "desktop.h"

#include <QFutureWatcher>
#include <QFileSystemWatcher>
#include <QObject>
#include <QUrl>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool personalActive READ personalActive NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool failed READ failed NOTIFY changed)
    Q_PROPERTY(QUrl wallpaper READ wallpaper NOTIFY changed)

public:
    explicit Backend(QString source);
    bool ready() const { return m_ready; }
    bool busy() const { return m_busy; }
    bool dirty() const { return m_draft != m_saved; }
    bool personalActive() const { return m_personalActive; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QString status() const { return m_status; }
    bool failed() const { return m_failed; }
    QUrl wallpaper() const;

    Q_INVOKABLE QVariantList fields(const QString &category) const;
    Q_INVOKABLE QVariant value(const QString &id) const;
    Q_INVOKABLE bool overridden(const QString &id) const;
    Q_INVOKABLE QString setValue(const QString &id, const QVariant &value);
    Q_INVOKABLE void reset(const QString &id);
    Q_INVOKABLE void defaults();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void reload();
    Q_INVOKABLE void apply();
    Q_INVOKABLE void openWallpaperFolder();

signals:
    void changed();
    void catalogChanged();

private:
    const Field *field(const QString &id) const;
    void edit(const QVariantMap &draft);

    Desktop m_desktop;
    QString m_source;
    QString m_revision;
    Catalog m_catalog;
    QVariantMap m_draft;
    QVariantMap m_saved;
    QList<QVariantMap> m_undo;
    QList<QVariantMap> m_redo;
    QFutureWatcher<QString> m_apply;
    QFileSystemWatcher m_watcher;
    QString m_status;
    bool m_ready = false;
    bool m_busy = false;
    bool m_failed = false;
    bool m_personalActive = false;
};
