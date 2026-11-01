#pragma once

#include <QDialog>
#include <QStringList>

class QPlainTextEdit;
class QListWidget;
class QLabel;
class QComboBox;
class GrassInterface;
class GrassMapCalcHighlighter;

// Dedicated dialog for r.mapcalc (and v.mapcalc).
//
// Layout:
//   ┌──────────────────────────────────────────────────────┐
//   │  expression editor (multi-line, syntax highlighted)  │
//   ├────────────────────┬─────────────────────────────────┤
//   │  Raster maps in    │  Operator / function buttons    │
//   │  current mapset    │                                 │
//   └────────────────────┴─────────────────────────────────┘
//   │  Output map name:  [_______________]  [Run]  [Close] │
//   └──────────────────────────────────────────────────────┘
class GrassMapCalcDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit GrassMapCalcDialog( GrassInterface *iface = nullptr,
                                  QWidget *parent = nullptr );

  private slots:
    void insertMapName( const QString &name );
    void insertText( const QString &text );
    void runExpression();
    void refreshMapList();

  private:
    void buildOperatorPanel( QWidget *parent );
    QStringList availableRasters() const;

    QPlainTextEdit          *mEditor    = nullptr;
    QListWidget             *mMapList   = nullptr;
    QLabel                  *mStatus    = nullptr;
    GrassInterface          *mIface     = nullptr;
    GrassMapCalcHighlighter *mHighlighter = nullptr;
};
