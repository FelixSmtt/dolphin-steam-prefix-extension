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

#include <cstdint>
#include <cstring>

struct VdfNode {
  enum Type { TypeObject, TypeString, TypeInt32 };
  Type type = TypeObject;
  QMap<QString, VdfNode> children;
  QString stringValue;
  int32_t intValue = 0;
};

// Recursive binary VDF stream parser matching Valve's specification
static bool parseBinaryVdf(const char *&ptr, const char *end, VdfNode &outObj) {
  outObj.type = VdfNode::TypeObject;
  while (ptr < end) {
    uint8_t typeByte = static_cast<uint8_t>(*ptr++);
    if (typeByte == 0x08) { // End of current object block
      return true;
    }

    // Read key name (null-terminated string)
    const char *keyStart = ptr;
    while (ptr < end && *ptr != '\0') {
      ptr++;
    }
    if (ptr >= end)
      return false;
    QString key = QString::fromUtf8(keyStart, ptr - keyStart);
    ptr++; // skip null terminator

    VdfNode val;
    if (typeByte == 0x00) {
      // Nested object
      if (!parseBinaryVdf(ptr, end, val)) {
        return false;
      }
      outObj.children[key] = val;
    } else if (typeByte == 0x01) {
      // Null-terminated string value
      const char *valStart = ptr;
      while (ptr < end && *ptr != '\0') {
        ptr++;
      }
      if (ptr >= end)
        return false;
      val.type = VdfNode::TypeString;
      val.stringValue = QString::fromUtf8(valStart, ptr - valStart);
      ptr++; // skip null terminator
      outObj.children[key] = val;
    } else if (typeByte == 0x02) {
      // 4-byte little-endian signed integer
      if (ptr + 4 > end)
        return false;
      int32_t intVal = 0;
      std::memcpy(&intVal, ptr, sizeof(int32_t));
      ptr += 4;
      val.type = VdfNode::TypeInt32;
      val.intValue = intVal;
      outObj.children[key] = val;
    } else {
      // Unknown or unsupported type byte encountered
      return false;
    }
  }
  return true;
}

SteamCompatPluginOverlay::SteamCompatPluginOverlay(QObject *parent,
                                                   const QList<QVariant> &args)
    : KOverlayIconPlugin(parent) {

  Q_UNUSED(args);
}

static QString getSteamRootPath(const QString &compatDataPath) {
  QDir compatDir(compatDataPath);
  if (!compatDir.cdUp() || !compatDir.cdUp()) {
    return QString();
  }
  QDir steamRoot = compatDir;
  if (!steamRoot.cdUp()) {
    return QString();
  }
  return steamRoot.absolutePath();
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

static QString loadArtworkFromLibraryCache(const QString &compatDataPath,
                                           const QString &appIdStr) {
  if (appIdStr == QLatin1String("0")) {
    return QString();
  }

  QString steamRootPath = getSteamRootPath(compatDataPath);
  if (steamRootPath.isEmpty()) {
    return QString();
  }

  QString appCachePath =
      QDir(steamRootPath)
          .filePath(QStringLiteral("appcache/librarycache/%1").arg(appIdStr));
  QDir appCacheDir(appCachePath);

  if (!appCacheDir.exists()) {
    return QString();
  }

  QStringList filters;
  filters << QStringLiteral("*.jpg") << QStringLiteral("*.png");
  appCacheDir.setNameFilters(filters);

  QFileInfoList list = appCacheDir.entryInfoList(QDir::Files);
  for (const QFileInfo &fileInfo : list) {
    QString baseName = fileInfo.completeBaseName();
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
        return fileInfo.absoluteFilePath();
      }
    }
  }

  return QString();
}

static VdfNode loadShortcutVDFNode() {
  QString shortcutsPath = getShortcutsFilePath();
  if (shortcutsPath.isEmpty()) {
    return VdfNode();
  }
  qDebug() << "Looking for non-Steam game artwork in:" << shortcutsPath;

  QFile file(shortcutsPath);
  if (!file.open(QIODevice::ReadOnly)) {
    return VdfNode();
  }

  QByteArray data = file.readAll();
  if (data.isEmpty()) {
    return VdfNode();
  }

  const char *ptr = data.constData();
  const char *end = ptr + data.size();

  VdfNode root;
  if (!parseBinaryVdf(ptr, end, root)) {
    qDebug() << "Failed to parse binary shortcuts.vdf structure.";
    return VdfNode();
  }

  return root;
}

static QString extractAndCacheExeIcon(const QString &compatDataPath,
                                      const QString &appIdStr,
                                      const QString &exePath) {
  if (exePath.isEmpty() || !QFile::exists(exePath)) {
    return QString();
  }

  QString steamRootPath = getSteamRootPath(compatDataPath);
  if (steamRootPath.isEmpty()) {
    return QString();
  }

  QString appCachePath =
      QDir(steamRootPath)
          .filePath(QStringLiteral("appcache/librarycache/%1").arg(appIdStr));
  QDir appCacheDir(appCachePath);

  QFileIconProvider iconProvider;
  QIcon systemIcon = iconProvider.icon(QFileInfo(exePath));
  if (systemIcon.isNull()) {
    return QString();
  }

  if (!appCacheDir.exists()) {
    appCacheDir.mkpath(appCachePath);
  }

  QString randomHash = QUuid::createUuid()
                           .toString(QUuid::WithoutBraces)
                           .remove(QLatin1Char('-'));
  QString cachedIconPath =
      appCacheDir.filePath(randomHash + QStringLiteral(".png"));

  QPixmap pixmap = systemIcon.pixmap(256, 256);
  if (pixmap.save(cachedIconPath, "PNG")) {
    qDebug() << "Cached extracted executable icon to librarycache:"
             << cachedIconPath;
    return cachedIconPath;
  }

  return QString();
}

static VdfNode loadAppIdShortcutVDFNode(const QString &appIdStr) {
  VdfNode root = loadShortcutVDFNode();
  if (root.children.isEmpty()) {
    qDebug() << "Failed to load or parse shortcuts.vdf.";
    return VdfNode();
  }

  QString shortcutsKey = QStringLiteral("shortcuts");
  if (!root.children.contains(shortcutsKey)) {
    qDebug() << "No 'shortcuts' key found in root of binary VDF.";
    return VdfNode();
  }

  const VdfNode &shortcutsObj = root.children[shortcutsKey];

  for (auto it = shortcutsObj.children.constBegin();
       it != shortcutsObj.children.constEnd(); ++it) {
    const VdfNode &shortcut = it.value();
    if (shortcut.type != VdfNode::TypeObject)
      continue;

    QString appIdKey = QStringLiteral("appid");
    if (shortcut.children.contains(appIdKey)) {
      const VdfNode &appIdNode = shortcut.children[appIdKey];
      if (appIdNode.type == VdfNode::TypeInt32) {
        uint32_t unsignedAppId = static_cast<uint32_t>(appIdNode.intValue);
        QString shortcutAppIdStr = QString::number(unsignedAppId);

        if (shortcutAppIdStr == appIdStr) {
          qDebug() << "Found matching non-Steam shortcut entry for App ID:"
                   << appIdStr;
          return shortcut; // Return the matched shortcut node directly
        }
      }
    }
  }

  return VdfNode();
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

  QString gameName = getGameNameFromManifest(localPath, appIdStr);
  if (!gameName.isEmpty()) {
    qDebug() << "Parsed Game Title via source-parsers:" << gameName;
  }

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
