#pragma once

#include <KOverlayIconPlugin>
#include <QStringList>
#include <QUrl>

class SteamCompatOverlayPlugin : public KOverlayIconPlugin {
  Q_OBJECT
public:
  explicit SteamCompatOverlayPlugin(QObject *parent = nullptr,
                                    const QList<QVariant> &args = {});
  QStringList getOverlays(const QUrl &url) override;
};
