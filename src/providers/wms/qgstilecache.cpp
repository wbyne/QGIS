/***************************************************************************
  qgstilecache.h
  --------------------------------------
  Date                 : September 2016
  Copyright            : (C) 2016 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgstilecache.h"

#include "qgsnetworkaccessmanager.h"

#include <QAbstractNetworkCache>
#include <QImage>

QCache<QUrl, QImage> QgsTileCache::sTileCache( 256 );
QMutex QgsTileCache::sTileCacheMutex;


void QgsTileCache::insertTile( const QUrl& url, const QImage& image )
{
  QMutexLocker locker( &sTileCacheMutex );
  sTileCache.insert( url, new QImage( image ) );
}

bool QgsTileCache::tile( const QUrl& url, QImage& image )
{
  QMutexLocker locker( &sTileCacheMutex );
  if ( QImage* i = sTileCache.object( url ) )
  {
    image = *i;
    return true;
  }
  else if ( QgsNetworkAccessManager::instance()->cache()->metaData( url ).isValid() )
  {
    if ( QIODevice* data = QgsNetworkAccessManager::instance()->cache()->data( url ) )
    {
      QByteArray imageData = data->readAll();
      delete data;

      image = QImage::fromData( imageData );

      // cache it as well (mutex is already locked)
      sTileCache.insert( url, new QImage( image ) );

      return true;
    }
  }
  return false;
}
