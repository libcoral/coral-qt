/*
 * Coral Qt Module
 * See copyright notice in LICENSE.md
 */

#include "EventHub.h"
#include "System_Base.h"
#include "ConnectionHub.h"
#include "TimerCallbackNotifier.h"
#include "AbstractItemModel.h"

#include <co/NotSupportedException.h>
#include <co/IllegalArgumentException.h>

#include <qt/Exception.h>
#include <qt/ItemDataRole.h>
#include <qt/IAbstractItemModel.h>
#include <qt/IAbstractItemModelDelegate.h>
#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QStringListModel>
#include <QStackedLayout>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QMessageBox>
#include <QFileDialog>
#include <QStatusBar>
#include <QBoxLayout>
#include <QFileInfo>
#include <QSplitter>
#include <QUiLoader>
#include <QToolBar>
#include <QAction>
#include <QLayout>
#include <QCursor>
#include <QLayout>
#include <QWidget>
#include <QFile>
#include <QMenu>

#include <sstream>

namespace {
	int dummy_argc = 1;
	const char* dummy_argv[] = { "", "" };
}

#define INSERT_WIDGET( element, pos, widget ) \
	if( pos >= 0 ) \
	{ \
		element->insertWidget( pos, widget ); \
	} \
	else \
	{ \
		element->addWidget( widget ); \
	}

namespace qt {

class System : public qt::System_Base
{
public:
	System() // must force _app initialization before _eventHub since _eventHub uses Qt qApp in constructor
		: _app( new QApplication( dummy_argc, const_cast<char**>( dummy_argv ) ) ), _eventHub()
	{
		_appObj.set( _app );
	}

	virtual ~System()
	{
		delete _app;
	}

	const qt::Object& getApp() { return _appObj; }

	void loadUi( const std::string& filePath, const qt::Object& parent, qt::Object& widget )
	{
		QWidget* parentWidget = 0;
		if( parent.get() )
		{
			parentWidget = qobject_cast<QWidget*>( parent.get() );
			if( !parentWidget )
				CORAL_THROW( co::IllegalArgumentException, "cannot set parent widget: 'parent' is not an instace of QWidget" );
		}

		QUiLoader loader;

		QFile uiFile( filePath.c_str() );
		if( !uiFile.exists() )
			CORAL_THROW( qt::Exception, "could not open '" << filePath << "'" );

		// change app work directory to ui's base directory
		// so the ui loader can find relative icon paths
		QString savedWorkDir = QDir::currentPath();

		QFileInfo fi( filePath.c_str() );
		QDir::setCurrent( fi.absolutePath() );

		QWidget* resWidget = loader.load( &uiFile, NULL );

		// restore workdir
		QDir::setCurrent( savedWorkDir );

		if( !resWidget )
		{
			CORAL_THROW( qt::Exception, "error loading ui file '" << filePath << "'"  );
		}

		resWidget->setParent( parentWidget );
		widget.set( resWidget );
	}

	void setSearchPaths( const std::string& prefix, co::Range<std::string const> searchPaths )
	{
		QStringList qtSearchPaths;
		while( searchPaths )
		{
			std::string path = searchPaths.getFirst();
			qtSearchPaths.push_back( path.c_str() );
			searchPaths.popFirst();
		}

		QDir::setSearchPaths( prefix.c_str(), qtSearchPaths );
	}

	void getExistingDirectory( const qt::Object& parent, const std::string& caption, const std::string& initialDir,
							   std::string& selectedDir )
	{
		QString dir = QFileDialog::getExistingDirectory( qobject_cast<QWidget*>( parent.get() ),
														 QString::fromStdString( caption ),
														 QString::fromStdString( initialDir ) );
		selectedDir = dir.toStdString();
	}

	void getOpenFileNames( const qt::Object& parent, const std::string& caption, const std::string& initialDir,
						   const std::string& filter, std::vector<std::string>& selectedFiles )
	{
		QStringList files = QFileDialog::getOpenFileNames( qobject_cast<QWidget*>( parent.get() ),
														 caption.c_str(), initialDir.c_str(), filter.c_str() );
		for( int i = 0; i < files.size(); ++i )
		{
			selectedFiles.push_back( files[i].toStdString() );
		}
	}

	void newInstanceOf( const std::string& className, const qt::Object& parent, qt::Object& object )
	{
		QUiLoader loader;
		QString name = className.c_str();
		QWidget* parentWidget = qobject_cast<QWidget*>( parent.get() );
		// check whether the className is a supported widget
		if( loader.availableWidgets().contains( name, Qt::CaseInsensitive ) )
		{
			object.set( loader.createWidget( name, parentWidget ) );
		}
		else if( loader.availableLayouts().contains( name, Qt::CaseInsensitive ) )
		{
			object.set( loader.createLayout( name, parentWidget ) );
		}
		else if( name == "QAction" )
		{
			object.set( loader.createAction( parentWidget ) );
		}
		else if( name == "QActionGroup" )
		{
			object.set( new QActionGroup( parentWidget ) );
		}
		else if( name == "QMessageBox" )
		{
			object.set( new QMessageBox( parentWidget ) );
		}
		else
		{
			CORAL_THROW( co::NotSupportedException,
						 "cannot create new instance for class '" << className << "': class not supported" );
		}
	}

	void addWidget( const qt::Object& parent, const qt::Object& widget )
	{
		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qwidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot add widget: 'widget' is not an instace of QWidget" );

		QLayout* qlayout = qobject_cast<QLayout*>( parent.get() );
		QSplitter* qsplitter = qobject_cast<QSplitter*>( parent.get() );
		if( qlayout )
			qlayout->addWidget( qwidget );
		else if( qsplitter )
			qsplitter->addWidget( qwidget );
		else
			CORAL_THROW( co::IllegalArgumentException, "cannot add widget: 'parent' is not an instace of QLayout nor QSplitter classs" );
	}

	void insertWidget( const qt::Object& parent, co::int32 beforeIndex, const qt::Object& widget )
	{
		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qwidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot add widget: 'widget' is not an instace of QWidget" );

		QSplitter* qsplitter = qobject_cast<QSplitter*>( parent.get() );
		QBoxLayout* qlayout = qobject_cast<QBoxLayout*>( parent.get() );
		QStatusBar* qstatusBar = qobject_cast<QStatusBar*>( parent.get() );
		QStackedLayout* qstackedLayout = qobject_cast<QStackedLayout*>( parent.get() );
		if( qsplitter )
		{
			INSERT_WIDGET( qsplitter, beforeIndex, qwidget );
		}
		else if( qlayout )
		{
			INSERT_WIDGET( qlayout, beforeIndex, qwidget );
		}
		else if( qstatusBar )
		{
			if( beforeIndex >= 0 )
			{
				qstatusBar->insertPermanentWidget( beforeIndex, qwidget );
			}
			else
			{
				qstatusBar->addPermanentWidget( qwidget );
			}
		}
		else if( qstackedLayout )
		{
			INSERT_WIDGET( qstackedLayout, beforeIndex, qwidget );
		}
		else
			CORAL_THROW( co::IllegalArgumentException, "cannot insert widget: 'parent' is not an instace of QSplitter, "
													   "QBoxLayout, QStatusBar nor QStackedLayout classs" );
	}

	void removeWidget( const qt::Object& parent, const qt::Object& widget )
	{
		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qwidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot remove widget: 'widget' is not an instace of QWidget" );

		QBoxLayout* qlayout = qobject_cast<QBoxLayout*>( parent.get() );
		QStatusBar* qstatusBar = qobject_cast<QStatusBar*>( parent.get() );
		QStackedLayout* qstackedLayout = qobject_cast<QStackedLayout*>( parent.get() );
		if( qlayout )
			qlayout->removeWidget( qwidget );
		else if( qstatusBar )
			qstatusBar->removeWidget( qwidget );
		else if( qstackedLayout )
			qstackedLayout->removeWidget( qwidget );
		else
			CORAL_THROW( co::IllegalArgumentException, "cannot remove widget: 'parent' is not an instace of "
													   "QBoxLayout, QStatusBar nor QStackedLayout classs" );

		qwidget->setParent( 0 );
	}

	void addDockWidget( const qt::Object& mainWindow, co::int32 dockArea, const qt::Object& dockWidget )
	{
		QMainWindow* qMainWindow = qobject_cast<QMainWindow*>( mainWindow.get() );
		if( !qMainWindow )
			CORAL_THROW( co::IllegalArgumentException, "cannot add QDockWidget: 'mainWindow' is not an instace of QMainWindow" );

		QDockWidget* qDockWidget = qobject_cast<QDockWidget*>( dockWidget.get() );
		if( !qDockWidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot add QDockWidget: 'dockWidget' is not an instace of QDockWidget" );

		Qt::DockWidgetArea qDockArea = static_cast<Qt::DockWidgetArea>( dockArea );
		qMainWindow->addDockWidget( qDockArea, qDockWidget );
	}

	void setWidget( const qt::Object& dockWidget, const qt::Object& widget )
	{
		QDockWidget* qDockWidget = qobject_cast<QDockWidget*>( dockWidget.get() );
		if( !qDockWidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot set widget: 'dockWidget' is not an instace of QDockWidget" );

		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qDockWidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot set widget: 'widget' is not an instace of QWidget" );

		qDockWidget->setWidget( qwidget );
	}

	void setLayout( const qt::Object& widget, const qt::Object& layout )
	{
		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qwidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot set layout: 'widget' is not an instace of QWidget" );

		QLayout* qlayout = qobject_cast<QLayout*>( layout.get() );
		if( !qlayout )
			CORAL_THROW( co::IllegalArgumentException, "cannot set layout: 'layout' is not an instace of QLayout" );

		qwidget->setLayout( qlayout );
	}

	void getLayout( const qt::Object& widget, qt::Object& layout )
	{
		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qwidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot get layout: 'widget' is not an instace of QWidget" );

		layout.set( qwidget->layout() );
	}
	
	void addActionIntoGroup( const qt::Object& actionGroup, const qt::Object& action )
	{
		QActionGroup* qActionGroup = qobject_cast<QActionGroup*>( actionGroup.get() );
		if( !qActionGroup )
			CORAL_THROW( co::IllegalArgumentException, "cannot insert action into group: 'actionGroup' is not an instance of QActionGroup" );
		
		QAction* qAction = qobject_cast<QAction*>( action.get() );
		if( !qAction )
			CORAL_THROW( co::IllegalArgumentException, "cannot insert action into group: 'action' is not an instance of QAction" );
		
		qActionGroup->addAction( qAction );
	}

	void insertAction( const qt::Object& widget, co::int32 beforeActionIndex, const qt::Object& action )
	{
		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qwidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot insert action: 'widget' is not an instace of QWidget" );

		QAction* qaction = qobject_cast<QAction*>( action.get() );
		if( !qaction )
			CORAL_THROW( co::IllegalArgumentException, "cannot insert action: 'action' is not an instace of QAction" );

		if( beforeActionIndex >= 0 )
		{
			QList<QAction*> actions = qwidget->actions();
			if( beforeActionIndex >= actions.size() )
				CORAL_THROW( qt::Exception, "cannot insert action: 'beforeActionIndex' out-of-bounds" );

			qwidget->insertAction( actions[beforeActionIndex], qaction );
		}
		else
			qwidget->addAction( qaction );
	}

	void removeAction( const qt::Object& widget, const qt::Object& action )
	{
		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qwidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot remove action: 'widget' is not an instace of QWidget" );

		QAction* qaction = qobject_cast<QAction*>( action.get() );
		if( !qaction )
			CORAL_THROW( co::IllegalArgumentException, "cannot remove action: 'action' is not an instace of QAction" );

		qwidget->removeAction( qaction );
	}

	void makeSeparator( const qt::Object& action )
	{
		QAction* qaction = qobject_cast<QAction*>( action.get() );
		if( !qaction )
			CORAL_THROW( co::IllegalArgumentException, "cannot make separator: 'action' is not an instace of QAction" );

		qaction->setSeparator( true );
	}

	void setMenu( const qt::Object& action, const qt::Object& menu )
	{
		QAction* qaction = qobject_cast<QAction*>( action.get() );
		if( !qaction )
			CORAL_THROW( co::IllegalArgumentException, "cannot set menu: 'action' is not an instace of QAction" );

		QMenu* qmenu = qobject_cast<QMenu*>( menu.get() );
		if( !qmenu )
			CORAL_THROW( co::IllegalArgumentException, "cannot set menu: 'menu' is not an instace of QMenu" );

		qaction->setMenu( qmenu );
	}

	void execMenu( const qt::Object& menu, co::int32 posX, co::int32 posY, qt::Object& selectedAction )
	{
		QMenu* qmenu = qobject_cast<QMenu*>( menu.get() );
		if( !qmenu )
			CORAL_THROW( co::IllegalArgumentException, "'menu' is not an instance of QMenu." );

		QPoint p( posX, posY );
		if( posX < 0 || posY < 0 )
			p = QCursor::pos();

		QAction* selected = qmenu->exec( p );
		selectedAction.set( selected );
	}

	void setCursor( const qt::Object& widget, co::int32 cursor )
	{
		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qwidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot set cursor: 'widget' is not an instance of QWidget." );

		qwidget->setCursor( QCursor( Qt::CursorShape( cursor ) ) );
	}

	void unsetCursor( const qt::Object& widget )
	{
		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qwidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot set cursor: 'widget' is not an instance of QWidget." );

		qwidget->unsetCursor();
	}

	void setCursorPosition( const qt::Object& widget, co::int32 posX, co::int32 posY )
	{
		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qwidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot set cursor position: 'widget' is not an instance of QWidget." );

		qwidget->cursor().setPos( QPoint( posX, posY ) );
	}

	void getCursorPosition( const qt::Object& widget, co::int32& posX, co::int32& posY )
	{
		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qwidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot get cursor position: 'widget' is not an instance of QWidget." );

		QPoint pos = qwidget->cursor().pos();
		posX = pos.x();
		posY = pos.y();
	}

	void mapFromGlobal( const qt::Object& widget, co::int32& posX, co::int32& posY )
	{
		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qwidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot get cursor position: 'widget' is not an instance of QWidget." );

		QPoint pos = qwidget->mapFromGlobal( QPoint( posX, posY ) );
		posX = pos.x();
		posY = pos.y();
	}

	void mapToGlobal( const qt::Object& widget, co::int32& posX, co::int32& posY )
	{
		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qwidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot get cursor position: 'widget' is not an instance of QWidget." );

		QPoint pos = qwidget->mapToGlobal( QPoint( posX, posY ) );
		posX = pos.x();
		posY = pos.y();
	}

	void assignModelToView( qt::Object& view, qt::IAbstractItemModel* model )
	{
		QAbstractItemView* qtView = qobject_cast<QAbstractItemView*>( view.get() );
		if( !qtView )
			CORAL_THROW( co::IllegalArgumentException,
						 "cannot assign model to view: 'view' object is not a subclass of QAbstractItemView" );

		QAbstractItemModel* qtModel = dynamic_cast<QAbstractItemModel*>( model );
		if( !qtModel )
			CORAL_THROW( co::IllegalArgumentException,
						 "cannot assign model to view: 'model' object is not a subclass of QAbstractItemModel" );

		qtView->setModel( qtModel );

		// connect AbstractItemView slots to model signals (to allow signal forwarding to delegate of IAbstractItemModel)
		QObject::connect( qtModel, SIGNAL( dataChanged( QModelIndex,QModelIndex )), qtView, SLOT( dataChanged(QModelIndex,QModelIndex) ) );
		QObject::connect( qtView, SIGNAL( activated( const QModelIndex& ) ), qtModel, SLOT( activated( const QModelIndex& ) ) );
		QObject::connect( qtView, SIGNAL( clicked( const QModelIndex& ) ), qtModel, SLOT( clicked( const QModelIndex& ) ) );
		QObject::connect( qtView, SIGNAL( doubleClicked( const QModelIndex& ) ), qtModel, SLOT( doubleClicked( const QModelIndex& ) ) );
		QObject::connect( qtView, SIGNAL( entered( const QModelIndex& ) ), qtModel, SLOT( entered( const QModelIndex& ) ) );
		QObject::connect( qtView, SIGNAL( pressed( const QModelIndex& ) ), qtModel, SLOT( pressed( const QModelIndex& ) ) );
	}

	void getModelFromView( const qt::Object& view, qt::IAbstractItemModel*& model  )
	{
		QAbstractItemView* qtView = qobject_cast<QAbstractItemView*>( view.get() );
		if( !qtView )
			CORAL_THROW( co::IllegalArgumentException,
						 "cannot retrieve model from view: 'view' object is not a subclass of QAbstractItemView" );

		model = dynamic_cast<qt::IAbstractItemModel*>( qtView->model() );
	}

	co::int64 installEventHandler( const qt::Object& watched, qt::IEventHandler* handler )
	{
		return _eventHub.installEventHandler( watched, handler );
	}

	void grabMouse( const qt::Object& widget )
	{
		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qwidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot grab mouse: 'widget' is not an instance of QWidget." );

		qwidget->grabMouse();
	}

	void releaseMouse( const qt::Object& widget )
	{
		QWidget* qwidget = qobject_cast<QWidget*>( widget.get() );
		if( !qwidget )
			CORAL_THROW( co::IllegalArgumentException, "cannot release mouse: 'widget' is not an instance of QWidget." );

		qwidget->releaseMouse();
	}

	void addTimerCallback( qt::ITimerCallback* callback )
	{
		if( callback && _callbackNotifier.isEmpty() )
		{
			// Needs to be less than 60 hertz or freezes on Windows since
			// qt 4.7.0 has a reported bug on timer callbacks on Windows
			_callbackNotifier.start( 1000.0 / 60.0 );
		}

		_callbackNotifier.addCallback( callback );
	}

	void removeTimerCallback( qt::ITimerCallback* callback )
	{
		_callbackNotifier.removeCallback( callback );

		if( _callbackNotifier.isEmpty() )
			_callbackNotifier.stop();
	}

	co::int32 connect( const qt::Object& sender, const std::string& signal, qt::IConnectionHandler* handler )
	{
		return _connectionHub.connect( sender, signal, handler );
	}

	void disconnect( co::int32 cookie )
	{
		_connectionHub.disconnect( cookie );
	}
	
	void exec()
	{
		_app->exec();
	}

	void processEvents()
	{
		_app->processEvents();
	}

	void quit()
	{
		_app->quit();
	}

private:
	QApplication* _app;
	qt::Object _appObj;
	EventHub _eventHub;
	ConnectionHub _connectionHub;
	TimerCallbackNotifier _callbackNotifier;
};

CORAL_EXPORT_COMPONENT( System, System )

} // namespace qt

