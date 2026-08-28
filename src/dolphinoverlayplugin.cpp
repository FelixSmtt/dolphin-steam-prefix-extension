#include "dolphinoverlayplugin.h"

#include <KPluginFactory>
#include <QDebug>
#include <QDir>
#include <QRegularExpression>

SteamCompatPluginOverlay::SteamCompatPluginOverlay(QObject *parent,
                                                   const QList<QVariant> &args)
    : KOverlayIconPlugin(parent) {

  Q_UNUSED(args);
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
