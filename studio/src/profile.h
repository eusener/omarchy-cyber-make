#pragma once

#include <QList>
#include <QStringList>
#include <QVariantMap>

enum class Kind { Color, Border, Bool, Int, Number, Choice, Text, Toml };

struct Field {
    QString id;
    QString label;
    QString section;
    Kind kind;
    QVariant fallback;
    double minimum = 0;
    double maximum = 0;
    QStringList choices = {};
};

struct Catalog {
    QList<Field> fields;
    QMap<QString, QString> wallpapers;
    QString still;
};

class ProfileError {
public:
    explicit ProfileError(QString message) : message(std::move(message)) {}
    QString message;
};

QString message(const char *source);
QByteArray readFile(const QString &path);
void writeFile(const QString &path, const QByteArray &data);
QString run(const QString &program, const QStringList &args, int timeout = 15000,
            const QString &temporaryHome = {});
QString omarchyRoot();
QString personalWallpaperFolder();
void refreshWallpapers(Catalog &catalog, const QString &source);

Catalog readCatalog(const QString &source);
QVariantMap readProfile(const QString &path);
QVariantMap validateProfile(const Catalog &catalog, const QVariantMap &profile);
QVariant normalize(const Field &field, const QVariant &value);
bool hasAppearance(const QVariantMap &profile);
void buildTheme(const QString &source, const QString &destination, const Catalog &catalog,
                const QVariantMap &profile);
