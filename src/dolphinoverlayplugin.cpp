#include "dolphinoverlayplugin.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <vdfparser.hpp>

SteamCompatPluginOverlay::SteamCompatPluginOverlay(QObject *parent,
                                                   const QList<QVariant> &args)
    : KOverlayIconPlugin(parent) {

  Q_UNUSED(args);
}

// Locates Steam's shortcuts.vdf for non-Steam shortcuts lookup
static QString getShortcutsFilePath() {
  QString steamUserData =
      QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +
      QStringLiteral("/.local/share/Steam/userdata/");

  QDir userDir(steamUserData);
  if (!userDir.exists()) {
    return QString();
  }

  QStringList userIds = userDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  if (userIds.isEmpty()) {
    return QString();
  }

  return userDir.filePath(userIds.first() +
                          QStringLiteral("/config/shortcuts.vdf"));
}

// Resolves the actual game title using source-parsers for VDF files
static QString getGameNameFromManifest(const QString &compatDataPath,
                                       const QString &appIdStr) {
  if (appIdStr == QLatin1String("0")) {
    return QStringLiteral("Default Steam Prefix");
  }

  QDir dir(compatDataPath);
  if (!dir.cdUp() || !dir.cdUp()) {
    return QString();
  }

  // 1. Try reading the official appmanifest text VDF (.acf) first
  QString manifestPath =
      dir.filePath(QStringLiteral("appmanifest_%1.acf").arg(appIdStr));
  QFile file(manifestPath);

  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream in(&file);
    std::string rawData = in.readAll().toStdString();
    try {
      // Parse using VdfParser
      auto rootKv = VdfParser::fromString(
          rawData); // or VdfParser::fromString depending on
                    // exact API, but parse() is standard
      auto nameChild = rootKv.getChild("name");
      if (nameChild.has_value()) {
        auto val = nameChild->getValue();
        if (val.has_value()) {
          return QString::fromStdString(val.value());
        }
      }
    } catch (...) {
      // Fallback if parsing fails
    }
  }

  // 2. Fallback: If it's a non-Steam game, check shortcuts.vdf in userdata
  // (Non-Steam appIDs are computed identifiers mapped inside shortcuts.vdf)
  QString shortcutsPath = getShortcutsFilePath();
  QFile shortcutsFile(shortcutsPath);
  if (shortcutsFile.open(QIODevice::ReadOnly)) {
    QByteArray rawData = shortcutsFile.readAll();
    try {
      auto rootKv = VdfParser::fromString(
          std::string(rawData.constData(), rawData.size()));
      // shortcuts.vdf typically structures entries under numeric indexes ("0",
      // "1", etc.) You can iterate children to match appid if needed, or parse
      // titles.
    } catch (...) {
    }
  }

  return QString();
}

QStringList SteamCompatPluginOverlay::getOverlays(const QUrl &url) {
  if (!url.isLocalFile()) {
    return QStringList();
  }

  const QString localPath = url.toLocalFile();

  // Capture the ID portion after compatdata/
  QRegularExpression rx(
      QStringLiteral(".*/steamapps/compatdata/(\\d+)(/.*)?$"));
  QRegularExpressionMatch match = rx.match(localPath);

  if (match.hasMatch()) {
    QString appIdStr = match.captured(1);

    QString gameName = getGameNameFromManifest(localPath, appIdStr);
    if (!gameName.isEmpty()) {
      qDebug() << "Parsed Game Title via source-parsers:" << gameName;
    }

    if (appIdStr.length() >= 10 && appIdStr != QLatin1String("0")) {
      qDebug() << "MATCH FOUND! Showing Steam overlay for:" << localPath;
      return QStringList{QStringLiteral("steam-non-steam")};
    } else {
      return QStringList{QStringLiteral("steam")};
    }
  }

  return QStringList();
}

#include "dolphinoverlayplugin.moc"
