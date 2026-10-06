#pragma once
#include <QObject>
#include <QWidget>
void checkForUpdates(QObject *owner, QWidget *parent, bool automatic);

QByteArray installerChecksum(const QByteArray &manifest, const QString &filename);
