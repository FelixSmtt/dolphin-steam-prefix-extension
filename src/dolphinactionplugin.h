#pragma once

#include <KAbstractFileItemActionPlugin>

class SteamCompatPluginAction : public KAbstractFileItemActionPlugin {
  Q_OBJECT
public:
  explicit SteamCompatPluginAction(QObject *parent,
                                   const QList<QVariant> &args);
  QList<QAction *> actions(const KFileItemListProperties &fileItemInfos,
                           QWidget *parentWidget) override;
};
