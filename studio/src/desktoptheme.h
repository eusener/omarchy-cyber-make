#pragma once

#include "desktop.h"

#include <QColor>
#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QReadWriteLock>
#include <QTimer>
#include <QTranslator>

class StudioTranslator : public QTranslator {
public:
    explicit StudioTranslator(const Desktop &desktop);
    bool isEmpty() const override { return false; }
    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation = nullptr, int n = -1) const override;
    void refresh();
    QString language() const { return m_language; }

private:
    QString m_path;
    QString m_language;
    QVariantMap m_strings;
    mutable QReadWriteLock m_lock;
};

class DesktopTheme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
    Q_PROPERTY(QString language READ language NOTIFY changed)

public:
    DesktopTheme(Desktop desktop, StudioTranslator &translator);
    ~DesktopTheme() override;
    QColor accent() const { return m_accent; }
    int radius() const { return m_radius; }
    QString language() const { return m_translator.language(); }
    void refresh();

signals:
    void changed();
    void languageChanged();

private:
    Desktop m_desktop;
    StudioTranslator &m_translator;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QProcess m_hyprland;
    QTimer m_queryTimeout;
    QColor m_accent = QColor("#a4a7ff");
    int m_radius = 0;
};
