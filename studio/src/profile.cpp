#include "profile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QSet>

#include <cmath>
#include <algorithm>
#include <sstream>
#include <toml++/toml.hpp>

QString message(const char *source)
{
    return QCoreApplication::translate("Studio", source);
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw ProfileError(message("Could not read %1: %2").arg(path, file.errorString()));
    const auto data = file.readAll();
    if (file.error() != QFile::NoError)
        throw ProfileError(message("Could not read %1: %2").arg(path, file.errorString()));
    return data;
}

void writeFile(const QString &path, const QByteArray &data)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        throw ProfileError(message("Could not write %1: %2").arg(path, file.errorString()));
}

QString omarchyRoot()
{
    return qEnvironmentVariable("OMARCHY_PATH", "/usr/share/omarchy");
}

QString run(const QString &program, const QStringList &args, int timeout, const QString &temporaryHome)
{
    QProcess process;
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("OMARCHY_PATH", omarchyRoot());
    env.insert("PATH", env.value("PATH") + ":" + omarchyRoot() + "/bin");
    if (!temporaryHome.isEmpty()) {
        env.insert("HOME", temporaryHome);
        env.insert("XDG_CONFIG_HOME", temporaryHome + "/.config");
        env.insert("XDG_STATE_HOME", temporaryHome + "/.local/state");
        env.insert("XDG_CACHE_HOME", temporaryHome + "/.cache");
        env.insert("XDG_RUNTIME_DIR", temporaryHome);
    }
    process.setProcessEnvironment(env);
    process.setStandardInputFile(QProcess::nullDevice());
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, args);
    if (!process.waitForStarted())
        throw ProfileError(message("Could not start %1: %2").arg(program, process.errorString()));
    if (!process.waitForFinished(timeout)) {
        process.kill();
        process.waitForFinished();
        throw ProfileError(message("%1 took too long to respond.").arg(program));
    }
    const auto output = QString::fromUtf8(process.readAll()).trimmed();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        throw ProfileError(message("%1 failed: %2").arg(program, output.isEmpty() ? process.errorString() : output));
    return output;
}

static void makeDirectory(const QString &path)
{
    if (!QDir().mkpath(path))
        throw ProfileError(message("Could not create %1.").arg(path));
}

static toml::table parseToml(const QByteArray &data, const QString &path)
{
    try {
        return toml::parse(std::string_view(data.constData(), data.size()), path.toStdString());
    } catch (const toml::parse_error &error) {
        std::ostringstream details;
        details << error;
        throw ProfileError(message("Invalid TOML in %1: %2").arg(path, QString::fromStdString(details.str())));
    }
}

static toml::table readToml(const QString &path)
{
    return parseToml(readFile(path), path);
}

static void writeToml(const QString &path, const toml::table &data)
{
    std::ostringstream result;
    result << toml::toml_formatter{data};
    writeFile(path, QByteArray::fromStdString(result.str()) + '\n');
}

static QString tomlValue(const toml::node &node)
{
    std::ostringstream result;
    result << toml::toml_formatter{node};
    return QString::fromStdString(result.str());
}

static QString fromUtf8(std::string_view value)
{
    return QString::fromUtf8(value.data(), value.size());
}

static bool isHex(const QString &value)
{
    static const QRegularExpression pattern(R"(^#[0-9a-fA-F]{6}$)");
    return pattern.match(value).hasMatch();
}

static bool shellBorderWidth(const QString &key)
{
    return key.endsWith("border-width") || (key.startsWith("border-") && key.endsWith("-width"));
}

static bool validBorder(const QString &value, bool shell)
{
    static const QRegularExpression stop(R"(^(#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?|rgba?\([0-9a-fA-F]{6,8}\)|0x[0-9a-fA-F]{8}|rgba?\(\d{1,3},\d{1,3},\d{1,3}(,[01](\.\d+)?)?\))$)");
    static const QRegularExpression angle(R"(^-?\d{1,3}(\.\d+)?deg$)");
    static const QSet<QString> aliases = {
        "accent", "selection", "muted", "background", "dark_background", "darker_background",
        "lighter_background", "foreground", "dark_foreground", "light_foreground", "bright_foreground",
        "red", "yellow", "orange", "green", "cyan", "blue", "magenta", "brown", "bright_red",
        "bright_yellow", "bright_green", "bright_cyan", "bright_blue", "bright_magenta"
    };
    if (shell && (value == "hyprland.active-border" || value == "hyprland.active-border-foreground"))
        return true;
    auto parts = value.split(' ', Qt::SkipEmptyParts);
    if (parts.size() > 1 && angle.match(parts.last()).hasMatch())
        parts.removeLast();
    if (parts.isEmpty() || parts.size() > 10)
        return false;
    for (const auto &part : parts)
        if (!stop.match(part).hasMatch() && (shell || !aliases.contains(part)))
            return false;
    return true;
}

QVariant normalize(const Field &field, const QVariant &value)
{
    const QString text = value.toString().trimmed();
    auto invalid = [&] {
        throw ProfileError(message("Invalid value for %1.").arg(field.id));
    };
    if (field.kind == Kind::Bool) {
        if (text != "true" && text != "false")
            invalid();
        return text == "true";
    }
    if (field.kind == Kind::Int || field.kind == Kind::Number) {
        bool ok = false;
        const double number = text.toDouble(&ok);
        if (!ok || !std::isfinite(number) || number < field.minimum || number > field.maximum
            || (field.kind == Kind::Int && std::trunc(number) != number))
            invalid();
        return field.kind == Kind::Int ? QVariant(static_cast<qint64>(number)) : QVariant(number);
    }
    if (text.size() > 4096 || text.contains('\n') || text.contains('\r') || text.contains(QChar::Null))
        invalid();
    switch (field.kind) {
    case Kind::Color:
        if (!isHex(text))
            invalid();
        return text.toLower();
    case Kind::Border:
        if (!validBorder(text, field.id.startsWith("shell.")) || text.size() > 250)
            invalid();
        return text.split(' ', Qt::SkipEmptyParts).join(' ');
    case Kind::Choice:
        if (!field.choices.contains(text))
            invalid();
        return text;
    case Kind::Toml: {
        const auto parsed = parseToml("value = " + text.toUtf8(), field.id);
        if (parsed.size() != 1 || !parsed.contains("value"))
            invalid();
        return tomlValue(*parsed.get("value"));
    }
    case Kind::Text:
        if (field.id == "identity.icons" && (text.isEmpty() || text.contains('/') || text.startsWith('.')))
            invalid();
        if (field.id.startsWith("windows.general.") && field.id.contains("gaps")) {
            static const QRegularExpression gaps(R"(^\d{1,3}( \d{1,3}){0,3}$)");
            if (!(field.id == "windows.general.float_gaps" && text == "-1") && !gaps.match(text).hasMatch())
                invalid();
        }
        if (field.id == "windows.decoration.shadow.offset") {
            static const QRegularExpression offset(R"(^-?\d{1,3}(\.\d{1,4})? -?\d{1,3}(\.\d{1,4})?$)");
            if (!offset.match(text).hasMatch())
                invalid();
        }
        if (field.id.startsWith("shell.") && shellBorderWidth(field.id.section('.', -1))) {
            static const QRegularExpression width(R"(^\d{1,2}(\.\d{1,2})?( \d{1,2}(\.\d{1,2})?){0,3}$)");
            if (!width.match(text).hasMatch())
                invalid();
        }
        return text;
    default:
        invalid();
    }
    return {};
}

QVariantMap readProfile(const QString &path)
{
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(readFile(path), &error);
    if (!doc.isObject())
        throw ProfileError(message("Invalid profile in %1: %2").arg(path, error.errorString()));
    return doc.object().toVariantMap();
}

QVariantMap validateProfile(const Catalog &catalog, const QVariantMap &profile)
{
    QVariantMap result;
    for (auto it = profile.cbegin(); it != profile.cend(); ++it) {
        auto field = std::find_if(catalog.fields.cbegin(), catalog.fields.cend(),
                                  [&](const Field &entry) { return entry.id == it.key(); });
        if (field == catalog.fields.cend())
            throw ProfileError(message("Unknown setting: %1").arg(it.key()));
        result.insert(it.key(), normalize(*field, it.value()));
    }
    return result;
}

static bool isMedia(const QFileInfo &file, bool videoOnly = false)
{
    static const QStringList videos{"mp4", "m4v", "mov", "webm", "mkv", "avi"};
    static const QStringList stills{"jpg", "jpeg", "png", "gif", "bmp", "webp"};
    const QString suffix = file.suffix().toLower();
    return file.isFile() && !file.isSymLink() && (videos.contains(suffix) || (!videoOnly && stills.contains(suffix)));
}

static QMap<QString, QString> wallpaperFiles(const QString &path)
{
    QMap<QString, QString> result;
    if (QFileInfo(path).isSymLink())
        return result;
    for (const auto &file : QDir(path).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name))
        if (isMedia(file))
            result.insert(file.fileName(), file.absoluteFilePath());
    return result;
}

QString personalWallpaperFolder()
{
    return QDir::homePath() + "/.config/omarchy/backgrounds/cyber-make-personal";
}

void refreshWallpapers(Catalog &catalog, const QString &source)
{
    catalog.wallpapers = wallpaperFiles(source + "/backgrounds");
    const auto personal = wallpaperFiles(personalWallpaperFolder());
    for (auto it = personal.cbegin(); it != personal.cend(); ++it)
        catalog.wallpapers.insert(it.key(), it.value());
    auto choices = catalog.wallpapers.keys();
    choices.prepend(QString{});
    for (auto &field : catalog.fields)
        if (field.id == "identity.wallpaper")
            field.choices = choices;
}

static void copyFile(const QString &from, const QString &to)
{
    if (!QFile::copy(from, to))
        throw ProfileError(message("Could not copy %1 to %2.").arg(from, to));
}

static void copyShell(const QString &source, const QString &destination)
{
    const QDir base(source);
    for (const auto &file : base.entryInfoList({"shell.toml", "shell.*.toml"}, QDir::Files | QDir::NoSymLinks))
        copyFile(file.absoluteFilePath(), destination + "/" + file.fileName());
}

static toml::table renderShell(const QString &source, const toml::table &palette)
{
    // Omarchy owns placeholder and color mixing semantics. Use an isolated HOME
    // because the renderer normally writes directly into the live staging tree.
    QTemporaryDir home;
    if (!home.isValid())
        throw ProfileError(message("Could not create a temporary directory."));
    const auto theme = home.filePath(".local/state/omarchy/current/next-theme");
    makeDirectory(theme);
    writeToml(theme + "/colors.toml", palette);
    copyShell(source, theme);

    const QString templates = qEnvironmentVariable("XDG_CONFIG_HOME", QDir::homePath() + "/.config")
                            + "/omarchy/themed/shell.toml.tpl";
    if (QFileInfo::exists(templates)) {
        const QString target = home.filePath(".config/omarchy/themed");
        makeDirectory(target);
        copyFile(templates, target + "/shell.toml.tpl");
    }
    run(omarchyRoot() + "/bin/omarchy-theme-set-templates", {}, 15000, home.path());
    return readToml(theme + "/shell.toml");
}

static Field shellField(const QString &section, const QString &key, const toml::node &node)
{
    Field field{"shell." + section + "." + key, key, section, Kind::Toml, tomlValue(node)};
    if (auto boolean = node.value_exact<bool>()) {
        field.kind = Kind::Bool;
        field.fallback = *boolean;
    } else if (auto integer = node.value_exact<std::int64_t>()) {
        field.kind = Kind::Int;
        field.fallback = static_cast<qint64>(*integer);
        field.minimum = -100000;
        field.maximum = 100000;
    } else if (auto number = node.value_exact<double>()) {
        field.kind = Kind::Number;
        field.fallback = *number;
        field.minimum = -100000;
        field.maximum = 100000;
    } else if (auto str = node.value_exact<std::string>()) {
        field.fallback = QString::fromStdString(*str);
        field.kind = isHex(field.fallback.toString()) ? Kind::Color : Kind::Text;
        if ((key.endsWith("border") || key.startsWith("active-border") || key.startsWith("border-"))
            && validBorder(field.fallback.toString(), true))
            field.kind = Kind::Border;
    }
    if ((field.kind == Kind::Int || field.kind == Kind::Number) && key.endsWith("-alpha")) {
        field.kind = Kind::Number;
        field.minimum = 0;
        field.maximum = 1;
    }
    if (shellBorderWidth(key))
        field.kind = Kind::Text;
    return field;
}

static QStringList shellSections()
{
    return {"bar", "launcher", "menu", "notifications", "popups", "tooltip", "controls",
            "spacing", "font", "hyprland", "polkit", "lock", "image-picker"};
}

static void addShellFields(Catalog &catalog, const toml::table &rendered)
{
    auto sections = shellSections();
    for (const auto &[key, node] : rendered) {
        const QString section = fromUtf8(key.str());
        if (node.is_table() && !sections.contains(section))
            sections << section;
    }
    const auto templatePath = omarchyRoot() + "/default/themed/shell.toml.tpl";
    static const QRegularExpression header(R"(^\[([a-zA-Z0-9_-]+)\])");
    static const QRegularExpression optional(R"(^\s*#\s*([a-zA-Z0-9_-]+)\s*=\s*(.+)$)");
    QMap<QString, QList<Field>> optionalFields;
    if (QFileInfo::exists(templatePath)) {
        QString section;
        for (const auto &line : QString::fromUtf8(readFile(templatePath)).split('\n')) {
            if (const auto match = header.match(line); match.hasMatch())
                section = match.captured(1);
            else if (const auto option = optional.match(line); option.hasMatch() && !section.isEmpty()) {
                const auto key = option.captured(1);
                if (!rendered[section.toStdString()][key.toStdString()]) {
                    try {
                        const auto parsed = parseToml("value = " + option.captured(2).toUtf8(), templatePath);
                        if (const auto *node = parsed.get("value"))
                            optionalFields[section].append(shellField(section, key, *node));
                    } catch (const ProfileError &) {
                        // A comment can contain an explanation in assignment form.
                    }
                }
            }
        }
    }
    for (const auto &section : sections) {
        if (const auto *table = rendered[section.toStdString()].as_table())
            for (const auto &[key, node] : *table)
                catalog.fields.append(shellField(section, fromUtf8(key.str()), node));
        catalog.fields.append(optionalFields.value(section));
    }
}

static void addShellBorderWidths(Catalog &catalog)
{
    const QMap<QString, QStringList> widths{
        {"controls", {"normal-border-width", "hover-cursor-border-width", "focus-border-width", "selected-border-width"}},
        {"hyprland", {"active-border-width"}},
        {"lock", {"border-width", "border-active-width", "border-error-width"}},
        {"menu", {"border-width", "selected-border-width"}},
        {"notifications", {"border-width"}},
        {"popups", {"border-width"}},
        {"tooltip", {"border-width"}},
    };
    for (auto section = widths.cbegin(); section != widths.cend(); ++section) {
        for (const auto &key : section.value()) {
            const auto id = "shell." + section.key() + "." + key;
            const auto present = [&](const QString &name) {
                return std::any_of(catalog.fields.cbegin(), catalog.fields.cend(),
                                   [&](const Field &field) { return field.id == name; });
            };
            if (!present(id))
                catalog.fields.append({id, key, section.key(), Kind::Text, "1"});
            if (section.key() == "hyprland")
                continue;
            for (const auto &side : {"top", "right", "bottom", "left"}) {
                const auto suffix = QString("-") + side;
                if (!present(id + suffix))
                    catalog.fields.append({id + suffix, key + suffix, section.key(), Kind::Number, 1.0, 0, 99});
            }
        }
    }
}

struct Animation {
    QString leaf;
    QString section;
    bool enabled;
    double speed;
    QString curve;
    QString style;
    QStringList styles;
};

static const QList<Animation> &animations()
{
    static const QStringList windows{"", "popin", "popin 60%", "popin 87%", "slide", "slide top", "slide bottom", "slide left", "slide right", "gnomed"};
    static const QStringList layers{"", "fade", "popin", "popin 87%", "slide", "slide top", "slide bottom", "slide left", "slide right"};
    static const QStringList workspaces{"", "slide", "slidevert", "fade", "slidefade", "slidefadevert", "slidefade 20%", "slidefadevert 20%"};
    static const QList<Animation> list{
        {"global", "Global", true, 10, "default", "", {}},
        {"windows", "Windows", true, 3.79, "easeOutQuint", "", windows},
        {"windowsIn", "Windows", true, 4.1, "easeOutQuint", "popin 87%", windows},
        {"windowsOut", "Windows", true, 1.49, "linear", "popin 87%", windows},
        {"windowsMove", "Windows", true, 3.79, "easeOutQuint", "", {}},
        {"border", "Windows", true, 5.39, "easeOutQuint", "", {}},
        {"borderangle", "Windows", true, 10, "default", "", {"", "once", "loop"}},
        {"fade", "Fading", true, 3.03, "quick", "", {}},
        {"fadeIn", "Fading", true, 1.73, "almostLinear", "", {}},
        {"fadeOut", "Fading", true, 1.46, "almostLinear", "", {}},
        {"fadeSwitch", "Fading", false, 3.03, "quick", "", {}},
        {"fadeShadow", "Fading", true, 3.03, "quick", "", {}},
        {"fadeDim", "Fading", true, 3.03, "quick", "", {}},
        {"fadeLayers", "Layers", true, 3.03, "quick", "", {}},
        {"fadeLayersIn", "Layers", true, 1.79, "almostLinear", "", {}},
        {"fadeLayersOut", "Layers", true, 1.39, "almostLinear", "", {}},
        {"layers", "Layers", true, 3.81, "easeOutQuint", "", layers},
        {"layersIn", "Layers", true, 4, "easeOutQuint", "fade", layers},
        {"layersOut", "Layers", true, 1.5, "linear", "fade", layers},
        {"workspaces", "Workspaces", false, 4, "easeOutQuint", "slide", workspaces},
        {"workspacesIn", "Workspaces", false, 4, "easeOutQuint", "slide", workspaces},
        {"workspacesOut", "Workspaces", false, 4, "easeOutQuint", "slide", workspaces},
        {"specialWorkspace", "Workspaces", false, 4, "easeOutQuint", "slidevert", workspaces},
        {"specialWorkspaceIn", "Workspaces", false, 4, "easeOutQuint", "slidevert", workspaces},
        {"specialWorkspaceOut", "Workspaces", false, 4, "easeOutQuint", "slidevert", workspaces},
        {"zoomFactor", "Global", true, 10, "default", "", {}},
        {"monitorAdded", "Global", true, 10, "default", "", {}},
    };
    return list;
}

static void addWindowFields(Catalog &catalog)
{
    catalog.fields.append({
        {"windows.decoration.rounding", "Corner radius", "Geometry", Kind::Int, 0, 0, 100},
        {"windows.decoration.rounding_power", "Corner curve", "Geometry", Kind::Number, 2.0, 1, 10},
        {"windows.general.border_size", "Border width", "Geometry", Kind::Int, 2, 0, 20},
        {"windows.general.gaps_in", "Window gaps", "Geometry", Kind::Text, "5"},
        {"windows.general.gaps_out", "Screen-edge gaps", "Geometry", Kind::Text, "10"},
        {"windows.general.float_gaps", "Floating window gaps", "Geometry", Kind::Text, "0"},
        {"windows.general.gaps_workspaces", "Workspace gaps", "Geometry", Kind::Int, 0, 0, 999},
        {"windows.decoration.active_opacity", "Active opacity", "Opacity", Kind::Number, 1.0, 0, 1},
        {"windows.decoration.inactive_opacity", "Inactive opacity", "Opacity", Kind::Number, 1.0, 0, 1},
        {"windows.decoration.fullscreen_opacity", "Fullscreen opacity", "Opacity", Kind::Number, 1.0, 0, 1},
        {"windows.decoration.dim_inactive", "Dim inactive windows", "Opacity", Kind::Bool, false},
        {"windows.decoration.dim_strength", "Inactive dimming", "Opacity", Kind::Number, 0.5, 0, 1},
        {"windows.decoration.dim_special", "Special workspace dimming", "Opacity", Kind::Number, 0.2, 0, 1},
        {"windows.decoration.dim_around", "Dimming around dialogs", "Opacity", Kind::Number, 0.4, 0, 1},
        {"windows.decoration.blur.enabled", "Window blur", "Blur", Kind::Bool, false},
        {"windows.decoration.blur.size", "Blur size", "Blur", Kind::Int, 4, 1, 100},
        {"windows.decoration.blur.passes", "Blur passes", "Blur", Kind::Int, 2, 1, 10},
        {"windows.decoration.blur.ignore_opacity", "Ignore window opacity", "Blur", Kind::Bool, true},
        {"windows.decoration.blur.new_optimizations", "Blur optimizations", "Blur", Kind::Bool, true},
        {"windows.decoration.blur.xray", "Blur wallpaper only", "Blur", Kind::Bool, false},
        {"windows.decoration.blur.noise", "Noise", "Blur", Kind::Number, 0.0117, 0, 1},
        {"windows.decoration.blur.contrast", "Contrast", "Blur", Kind::Number, 0.8916, 0, 2},
        {"windows.decoration.blur.brightness", "Brightness", "Blur", Kind::Number, 1.0, 0, 2},
        {"windows.decoration.blur.vibrancy", "Vibrancy", "Blur", Kind::Number, 0.1696, 0, 1},
        {"windows.decoration.blur.vibrancy_darkness", "Vibrancy in dark areas", "Blur", Kind::Number, 0.0, 0, 1},
        {"windows.decoration.blur.special", "Blur special workspace", "Blur", Kind::Bool, false},
        {"windows.decoration.blur.popups", "Blur window popups", "Blur", Kind::Bool, false},
        {"windows.decoration.blur.popups_ignorealpha", "Minimum popup alpha", "Blur", Kind::Number, 0.2, 0, 1},
        {"windows.decoration.blur.input_methods", "Blur input methods", "Blur", Kind::Bool, false},
        {"windows.decoration.blur.input_methods_ignorealpha", "Minimum input method alpha", "Blur", Kind::Number, 0.2, 0, 1},
        {"windows.shell.blur", "Blur Omarchy shell surfaces", "Blur", Kind::Bool, false},
        {"windows.decoration.shadow.enabled", "Window shadows", "Shadow", Kind::Bool, false},
        {"windows.decoration.shadow.range", "Shadow range", "Shadow", Kind::Int, 4, 0, 100},
        {"windows.decoration.shadow.render_power", "Shadow falloff", "Shadow", Kind::Int, 3, 1, 4},
        {"windows.decoration.shadow.sharp", "Sharp shadow", "Shadow", Kind::Bool, false},
        {"windows.decoration.shadow.scale", "Shadow scale", "Shadow", Kind::Number, 1.0, 0.05, 2},
        {"windows.decoration.shadow.offset", "Shadow offset (X Y)", "Shadow", Kind::Text, "0 0"},
        {"windows.decoration.shadow.color", "Active shadow color", "Shadow", Kind::Color, "#1a1a1a"},
        {"windows.decoration.shadow.color_inactive", "Inactive shadow color", "Shadow", Kind::Color, "#1a1a1a"},
        {"windows.decoration.glow.enabled", "Inner window glow", "Glow", Kind::Bool, false},
        {"windows.decoration.glow.range", "Glow range", "Glow", Kind::Int, 10, 0, 100},
        {"windows.decoration.glow.render_power", "Glow falloff", "Glow", Kind::Int, 3, 1, 4},
        {"windows.decoration.glow.color", "Active glow color", "Glow", Kind::Color, "#1a1a1a"},
        {"windows.decoration.glow.color_inactive", "Inactive glow color", "Glow", Kind::Color, "#1a1a1a"},
        {"windows.decoration.motion_blur.enabled", "Blur moving windows", "Motion blur", Kind::Bool, false},
        {"windows.decoration.motion_blur.samples", "Motion blur samples", "Motion blur", Kind::Int, 7, 1, 64},
    });
    catalog.fields.append({
        {"motion.animations.enabled", "Enable animations", "Global", Kind::Bool, true},
        {"motion.animations.workspace_wraparound", "Wrap workspace animations", "Global", Kind::Bool, false},
    });
    for (const auto &animation : animations()) {
        const auto id = "motion." + animation.leaf + ".";
        catalog.fields.append({
            {id + "enabled", animation.leaf + " · enabled", animation.section, Kind::Bool, animation.enabled},
            {id + "speed", animation.leaf + " · speed", animation.section, Kind::Number, animation.speed, 0.1, 20},
            {id + "curve", animation.leaf + " · curve", animation.section, Kind::Choice, animation.curve, 0, 0,
             {"default", "easeOutQuint", "easeInOutCubic", "linear", "almostLinear", "quick"}},
        });
        if (!animation.styles.isEmpty())
            catalog.fields.append({id + "style", animation.leaf + " · style", animation.section, Kind::Choice,
                                   animation.style, 0, 0, animation.styles});
    }
}

Catalog readCatalog(const QString &source)
{
    Catalog catalog;
    const auto palette = readToml(source + "/colors.toml");
    for (const auto &key : {"accent", "background", "foreground"}) {
        const auto value = palette[key].value_exact<std::string>();
        if (!value || !isHex(QString::fromStdString(*value)))
            throw ProfileError(message("Expected a Cyber Make palette in %1.").arg(source));
    }
    catalog.fields.append({"palette.mode", "Appearance mode", "Base colors", Kind::Choice,
                           fromUtf8(palette["mode"].value_or<std::string>("dark")), 0, 0, {"dark", "light"}});
    const QStringList primary{"accent", "background", "foreground", "selection", "muted", "dark_background",
                              "darker_background", "lighter_background", "dark_foreground", "light_foreground",
                              "bright_foreground"};
    const QStringList accents{"red", "yellow", "orange", "green", "cyan", "blue", "magenta", "brown",
                              "bright_red", "bright_yellow", "bright_green", "bright_cyan", "bright_blue",
                              "bright_magenta"};
    auto addColors = [&](const QStringList &keys, const QString &section) {
        for (const auto &key : keys) {
            const auto value = palette[key.toStdString()].value_exact<std::string>();
            if (value && isHex(QString::fromStdString(*value)))
                catalog.fields.append({"palette." + key, key, section, Kind::Color, QString::fromStdString(*value)});
        }
    };
    addColors(primary, "Base colors");
    addColors(accents, "Semantic colors");
    catalog.fields.append({
        {"palette.hyprland_active_border", "Active window border", "Window borders", Kind::Border,
         fromUtf8(palette["hyprland_active_border"].value_or<std::string>("accent"))},
        {"palette.hyprland_inactive_border", "Inactive window border", "Window borders", Kind::Border,
         fromUtf8(palette["hyprland_inactive_border"].value_or<std::string>("rgba(595959aa)"))},
    });
    QStringList other;
    for (const auto &[key, node] : palette) {
        const auto name = fromUtf8(key.str());
        if (node.is_string() && !primary.contains(name) && !accents.contains(name))
            other << name;
    }
    addColors(other, "Other palette colors");

    addWindowFields(catalog);
    addShellFields(catalog, renderShell(source, palette));
    addShellBorderWidths(catalog);
    const QString preview = source + "/preview.png";
    if (QFileInfo::exists(preview))
        catalog.still = preview;
    else
        for (const auto &path : wallpaperFiles(source + "/backgrounds"))
            if (!isMedia(QFileInfo(path), true)) {
                catalog.still = path;
                break;
            }
    QString icons = "Yaru-blue";
    if (QFileInfo::exists(source + "/icons.theme"))
        icons = QString::fromUtf8(readFile(source + "/icons.theme")).trimmed();
    catalog.fields.append({
        {"identity.wallpaper", "Wallpaper", "Wallpaper", Kind::Choice, ""},
        {"identity.intro", "Include wallpaper intro", "Wallpaper", Kind::Bool, true},
        {"identity.icons", "Icon theme name", "Icons", Kind::Text, icons},
    });
    refreshWallpapers(catalog, source);
    return catalog;
}

static void insertValue(toml::table &table, const Field &field, const QVariant &value)
{
    const auto key = field.id.section('.', -1).toStdString();
    switch (field.kind) {
    case Kind::Bool:
        table.insert_or_assign(key, value.toBool());
        break;
    case Kind::Int:
        table.insert_or_assign(key, value.toLongLong());
        break;
    case Kind::Number:
        table.insert_or_assign(key, value.toDouble());
        break;
    case Kind::Toml: {
        const auto parsed = parseToml("value = " + value.toString().toUtf8(), field.id);
        table.insert_or_assign(key, *parsed.get("value"));
        break;
    }
    default:
        table.insert_or_assign(key, value.toString().toStdString());
        break;
    }
}

static QByteArray luaLiteral(const QVariant &value)
{
    if (value.metaType().id() == QMetaType::QStringList)
        return "{ " + value.toStringList().join(", ").toLatin1() + " }";
    // All emitted strings are single-line names, colors, styles, or CSS gaps.
    if (value.metaType().id() == QMetaType::QString) {
        auto text = value.toString().toUtf8();
        text.replace("\\", "\\\\").replace("\"", "\\\"");
        return '"' + text + '"';
    }
    return value.toString().toLatin1();
}

static QByteArray luaTable(const QVariantMap &values, const QByteArray &indent)
{
    QByteArray result = "{\n";
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const auto literal = it.value().metaType().id() == QMetaType::QVariantMap
                                 ? luaTable(it.value().toMap(), indent + "  ") : luaLiteral(it.value());
        result += indent + "  " + it.key().toLatin1() + " = " + literal + ",\n";
    }
    return result + indent + "}";
}

bool hasAppearance(const QVariantMap &profile)
{
    for (const auto &id : profile.keys())
        if (id.startsWith("windows.") || id.startsWith("motion."))
            return true;
    return false;
}

static void setLuaValue(QVariantMap &values, const QStringList &path, const QVariant &value)
{
    const auto &key = path.first();
    if (path.size() == 1) {
        values.insert(key, value);
        return;
    }
    auto children = values.value(key).toMap();
    setLuaValue(children, path.mid(1), value);
    values.insert(key, children);
}

static QVariant cssGaps(const QString &value)
{
    const auto parts = value.split(' ');
    const auto top = parts[0].toInt();
    if (parts.size() == 1)
        return top;
    const auto right = parts[1].toInt();
    return QVariantMap{{"top", top}, {"right", right}, {"bottom", parts.value(2, parts[0]).toInt()},
                       {"left", parts.value(3, parts[1]).toInt()}};
}

static QByteArray appearance(const QVariantMap &profile)
{
    QByteArray lua = "-- Generated by Cyber Make Studio.\n";
    QVariantMap config;
    for (auto it = profile.cbegin(); it != profile.cend(); ++it) {
        const auto &id = it.key();
        if (id.startsWith("motion.animations.")) {
            setLuaValue(config, id.split('.').mid(1), it.value());
            continue;
        }
        if (!id.startsWith("windows.") || id == "windows.shell.blur")
            continue;
        QVariant value = it.value();
        if (id.startsWith("windows.decoration.shadow.color") || id.startsWith("windows.decoration.glow.color"))
            value = "rgb(" + value.toString().mid(1) + ")";
        else if (id == "windows.decoration.shadow.offset")
            value = value.toString().split(' ');
        else if (id == "windows.general.gaps_in" || id == "windows.general.gaps_out"
                 || id == "windows.general.float_gaps")
            value = cssGaps(value.toString());
        setLuaValue(config, id.split('.').mid(1), value);
    }
    if (!config.isEmpty())
        lua += "hl.config(" + luaTable(config, {}) + ")\n";

    if (profile.contains("windows.shell.blur")) {
        lua += "hl.layer_rule({\n"
               "  match = { namespace = \"^(omarchy-bar|omarchy-menu|omarchy-launcher|omarchy-notifications|"
               "omarchy-osd|omarchy-polkit|omarchy-reminders|omarchy-clipboard|omarchy-emojis|"
               "omarchy-image-selector|omarchy-keyboard-panel|omarchy-network-qr)$\" },\n"
               "  blur = " + luaLiteral(profile.value("windows.shell.blur")) + ",\n"
               "  ignore_alpha = 0.3,\n"
               "})\n";
    }

    for (const auto &animation : animations()) {
        const auto id = "motion." + animation.leaf + ".";
        if (!profile.contains(id + "enabled") && !profile.contains(id + "speed") &&
            !profile.contains(id + "curve") && !profile.contains(id + "style"))
            continue;
        const bool enabled = profile.value(id + "enabled", animation.enabled).toBool();
        lua += "hl.animation({ leaf = " + luaLiteral(animation.leaf);
        if (!enabled) {
            lua += ", enabled = false })\n";
            continue;
        }
        const auto speed = profile.value(id + "speed", animation.speed);
        const auto curve = profile.value(id + "curve", animation.curve);
        const auto style = profile.value(id + "style", animation.style);
        lua += ", enabled = true, speed = " + luaLiteral(speed) + ", bezier = " + luaLiteral(curve);
        if (!style.toString().isEmpty())
            lua += ", style = " + luaLiteral(style);
        lua += " })\n";
    }
    return lua;
}

void buildTheme(const QString &source, const QString &destination, const Catalog &catalog,
                const QVariantMap &profile)
{
    auto palette = readToml(source + "/colors.toml");
    for (const auto &field : catalog.fields)
        if (field.id.startsWith("palette.") && profile.contains(field.id))
            insertValue(palette, field, profile.value(field.id));
    writeToml(destination + "/colors.toml", palette);
    copyShell(source, destination);

    QSet<QString> shellEdits;
    for (const auto &field : catalog.fields)
        if (field.id.startsWith("shell.") && profile.contains(field.id))
            shellEdits.insert(field.section);
    if (!shellEdits.isEmpty()) {
        const auto defaults = renderShell(source, palette);
        for (const auto &section : shellEdits) {
            toml::table values;
            if (const auto *original = defaults[section.toStdString()].as_table())
                values = *original;
            for (const auto &field : catalog.fields)
                if (field.id.startsWith("shell.") && field.section == section && profile.contains(field.id))
                    insertValue(values, field, profile.value(field.id));
            toml::table sectionTable;
            sectionTable.insert(section.toStdString(), values);
            writeToml(destination + "/shell." + section + ".toml", sectionTable);
        }
    }

    const auto icon = profile.value("identity.icons").toString();
    if (!icon.isEmpty())
        writeFile(destination + "/icons.theme", icon.toUtf8() + '\n');
    else if (QFileInfo::exists(source + "/icons.theme"))
        copyFile(source + "/icons.theme", destination + "/icons.theme");

    auto wallpapers = wallpaperFiles(source + "/backgrounds");
    const auto selected = profile.value("identity.wallpaper").toString();
    if (!selected.isEmpty())
        wallpapers.insert(selected, catalog.wallpapers.value(selected));
    if (!wallpapers.isEmpty()) {
        makeDirectory(destination + "/backgrounds");
        for (auto it = wallpapers.cbegin(); it != wallpapers.cend(); ++it)
            copyFile(it.value(), destination + "/backgrounds/" + it.key());
        const QDir intros(source + "/backgrounds/intros");
        if (profile.value("identity.intro", true).toBool() && !QFileInfo(intros.path()).isSymLink()) {
            for (const auto &file : intros.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name)) {
                if (!isMedia(file, true))
                    continue;
                makeDirectory(destination + "/backgrounds/intros");
                copyFile(file.absoluteFilePath(), destination + "/backgrounds/intros/" + file.fileName());
            }
        }
    }

    if (hasAppearance(profile)) {
        QTemporaryDir verification;
        if (!verification.isValid())
            throw ProfileError(message("Could not create a temporary directory."));
        const auto code = appearance(profile);
        const auto file = verification.filePath("verify.lua");
        writeFile(file, readFile(omarchyRoot() + "/default/hypr/looknfeel.lua") + '\n' + code);
        run("Hyprland", {"--verify-config", "-c", file}, 15000, verification.path());
        writeFile(destination + "/cyber-make-appearance.lua", code);
    }
    writeFile(destination + "/.studio.json", QJsonDocument::fromVariant(profile).toJson(QJsonDocument::Indented));
}
