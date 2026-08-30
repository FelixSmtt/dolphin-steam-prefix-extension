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
  return loadGameArtwork(localPath, appIdStr);
}

#include "dolphinoverlayplugin.moc"
