#include "Window.h"
#include "logging.h"

#include <QTimer>
#include <QUrl>

using namespace redtimer;
using namespace std;

Window::Window( QString qml, QQuickView* parent )
    : QQuickView( QUrl(qml), parent )
{
    ENTER();

    // Issue selector window access members
    ctx_ = rootContext();
    item_ = qobject_cast<QQuickItem*>( rootObject() );

    RETURN();
}

bool
Window::event( QEvent* e )
{
    // Show warning on close and if timer is running
    if( emitClosedSignal_ && e->type() == QEvent::Close )
    {
        DEBUG()(e->type());
        emitClosedSignal_ = false;
        closed();
    }

    return QQuickView::event( e );
}


void
Window::message( QString text, QtMsgType type, int timeout )
{
    ENTER()(text)(type)(timeout);

    QString colour;

    switch( type )
    {
    case QtInfoMsg:
        colour = "#006400";
        break;
    case QtWarningMsg:
        colour = "#FF8C00";
        break;
    case QtCriticalMsg:
        colour = "#8B0000";
        break;
    case QtDebugMsg:
    case QtFatalMsg:
        DEBUG() << "Error: Unsupported message type";
        RETURN();
    }

    QQuickView* view = new QQuickView( QUrl(QStringLiteral("qrc:/MessageBox.qml")), this );
    QQuickItem* item = view->rootObject();
    item->setParentItem( item_ );

    item->findChild<QQuickItem*>("message")->setProperty( "color", colour );
    item->findChild<QQuickItem*>("message")->setProperty( "text", text );

    QTimer* errorTimer = new QTimer( this );
    errorTimer->singleShot( timeout, this, [=](){ if(item) item->deleteLater(); } );

    RETURN();
}

QQuickItem*
Window::qml( QString qmlItem )
{
    ENTER()(qmlItem);
    RETURN( item_->findChild<QQuickItem*>(qmlItem) );
}