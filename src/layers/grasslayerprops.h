#pragma once

#include <QDialog>
#include <QStandardItemModel>

class GrassMapCanvas;

// -----------------------------------------------------------------------
// GrassRasterPropsDialog
// Sidebar tabs: Metadata | Symbology | Rendering | Histogram | Null values | Categories
// -----------------------------------------------------------------------
class GrassRasterPropsDialog : public QDialog
{
    Q_OBJECT
  public:
    explicit GrassRasterPropsDialog( const QString &name,
                                      const QString &mapset,
                                      GrassMapCanvas *canvas,
                                      QWidget *parent = nullptr );
  private:
    void buildSymbologyTab( QWidget *tab, const QString &name,
                             const QString &mapset );
    void buildRenderingTab( QWidget *tab, const QString &name,
                             const QString &mapset );
    void buildHistogramTab( QWidget *tab, const QString &name,
                             const QString &mapset );
    void buildNullTab( QWidget *tab, const QString &name,
                       const QString &mapset );
    void buildCategoriesTab( QWidget *tab, const QString &name,
                              const QString &mapset );
    void buildMetadataTab( QWidget *tab, const QString &name,
                            const QString &mapset );

    // Run `r.info [flags] map=X` and return stdout
    QString rInfo( const QString &name, const QString &mapset,
                   const QStringList &flags = {} ) const;

    GrassMapCanvas *mCanvas;
};

// -----------------------------------------------------------------------
// GrassVectorPropsDialog
// Sidebar tabs: Metadata | Symbology | Fields
// -----------------------------------------------------------------------
class GrassVectorPropsDialog : public QDialog
{
    Q_OBJECT
  public:
    explicit GrassVectorPropsDialog( const QString &name,
                                      const QString &mapset,
                                      GrassMapCanvas *canvas,
                                      QWidget *parent = nullptr );
  private:
    void buildSymbologyTab(  QWidget *tab, const QString &name,
                              const QString &mapset );
    void buildFieldsTab(     QWidget *tab, const QString &name,
                              const QString &mapset );
    void buildMetadataTab(   QWidget *tab, const QString &name,
                              const QString &mapset );

    QString vInfo( const QString &name, const QString &mapset,
                   const QStringList &flags = {} ) const;

    GrassMapCanvas *mCanvas;
};
