#include "dolphinoverlayplugin.h"

#include <KPluginFactory>
#include <QDebug>
#include <QDir>
#include <QRegularExpression>

SteamCompatOverlayPlugin::SteamCompatOverlayPlugin(QObject *parent,
                                                   const QList<QVariant> &args)
    : KOverlayIconPlugin(parent) {
  Q_UNUSED(args);
}

QStringList SteamCompatOverlayPlugin::getOverlays(const QUrl &url) {
  if (!url.isLocalFile()) {
    return QStringList();
  }

  const QString localPath = url.toLocalFile();

  // Match Steam compatdata folders
  QRegularExpression rx(
      QStringLiteral(".*/steamapps/compatdata/(\\d+)(/.*)?$"));
  QRegularExpressionMatch match = rx.match(localPath);

  if (match.hasMatch()) {
    // "steam" will look for the system Steam icon theme badge
    return QStringList{QStringLiteral("steam")};
  }

  return QStringList();
}

K_PLUGIN_CLASS_WITH_JSON(SteamCompatOverlayPlugin, "dolphinoverlayplugin.json")

#include "dolphinoverlayplugin.moc"
