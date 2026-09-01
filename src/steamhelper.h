#pragma once

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QIcon>
#include <QMap>
#include <QPixmap>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QUuid>
#include <cstdint>
#include <cstring>
#include <qlist.h>

struct VdfNode {
  enum Type { TypeObject, TypeString, TypeInt32 };
  Type type = TypeObject;
  QMap<QString, VdfNode> children;
  QString stringValue;
  int32_t intValue = 0;
};

static QList<QString> getShortcutsFilePaths() {
  QString steamUserData =
      QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +
      QStringLiteral("/.local/share/Steam/userdata/");

  QDir userDir(steamUserData);
  if (!userDir.exists()) {
    return QList<QString>();
  }

  QStringList userIds = userDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  if (userIds.isEmpty()) {
    return QList<QString>();
  }

  QList<QString> shortcutPaths;
  shortcutPaths.reserve(userIds.size());

  for (const QString &userId : userIds) {
    QString path =
        userDir.filePath(userId + QStringLiteral("/config/shortcuts.vdf"));
    if (QFileInfo::exists(path)) {
      shortcutPaths.append(path);
    }
  }

  return shortcutPaths;
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

inline VdfNode loadShortcutVDFNode(const QString &shortcutsPath) {
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

inline QList<VdfNode> loadShortcutNodes() {
  QList<QString> shortcutPaths = getShortcutsFilePaths();
  QList<VdfNode> shortcutNodes;

  for (const QString &path : shortcutPaths) {
    VdfNode node = loadShortcutVDFNode(path);
    if (!node.children.isEmpty()) {
      shortcutNodes.append(node);
    }
  }

  return shortcutNodes;
}

inline VdfNode loadAppIdShortcutVDFNode(const QString &appIdStr) {
  QList<VdfNode> root_nodes = loadShortcutNodes();
  if (root_nodes.isEmpty()) {
    return VdfNode();
  }

  QString shortcutsKey = QStringLiteral("shortcuts");
  QString appIdKey = QStringLiteral("appid");

  for (const VdfNode &root : root_nodes) {
    if (!root.children.contains(shortcutsKey)) {
      continue;
    }

    const VdfNode &shortcutsObj = root.children[shortcutsKey];

    for (auto it = shortcutsObj.children.constBegin();
         it != shortcutsObj.children.constEnd(); ++it) {
      const VdfNode &shortcut = it.value();
      if (shortcut.type != VdfNode::TypeObject)
        continue;

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

  if (!appCacheDir.exists()) {
    if (!appCacheDir.mkpath(appCachePath)) {
      return QString();
    }
  }

  QString randomHash = QUuid::createUuid()
                           .toString(QUuid::WithoutBraces)
                           .remove(QLatin1Char('-'));
  QString cachedIconPath =
      appCacheDir.filePath(randomHash + QStringLiteral(".png"));

  QString cleanExePath = exePath;
  cleanExePath.remove(QLatin1Char('"'));

  QProcess wrestoolProc;
  wrestoolProc.start(
      QStringLiteral("wrestool"),
      {QStringLiteral("-x"), QStringLiteral("-t14"), cleanExePath});

  if (!wrestoolProc.waitForStarted(2000) ||
      !wrestoolProc.waitForFinished(3000) ||
      wrestoolProc.exitStatus() != QProcess::NormalExit) {
    return QString();
  }

  QByteArray icoData = wrestoolProc.readAllStandardOutput();
  if (icoData.isEmpty()) {
    return QString();
  }

  QString tempIcoPath =
      appCacheDir.filePath(randomHash + QStringLiteral(".ico"));
  QFile tempIcoFile(tempIcoPath);
  if (!tempIcoFile.open(QIODevice::WriteOnly)) {
    return QString();
  }
  tempIcoFile.write(icoData);
  tempIcoFile.close();

  QProcess icotoolProc;
  icotoolProc.start(
      QStringLiteral("icotool"),
      {QStringLiteral("-x"), QStringLiteral("-o"), appCachePath, tempIcoPath});

  if (!icotoolProc.waitForStarted(2000) || !icotoolProc.waitForFinished(3000)) {
    QFile::remove(tempIcoPath);
    return QString();
  }

  QString filterPattern = randomHash + QStringLiteral("*.png");
  QStringList pngFiles =
      appCacheDir.entryList(QStringList() << filterPattern, QDir::Files);

  QString bestPng;
  int maxResolution = 0;
  for (const QString &pngFile : pngFiles) {
    QString fullPngPath = appCacheDir.filePath(pngFile);
    if (fullPngPath == cachedIconPath)
      continue;

    int res = 0;
    if (pngFile.contains(QStringLiteral("256x256")))
      res = 256;
    else if (pngFile.contains(QStringLiteral("128x128")))
      res = 128;
    else if (pngFile.contains(QStringLiteral("64x64")))
      res = 64;
    else if (pngFile.contains(QStringLiteral("48x48")))
      res = 48;
    else if (pngFile.contains(QStringLiteral("32x32")))
      res = 32;
    else
      res = 16;

    if (res >= maxResolution) {
      maxResolution = res;
      bestPng = fullPngPath;
    }
  }

  QFile::remove(tempIcoPath);

  if (bestPng.isEmpty()) {
    qWarning() << "[extractAndCacheExeIcon] FAIL: No matching resolution PNG "
                  "files found for AppID:"
               << appIdStr;
    return QString();
  }

  if (QFile::exists(cachedIconPath)) {
    QFile::remove(cachedIconPath);
  }

  if (!QFile::rename(bestPng, cachedIconPath)) {
    qWarning() << "[extractAndCacheExeIcon] FAIL: Failed to rename extracted "
                  "PNG icon for AppID:"
               << appIdStr;
    return QString();
  }

  // Clean up residue resolution layers generated by icotool
  for (const QString &pngFile : pngFiles) {
    QString fullPngPath = appCacheDir.filePath(pngFile);
    if (fullPngPath != cachedIconPath) {
      QFile::remove(fullPngPath);
    }
  }

  return cachedIconPath;
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

  if (appIdStr.length() < 10) {
    return getGameNameFromManifest(compatDataPath, appIdStr);
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
  return QStringLiteral("Unknown Application:  %1").arg(appIdStr);
}

inline QString loadNonSteamGameArtwork(const QString &compatDataPath,
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

inline QStringList loadGameArtwork(const QString &compatDataPath,
                                   const QString &appIdStr) {
  if (appIdStr.length() >= 10 && appIdStr != QLatin1String("0")) {
    QString artworkPath = loadNonSteamGameArtwork(compatDataPath, appIdStr);
    if (!artworkPath.isEmpty()) {
      return QStringList{artworkPath};
    } else {
      return QStringList{QStringLiteral("steam-non-steam")};
    }
  } else {
    QString artworkPath = loadArtworkFromLibraryCache(compatDataPath, appIdStr);
    if (!artworkPath.isEmpty()) {
      return QStringList{artworkPath};
    } else {
      return QStringList{QStringLiteral("steam")};
    }
  }
}
