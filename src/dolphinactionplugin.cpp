#include <KAbstractFileItemActionPlugin>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KPluginFactory>

#include <QAction>
#include <QDebug>
#include <QList>
#include <QString>
#include <QWidget>

#include "dolphinactionplugin.h"
#include "steamhelper.h"

K_PLUGIN_CLASS_WITH_JSON(SteamCompatPluginAction, "dolphinactionplugin.json")

void OpenInSteam(const QString &compatId) {
  QString steamUrl =
      QStringLiteral("steam://nav/games/details/%1").arg(compatId);
  QDesktopServices::openUrl(QUrl(steamUrl));
}

void OpenInDolphin(const QString &targetFolder) {
  QProcess::startDetached(QStringLiteral("dolphin"),
                          {QStringLiteral("--new-window"), targetFolder});
}

SteamCompatPluginAction::SteamCompatPluginAction(QObject *parent,
                                                 const QList<QVariant> &)
    : KAbstractFileItemActionPlugin(parent) {}
QList<QAction *>
SteamCompatPluginAction::actions(const KFileItemListProperties &fileItemInfos,
                                 QWidget *parentWidget) {
  if (!fileItemInfos.isLocal())
    return {};

  const QList<QUrl> urls = fileItemInfos.urlList();
  if (urls.isEmpty())
    return {};

  QList<QAction *> generatedActions;

  for (const auto &url : urls) {
    QString localPath = url.toLocalFile();

    QRegularExpression rx(
        QStringLiteral(".*/steamapps/compatdata/(\\d+)(/.*)?$"));
    QRegularExpressionMatch match = rx.match(localPath);

    if (match.hasMatch()) {
      QString compatId = match.captured(1);

      QString gameName = getGameName(localPath, compatId);
      if (gameName.isEmpty()) {
        gameName =
            QString(QStringLiteral("Unknown Game (ID: %1)")).arg(compatId);
      }

      bool isNative =
          (compatId.length() < 10 && compatId != QLatin1String("0"));

      QStringList iconPaths = loadGameArtwork(localPath, compatId);
      QIcon actionIcon;
      if (!iconPaths.isEmpty()) {
        QString primaryPath = iconPaths.first();
        if (QFile::exists(primaryPath)) {
          actionIcon = QIcon(primaryPath);
        } else {
          actionIcon = QIcon::fromTheme(primaryPath);
        }
      }

      if (isNative) {
        // Native Steam game: Open in Steam
        QString actionText =
            QString(QStringLiteral("Open in Steam: %1")).arg(gameName);
        auto action = new QAction(actionIcon, actionText,
                                  static_cast<QObject *>(parentWidget));

        connect(action, &QAction::triggered,
                [compatId]() { OpenInSteam(compatId); });

        generatedActions.append(action);

      } else {
        // Non-Steam game: Open local game folder
        VdfNode shortcut = loadAppIdShortcutVDFNode(compatId);
        QString targetFolder;

        if (!shortcut.children.isEmpty()) {
          if (shortcut.children.contains(QStringLiteral("StartDir"))) {
            targetFolder =
                shortcut.children[QStringLiteral("StartDir")].stringValue;
            targetFolder.remove(QLatin1Char('"'));
          }
          if (targetFolder.isEmpty() &&
              shortcut.children.contains(QStringLiteral("Exe"))) {
            QString exe = shortcut.children[QStringLiteral("Exe")].stringValue;
            exe.remove(QLatin1Char('"'));
            targetFolder = QFileInfo(exe).absolutePath();
          }
        }

        if (targetFolder.isEmpty() || !QDir(targetFolder).exists()) {
          continue; // Skip action creation if the target folder is invalid
        }

        QString actionText =
            QString(QStringLiteral("Open Game Folder: %1")).arg(gameName);
        auto action = new QAction(actionIcon, actionText,
                                  static_cast<QObject *>(parentWidget));

        connect(action, &QAction::triggered,
                [targetFolder]() { OpenInDolphin(targetFolder); });

        generatedActions.append(action);
      }
    }
  }

  qDebug() << "Total actions returning to Dolphin:" << generatedActions.size();
  return generatedActions;
}

#include "dolphinactionplugin.moc"
