#include "../grassenv.h"
#include "grassmapcalcdialog.h"
#include "../grassinterface.h"
#include "../module/grassmoduledialog.h"

#include <QDialogButtonBox>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVBoxLayout>
#include <QDebug>

extern "C" {
#include <grass/gis.h>
}

// ============================================================
// Syntax highlighter for GRASS map algebra expressions
// ============================================================
class GrassMapCalcHighlighter : public QSyntaxHighlighter
{
  public:
    explicit GrassMapCalcHighlighter( QTextDocument *doc )
        : QSyntaxHighlighter( doc )
    {
        // Keywords / control
        QTextCharFormat kwFmt;
        kwFmt.setForeground( QColor( "#c678dd" ) );
        kwFmt.setFontWeight( QFont::Bold );
        for ( const QString &kw : QStringList{ "if", "else", "isnull", "null", "eval",
                                               "graph", "string" } )
            mRules.append( { QRegularExpression( "\\b" + kw + "\\b" ), kwFmt } );

        // Functions
        QTextCharFormat fnFmt;
        fnFmt.setForeground( QColor( "#61afef" ) );
        const QStringList funcs = {
            "abs", "atan", "cos", "exp", "int", "log", "round",
            "sin", "sqrt", "tan", "float", "double", "rand",
            "min", "max", "median", "mode", "nmedian",
            "ewres", "nsres", "row", "col", "x", "y",
            "acos", "asin", "pow", "hypot", "fabs"
        };
        for ( const QString &f : funcs )
            mRules.append( { QRegularExpression( "\\b" + f + "\\b(?=\\s*\\()" ), fnFmt } );

        // Numeric literals
        QTextCharFormat numFmt;
        numFmt.setForeground( QColor( "#d19a66" ) );
        mRules.append( { QRegularExpression( "\\b[0-9]+(\\.[0-9]+)?\\b" ), numFmt } );

        // String literals (quoted map names or strings)
        QTextCharFormat strFmt;
        strFmt.setForeground( QColor( "#98c379" ) );
        mRules.append( { QRegularExpression( "\"[^\"]*\"" ), strFmt } );

        // Operators
        QTextCharFormat opFmt;
        opFmt.setForeground( QColor( "#e06c75" ) );
        mRules.append( { QRegularExpression( "[+\\-*/^%=<>!&|]+" ), opFmt } );

        // Output map name (left side of =)
        QTextCharFormat outFmt;
        outFmt.setForeground( QColor( "#e5c07b" ) );
        outFmt.setFontWeight( QFont::Bold );
        mRules.append( { QRegularExpression( "^\\s*[A-Za-z_][A-Za-z0-9_.@]*\\s*(?==)" ), outFmt } );

        // Comments
        QTextCharFormat cmt;
        cmt.setForeground( QColor( "#5c6370" ) );
        cmt.setFontItalic( true );
        mRules.append( { QRegularExpression( "#[^\n]*" ), cmt } );
    }

  protected:
    void highlightBlock( const QString &text ) override
    {
        for ( const auto &rule : mRules )
        {
            QRegularExpressionMatchIterator it = rule.pattern.globalMatch( text );
            while ( it.hasNext() )
            {
                const QRegularExpressionMatch m = it.next();
                setFormat( m.capturedStart(), m.capturedLength(), rule.format );
            }
        }
    }

  private:
    struct Rule { QRegularExpression pattern; QTextCharFormat format; };
    QList<Rule> mRules;
};

// ============================================================
// GrassMapCalcDialog
// ============================================================
GrassMapCalcDialog::GrassMapCalcDialog( GrassInterface *iface, QWidget *parent )
    : QDialog( parent )
    , mIface( iface )
{
    setWindowTitle( "GRASS Raster Map Calculator — r.mapcalc" );
    setMinimumSize( 760, 520 );

    QVBoxLayout *root = new QVBoxLayout( this );

    // ---- Expression editor ----
    QGroupBox *editorBox = new QGroupBox( "Expression", this );
    QVBoxLayout *edLay = new QVBoxLayout( editorBox );

    mEditor = new QPlainTextEdit( editorBox );
    mEditor->setFont( QFont( "Monospace", 11 ) );
    mEditor->setStyleSheet( "background:#1e1e1e; color:#abb2bf; "
                             "selection-background-color:#3e4451;" );
    mEditor->setPlaceholderText(
        "result = elevation * 0.001 + 10\n"
        "# or multi-line:\n"
        "slope = if(elevation > 0, sqrt(dx()^2 + dy()^2), null())" );
    mHighlighter = new GrassMapCalcHighlighter( mEditor->document() );
    edLay->addWidget( mEditor );

    QLabel *hint = new QLabel(
        "Tip: result_map = expression  (one assignment per line)", editorBox );
    hint->setStyleSheet( "color:#5c6370; font-size:10px;" );
    edLay->addWidget( hint );
    root->addWidget( editorBox, 3 );

    // ---- Maps + operators ----
    QHBoxLayout *midRow = new QHBoxLayout();

    // Available raster maps
    QGroupBox *mapBox = new QGroupBox( "Available raster maps (double-click to insert)", this );
    QVBoxLayout *mapLay = new QVBoxLayout( mapBox );
    mMapList = new QListWidget( mapBox );
    mMapList->setToolTip( "Double-click a map name to insert it at cursor" );
    mapLay->addWidget( mMapList );
    QPushButton *refreshBtn = new QPushButton( "Refresh", mapBox );
    mapLay->addWidget( refreshBtn );
    midRow->addWidget( mapBox, 2 );

    // Operator/function buttons
    QGroupBox *opBox = new QGroupBox( "Operators & functions", this );
    buildOperatorPanel( opBox );
    midRow->addWidget( opBox, 3 );

    root->addLayout( midRow, 1 );

    // ---- Status + buttons ----
    mStatus = new QLabel( this );
    mStatus->setStyleSheet( "color:#c00;" );
    root->addWidget( mStatus );

    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *runBtn   = new QPushButton( "Run", this );
    QPushButton *closeBtn = new QPushButton( "Close", this );
    runBtn->setDefault( true );
    btnRow->addWidget( runBtn );
    btnRow->addStretch();
    btnRow->addWidget( closeBtn );
    root->addLayout( btnRow );

    // Connections
    connect( mMapList,  &QListWidget::itemDoubleClicked,
             this, [this]( QListWidgetItem *item ) {
                 insertMapName( item->text().split( ' ' ).first() );
             } );
    connect( refreshBtn, &QPushButton::clicked,
             this, &GrassMapCalcDialog::refreshMapList );
    connect( runBtn,     &QPushButton::clicked,
             this, &GrassMapCalcDialog::runExpression );
    connect( closeBtn,   &QPushButton::clicked,
             this, &QDialog::accept );

    refreshMapList();
}

// ---------------------------------------------------------------------------
// Build operator/function button grid
// ---------------------------------------------------------------------------
void GrassMapCalcDialog::buildOperatorPanel( QWidget *parent )
{
    QVBoxLayout *lay = new QVBoxLayout( parent );

    struct BtnGroup {
        QString label;
        QStringList items;
    };
    const QList<BtnGroup> groups = {
        { "Arithmetic",  { "+", "-", "*", "/", "^", "%" } },
        { "Comparison",  { "==", "!=", ">", "<", ">=", "<=" } },
        { "Logic",       { "&&", "||", "!", "if(,,)", "isnull()", "null()" } },
        { "Math funcs",  { "sin()", "cos()", "tan()", "sqrt()", "exp()", "log()" } },
        { "Grid",        { "row()", "col()", "x()", "y()", "ewres()", "nsres()" } },
        { "Type",        { "int()", "float()", "round()", "abs()", "min(,)", "max(,)" } },
    };

    for ( const BtnGroup &g : groups )
    {
        QLabel *lbl = new QLabel( "<b>" + g.label + "</b>", parent );
        lbl->setStyleSheet( "margin-top:4px;" );
        lay->addWidget( lbl );

        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing( 3 );
        for ( const QString &op : g.items )
        {
            QPushButton *btn = new QPushButton( op, parent );
            btn->setFixedHeight( 24 );
            btn->setStyleSheet( "font-family:monospace; font-size:10px; padding:1px 4px;" );
            connect( btn, &QPushButton::clicked, this, [this, op] {
                insertText( op );
            } );
            row->addWidget( btn );
        }
        row->addStretch();
        lay->addLayout( row );
    }
    lay->addStretch();
}

// ---------------------------------------------------------------------------
// Insert map name or operator at cursor
// ---------------------------------------------------------------------------
void GrassMapCalcDialog::insertMapName( const QString &name )
{
    QTextCursor cur = mEditor->textCursor();
    cur.insertText( name );
    mEditor->setTextCursor( cur );
    mEditor->setFocus();
}

void GrassMapCalcDialog::insertText( const QString &text )
{
    QTextCursor cur = mEditor->textCursor();
    // For function placeholders like "if(,,)", position cursor inside
    const int parenPos = text.indexOf( '(' );
    cur.insertText( text );
    if ( parenPos >= 0 )
    {
        cur.movePosition( QTextCursor::Left,
                          QTextCursor::MoveAnchor,
                          text.length() - parenPos - 1 );
        mEditor->setTextCursor( cur );
    }
    mEditor->setFocus();
}

// ---------------------------------------------------------------------------
// Refresh available raster map list
// ---------------------------------------------------------------------------
QStringList GrassMapCalcDialog::availableRasters() const
{
    // Use g.list to get rasters in all accessible mapsets
    QProcess proc;
    proc.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );

    const QString gisBase = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    if ( gisBase.isEmpty() ) return {};

    proc.start( gisBase + "/bin/g.list",
                { "type=raster", "mapset=.", "separator=newline" } );
    proc.waitForFinished( 5000 );

    return QString::fromLocal8Bit( proc.readAllStandardOutput() )
           .split( '\n', Qt::SkipEmptyParts );
}

void GrassMapCalcDialog::refreshMapList()
{
    mMapList->clear();
    const QStringList maps = availableRasters();
    for ( const QString &m : maps )
    {
        const QString name = m.trimmed();
        if ( !name.isEmpty() )
            mMapList->addItem( name );
    }
    if ( maps.isEmpty() )
        mMapList->addItem( "(no rasters found — open a mapset first)" );
}

// ---------------------------------------------------------------------------
// Run the expression
// ---------------------------------------------------------------------------
void GrassMapCalcDialog::runExpression()
{
    const QString expr = mEditor->toPlainText().trimmed();
    if ( expr.isEmpty() )
    {
        mStatus->setText( "Expression is empty." );
        return;
    }
    mStatus->clear();

    // Launch GrassModuleDialog with the expression pre-filled
    auto *dlg = new GrassModuleDialog(
        "r.mapcalc",
        { "expression=" + expr },
        mIface,
        this );
    dlg->setAttribute( Qt::WA_DeleteOnClose );
    dlg->show();
}
