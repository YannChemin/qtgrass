#include "grasschartwidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <cmath>
#include <algorithm>

GrassChartWidget::GrassChartWidget( Type type, QWidget *parent )
    : QWidget( parent ), mType( type )
{
    setBackgroundRole( QPalette::Base );
    setAutoFillBackground( true );
    setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
}

void GrassChartWidget::setData( const QVector<QPair<double, double>> &data,
                                  const QString &xLabel,
                                  const QString &yLabel )
{
    mData    = data;
    mXLabel  = xLabel;
    mYLabel  = yLabel;
    mBarLabels.clear();
    update();
}

void GrassChartWidget::setBarData( const QVector<QPair<QString, int>> &data,
                                    const QString &xLabel,
                                    const QString &yLabel )
{
    mData.clear();
    mBarLabels.clear();
    for ( const auto &kv : data )
    {
        mData.append( { static_cast<double>( mData.size() ),
                        static_cast<double>( kv.second ) } );
        mBarLabels.append( kv.first );
    }
    mXLabel = xLabel;
    mYLabel = yLabel;
    update();
}

// ---------------------------------------------------------------------------
// paintEvent
// ---------------------------------------------------------------------------
void GrassChartWidget::paintEvent( QPaintEvent * )
{
    QPainter p( this );
    p.setRenderHint( QPainter::Antialiasing );

    // Background
    p.fillRect( rect(), QColor( "#1e1e1e" ) );

    if ( mData.isEmpty() )
    {
        p.setPen( QColor( "#555" ) );
        p.drawText( rect(), Qt::AlignCenter, "No data" );
        return;
    }

    const int sideMargin = 55;
    // Bar charts need more bottom space for rotated x-axis labels + axis title
    const int bottomMargin = ( mType == BarChart && !mBarLabels.isEmpty() ) ? 72 : 50;
    const QRect plotArea = rect().adjusted( sideMargin, 12, -12, -bottomMargin );
    if ( plotArea.width() < 10 || plotArea.height() < 10 ) return;

    // Axis lines
    p.setPen( QPen( QColor( "#888" ), 1 ) );
    p.drawLine( plotArea.bottomLeft(), plotArea.topLeft() );
    p.drawLine( plotArea.bottomLeft(), plotArea.bottomRight() );

    // Axis labels
    p.setPen( QColor( "#aaa" ) );
    QFont lf( "sans", 9 );
    p.setFont( lf );

    if ( !mYLabel.isEmpty() )
    {
        p.save();
        p.translate( 12, plotArea.center().y() );
        p.rotate( -90 );
        p.drawText( QRect( -60, -12, 120, 20 ), Qt::AlignCenter, mYLabel );
        p.restore();
    }
    // Line chart X title drawn here; bar chart title drawn after drawBarChart()
    if ( mType == LineChart && !mXLabel.isEmpty() )
        p.drawText( QRect( plotArea.left(), plotArea.bottom() + 30,
                           plotArea.width(), 16 ),
                    Qt::AlignCenter, mXLabel );

    if ( mType == LineChart )
    {
        drawLineChart( p, plotArea );
    }
    else
    {
        // For bar charts with rotated labels we draw the X-axis title lower.
        drawBarChart( p, plotArea );
        if ( !mXLabel.isEmpty() )
        {
            p.setPen( QColor( "#aaa" ) );
            p.setFont( QFont( "sans", 9 ) );
            p.drawText( QRect( plotArea.left(), plotArea.bottom() + 54,
                               plotArea.width(), 16 ),
                        Qt::AlignCenter, mXLabel );
        }
    }
}

// ---------------------------------------------------------------------------
// Line chart
// ---------------------------------------------------------------------------
void GrassChartWidget::drawLineChart( QPainter &p, const QRect &pa ) const
{
    double xMin = mData.first().first,  xMax = mData.last().first;
    double yMin = mData.first().second, yMax = mData.first().second;
    for ( const auto &kv : mData )
    {
        yMin = std::min( yMin, kv.second );
        yMax = std::max( yMax, kv.second );
    }
    if ( xMax == xMin ) xMax = xMin + 1;
    if ( yMax == yMin ) { yMin -= 1; yMax += 1; }

    auto toPixel = [&]( double x, double y ) -> QPointF {
        return {
            pa.left()  + ( x - xMin ) / ( xMax - xMin ) * pa.width(),
            pa.bottom()- ( y - yMin ) / ( yMax - yMin ) * pa.height()
        };
    };

    // Y-axis ticks
    p.setPen( QColor( "#555" ) );
    const int nTicks = 5;
    for ( int i = 0; i <= nTicks; ++i )
    {
        double yVal = yMin + i * ( yMax - yMin ) / nTicks;
        QPointF pt  = toPixel( xMin, yVal );
        p.drawLine( pt - QPointF( 4, 0 ), pt );
        p.setPen( QColor( "#888" ) );
        p.drawText( QRectF( pa.left() - 48, pt.y() - 8, 42, 16 ),
                    Qt::AlignRight | Qt::AlignVCenter,
                    QString::number( yVal, 'g', 4 ) );
        p.setPen( QColor( "#333" ) );
        p.drawLine( pt, { static_cast<double>( pa.right() ), pt.y() } );
        p.setPen( QColor( "#555" ) );
    }

    // Line path
    QPainterPath path;
    bool first = true;
    for ( const auto &kv : mData )
    {
        if ( kv.second != kv.second ) continue;  // skip NaN (marked as *)
        QPointF px = toPixel( kv.first, kv.second );
        if ( first ) { path.moveTo( px ); first = false; }
        else         path.lineTo( px );
    }

    // Fill under line
    if ( !path.isEmpty() )
    {
        QPainterPath fill = path;
        fill.lineTo( toPixel( mData.last().first,  yMin ) );
        fill.lineTo( toPixel( mData.first().first, yMin ) );
        fill.closeSubpath();
        p.fillPath( fill, QColor( 100, 160, 240, 60 ) );
    }

    p.setPen( QPen( QColor( "#5aabff" ), 2 ) );
    p.setBrush( Qt::NoBrush );
    p.drawPath( path );

    // X axis ticks (distance)
    p.setPen( QColor( "#888" ) );
    for ( int i = 0; i <= 5; ++i )
    {
        double xVal = xMin + i * ( xMax - xMin ) / 5.0;
        QPointF pt  = toPixel( xVal, yMin );
        p.drawText( QRectF( pt.x() - 25, pa.bottom() + 6, 50, 14 ),
                    Qt::AlignCenter,
                    QString::number( xVal, 'f', 0 ) );
    }
}

// ---------------------------------------------------------------------------
// Bar chart
// ---------------------------------------------------------------------------
void GrassChartWidget::drawBarChart( QPainter &p, const QRect &pa ) const
{
    double yMax = 1;
    for ( const auto &kv : mData ) yMax = std::max( yMax, kv.second );

    const int n = mData.size();
    if ( n == 0 ) return;

    const double barW = pa.width() / static_cast<double>( n );

    // Y ticks
    p.setPen( QColor( "#888" ) );
    for ( int i = 0; i <= 4; ++i )
    {
        double yVal = yMax * i / 4.0;
        double yPx  = pa.bottom() - yVal / yMax * pa.height();
        p.drawText( QRectF( pa.left() - 48, yPx - 8, 42, 16 ),
                    Qt::AlignRight | Qt::AlignVCenter,
                    QString::number( static_cast<long long>( yVal ) ) );
        p.setPen( QColor( "#333" ) );
        p.drawLine( QPointF( pa.left(), yPx ), QPointF( pa.right(), yPx ) );
        p.setPen( QColor( "#888" ) );
    }

    // ── Determine label step from font metrics ────────────────────────────────
    // Use QFontMetrics to find the longest label, then compute how many fit
    // horizontally without overlap, and rotate 45° to save vertical space.
    QFont xFont( "sans", 7 );
    QFontMetrics fm( xFont );
    int maxLabelPx = 10;
    for ( const QString &lbl : mBarLabels )
        maxLabelPx = std::max( maxLabelPx, fm.horizontalAdvance( lbl ) );

    // With 45° rotation each label needs maxLabelPx * cos(45°) ≈ 0.71 × px
    // horizontal space.  We require at least 1.2× that for breathing room.
    const double labelSlotW = maxLabelPx * 0.71 * 1.2;

    // How many labels can fit?  Cap between 2 and 8.
    const int maxLabels = std::min( 8, std::max( 2,
        static_cast<int>( pa.width() / std::max( labelSlotW, 1.0 ) ) ) );

    // Step between labelled bars
    int step = 1;
    if ( n > maxLabels )
        step = ( n + maxLabels - 1 ) / maxLabels;

    // ── Bars ─────────────────────────────────────────────────────────────────
    for ( int i = 0; i < n; ++i )
    {
        const double h = mData[i].second / yMax * pa.height();
        QRectF bar( pa.left() + i * barW + 1, pa.bottom() - h, barW - 2, h );
        p.fillRect( bar, QColor( 80, 160, 220, 200 ) );
        p.setPen( QColor( "#5aabff" ) );
        p.drawRect( bar );
    }

    // ── X-axis labels (rotated 45°) ───────────────────────────────────────────
    p.setFont( xFont );
    for ( int i = 0; i < n; ++i )
    {
        // Always label first and last; otherwise every `step` bars
        const bool isFirst = ( i == 0 );
        const bool isLast  = ( i == n - 1 );
        if ( !isFirst && !isLast && ( i % step != 0 ) ) continue;
        if ( i >= mBarLabels.size() ) continue;

        // Shorten long labels: try to fit in at most 8 significant chars
        QString lbl = mBarLabels[i];
        bool ok;
        const double val = lbl.toDouble( &ok );
        if ( ok )
            lbl = QString::number( val, 'g', 5 );   // ≤5 sig. figs.

        // Tick mark
        const double xCenter = pa.left() + ( i + 0.5 ) * barW;
        p.setPen( QColor( "#555" ) );
        p.drawLine( QPointF( xCenter, pa.bottom() ),
                    QPointF( xCenter, pa.bottom() + 4 ) );

        // Rotated label: anchor at tick, text goes down-right at 45°
        p.setPen( QColor( "#aaa" ) );
        p.save();
        p.translate( xCenter, pa.bottom() + 5 );
        p.rotate( 45 );
        p.drawText( QRect( 0, -fm.height() / 2, maxLabelPx + 10, fm.height() ),
                    Qt::AlignLeft | Qt::AlignVCenter, lbl );
        p.restore();
    }
}
