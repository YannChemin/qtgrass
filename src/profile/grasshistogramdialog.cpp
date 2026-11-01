#include "../grassenv.h"
#include "grasshistogramdialog.h"
#include "grasschartwidget.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QDebug>

GrassHistogramDialog::GrassHistogramDialog( const QString &rasterMap,
                                             const QString &mapset,
                                             QWidget *parent )
    : QDialog( parent )
{
    mMapRef = mapset.isEmpty() ? rasterMap : rasterMap + "@" + mapset;
    setWindowTitle( "Histogram — " + mMapRef );
    setMinimumSize( 600, 380 );

    QVBoxLayout *lay = new QVBoxLayout( this );

    // Controls
    QHBoxLayout *ctrlRow = new QHBoxLayout();
    ctrlRow->addWidget( new QLabel( "Bins:", this ) );
    mSteps = new QSpinBox( this );
    mSteps->setRange( 5, 500 );
    mSteps->setValue( 50 );
    ctrlRow->addWidget( mSteps );
    QPushButton *refreshBtn = new QPushButton( "Refresh", this );
    ctrlRow->addWidget( refreshBtn );
    ctrlRow->addStretch();
    lay->addLayout( ctrlRow );

    mChart = new GrassChartWidget( GrassChartWidget::BarChart, this );
    lay->addWidget( mChart, 1 );

    mStatus = new QLabel( this );
    lay->addWidget( mStatus );

    QDialogButtonBox *btns = new QDialogButtonBox( QDialogButtonBox::Close, this );
    connect( btns,       &QDialogButtonBox::rejected, this, &QDialog::accept );
    connect( refreshBtn, &QPushButton::clicked, this, &GrassHistogramDialog::refresh );
    lay->addWidget( btns );

    loadData( mSteps->value() );
}

void GrassHistogramDialog::refresh()
{
    loadData( mSteps->value() );
}

void GrassHistogramDialog::loadData( int nSteps )
{
    mStatus->setText( "Running r.stats…" );

    const QString gisBase = QString::fromLocal8Bit( qgetenv( "GISBASE" ) );
    if ( gisBase.isEmpty() ) { mStatus->setText( "GISBASE not set." ); return; }

    QProcess proc;
    proc.setProcessEnvironment( GrassEnv::instance()->buildSubprocessEnv() );
    proc.start( gisBase + "/bin/r.stats",
                { "-c", "-n",
                  "input=" + mMapRef,
                  "nsteps=" + QString::number( nSteps ),
                  "separator=pipe" } );

    if ( !proc.waitForFinished( 30000 ) )
    {
        mStatus->setText( "r.stats timed out." );
        return;
    }

    if ( proc.exitCode() != 0 )
    {
        mStatus->setText( "r.stats failed: " +
                          proc.readAllStandardError().simplified() );
        return;
    }

    const QString out = proc.readAllStandardOutput();
    QVector<QPair<QString, int>> bars;

    for ( const QString &line : out.split( '\n', Qt::SkipEmptyParts ) )
    {
        const QStringList parts = line.trimmed().split( '|', Qt::SkipEmptyParts );
        if ( parts.size() < 2 ) continue;
        bool ok;
        const int cnt = parts[1].trimmed().toInt( &ok );
        if ( ok ) bars.append( { parts[0].trimmed(), cnt } );
    }

    if ( bars.isEmpty() )
    {
        mStatus->setText( "No histogram data." );
        return;
    }

    mChart->setBarData( bars, mMapRef, "Count" );
    mStatus->setText( QString( " %1 bins" ).arg( bars.size() ) );
}
