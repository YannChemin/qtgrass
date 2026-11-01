#include "../grassenv.h"
#include "grassprofiledialog.h"
#include "grasschartwidget.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QProcess>
#include <QProcessEnvironment>
#include <QVBoxLayout>
#include <QDebug>

GrassProfileDialog::GrassProfileDialog( QWidget *parent )
    : QDialog( parent )
{
    setWindowTitle( "Raster Profile — r.profile" );
    setMinimumSize( 600, 350 );

    QVBoxLayout *lay = new QVBoxLayout( this );

    mInfoLabel = new QLabel( "Draw a line on the canvas to extract a profile.", this );
    mInfoLabel->setStyleSheet( "color:#888; font-style:italic;" );
    lay->addWidget( mInfoLabel );

    mChart = new GrassChartWidget( GrassChartWidget::LineChart, this );
    lay->addWidget( mChart, 1 );

    mStatus = new QLabel( this );
    lay->addWidget( mStatus );

    QDialogButtonBox *btns = new QDialogButtonBox( QDialogButtonBox::Close, this );
    connect( btns, &QDialogButtonBox::rejected, this, &QDialog::accept );
    lay->addWidget( btns );
}

void GrassProfileDialog::runProfile( const QString &rasterMap,
                                      const QString &mapset,
                                      const QVector<QPointF> &coords )
{
    if ( coords.size() < 2 )
    {
        mStatus->setText( "Need at least 2 points for a profile." );
        return;
    }

    const QString gisBase = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    if ( gisBase.isEmpty() ) { mStatus->setText( "GISBASE not set." ); return; }

    const QString mapRef = mapset.isEmpty() ? rasterMap : rasterMap + "@" + mapset;

    // Build coordinates string: "e1,n1,e2,n2,..."
    QStringList coordParts;
    for ( const QPointF &pt : coords )
        coordParts << QString::number( pt.x(), 'f', 2 )
                    + ","
                    + QString::number( pt.y(), 'f', 2 );
    const QString coordStr = coordParts.join( "," );

    mInfoLabel->setText( QString( "Profile: %1  [%2 waypoints]" )
                         .arg( mapRef ).arg( coords.size() ) );
    mStatus->setText( "Running r.profile…" );

    QProcess proc;
    proc.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
    proc.start( gisBase + "/bin/r.profile",
                { "input=" + mapRef,
                  "coordinates=" + coordStr,
                  "separator=space" } );

    if ( !proc.waitForFinished( 30000 ) )
    {
        mStatus->setText( "r.profile timed out." );
        return;
    }

    if ( proc.exitCode() != 0 )
    {
        mStatus->setText( "r.profile failed: " +
                          proc.readAllStandardError().simplified() );
        return;
    }

    parseAndDisplay( proc.readAllStandardOutput() );
}

void GrassProfileDialog::parseAndDisplay( const QString &output )
{
    QVector<QPair<double, double>> data;

    for ( const QString &line : output.split( '\n', Qt::SkipEmptyParts ) )
    {
        const QStringList parts = line.trimmed().split( ' ', Qt::SkipEmptyParts );
        if ( parts.size() < 2 ) continue;

        bool okX, okY;
        const double dist = parts[0].toDouble( &okX );
        const double val  = parts[1].toDouble( &okY );

        if ( !okX ) continue;
        // '*' means null — represent as NaN so the chart can skip it
        data.append( { dist, okY ? val : std::numeric_limits<double>::quiet_NaN() } );
    }

    if ( data.isEmpty() )
    {
        mStatus->setText( "No profile data returned." );
        return;
    }

    mChart->setData( data, "Distance [m]", "Value" );
    mStatus->setText( QString( " %1 sample points" ).arg( data.size() ) );
}
