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

  // Debug print to see every item Dolphin asks about
  // qDebug() << "Checking URL for overlay:" << localPath;

  // Match Steam compatdata folders
  QRegularExpression rx(
      QStringLiteral(".*/steamapps/compatdata/(\\d+)(/.*)?$"));
  QRegularExpressionMatch match = rx.match(localPath);

  if (match.hasMatch()) {
    qDebug() << "MATCH FOUND! Showing Steam overlay for:" << localPath;
    return QStringList{QStringLiteral("steam")};
  }

  return QStringList();
}

#include "dolphinoverlayplugin.moc"
