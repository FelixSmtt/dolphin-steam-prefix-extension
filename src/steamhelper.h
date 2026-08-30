#pragma once

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QIcon>
#include <QMap>
#include <QPixmap>
#include <QStandardPaths>
#include <QString>
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

inline bool parseBinaryVdf(const char *&ptr, const char *end, VdfNode &outObj) {
  outObj.type = VdfNode::TypeObject;
  while (ptr < end) {
    uint8_t typeByte = static_cast<uint8_t>(*ptr++);
    if (typeByte == 0x08) {
      return true;
    }

    const char *keyStart = ptr;
    while (ptr < end && *ptr != '\0') {
      ptr++;
    }
    if (ptr >= end)
      return false;
    QString key = QString::fromUtf8(keyStart, ptr - keyStart);
    ptr++;

    VdfNode val;
    if (typeByte == 0x00) {
      if (!parseBinaryVdf(ptr, end, val)) {
        return false;
      }
      outObj.children[key] = val;
    } else if (typeByte == 0x01) {
      const char *valStart = ptr;
      while (ptr < end && *ptr != '\0') {
        ptr++;
      }
      if (ptr >= end)
        return false;
      val.type = VdfNode::TypeString;
      val.stringValue = QString::fromUtf8(valStart, ptr - valStart);
      ptr++;
      outObj.children[key] = val;
    } else if (typeByte == 0x02) {
      if (ptr + 4 > end)
        return false;
      int32_t intVal = 0;
      std::memcpy(&intVal, ptr, sizeof(int32_t));
      ptr += 4;
      val.type = VdfNode::TypeInt32;
      val.intValue = intVal;
      outObj.children[key] = val;
    } else {
      return false;
    }
  }
  return true;
}

inline QString getSteamRootPath(const QString &compatDataPath) {
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

inline QString loadArtworkFromLibraryCache(const QString &compatDataPath,
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

inline VdfNode loadShortcutVDFNode() {
  QString shortcutsPath = getShortcutsFilePath();
  if (shortcutsPath.isEmpty()) {
    return VdfNode();
  }

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
    return VdfNode();
  }

  return root;
}

inline VdfNode loadAppIdShortcutVDFNode(const QString &appIdStr) {
  VdfNode root = loadShortcutVDFNode();
  if (root.children.isEmpty()) {
    return VdfNode();
  }

  QString shortcutsKey = QStringLiteral("shortcuts");
  if (!root.children.contains(shortcutsKey)) {
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
        if (QString::number(unsignedAppId) == appIdStr) {
          return shortcut;
        }
      }
    }
  }

  return VdfNode();
}

inline QString extractAndCacheExeIcon(const QString &compatDataPath,
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
    return cachedIconPath;
  }

  return QString();
}

inline QString getGameNameFromManifest(const QString &compatDataPath,
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
    return QStringLiteral("App %1").arg(appIdStr);
  }

  QTextStream in(&file);
  QRegularExpression nameRegex(QStringLiteral("^\\s*\"name\"\\s+\"([^\"]+)\""),
                               QRegularExpression::CaseInsensitiveOption);

  while (!in.atEnd()) {
    QString line = in.readLine();
    QRegularExpressionMatch match = nameRegex.match(line);
    if (match.hasMatch()) {
      return match.captured(1);
    }
  }

  return QStringLiteral("App %1").arg(appIdStr);
}

inline QString getGameName(const QString &compatDataPath,
                           const QString &appIdStr) {
  if (appIdStr == QLatin1String("0")) {
    return QStringLiteral("Default Steam Prefix");
  }

  // 1. Try checking if it's a native Steam game manifest
  QDir dir(compatDataPath);
  if (dir.cdUp() && dir.cdUp()) {
    QString manifestPath =
        dir.filePath(QStringLiteral("appmanifest_%1.acf").arg(appIdStr));
    if (QFile::exists(manifestPath)) {
      return getGameNameFromManifest(compatDataPath, appIdStr);
    }
  }

  // 2. Otherwise, look for it inside shortcuts.vdf as a non-Steam game
  VdfNode shortcut = loadAppIdShortcutVDFNode(appIdStr);
  if (!shortcut.children.isEmpty()) {
    QString appNameKey = QStringLiteral("AppName");
    if (shortcut.children.contains(appNameKey)) {
      QString name = shortcut.children[appNameKey].stringValue;
      if (!name.isEmpty()) {
        return name;
      }
    }
  }

  // 3. Fallback generic name if nothing matches
  return QStringLiteral("App %1").arg(appIdStr);
}
