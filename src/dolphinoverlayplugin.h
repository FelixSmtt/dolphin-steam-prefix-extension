#pragma once

#include <KOverlayIconPlugin>
#include <QStringList>
#include <QUrl>
#include <QVariant>

class SteamCompatPluginOverlay : public KOverlayIconPlugin {
  Q_PLUGIN_METADATA(IID "de.steamcompat.ovarlayiconplugin")
  Q_OBJECT
public:
  explicit SteamCompatPluginOverlay(QObject *parent = nullptr,
                                    const QList<QVariant> &args = {});
  QStringList getOverlays(const QUrl &url) override;
};
