#pragma once

/*!
    \file grassmessagebar.h
    \brief Horizontal notification bar with stacked message queue.
*/

#include <QWidget>

class QLabel;
class QToolButton;
class QTimer;
class QHBoxLayout;

/*!
    \class GrassMessageBar
    \brief Horizontal notification bar shown at the top of the map area.

    Messages are stacked; the most recent is always shown.  The API mirrors the
    contract of \c QgsMessageBar so that ported QGIS GRASS plugin code requires
    minimal adaptation.

    \sa GrassApp::messageBar()
*/
class GrassMessageBar : public QWidget
{
    Q_OBJECT

  public:
    /*!
        \brief Severity level of a notification message.
    */
    enum Level { Info = 0, Warning = 1, Critical = 2 };

    /*! \brief Constructs an initially hidden message bar. */
    explicit GrassMessageBar( QWidget *parent = nullptr );

    /*!
        \brief Push a titled message onto the bar.

        \c Info messages auto-dismiss after \a duration seconds (0 = never).
        \c Warning and \c Critical messages stay until the user dismisses them.

        \param title    Optional bold title prepended to the message text.
        \param text     Main message body.
        \param level    Severity — controls the background colour and icon.
        \param duration Auto-dismiss delay in seconds; 0 disables auto-dismiss.
    */
    void pushMessage( const QString &title,
                      const QString &text,
                      Level level = Info,
                      int duration = 5 );

    /*!
        \brief Convenience overload without a title.

        Matches the \c QgsMessageBar::pushMessage(text, level, duration) signature
        for drop-in compatibility with ported QGIS code.

        \param text     Main message body.
        \param level    Severity level.
        \param duration Auto-dismiss delay in seconds.
    */
    void pushMessage( const QString &text,
                      Level level = Info,
                      int duration = 5 )
    { pushMessage( {}, text, level, duration ); }

  public slots:
    /*! \brief Dismiss the current message and show the next queued one. */
    void clearWidgets();

  private slots:
    void onTimeout();

  private:
    void applyStyle( Level level );

    QHBoxLayout *mLayout   = nullptr;
    QLabel      *mIcon     = nullptr;
    QLabel      *mText     = nullptr;
    QToolButton *mClose    = nullptr;
    QTimer      *mTimer    = nullptr;

    struct Message {
        QString title;
        QString text;
        Level   level;
        int     duration;
    };
    QList<Message> mQueue;

    void showNext();
};
