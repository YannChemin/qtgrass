#include "grasslayerprops.h"

#include "../canvas/grassmapcanvas.h"
#include "../canvas/grassosmcarto.h"
#include "../canvas/grassrastermaplayer.h"
#include "../canvas/grassvectormaplayer.h"
#include "../colors/grasscolortabledialog.h"
#include "../grassenv.h"
#include "../profile/grasshistogramdialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QLoggingCategory>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QFrame>
#include <QListWidget>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcLayerProps, "qtgrass.layer.props")

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------
static QWidget *scrollWrap( QWidget *inner )
{
    auto *scroll = new QScrollArea();
    scroll->setWidget( inner );
    scroll->setWidgetResizable( true );
    scroll->setFrameShape( QFrame::NoFrame );
    return scroll;
}

static QString runModule( const QString &prog,
                           const QStringList &args )
{
    const QString gb = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    if ( gb.isEmpty() ) return "(GISBASE not set)";

    QProcess p;
    p.setProcessChannelMode( QProcess::SeparateChannels );
    QProcessEnvironment env = GrassEnv::instance()->buildSubprocessEnv();
    p.setProcessEnvironment( env );

    const QString binary = gb + "/bin/" + prog;
    qCInfo(lcLayerProps) << "runModule:" << binary << args;
    p.start( binary, args );

    if ( !p.waitForStarted( 5000 ) )
        return QString("(failed to start %1: %2)").arg(prog).arg(p.errorString());

    if ( !p.waitForFinished( 15000 ) )
    {
        p.kill();
        return QString("(timeout running %1)").arg(prog);
    }

    const QString out = QString::fromLocal8Bit( p.readAllStandardOutput() );
    const QString err = QString::fromLocal8Bit( p.readAllStandardError() );

    if ( !err.isEmpty() )
        qCWarning(lcLayerProps) << "runModule" << prog << "stderr:" << err.left(500);

    if ( out.isEmpty() && !err.isEmpty() )
        return err;   // show the error text so the user knows what went wrong

    return out;
}

// ---------------------------------------------------------------------------
// Metadata helpers
// ---------------------------------------------------------------------------

// Add a themed group box with key-value rows from a JSON object.
// `keys` is an ordered list of {jsonKey, humanLabel} pairs; null/empty values
// are omitted so the group box collapses cleanly when data is absent.
static QGroupBox *jsonGroup( const QString &title,
                              const QJsonObject &obj,
                              const QVector<QPair<QString,QString>> &keys,
                              QWidget *parent )
{
    auto *box  = new QGroupBox( title, parent );
    auto *form = new QFormLayout( box );
    form->setLabelAlignment( Qt::AlignRight | Qt::AlignVCenter );
    form->setContentsMargins( 8, 4, 8, 6 );

    for ( const auto &[k, lbl] : keys )
    {
        const QJsonValue v = obj.value( k );
        if ( v.isUndefined() || v.isNull() ) continue;
        QString text;
        if ( v.isBool() )      text = v.toBool() ? "yes" : "no";
        else if ( v.isDouble() ) text = QString::number( v.toDouble(), 'g', 12 );
        else                     text = v.toString().trimmed();
        if ( text.isEmpty() ) continue;
        auto *valLbl = new QLabel( text, box );
        valLbl->setTextInteractionFlags( Qt::TextSelectableByMouse );
        valLbl->setWordWrap( true );
        form->addRow( "<b>" + lbl + "</b>", valLbl );
    }
    return box;
}

// Build a QTableWidget from a JSON array of column objects (v.info -c).
static QTableWidget *columnsTable( const QJsonArray &cols, QWidget *parent )
{
    auto *tbl = new QTableWidget( 0, 3, parent );
    tbl->setHorizontalHeaderLabels( { "Column", "SQL type", "Numeric" } );
    tbl->horizontalHeader()->setStretchLastSection( false );
    tbl->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::Stretch );
    tbl->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    tbl->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
    tbl->verticalHeader()->setVisible( false );
    tbl->setEditTriggers( QAbstractItemView::NoEditTriggers );
    tbl->setSelectionBehavior( QAbstractItemView::SelectRows );
    tbl->setAlternatingRowColors( true );
    tbl->setMinimumHeight( 120 );

    for ( const QJsonValue &v : cols )
    {
        const QJsonObject col = v.toObject();
        const int row = tbl->rowCount();
        tbl->insertRow( row );
        tbl->setItem( row, 0, new QTableWidgetItem( col.value("name").toString() ) );
        tbl->setItem( row, 1, new QTableWidgetItem( col.value("sql_type").toString() ) );
        tbl->setItem( row, 2, new QTableWidgetItem(
            col.value("is_number").toBool() ? "yes" : "no" ) );
    }
    return tbl;
}

// ============================================================
// GrassRasterPropsDialog
// ============================================================
GrassRasterPropsDialog::GrassRasterPropsDialog( const QString &name,
                                                  const QString &mapset,
                                                  GrassMapCanvas *canvas,
                                                  QWidget *parent )
    : QDialog( parent ), mCanvas( canvas )
{
    const QString mapRef = mapset.isEmpty() ? name : name + "@" + mapset;
    setWindowTitle( "Raster Layer Properties — " + mapRef );
    setMinimumSize( 640, 480 );

    auto *root = new QVBoxLayout( this );

    // ── Vertical sidebar + stacked pages (QGIS-style) ────────────────────────
    auto *body   = new QHBoxLayout();
    auto *sidebar = new QListWidget( this );
    auto *stack   = new QStackedWidget( this );

    sidebar->setFixedWidth( 110 );
    sidebar->setFrameShape( QFrame::NoFrame );
    sidebar->setSpacing( 2 );

    body->addWidget( sidebar );
    body->addWidget( stack, 1 );
    root->addLayout( body, 1 );

    auto addPage = [&]( const QString &title, QWidget *page ) {
        sidebar->addItem( title );
        stack->addWidget( page );
    };

    connect( sidebar, &QListWidget::currentRowChanged,
             stack,   &QStackedWidget::setCurrentIndex );

    // -- Metadata (replaces General; first = default) --
    auto *metaWidget = new QWidget();
    buildMetadataTab( metaWidget, name, mapset );
    addPage( "Metadata", scrollWrap( metaWidget ) );

    // -- Symbology --
    auto *symWidget = new QWidget();
    buildSymbologyTab( symWidget, name, mapset );
    addPage( "Symbology", symWidget );

    // -- Rendering --
    auto *renWidget = new QWidget();
    buildRenderingTab( renWidget, name, mapset );
    addPage( "Rendering", renWidget );

    // -- Histogram --
    auto *histWidget = new QWidget();
    buildHistogramTab( histWidget, name, mapset );
    addPage( "Histogram", histWidget );

    // -- Null values --
    auto *nullWidget = new QWidget();
    buildNullTab( nullWidget, name, mapset );
    addPage( "Null values", nullWidget );

    // -- Categories --
    auto *catWidget = new QWidget();
    buildCategoriesTab( catWidget, name, mapset );
    addPage( "Categories", catWidget );

    sidebar->setCurrentRow( 0 );

    auto *btns = new QDialogButtonBox( QDialogButtonBox::Close, this );
    connect( btns, &QDialogButtonBox::rejected, this, &QDialog::accept );
    root->addWidget( btns );
}


void GrassRasterPropsDialog::buildSymbologyTab( QWidget *tab,
                                                 const QString &name,
                                                 const QString &mapset )
{
    QVBoxLayout *lay = new QVBoxLayout( tab );

    // Embed GrassColorTableDialog as a child widget.
    // setWindowFlags(Qt::Widget) hides any widget — call show() afterwards.
    auto *colorDlg = new GrassColorTableDialog( name, mapset, mCanvas, tab );
    colorDlg->setWindowFlags( Qt::Widget );
    colorDlg->show();
    lay->addWidget( colorDlg, 1 );
}

void GrassRasterPropsDialog::buildHistogramTab( QWidget *tab,
                                                 const QString &name,
                                                 const QString &mapset )
{
    QVBoxLayout *lay = new QVBoxLayout( tab );
    const QString mapRef = mapset.isEmpty() ? name : name + "@" + mapset;
    // Embed GrassHistogramDialog contents inline
    auto *hist = new GrassHistogramDialog( name, mapset, tab );
    hist->setWindowFlags( Qt::Widget );
    lay->addWidget( hist, 1 );
}

void GrassRasterPropsDialog::buildMetadataTab( QWidget *tab,
                                                const QString &name,
                                                const QString &mapset )
{
    auto *lay = new QVBoxLayout( tab );
    lay->setSpacing( 6 );

    const QString raw = rInfo( name, mapset, { "format=json" } );
    const QJsonDocument doc = QJsonDocument::fromJson( raw.toUtf8() );
    if ( doc.isNull() || !doc.isObject() )
    {
        // JSON not supported — fall back to plain text
        auto *b = new QTextBrowser( tab );
        b->setFont( QFont("Monospace", 9) );
        b->setPlainText( raw.isEmpty() ? "(no output from r.info)" : raw );
        lay->addWidget( b );
        return;
    }

    const QJsonObject obj = doc.object();

    // ── Map Info ─────────────────────────────────────────────────────────────
    lay->addWidget( jsonGroup( "Map Info", obj, {
        { "map",            "Map"            },
        { "maptype",        "Map type"       },
        { "mapset",         "Mapset"         },
        { "project",        "Project"        },
        { "database",       "Database"       },
        { "title",          "Title"          },
        { "date",           "Created"        },
        { "creator",        "Creator"        },
        { "timestamp",      "Timestamp"      },
    }, tab ) );

    // ── Extent & Resolution ───────────────────────────────────────────────────
    lay->addWidget( jsonGroup( "Extent & Resolution", obj, {
        { "north",  "North"       },
        { "south",  "South"       },
        { "east",   "East"        },
        { "west",   "West"        },
        { "nsres",  "NS res"      },
        { "ewres",  "EW res"      },
        { "rows",   "Rows"        },
        { "cols",   "Columns"     },
        { "cells",  "Cells"       },
    }, tab ) );

    // ── Data & Statistics ─────────────────────────────────────────────────────
    lay->addWidget( jsonGroup( "Data & Statistics", obj, {
        { "datatype",  "Data type"     },
        { "ncats",     "Categories"    },
        { "min",       "Minimum"       },
        { "max",       "Maximum"       },
        { "n",         "Valid cells"   },
        { "mean",      "Mean"          },
        { "stddev",    "Std. deviation"},
        { "sum",       "Sum"           },
    }, tab ) );

    // ── Description ───────────────────────────────────────────────────────────
    lay->addWidget( jsonGroup( "Description", obj, {
        { "units",          "Units"          },
        { "vdatum",         "Vertical datum" },
        { "semantic_label", "Semantic label" },
        { "description",    "Description"    },
        { "source1",        "Source 1"       },
        { "source2",        "Source 2"       },
        { "comments",       "Comments"       },
    }, tab ) );

    lay->addStretch();
}

void GrassRasterPropsDialog::buildNullTab( QWidget *tab,
                                            const QString &name,
                                            const QString &mapset )
{
    const QString mapRef = mapset.isEmpty() ? name : name + "@" + mapset;
    auto *root = new QVBoxLayout( tab );
    root->setSpacing( 10 );

    // ── Set values to NULL ────────────────────────────────────────────────────
    auto *setBox  = new QGroupBox( "Set values to NULL  (r.null setnull=…)", tab );
    auto *setForm = new QFormLayout( setBox );
    setForm->setLabelAlignment( Qt::AlignRight );

    auto *setEdit = new QLineEdit( setBox );
    setEdit->setPlaceholderText( "e.g.  -9999, 0, 250-255" );
    setEdit->setToolTip(
        "Comma-separated values or ranges to mark as NULL.\n"
        "Examples:\n"
        "  -9999          — single value\n"
        "  0, -9999       — two values\n"
        "  250-255        — inclusive range\n"
        "  0, 250-255     — value + range" );
    setForm->addRow( "Values / ranges:", setEdit );

    auto *setApply = new QPushButton( "Apply", setBox );
    setForm->addRow( "", setApply );
    root->addWidget( setBox );

    // ── Remove NULL (replace with a value) ───────────────────────────────────
    auto *remBox  = new QGroupBox( "Replace NULL with a value  (r.null null=…)", tab );
    auto *remForm = new QFormLayout( remBox );
    remForm->setLabelAlignment( Qt::AlignRight );

    auto *remSpin = new QDoubleSpinBox( remBox );
    remSpin->setRange( -1e15, 1e15 );
    remSpin->setDecimals( 6 );
    remSpin->setValue( 0.0 );
    remSpin->setToolTip( "NULL cells will be filled with this value." );
    remForm->addRow( "Replacement value:", remSpin );

    auto *remApply = new QPushButton( "Apply", remBox );
    remForm->addRow( "", remApply );
    root->addWidget( remBox );

    // ── Output log ───────────────────────────────────────────────────────────
    auto *outBox = new QGroupBox( "Output", tab );
    auto *outLay = new QVBoxLayout( outBox );
    auto *outLog = new QTextBrowser( outBox );
    outLog->setFont( QFont( "Monospace", 9 ) );
    outLog->setMinimumHeight( 80 );
    outLay->addWidget( outLog );
    root->addWidget( outBox, 1 );

    // ── Helpers ───────────────────────────────────────────────────────────────
    // Run r.null, show result in the log, then refresh the canvas layer so
    // newly-masked null pixels render transparent immediately.
    auto runNull = [this, outLog, mapRef, name]( const QStringList &extraArgs ) {
        outLog->clear();
        const QString gb = QString::fromLocal8Bit( qgetenv("GISBASE") );
        if ( gb.isEmpty() ) { outLog->setPlainText("(GISBASE not set)"); return; }

        QProcess p;
        p.setProcessChannelMode( QProcess::MergedChannels );
        p.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );

        QStringList args;
        args << "map=" + mapRef;
        args << extraArgs;

        p.start( gb + "/bin/r.null", args );
        if ( !p.waitForStarted(5000) )
            { outLog->setPlainText("(failed to start r.null)"); return; }
        if ( !p.waitForFinished(30000) )
            { p.kill(); outLog->setPlainText("(r.null timed out)"); return; }

        const QString out = QString::fromLocal8Bit( p.readAllStandardOutput() ).trimmed();
        outLog->setPlainText( out.isEmpty() ? "(done — no output)" : out );

        // Invalidate the tile cache and re-render so null pixels go transparent.
        if ( p.exitCode() == 0 ) {
            for ( GrassMapLayer *l : mCanvas->layers() ) {
                if ( l->name() == name ) {
                    l->invalidateCache();
                    break;
                }
            }
            mCanvas->scheduleRender();
        }
    };

    // "Set to NULL" — parse the edit field, build setnull=... arg
    connect( setApply, &QPushButton::clicked, tab, [setEdit, runNull]() {
        const QString txt = setEdit->text().trimmed();
        if ( txt.isEmpty() ) return;
        // Normalise: remove spaces around commas/hyphens so r.null is happy
        QString val = txt;
        val.replace( QRegularExpression( R"(\s*,\s*)" ), "," );
        runNull( { "setnull=" + val } );
    } );

    // "Replace NULL" — build null=... arg
    connect( remApply, &QPushButton::clicked, tab, [remSpin, runNull]() {
        runNull( { "null=" + QString::number( remSpin->value(), 'g', 15 ) } );
    } );
}

// ---------------------------------------------------------------------------
void GrassRasterPropsDialog::buildRenderingTab( QWidget *tab,
                                                 const QString &name,
                                                 const QString &mapset )
{
    const QString mapRef = mapset.isEmpty() ? name : name + "@" + mapset;
    auto *root = new QVBoxLayout( tab );
    root->setSpacing( 10 );

    // ── Opacity ───────────────────────────────────────────────────────────────
    auto *opBox  = new QGroupBox( "Layer opacity", tab );
    auto *opForm = new QFormLayout( opBox );

    auto *opSlider = new QSlider( Qt::Horizontal, opBox );
    opSlider->setRange( 0, 100 );

    // Initialise from the live canvas layer (if present)
    double initOpacity = 1.0;
    for ( GrassMapLayer *l : mCanvas->layers() )
        if ( l->name() == name ) { initOpacity = l->opacity(); break; }
    opSlider->setValue( qRound( initOpacity * 100 ) );

    auto *opPct = new QLabel( QString::number( opSlider->value() ) + " %", opBox );
    opPct->setMinimumWidth( 40 );
    auto *opRow = new QHBoxLayout;
    opRow->addWidget( opSlider );
    opRow->addWidget( opPct );
    opForm->addRow( "Opacity:", opRow );

    auto *opApply = new QPushButton( "Apply", opBox );
    opForm->addRow( "", opApply );
    root->addWidget( opBox );

    connect( opSlider, &QSlider::valueChanged, opPct,
             [opPct]( int v ) { opPct->setText( QString::number(v) + " %" ); } );
    connect( opApply, &QPushButton::clicked, tab,
             [this, name, opSlider]() {
                 for ( GrassMapLayer *l : mCanvas->layers() )
                     if ( l->name() == name ) {
                         l->setOpacity( opSlider->value() / 100.0 );
                         l->invalidateCache();
                         mCanvas->scheduleRender();
                         break;
                     }
             } );

    // ── Color stretch shortcuts ───────────────────────────────────────────────
    auto *csBox  = new QGroupBox( "Color stretch  (r.colors)", tab );
    auto *csLay  = new QVBoxLayout( csBox );

    // Quick preset combo shared with Symbology (subset of kColorTables)
    static const QStringList kQuick = {
        "grey","grey.eq","grey.log","elevation","srtm","haxby","ndvi",
        "rainbow","ramp","terrain","viridis","plasma","inferno","magma",
        "blues","reds","greens","bgyr","byg","ryb","sepia","curvature","slope"
    };
    auto *csCombo = new QComboBox( csBox );
    csCombo->addItems( kQuick );
    auto *csForm = new QFormLayout;
    csForm->addRow( "Color table:", csCombo );
    csLay->addLayout( csForm );

    auto *csRow = new QHBoxLayout;
    auto *csApply = new QPushButton( "Apply",       csBox );
    auto *csEq    = new QPushButton( "grey.eq",     csBox );
    auto *csLog   = new QPushButton( "grey.log",    csBox );
    csApply->setToolTip( "Apply selected color table via r.colors" );
    csEq->setToolTip(    "Histogram-equalized grey (r.colors color=grey.eq)" );
    csLog->setToolTip(   "Log-stretched grey (r.colors color=grey.log)" );
    csRow->addWidget( csApply );
    csRow->addWidget( csEq );
    csRow->addWidget( csLog );
    csLay->addLayout( csRow );

    auto *csLog2 = new QTextBrowser( csBox );
    csLog2->setFont( QFont( "Monospace", 9 ) );
    csLog2->setMaximumHeight( 50 );
    csLay->addWidget( csLog2 );
    root->addWidget( csBox );
    root->addStretch();

    auto runColors = [csLog2, mapRef]( const QStringList &args ) {
        csLog2->clear();
        const QString gb = QString::fromLocal8Bit( qgetenv("GISBASE") );
        if ( gb.isEmpty() ) { csLog2->setPlainText("(GISBASE not set)"); return; }
        QProcess p;
        p.setProcessChannelMode( QProcess::MergedChannels );
        p.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
        p.start( gb + "/bin/r.colors", args );
        if ( !p.waitForFinished(15000) ) { p.kill(); csLog2->setPlainText("(timeout)"); return; }
        const QString out = QString::fromLocal8Bit( p.readAllStandardOutput() ).trimmed();
        csLog2->setPlainText( out.isEmpty() ? "Done." : out );
    };

    connect( csApply, &QPushButton::clicked, tab,
             [csCombo, mapRef, runColors]() {
                 runColors( { "map=" + mapRef, "color=" + csCombo->currentText() } );
             } );
    connect( csEq,  &QPushButton::clicked, tab,
             [mapRef, runColors]() { runColors( { "map=" + mapRef, "color=grey.eq" } ); } );
    connect( csLog, &QPushButton::clicked, tab,
             [mapRef, runColors]() { runColors( { "map=" + mapRef, "color=grey.log" } ); } );
}

// ---------------------------------------------------------------------------
void GrassRasterPropsDialog::buildCategoriesTab( QWidget *tab,
                                                   const QString &name,
                                                   const QString &mapset )
{
    const QString mapRef = mapset.isEmpty() ? name : name + "@" + mapset;
    auto *root = new QVBoxLayout( tab );
    root->setSpacing( 6 );

    auto *info = new QLabel(
        "Category labels stored with this raster map.", tab );
    info->setWordWrap( true );
    root->addWidget( info );

    auto *catView = new QTextBrowser( tab );
    catView->setFont( QFont( "Monospace", 9 ) );
    root->addWidget( catView, 1 );

    // ── Load / refresh ────────────────────────────────────────────────────────
    auto loadCats = [catView, mapRef]() {
        catView->clear();
        const QString gb = QString::fromLocal8Bit( qgetenv("GISBASE") );
        if ( gb.isEmpty() ) { catView->setPlainText("(GISBASE not set)"); return; }
        QProcess p;
        p.setProcessChannelMode( QProcess::MergedChannels );
        p.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
        p.start( gb + "/bin/r.category", { "map=" + mapRef } );
        if ( !p.waitForFinished(10000) ) { p.kill(); catView->setPlainText("(timeout)"); return; }
        const QString out = QString::fromLocal8Bit( p.readAllStandardOutput() ).trimmed();
        catView->setPlainText( out.isEmpty() ? "(no categories defined)" : out );
    };
    loadCats();

    // ── Set / overwrite a single category label ───────────────────────────────
    auto *editBox  = new QGroupBox( "Set category label  (r.category rules)", tab );
    auto *editForm = new QFormLayout( editBox );

    auto *catVal  = new QSpinBox( editBox );
    catVal->setRange( 0, 1000000 );
    editForm->addRow( "Category value:", catVal );

    auto *catLbl  = new QLineEdit( editBox );
    catLbl->setPlaceholderText( "label text" );
    editForm->addRow( "Label:", catLbl );

    auto *catSet  = new QPushButton( "Set", editBox );
    auto *catRefr = new QPushButton( "Refresh", editBox );
    auto *catRow  = new QHBoxLayout;
    catRow->addWidget( catSet );
    catRow->addWidget( catRefr );
    editForm->addRow( "", catRow );

    auto *catOut = new QTextBrowser( editBox );
    catOut->setFont( QFont( "Monospace", 9 ) );
    catOut->setMaximumHeight( 40 );
    editForm->addRow( catOut );
    root->addWidget( editBox );

    connect( catRefr, &QPushButton::clicked, tab, [loadCats]() { loadCats(); } );

    connect( catSet, &QPushButton::clicked, tab,
             [catVal, catLbl, catOut, mapRef, loadCats]() {
                 const QString rule =
                     QString::number( catVal->value() ) + ":" + catLbl->text().trimmed();
                 const QString gb = QString::fromLocal8Bit( qgetenv("GISBASE") );
                 if ( gb.isEmpty() ) { catOut->setPlainText("(GISBASE not set)"); return; }
                 QProcess p;
                 p.setProcessChannelMode( QProcess::MergedChannels );
                 p.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
                 p.start( gb + "/bin/r.category",
                          { "map=" + mapRef, "rules=-", "separator=colon" } );
                 if ( p.waitForStarted(5000) )
                 {
                     p.write( ( rule + "\n" ).toLocal8Bit() );
                     p.closeWriteChannel();
                     p.waitForFinished( 10000 );
                 }
                 const QString out = QString::fromLocal8Bit( p.readAllStandardOutput() ).trimmed();
                 catOut->setPlainText( out.isEmpty() ? "Done." : out );
                 loadCats();
             } );
}

QString GrassRasterPropsDialog::rInfo( const QString &name,
                                        const QString &mapset,
                                        const QStringList &flags ) const
{
    const QString mapRef = mapset.isEmpty() ? name : name + "@" + mapset;
    QStringList args = flags;
    args << "map=" + mapRef;
    return runModule( "r.info", args );
}

// ============================================================
// GrassVectorPropsDialog
// ============================================================
GrassVectorPropsDialog::GrassVectorPropsDialog( const QString &name,
                                                  const QString &mapset,
                                                  GrassMapCanvas *canvas,
                                                  QWidget *parent )
    : QDialog( parent ), mCanvas( canvas )
{
    const QString mapRef = mapset.isEmpty() ? name : name + "@" + mapset;
    setWindowTitle( "Vector Layer Properties — " + mapRef );
    setMinimumSize( 640, 480 );

    auto *root   = new QVBoxLayout( this );
    auto *body   = new QHBoxLayout();
    auto *sidebar = new QListWidget( this );
    auto *stack   = new QStackedWidget( this );

    sidebar->setFixedWidth( 110 );
    sidebar->setFrameShape( QFrame::NoFrame );
    sidebar->setSpacing( 2 );

    body->addWidget( sidebar );
    body->addWidget( stack, 1 );
    root->addLayout( body, 1 );

    auto addPage = [&]( const QString &title, QWidget *page ) {
        sidebar->addItem( title );
        stack->addWidget( page );
    };

    connect( sidebar, &QListWidget::currentRowChanged,
             stack,   &QStackedWidget::setCurrentIndex );

    // -- Metadata (replaces General; first = default) --
    auto *metWidget = new QWidget();
    buildMetadataTab( metWidget, name, mapset );
    addPage( "Metadata", scrollWrap( metWidget ) );

    // -- Symbology --
    auto *symWidget = new QWidget();
    buildSymbologyTab( symWidget, name, mapset );
    addPage( "Symbology", symWidget );

    // -- Fields --
    auto *fldWidget = new QWidget();
    buildFieldsTab( fldWidget, name, mapset );
    addPage( "Fields", fldWidget );

    sidebar->setCurrentRow( 0 );

    auto *btns = new QDialogButtonBox( QDialogButtonBox::Close, this );
    connect( btns, &QDialogButtonBox::rejected, this, &QDialog::accept );
    root->addWidget( btns );
}

void GrassVectorPropsDialog::buildSymbologyTab( QWidget *tab,
                                                 const QString &name,
                                                 const QString &mapset )
{
    Q_UNUSED( mapset )   // layer is matched by name; mapset not used in canvas lookup

    // Find the live layer object so we can update its style in-place
    GrassVectorMapLayer *vl = nullptr;
    for ( GrassMapLayer *l : mCanvas->layers() )
        if ( l->name() == name )
            { vl = dynamic_cast<GrassVectorMapLayer *>( l ); break; }

    auto *root = new QVBoxLayout( tab );

    if ( !vl ) { root->addWidget( new QLabel("(layer not on canvas)", tab) ); return; }

    GrassVectorStyle &st = vl->style();

    // ── OSM Carto preset selector ─────────────────────────────────────────────
    auto *presetBox = new QGroupBox( "OSM Carto style preset", tab );
    auto *pLay     = new QFormLayout( presetBox );

    // Detect geometry type from loaded geometry
    const bool hasLines = !vl->lines().isEmpty();
    const bool hasAreas = !vl->areas().isEmpty();

    const QVector<OsmCartoPreset> &presetList =
        hasAreas ? osmCartoAreas() :
        hasLines ? osmCartoLines() :
                   osmCartoPoints();

    auto *presetCombo = new QComboBox( tab );
    presetCombo->addItem( "(Custom / no preset)", "" );
    for ( const OsmCartoPreset &pr : presetList )
        presetCombo->addItem( pr.name, pr.key );

    // Select current preset if set
    if ( !st.osmPreset.isEmpty() )
    {
        const int idx = presetCombo->findData( st.osmPreset );
        if ( idx >= 0 ) presetCombo->setCurrentIndex( idx );
    }
    pLay->addRow( "Preset:", presetCombo );
    root->addWidget( presetBox );

    // ── Manual overrides ──────────────────────────────────────────────────────
    auto *manBox  = new QGroupBox( "Colour & size overrides", tab );
    auto *form    = new QFormLayout( manBox );
    form->setLabelAlignment( Qt::AlignRight );

    // Color buttons
    auto makeColorBtn = [&]( const QString &lbl, QColor &ref ) -> QPushButton * {
        auto *btn = new QPushButton( tab );
        auto refresh = [btn, &ref]() {
            QPixmap px(48,16); px.fill(ref);
            btn->setIcon(QIcon(px)); btn->setText(ref.name());
        };
        refresh();
        connect( btn, &QPushButton::clicked, btn, [btn, &ref, refresh]() mutable {
            QColor c = QColorDialog::getColor(ref, btn);
            if (c.isValid()) { ref = c; refresh(); }
        });
        form->addRow( new QLabel(lbl, tab), btn );
        return btn;
    };

    auto *lineColorBtn  = makeColorBtn( "Line / border color:", st.lineColor  );
    auto *fillColorBtn  = makeColorBtn( "Fill color:",          st.fillColor  );
    auto *pointColorBtn = makeColorBtn( "Point color:",         st.pointColor );

    auto *widthSpin = new QDoubleSpinBox( tab );
    widthSpin->setRange(0.1,20); widthSpin->setSingleStep(0.5);
    widthSpin->setValue(st.lineWidth);
    form->addRow("Line width:", widthSpin);

    auto *sizeSpin = new QDoubleSpinBox( tab );
    sizeSpin->setRange(1,40); sizeSpin->setValue(st.pointSize);
    form->addRow("Point size:", sizeSpin);

    auto *fillChk   = new QCheckBox("Draw fill (polygon interiors)", tab);
    fillChk->setChecked(st.drawFill);
    form->addRow(fillChk);

    auto *linesChk  = new QCheckBox("Draw line / border", tab);
    linesChk->setChecked(st.drawLines);
    form->addRow(linesChk);

    auto *pointsChk = new QCheckBox("Draw points", tab);
    pointsChk->setChecked(st.drawPoints);
    form->addRow(pointsChk);

    root->addWidget(manBox);
    root->addStretch();

    // ── Apply button ──────────────────────────────────────────────────────────
    auto *applyBtn = new QPushButton("Apply", tab);
    root->addWidget(applyBtn);

    // When a preset is chosen, apply it and refresh the manual controls
    connect( presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
             tab, [presetCombo, vl, widthSpin, sizeSpin, fillChk,
                   linesChk, pointsChk,
                   lineColorBtn, fillColorBtn, pointColorBtn]( int )
    {
        const QString key = presetCombo->currentData().toString();
        if ( key.isEmpty() ) return;
        const OsmCartoPreset *pr = findOsmPreset(key);
        if (!pr) return;
        vl->style().applyOsmPreset(*pr);
        auto refresh = [](QPushButton *btn, const QColor &c){
            QPixmap px(48,16); px.fill(c);
            btn->setIcon(QIcon(px)); btn->setText(c.name());
        };
        refresh(lineColorBtn,  vl->style().lineColor);
        refresh(fillColorBtn,  vl->style().fillColor);
        refresh(pointColorBtn, vl->style().pointColor);
        widthSpin->setValue(vl->style().lineWidth);
        sizeSpin->setValue(vl->style().pointSize);
        fillChk->setChecked(vl->style().drawFill);
        linesChk->setChecked(vl->style().drawLines);
        pointsChk->setChecked(vl->style().drawPoints);
    });

    connect( applyBtn, &QPushButton::clicked, tab,
             [this, vl, widthSpin, sizeSpin, fillChk, linesChk, pointsChk]()
    {
        vl->style().lineWidth  = static_cast<float>( widthSpin->value() );
        vl->style().pointSize  = static_cast<float>( sizeSpin->value() );
        vl->style().drawFill   = fillChk->isChecked();
        vl->style().drawLines  = linesChk->isChecked();
        vl->style().drawPoints = pointsChk->isChecked();
        vl->invalidateCache();
        mCanvas->scheduleRender();
    });
}

void GrassVectorPropsDialog::buildMetadataTab( QWidget *tab,
                                                const QString &name,
                                                const QString &mapset )
{
    auto *lay = new QVBoxLayout( tab );
    lay->setSpacing( 6 );

    // v.info map=X format=json — all info (text+region+topo) in one call
    const QString raw = vInfo( name, mapset, { "format=json" } );
    const QJsonDocument doc = QJsonDocument::fromJson( raw.toUtf8() );

    if ( doc.isNull() || !doc.isObject() )
    {
        auto *b = new QTextBrowser( tab );
        b->setFont( QFont("Monospace", 9) );
        b->setPlainText( raw.isEmpty() ? "(no output from v.info)" : raw );
        lay->addWidget( b );
        return;
    }

    const QJsonObject obj = doc.object();

    // ── Map Info ─────────────────────────────────────────────────────────────
    lay->addWidget( jsonGroup( "Map Info", obj, {
        { "name",         "Name"         },
        { "mapset",       "Mapset"       },
        { "project",      "Project"      },
        { "database",     "Database"     },
        { "title",        "Title"        },
        { "scale",        "Scale"        },
        { "creator",      "Creator"      },
        { "organization", "Organization" },
        { "source_date",  "Source date"  },
        { "timestamp",    "Timestamp"    },
        { "format",       "Format"       },
        { "level",        "Level"        },
        { "map3d",        "3D map"       },
    }, tab ) );

    // ── Extent ────────────────────────────────────────────────────────────────
    lay->addWidget( jsonGroup( "Extent", obj, {
        { "north",  "North"  },
        { "south",  "South"  },
        { "east",   "East"   },
        { "west",   "West"   },
        { "top",    "Top"    },
        { "bottom", "Bottom" },
    }, tab ) );

    // ── Topology ──────────────────────────────────────────────────────────────
    lay->addWidget( jsonGroup( "Topology", obj, {
        { "nodes",      "Nodes"      },
        { "points",     "Points"     },
        { "lines",      "Lines"      },
        { "boundaries", "Boundaries" },
        { "centroids",  "Centroids"  },
        { "areas",      "Areas"      },
        { "islands",    "Islands"    },
        { "faces",      "Faces"      },
        { "kernels",    "Kernels"    },
        { "volumes",    "Volumes"    },
        { "holes",      "Holes"      },
        { "primitives", "Primitives" },
    }, tab ) );

    // ── Attribute Columns ─────────────────────────────────────────────────────
    const QString colRaw = vInfo( name, mapset, { "-c", "format=json" } );
    const QJsonDocument colDoc = QJsonDocument::fromJson( colRaw.toUtf8() );
    if ( !colDoc.isNull() && colDoc.isArray() )
    {
        const QJsonArray cols = colDoc.array();
        if ( !cols.isEmpty() )
        {
            auto *colBox = new QGroupBox( "Attribute Columns", tab );
            auto *colLay = new QVBoxLayout( colBox );
            colLay->setContentsMargins( 8, 4, 8, 6 );
            colLay->addWidget( columnsTable( cols, colBox ) );
            lay->addWidget( colBox );
        }
    }

    lay->addStretch();
}

// ---------------------------------------------------------------------------
void GrassVectorPropsDialog::buildFieldsTab( QWidget *tab,
                                               const QString &name,
                                               const QString &mapset )
{
    const QString mapRef = mapset.isEmpty() ? name : name + "@" + mapset;
    const QString activeMapset = QString::fromLocal8Bit( qgetenv("MAPSET") );
    const QString effectiveMapset = mapset.isEmpty() ? activeMapset : mapset;
    const bool readOnly = !activeMapset.isEmpty() && effectiveMapset != activeMapset;

    auto *root = new QVBoxLayout( tab );
    root->setSpacing( 6 );

    // ── Column table ──────────────────────────────────────────────────────────
    auto *tbl = new QTableWidget( 0, 2, tab );
    tbl->setHorizontalHeaderLabels( { "Column", "Type" } );
    tbl->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::Stretch );
    tbl->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    tbl->verticalHeader()->setVisible( false );
    tbl->setEditTriggers( QAbstractItemView::NoEditTriggers );
    tbl->setSelectionBehavior( QAbstractItemView::SelectRows );
    tbl->setAlternatingRowColors( true );
    root->addWidget( tbl, 1 );

    // ── Output log ────────────────────────────────────────────────────────────
    auto *outLog = new QTextBrowser( tab );
    outLog->setFont( QFont( "Monospace", 9 ) );
    outLog->setMaximumHeight( 45 );
    root->addWidget( outLog );

    // ── Run v.db.* helper ─────────────────────────────────────────────────────
    auto runVdb = [outLog, mapRef]( const QString &prog, const QStringList &args ) -> bool {
        const QString gb = QString::fromLocal8Bit( qgetenv("GISBASE") );
        if ( gb.isEmpty() ) { outLog->setPlainText("(GISBASE not set)"); return false; }
        QProcess p;
        p.setProcessChannelMode( QProcess::MergedChannels );
        p.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
        QStringList a = { "map=" + mapRef };
        a << args;
        p.start( gb + "/scripts/" + prog, a );
        if ( !p.waitForFinished(15000) ) { p.kill(); outLog->setPlainText("(timeout)"); return false; }
        const QString out = QString::fromLocal8Bit( p.readAllStandardOutput() ).trimmed();
        outLog->setPlainText( out.isEmpty() ? "Done." : out );
        return p.exitCode() == 0;
    };

    // ── Populate table from v.info -c ─────────────────────────────────────────
    auto refresh = [tbl, mapRef]() {
        tbl->setRowCount( 0 );
        const QString gb = QString::fromLocal8Bit( qgetenv("GISBASE") );
        if ( gb.isEmpty() ) return;
        QProcess p;
        p.setProcessChannelMode( QProcess::SeparateChannels );
        p.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
        p.start( gb + "/bin/v.info", { "-c", "format=json", "map=" + mapRef } );
        if ( !p.waitForFinished(10000) ) { p.kill(); return; }
        const QJsonDocument doc = QJsonDocument::fromJson(
            p.readAllStandardOutput() );
        const QJsonArray cols = doc.isArray() ? doc.array()
                              : doc.object().value("columns").toArray();
        for ( const QJsonValue &v : cols ) {
            const QJsonObject col = v.toObject();
            const int row = tbl->rowCount();
            tbl->insertRow( row );
            tbl->setItem( row, 0, new QTableWidgetItem( col.value("name").toString() ) );
            tbl->setItem( row, 1, new QTableWidgetItem( col.value("sql_type").toString() ) );
        }
    };
    refresh();

    if ( readOnly )
    {
        auto *roNote = new QLabel(
            QString( "Read-only — map lives in mapset '%1'." ).arg( effectiveMapset ),
            tab );
        roNote->setStyleSheet( "color: #a00;" );
        root->addWidget( roNote );
        return;
    }

    // ── Edit buttons (write-capable mapset only) ──────────────────────────────
    auto *btnRow = new QHBoxLayout;
    auto *addBtn    = new QPushButton( "Add column…",    tab );
    auto *renBtn    = new QPushButton( "Rename…",        tab );
    auto *dropBtn   = new QPushButton( "Drop column…",   tab );
    btnRow->addWidget( addBtn );
    btnRow->addWidget( renBtn );
    btnRow->addWidget( dropBtn );
    root->addLayout( btnRow );

    // SQL types offered in the Add dialog
    static const QStringList kTypes = {
        "INTEGER", "DOUBLE PRECISION", "VARCHAR(50)", "VARCHAR(255)", "TEXT", "DATE"
    };

    connect( addBtn, &QPushButton::clicked, tab,
             [tab, tbl, outLog, mapRef, runVdb, refresh]() {
                 QDialog dlg( tab );
                 dlg.setWindowTitle( "Add column" );
                 auto *f = new QFormLayout( &dlg );
                 auto *nameEdit = new QLineEdit( &dlg );
                 nameEdit->setPlaceholderText( "column_name" );
                 f->addRow( "Column name:", nameEdit );
                 auto *typeCombo = new QComboBox( &dlg );
                 typeCombo->addItems({ "INTEGER", "DOUBLE PRECISION",
                                       "VARCHAR(50)", "VARCHAR(255)", "TEXT", "DATE" });
                 f->addRow( "SQL type:", typeCombo );
                 auto *bb = new QDialogButtonBox(
                     QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg );
                 connect( bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
                 connect( bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );
                 f->addRow( bb );
                 if ( dlg.exec() != QDialog::Accepted ) return;
                 const QString col = nameEdit->text().trimmed();
                 if ( col.isEmpty() ) return;
                 if ( runVdb( "v.db.addcolumn",
                              { "columns=" + col + " " + typeCombo->currentText() } ) )
                     refresh();
             } );

    connect( renBtn, &QPushButton::clicked, tab,
             [tab, tbl, runVdb, refresh]() {
                 const auto sel = tbl->selectedItems();
                 if ( sel.isEmpty() ) return;
                 const QString old = tbl->item( sel.first()->row(), 0 )->text();
                 bool ok;
                 const QString nw = QInputDialog::getText( tab, "Rename column",
                     QString( "New name for '%1':" ).arg(old), QLineEdit::Normal, old, &ok );
                 if ( !ok || nw.trimmed().isEmpty() || nw == old ) return;
                 if ( runVdb( "v.db.renamecolumn",
                              { "column=" + old + "," + nw.trimmed() } ) )
                     refresh();
             } );

    connect( dropBtn, &QPushButton::clicked, tab,
             [tab, tbl, runVdb, refresh]() {
                 const auto sel = tbl->selectedItems();
                 if ( sel.isEmpty() ) return;
                 const QString col = tbl->item( sel.first()->row(), 0 )->text();
                 if ( QMessageBox::question( tab, "Drop column",
                          QString( "Permanently remove column '%1'?" ).arg(col),
                          QMessageBox::Yes | QMessageBox::No ) != QMessageBox::Yes )
                     return;
                 if ( runVdb( "v.db.dropcolumn", { "columns=" + col } ) )
                     refresh();
             } );
}

QString GrassVectorPropsDialog::vInfo( const QString &name,
                                        const QString &mapset,
                                        const QStringList &flags ) const
{
    const QString mapRef = mapset.isEmpty() ? name : name + "@" + mapset;
    QStringList args = flags;
    args << "map=" + mapRef;
    return runModule( "v.info", args );
}
