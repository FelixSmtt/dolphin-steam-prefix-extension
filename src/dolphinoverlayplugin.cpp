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

static QString getGameNameFromManifest(const QString &compatDataPath,
                                       const QString &appIdStr) {
  if (appIdStr == QLatin1String("0")) {
    return QStringLiteral("Default Steam Prefix");
  }

  QDir dir(compatDataPath);
  if (!dir.cdUp() || !dir.cdUp()) {
    return QStringLiteral("App ID: %1").arg(appIdStr);
  }

  QString manifestPath =
      dir.filePath(QStringLiteral("appmanifest_%1.acf").arg(appIdStr));
  QFile file(manifestPath);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qDebug() << "Failed to open manifest file:" << manifestPath;
    return QStringLiteral("App %1").arg(appIdStr);
  }

  QTextStream in(&file);
  // Matches lines like: "name" "Cyberpunk 2077" (case-insensitive)
  QRegularExpression nameRegex(QStringLiteral("^\\s*\"name\"\\s+\"([^\"]+)\""),
                               QRegularExpression::CaseInsensitiveOption);

  while (!in.atEnd()) {
    QString line = in.readLine();
    QRegularExpressionMatch match = nameRegex.match(line);
    if (match.hasMatch()) {
      return match.captured(1);
    }
  }

  qDebug() << "Failed to find name field in manifest:" << manifestPath;
  return QStringLiteral("App %1").arg(appIdStr);
}

static QString findGameArtwork(const QString &compatDataPath,
                               const QString &appIdStr) {
  if (appIdStr == QLatin1String("0")) {
    return QString();
  }

  QDir compatDir(compatDataPath);
  if (!compatDir.cdUp() || !compatDir.cdUp()) {
    return QString();
  }

  QDir steamRoot = compatDir;
  if (!steamRoot.cdUp()) {
    return QString();
  }

  QString appCachePath = steamRoot.filePath(
      QStringLiteral("appcache/librarycache/%1").arg(appIdStr));
  QDir appCacheDir(appCachePath);

  if (!appCacheDir.exists()) {
    return QString();
  }

  QStringList filters;
  filters << QStringLiteral("*.jpg") << QStringLiteral("*.png");
  appCacheDir.setNameFilters(filters);

  QFileInfoList list = appCacheDir.entryInfoList(QDir::Files);
  for (const QFileInfo &fileInfo : list) {
    QString baseName =
        fileInfo.completeBaseName(); // filename without extension

    // Steam cache hashes are typically 32 (MD5) or 40 (SHA-1) hex characters
    // long
    if (baseName.length() == 32 || baseName.length() == 40) {
      bool isHex = true;
      for (QChar c : baseName) {
        char ch = c.toLatin1();
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
              (ch >= 'A' && ch <= 'F'))) {
          isHex = false;
          break;
        }
      }
      if (isHex) {
        return fileInfo
            .absoluteFilePath(); // Found the hash-named artwork file!
      }
    }
  }

  return QString();
}

QStringList SteamCompatPluginOverlay::getOverlays(const QUrl &url) {
  if (!url.isLocalFile()) {
    return QStringList();
  }

  const QString localPath = url.toLocalFile();
  if (localPath.isEmpty()) {
    return QStringList();
  }

  QFileInfo fileInfo(localPath);
  if (!fileInfo.isDir()) {
    return QStringList();
  }

  QDir parentDir(fileInfo.absolutePath());
  if (parentDir.dirName() != QStringLiteral("compatdata")) {
    return QStringList(); // Not a direct child of compatdata/
  }

  QString appIdStr = url.fileName();

  QString gameName = getGameNameFromManifest(localPath, appIdStr);
  if (!gameName.isEmpty()) {
    qDebug() << "Parsed Game Title via source-parsers:" << gameName;
  }

  if (appIdStr.length() >= 10 && appIdStr != QLatin1String("0")) {
    return QStringList{QStringLiteral("steam-non-steam")};
  } else {
    QString artworkPath = findGameArtwork(localPath, appIdStr);
    if (!artworkPath.isEmpty()) {
      return QStringList{artworkPath};
    } else {
      return QStringList{QStringLiteral("steam")};
    }
  }

  return QStringList();
}

#include "dolphinoverlayplugin.moc"
