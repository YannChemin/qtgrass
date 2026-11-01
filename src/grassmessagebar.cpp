#include "grassmessagebar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QToolButton>
#include <QStyle>

GrassMessageBar::GrassMessageBar( QWidget *parent )
    : QWidget( parent )
{
    setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Maximum );
    setVisible( false );

    mLayout = new QHBoxLayout( this );
    mLayout->setContentsMargins( 6, 2, 6, 2 );
    mLayout->setSpacing( 8 );

    mIcon = new QLabel( this );
    mIcon->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    mLayout->addWidget( mIcon );

    mText = new QLabel( this );
    mText->setWordWrap( false );
    mText->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
    mLayout->addWidget( mText, 1 );

    mClose = new QToolButton( this );
    mClose->setText( "✕" );
    mClose->setAutoRaise( true );
    mClose->setToolTip( "Dismiss" );
    mLayout->addWidget( mClose );
    connect( mClose, &QToolButton::clicked, this, &GrassMessageBar::clearWidgets );

    mTimer = new QTimer( this );
    mTimer->setSingleShot( true );
    connect( mTimer, &QTimer::timeout, this, &GrassMessageBar::onTimeout );
}

void GrassMessageBar::pushMessage( const QString &title,
                                    const QString &text,
                                    Level level,
                                    int duration )
{
    // Newest message always takes precedence — prepend so it shows immediately.
    mQueue.prepend( { title, text, level, duration } );
    mTimer->stop();
    showNext();
}

void GrassMessageBar::clearWidgets()
{
    mTimer->stop();
    mQueue.clear();
    setVisible( false );
}

void GrassMessageBar::onTimeout()
{
    if ( !mQueue.isEmpty() )
        mQueue.removeFirst();
    // Discard any remaining persistent (duration==0) messages that have been
    // superseded by a timed one — they're stale after the new message shows.
    while ( !mQueue.isEmpty() && mQueue.first().duration == 0 )
        mQueue.removeFirst();
    if ( mQueue.isEmpty() )
        setVisible( false );
    else
        showNext();
}

void GrassMessageBar::showNext()
{
    if ( mQueue.isEmpty() ) { setVisible( false ); return; }
    const Message &m = mQueue.first();

    QString label = m.title.isEmpty() ? m.text
                                      : QString( "<b>%1</b> %2" ).arg( m.title, m.text );
    mText->setText( label );
    applyStyle( m.level );
    setVisible( true );

    if ( m.duration > 0 )
        mTimer->start( m.duration * 1000 );
    else
        mTimer->stop();
}

void GrassMessageBar::applyStyle( Level level )
{
    static const char *colours[] = {
        "#d4edda",  // Info    — green tint
        "#fff3cd",  // Warning — amber tint
        "#f8d7da",  // Critical — red tint
    };
    static const char *icons[] = { "ℹ", "⚠", "✖" };

    int idx = static_cast<int>( level );
    setStyleSheet( QString( "GrassMessageBar { background: %1; border-bottom: 1px solid #ccc; }" )
                   .arg( colours[idx] ) );
    mIcon->setText( icons[idx] );
}
