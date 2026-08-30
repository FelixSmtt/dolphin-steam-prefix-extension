#include "dolphinoverlayplugin.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QIcon>
#include <QMap>
#include <QPixmap>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <QUuid>

#include "steamhelper.h"

SteamCompatPluginOverlay::SteamCompatPluginOverlay(QObject *parent,
                                                   const QList<QVariant> &args)
    : KOverlayIconPlugin(parent) {

  Q_UNUSED(args);
}

static QString findNonSteamGameArtwork(const QString &compatDataPath,
                                       const QString &appIdStr) {
  if (appIdStr == QLatin1String("0")) {
    return QString();
  }

  // 1. Check existing library cache first
  auto cachedArtworkPath =
      loadArtworkFromLibraryCache(compatDataPath, appIdStr);
  if (!cachedArtworkPath.isEmpty()) {
    return cachedArtworkPath;
  }

  // 2. Fetch the specific shortcut node using the new helper
  VdfNode shortcut = loadAppIdShortcutVDFNode(appIdStr);
  if (shortcut.children.isEmpty()) {
    return QString();
  }

  // 3. Check custom icon path
  QString iconKey = QStringLiteral("icon");
  if (shortcut.children.contains(iconKey)) {
    QString iconPath = shortcut.children[iconKey].stringValue;
    if (!iconPath.isEmpty() && QFile::exists(iconPath)) {
      return iconPath;
    }
  }

  // 4. Fallback: Extract from executable and cache it
  QString exeKey = QStringLiteral("Exe");
  if (shortcut.children.contains(exeKey)) {
    QString exePath = shortcut.children[exeKey].stringValue;
    exePath.remove(QLatin1Char('"'));

    QString extractedIcon =
        extractAndCacheExeIcon(compatDataPath, appIdStr, exePath);
    if (!extractedIcon.isEmpty()) {
      return extractedIcon;
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

  if (appIdStr.length() >= 10 && appIdStr != QLatin1String("0")) {
    QString artworkPath = findNonSteamGameArtwork(localPath, appIdStr);
    if (!artworkPath.isEmpty()) {
      return QStringList{artworkPath};
    } else {
      return QStringList{QStringLiteral("steam-non-steam")};
    }
  } else {
    QString artworkPath = loadArtworkFromLibraryCache(localPath, appIdStr);
    if (!artworkPath.isEmpty()) {
      return QStringList{artworkPath};
    } else {
      return QStringList{QStringLiteral("steam")};
    }
  }

  return QStringList();
}

#include "dolphinoverlayplugin.moc"
