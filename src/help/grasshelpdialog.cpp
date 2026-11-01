#include "grasshelpdialog.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QUrl>
#include <QDebug>

GrassHelpDialog::GrassHelpDialog( const QString &moduleName, QWidget *parent )
    : QDialog( parent )
{
    setWindowTitle( "GRASS Help" );
    setMinimumSize( 700, 520 );

    QVBoxLayout *lay = new QVBoxLayout( this );

    // Title bar + search
    QHBoxLayout *topRow = new QHBoxLayout();
    mTitle = new QLabel( this );
    mTitle->setStyleSheet( "font-weight:bold; font-size:13px;" );
    topRow->addWidget( mTitle, 1 );
    mSearch = new QLineEdit( this );
    mSearch->setPlaceholderText( "Find in page…" );
    mSearch->setClearButtonEnabled( true );
    mSearch->setMaximumWidth( 200 );
    topRow->addWidget( mSearch );
    lay->addLayout( topRow );

    mBrowser = new QTextBrowser( this );
    mBrowser->setOpenExternalLinks( false );
    mBrowser->setOpenLinks( true );
    // Allow local file:// links between help pages
    mBrowser->setSearchPaths( { QString::fromLocal8Bit( qgetenv( "GISBASE" ) )
                                  + "/docs/html/" } );
    lay->addWidget( mBrowser, 1 );

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Close, this );
    connect( btns, &QDialogButtonBox::rejected, this, &QDialog::accept );
    lay->addWidget( btns );

    connect( mSearch, &QLineEdit::textChanged,
             this, &GrassHelpDialog::onSearch );

    showHelp( moduleName );
}

void GrassHelpDialog::showHelp( const QString &moduleName )
{
    mCurrentModule = moduleName;
    setWindowTitle( "GRASS Help — " + moduleName );
    mTitle->setText( moduleName );

    const QString path = htmlPath( moduleName );
    if ( QFile::exists( path ) )
    {
        mBrowser->setSource( QUrl::fromLocalFile( path ) );
    }
    else
    {
        mBrowser->setHtml( QString(
            "<html><body style='font-family:sans-serif;padding:20px;'>"
            "<h2>%1</h2>"
            "<p style='color:#888;'>Help page not found at:<br>"
            "<code>%2</code></p>"
            "<p>Run <code>%1 --help</code> in a terminal for usage information.</p>"
            "</body></html>" ).arg( moduleName, path ) );
    }
}

void GrassHelpDialog::onSearch( const QString &text )
{
    if ( text.isEmpty() )
    {
        // Clear highlight by re-finding empty string
        mBrowser->find( "" );
        return;
    }
    mBrowser->find( text );
}

QString GrassHelpDialog::htmlPath( const QString &moduleName ) const
{
    const QString gb = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    return gb + "/docs/html/" + moduleName + ".html";
}
