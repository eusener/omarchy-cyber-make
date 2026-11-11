#pragma once

#include "profile.h"

struct Desktop {
    QString home;
    QString variant;
    QString revisions;
    QString state;
    QString hyprlandConfig;

    Desktop();
    QString activeTheme() const;
    QString revision() const;
    QString defaultSource() const;
    void apply(const QString &source, const Catalog &catalog, const QVariantMap &profile,
               const QString &expectedRevision) const;
};

void exportTheme(const QString &source, const QString &destination, const Catalog &catalog,
                 const QVariantMap &profile);
