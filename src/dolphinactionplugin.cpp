#include <KAbstractFileItemActionPlugin>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KPluginFactory>

#include <QAction>
#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QRegularExpression>
#include <QUrl>
#include <QWidget>

#include "dolphinactionplugin.h"

K_PLUGIN_CLASS_WITH_JSON(SteamCompatPluginAction, "dolphinactionplugin.json")

SteamCompatPluginAction::SteamCompatPluginAction(QObject *parent,
                                                 const QList<QVariant> &)
    : KAbstractFileItemActionPlugin(parent) {}

QString getGameNameForId(const QString &compatId) {
  QString cachePath =
      QDir::home().filePath(QStringLiteral(".cache/steam-game-names.json"));
  QFile file(cachePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return QString();
  }

  QByteArray data = file.readAll();
  QJsonDocument doc = QJsonDocument::fromJson(data);
  if (!doc.isObject()) {
    return QString();
  }

  QJsonObject obj = doc.object();
  if (obj.contains(compatId)) {
    return obj.value(compatId).toString();
  }

  return QString();
}

QList<QAction *>
SteamCompatPluginAction::actions(const KFileItemListProperties &fileItemInfos,
                                 QWidget *parentWidget) {
  qDebug() << "SteamCompatPluginAction::actions() called! Local:"
           << fileItemInfos.isLocal();

  if (!fileItemInfos.isLocal())
    return {};

  const QList<QUrl> urls = fileItemInfos.urlList();
  if (urls.isEmpty())
    return {};

  QList<QAction *> generatedActions;

  // Universal debug action
  auto debugAction = new QAction(QStringLiteral("[DEBUG] Test Context Action"),
                                 static_cast<QObject *>(parentWidget));
  connect(debugAction, &QAction::triggered, [urls]() {
    for (const auto &u : urls) {
      qDebug() << "Debug action clicked for path:" << u.toLocalFile();
    }
  });
  generatedActions.append(debugAction);

  for (const auto &url : urls) {
    QString localPath = url.toLocalFile();
    qDebug() << "Checking path:" << localPath;

    QRegularExpression rx(
        QStringLiteral(".*/steamapps/compatdata/(\\d+)(/.*)?$"));
    QRegularExpressionMatch match = rx.match(localPath);

    if (match.hasMatch()) {
      QString compatId = match.captured(1);
      qDebug() << "Matched compat ID:" << compatId;

      QString gameName = getGameNameForId(compatId);
      qDebug() << "Retrieved game name from cache:" << gameName;

      if (gameName.isEmpty()) {
        gameName =
            QString(QStringLiteral("Unknown Game (ID: %1)")).arg(compatId);
      }

      QString actionText =
          QString(QStringLiteral("Steam Game: %1")).arg(gameName);
      auto action =
          new QAction(actionText, static_cast<QObject *>(parentWidget));

      connect(action, &QAction::triggered, [localPath]() {
        QGuiApplication::clipboard()->setText(localPath);
      });

      generatedActions.append(action);
    } else {
      qDebug() << "Skipped: Path did not match regex.";
    }
  }

  qDebug() << "Total actions returning to Dolphin:" << generatedActions.size();
  return generatedActions;
}

#include "dolphinactionplugin.moc"
