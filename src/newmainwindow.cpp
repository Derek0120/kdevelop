/***************************************************************************
 newmainwindow.cpp
                             -------------------
    begin                : october 2003
    copyright            : (C) 2003, 2004 by the KDevelop team
    email                : team@kdevelop.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <qlayout.h>
#include <qmultilineedit.h>
#include <qvbox.h>
#include <qcheckbox.h>
#include <qvaluelist.h>
#include <qobjectlist.h>
#include <qtabbar.h>
#include <qtoolbutton.h>

#include <kdeversion.h>
#include <kapplication.h>
#include <kstdaction.h>
#include <kdebug.h>
#include <kaction.h>
#include <klocale.h>
#include <kmenubar.h>
#include <kconfig.h>
#include <kstatusbar.h>
#include <kdialogbase.h>
#include <kkeydialog.h>
#include <kmessagebox.h>
#include <kiconloader.h>
#include <kdevproject.h>
#include <ktexteditor/view.h>
#include <ktexteditor/document.h>
#include <kmultitabbar.h>
#include <ktabwidget.h>
#include <kparts/part.h>
#include <kdockwidget.h>

#if (KDE_VERSION > 305)
#include <knotifydialog.h>
#endif

#include <kedittoolbar.h>

#include "kdevplugin.h"

#include "projectmanager.h"
#include "plugincontroller.h"
#include "partcontroller.h"
#include "kdevcore.h"
#include "kdevpartcontroller.h"
#include "partselectwidget.h"
#include "api.h"
#include "core.h"
#include "settingswidget.h"
#include "statusbar.h"

#include "kpopupmenu.h"
#include "documentationpart.h"
#include "editorproxy.h"

#include "toplevel.h"
#include "mainwindowshare.h"
#include "newmainwindow.h"

/***************************
WHAT'S NOT WORKING:

Toolview widgets without a name doesn't show in the side toolbars. (Port safety method from old mainwindow)
Culprits are: debugger views, problem reporter, more???

UI mode can now only be changed via a restart. This is intentional!


****************************/

NewMainWindow::NewMainWindow(QWidget *parent, const char *name, KMdi::MdiMode mdimode)
 : KMdiMainFrm(parent, name, mdimode )
// ,m_pWindowMenu(0L)
// ,m_bSwitching(false)
{
    resize( 800, 600 ); // starts kdevelop at 800x600 the first time
  
	// isn't it fun with stuff you have no idea what it does?
    this->setManagedDockPositionModeEnabled(true);
	
	// does this work at all?
    this->setStandardMDIMenuEnabled( false );

    KConfig *config = kapp->config();
    config->setGroup("UI");
    int mdiStyle = config->readNumEntry("MDIStyle", 1);

    this->setToolviewStyle( mdiStyle ); // KDEV3 style of KMultiTabBar
    
    m_pMainWindowShare = new MainWindowShare(this);
    
	m_raiseEditor = new KAction( i18n("Raise &Editor"), ALT+Key_C,
                                 this, SLOT(raiseEditor()),
                                 actionCollection(), "raise_editor");
    m_raiseEditor->setToolTip(i18n("Raise editor"));
    m_raiseEditor->setWhatsThis(i18n("<b>Raise editor</b><p>Focuses the editor."));
	
	//@fixme why is this part of KDevMainWindow?
//    previous_output_view = NULL;
}


void NewMainWindow::init() {

    actionCollection()->setHighlightingEnabled( true );
//    m_windowDynamicMenus.setAutoDelete(true);

    setStandardToolBarMenuEnabled( true );

    if (TopLevel::mode == TopLevel::AssistantMode)
        setXMLFile("kdevassistantui.rc");
    else
        setXMLFile("kdevelopui.rc");

    createFramework();
    createActions();
    createStatusBar();

    createGUI(0);

	QString uimode = QString( "kdevuimode%1rc" ).arg( m_mdiMode );
	KConfig uiConfig( uimode, true );
	readDockConfig( &uiConfig );

    m_pMainWindowShare->init();
	
	// remove the kmdi-created menu
	delete m_pWindowMenu;
	
	// get the xmlgui created one instead
    m_pWindowMenu = static_cast<QPopupMenu*>(main()->child( "window", "KPopupMenu" ));

    if( !m_pWindowMenu )
	{
		kdDebug(9000) << "Couldn't find the XMLGUI window menu. Creating new." << endl;
		
		m_pWindowMenu = new QPopupMenu( main(), "window");
		menuBar()->insertItem(i18n("&Window"),m_pWindowMenu);
	}
	
	actionCollection()->action( "file_close" )->plug( m_pWindowMenu );
	actionCollection()->action( "file_close_all" )->plug( m_pWindowMenu );
	actionCollection()->action( "file_closeother" )->plug( m_pWindowMenu );

	QObject::connect( m_pWindowMenu, SIGNAL(activated(int)), this, SLOT(openURL(int )) );
    QObject::connect( m_pWindowMenu, SIGNAL(aboutToShow()), this, SLOT(fillWindowMenu()) );
			   	   
	menuBar()->setEnabled( false );
	
    if ( PluginController::pluginServices().isEmpty() ) {
        KMessageBox::sorry( this, i18n("Unable to find plugins, KDevelop will not work properly!\nPlease make sure "
                                       "that KDevelop is installed in your KDE directory, otherwise you have to add KDevelop's installation "
                                       "path to the environment variable KDEDIRS and run kbuildsycoca. Restart KDevelop afterwards.\n"
                                       "Example for BASH users:\nexport KDEDIRS=/path/to/kdevelop:$KDEDIRS && kbuildsycoca"),
                            i18n("Could Not Find Plugins") );
    }
	
	connect( Core::getInstance(), SIGNAL(coreInitialized()), this, SLOT(slotCoreInitialized()) );
	
	connect( PartController::getInstance(), SIGNAL(partURLChanged(KParts::ReadOnlyPart * )),
		this, SLOT(slotPartURLChanged(KParts::ReadOnlyPart * )) );
	
	connect( PartController::getInstance(), SIGNAL(documentChangedState(const KURL &, DocumentState)), 
		this, SLOT(documentChangedState(const KURL&, DocumentState )) );


	Core::getInstance()->loadLicenses();

	if ( tabWidget() )
	{
		KConfig *config = kapp->config();
		config->setGroup("UI");
		
		int tabvisibility = config->readNumEntry( "TabWidgetVisibility", KMdi::AlwaysShowTabs );
		setTabWidgetVisibility( (KMdi::TabWidgetVisibility)tabvisibility );
		
		bool CloseOnHover = config->readBoolEntry( "CloseOnHover", false );
		tabWidget()->setHoverCloseButton( CloseOnHover );
		
		bool CloseOnHoverDelay = config->readBoolEntry( "CloseOnHoverDelay", false );
		tabWidget()->setHoverCloseButtonDelayed( CloseOnHoverDelay );
		
		openNewTabAfterCurrent = config->readBoolEntry( "OpenNewTabAfterCurrent", false );
		showTabIcons = config->readBoolEntry( "ShowTabIcons", true );
  
		if (config->readBoolEntry( "ShowCloseTabsButton", true ))
		{
			QToolButton *but = new QToolButton(tabWidget());
			but->setIconSet(SmallIcon("tab_remove"));
			but->adjustSize();
			but->hide();
			connect(but, SIGNAL(clicked()), actionCollection()->action( "file_close" ), SIGNAL(activated()));
			tabWidget()->setCornerWidget(but, TopRight);
		}
		tabWidget()->setTabReorderingEnabled(true);
		connect(tabWidget(), SIGNAL(movedTab(int, int)), this, SLOT(tabMoved(int, int)));
	}
}

NewMainWindow::~NewMainWindow() 
{
	TopLevel::invalidateInstance( this );
}

void NewMainWindow::slotCoreInitialized( )
{
	menuBar()->setEnabled( true );
}

void NewMainWindow::openURL( int id )
{
	QValueList< QPair< int, KURL > >::ConstIterator it = m_windowList.begin();
	while ( it != m_windowList.end() )
	{
		if ( (*it).first == id )
		{
			KURL url( (*it).second );
			if ( !url.isEmpty() )
			{
				PartController::getInstance()->activatePart( PartController::getInstance()->partForURL( url ) );
				return;
			}
		}
		++it;
	}
}

void NewMainWindow::fillWindowMenu()
{
	bool hasWidget = ( PartController::getInstance()->activeWidget() != 0 );	// hmmm... works??
	
	// clear menu
	QValueList< QPair< int, KURL > >::ConstIterator it = m_windowList.begin();
	while ( it != m_windowList.end() )
	{
		m_pWindowMenu->removeItem( (*it).first );
		++it;
	}
	
	int temp = 0;
		
	if	( m_mdiMode == KMdi::ChildframeMode )
	{
		temp = m_pWindowMenu->insertItem(i18n("&Minimize All"), this, SLOT(iconifyAllViews()));
		m_pWindowMenu->setItemEnabled( temp, hasWidget );
		m_windowList << qMakePair( temp, KURL() );
		
		m_windowList << qMakePair( m_pWindowMenu->insertSeparator(), KURL() );
		
		temp = m_pWindowMenu->insertItem(i18n("&Tile..."), m_pPlacingMenu);
		m_windowList << qMakePair( temp, KURL() );
		m_pWindowMenu->setItemEnabled( temp, hasWidget );
	
		m_pPlacingMenu->clear();
		m_pPlacingMenu->insertItem(i18n("Ca&scade Windows"), m_pMdi,SLOT(cascadeWindows()));
		m_pPlacingMenu->insertItem(i18n("Cascade &Maximized"), m_pMdi,SLOT(cascadeMaximized()));
		m_pPlacingMenu->insertItem(i18n("Expand &Vertically"), m_pMdi,SLOT(expandVertical()));
		m_pPlacingMenu->insertItem(i18n("Expand &Horizontally"), m_pMdi,SLOT(expandHorizontal()));
		m_pPlacingMenu->insertItem(i18n("Tile &Non-overlapped"), m_pMdi,SLOT(tileAnodine()));
		m_pPlacingMenu->insertItem(i18n("Tile Overla&pped"), m_pMdi,SLOT(tilePragma()));
		m_pPlacingMenu->insertItem(i18n("Tile V&ertically"), m_pMdi,SLOT(tileVertically()));
	}

	m_windowList << qMakePair( m_pWindowMenu->insertSeparator(), KURL() );
	
	QMap<QString, KURL> map;    
	QStringList string_list;
	KURL::List list = PartController::getInstance()->openURLs();	
	KURL::List::Iterator itt = list.begin();
	while ( itt != list.end() )
	{
		map[(*itt).fileName()] = *itt;
		string_list.append((*itt).fileName());
		++itt;
	}
	string_list.sort();
	
	list.clear();
	for(uint i = 0; i != string_list.size(); ++i)
		list.append(map[string_list[i]]);
	
	itt = list.begin();
	while ( itt != list.end() )
	{
		temp = m_pWindowMenu->insertItem( (*itt).fileName() );
		m_windowList << qMakePair( temp, *itt );
		
		++itt;
	}
}

bool NewMainWindow::queryClose()
{
    saveSettings(); //moved from queryClose so tab settings can be saved while
                    //the components still exist.
    return Core::getInstance()->queryClose();
}

bool NewMainWindow::queryExit()
{
  return true;
}

KMainWindow *NewMainWindow::main() {
    return this;
}

void NewMainWindow::createStatusBar() {
    (void) new KDevStatusBar(this);
//    QMainWindow::statusBar();
}


void NewMainWindow::createFramework() {

    PartController::createInstance( this );

    connect(this, SIGNAL(viewActivated(KMdiChildView*)), this, SLOT(slotViewActivated(KMdiChildView*)) );
    connect(this, SIGNAL(currentChanged(QWidget*)), PartController::getInstance(), SLOT(slotCurrentChanged(QWidget*)));
//    connect(this, SIGNAL(sigCloseWindow(const QWidget *)), PartController::getInstance(),SLOT(slotClosePartForWidget(const QWidget *)));
    connect(PartController::getInstance(), SIGNAL(activePartChanged(KParts::Part*)), this, SLOT(createGUI(KParts::Part*)));
}

void NewMainWindow::slotViewActivated(KMdiChildView* child)
{
    kdDebug(9000) << "======> view activated: " << child << endl;
    if( !child || !child->focusedChildWidget() )
        return;
    
    emit currentChanged( child->focusedChildWidget() );
}

void NewMainWindow::createActions() 
{
    m_pMainWindowShare->createActions();

    connect(m_pMainWindowShare, SIGNAL(gotoNextWindow()), this, SLOT(activateNextWin()) );
    connect(m_pMainWindowShare, SIGNAL(gotoPreviousWindow()), this, SLOT(activatePrevWin()) );
    connect(m_pMainWindowShare, SIGNAL(gotoFirstWindow()), this, SLOT(activateFirstWin()) );
    connect(m_pMainWindowShare, SIGNAL(gotoLastWindow()), this, SLOT(activateLastWin()) );
}

void NewMainWindow::embedPartView(QWidget *view, const QString &name, const QString& ) 
{
	if( !view ) return;

    QString shortName = name;
    shortName = shortName.right( shortName.length() - (shortName.findRev('/') +1));

    KMdiChildView * child = createWrapper(view, name, shortName);
    
    if (showTabIcons)
    {
        const QPixmap* wndIcon = view->icon();
        if (!wndIcon || (wndIcon && (wndIcon->size().height() > 16)))
            child->setIcon(SmallIcon("kdevelop")); // was empty or too big, take something useful
    }
    else
        child->setIcon(QPixmap());

    unsigned int mdiFlags = KMdi::StandardAdd | KMdi::UseKMdiSizeHint;
    int tabIndex = -1;
    if ( tabWidget() && openNewTabAfterCurrent)
        if (tabWidget()->count() > 0)
            tabIndex = tabWidget()->currentPageIndex() + 1;
    addWindow(child, mdiFlags, tabIndex);
}

void NewMainWindow::embedSelectView(QWidget *view, const QString &name, const QString &toolTip) 
{
	embedView( KDockWidget::DockLeft, view, name, toolTip );
}

void NewMainWindow::embedSelectViewRight ( QWidget* view, const QString& name, const QString &toolTip) 
{
	embedView( KDockWidget::DockRight, view, name, toolTip );
//	if (TopLevel::mode == TopLevel::AssistantMode)
//		raiseView(view);
}

void NewMainWindow::embedOutputView(QWidget *view, const QString &name, const QString &toolTip) 
{
	embedView( KDockWidget::DockBottom, view, name, toolTip );
}

KDockWidget::DockPosition NewMainWindow::recallToolViewPosition( const QString & widgetName, KDockWidget::DockPosition defaultPos )
{
	KConfig * config = kapp->config();
	config->setGroup( "ToolDockPosition" );

	QString position = config->readEntry( widgetName, "DockNone" );

	if ( position == "DockLeft" ) return KDockWidget::DockLeft;
	if ( position == "DockRight" ) return KDockWidget::DockRight;
	if ( position == "DockBottom" ) return KDockWidget::DockBottom;
	if ( position == "DockTop" ) return KDockWidget::DockTop;

	return defaultPos;
}

void NewMainWindow::rememberToolViewPosition( const QString & widgetName, KDockWidget::DockPosition pos )
{
	KConfig * config = kapp->config();
	config->setGroup( "ToolDockPosition" );

	QString position = "DockNone";

	switch( pos )
	{
		case KDockWidget::DockLeft:
			position = "DockLeft";
			break;
		case KDockWidget::DockRight:
			position = "DockRight";
			break;
		case KDockWidget::DockBottom:
			position = "DockBottom";
			break;
		case KDockWidget::DockTop:
			position = "DockTop";
			break;
		default: ;
	}

	config->writeEntry( widgetName, position );
}

void NewMainWindow::embedView( KDockWidget::DockPosition pos, QWidget *view, const QString &name, const QString &toolTip)
{
	if( !view ) return;

	if ( !m_availableToolViews.contains( view ) && !m_unAvailableToolViews.contains( view ) )
	{
		pos = recallToolViewPosition( view->name(), pos );	// we have a new view. where should it go?
	}
	
	KMdiMainFrm::addToolWindow( view, pos, getMainDockWidget(), 20, toolTip, name );
	m_availableToolViews.insert( view, ToolViewData( pos, name, toolTip ) );
}

void NewMainWindow::childWindowCloseRequest( KMdiChildView * childView )
{
	const QPtrList<KParts::Part> * partlist = PartController::getInstance()->parts();
	QPtrListIterator<KParts::Part> it( *partlist );
	while ( KParts::Part* part = it.current() )
	{
		QWidget * widget = EditorProxy::getInstance()->topWidgetForPart( part );
		if ( widget && widget->parentWidget() == childView )
		{
			PartController::getInstance()->closePart( part );
			return;
		}
		++it;
	}
}

static KDockWidget::DockPosition getDockWidgetDockingBorder( QWidget * w )
{
	int depth = 0;
	while( w && depth < 10 )
	{
		if ( KDockWidget * dockWidget = dynamic_cast<KDockWidget*>( w ) )
		{
			if ( dockWidget->currentDockPosition() == KDockWidget::DockLeft ||
				dockWidget->currentDockPosition() == KDockWidget::DockRight ||
				dockWidget->currentDockPosition() == KDockWidget::DockBottom ||
				dockWidget->currentDockPosition() == KDockWidget::DockTop )
			{
				return dockWidget->currentDockPosition();
			}
		}
		depth++;
		w = w->parentWidget();
	}
	return KDockWidget::DockNone;
}

void NewMainWindow::removeView( QWidget * view )
{
    kdDebug(9000) << k_funcinfo << " - view: " << view << endl;
    
	if( !view || !view->parentWidget() )
        return;
    
	kdDebug(9000) << "parentWidget: " << view->parentWidget() << endl;

	if( KMdiChildView * childView = static_cast<KMdiChildView*>(view->parentWidget()->qt_cast("KMdiChildView")) ) 
	{
		(void) view->reparent(0, QPoint(0,0), false );
		closeWindow( childView );
	} 
	else if( view->parentWidget()->qt_cast("KDockWidget") ) 
	{
		rememberToolViewPosition( view->name(), getDockWidgetDockingBorder( view ) );
		(void) view->reparent(0, QPoint(0,0), false );
		deleteToolWindow( view );
	}
}

void NewMainWindow::setViewAvailable(QWidget * view, bool bEnabled) 
{
	if ( !view ) return;

	if ( bEnabled )
	{
		if ( m_availableToolViews.contains( view ) ) return; // already visible

		if ( m_unAvailableToolViews.contains( view ) )
		{
			ToolViewData t = m_unAvailableToolViews[ view ];
			m_unAvailableToolViews.remove( view );

			KMdiMainFrm::addToolWindow( view, t.position, getMainDockWidget(), 20, t.toolTip, t.name );
			m_availableToolViews.insert( view, t );
		}
	}
	else
	{
		if ( m_unAvailableToolViews.contains( view ) ) return; // already hidden

		if ( m_availableToolViews.contains( view ) )
		{
			ToolViewData t = m_availableToolViews[ view ];
			m_availableToolViews.remove( view );

			KDockWidget::DockPosition pos = getDockWidgetDockingBorder( view );
			t.position = ( pos != KDockWidget::DockNone ? pos : t.position );	// the view might have changed position

			removeView( view );
			m_unAvailableToolViews.insert( view, t );
		}
	}
}

void NewMainWindow::raiseView(QWidget *view) 
{
	kdDebug(9000) << k_funcinfo << endl;
    
    if( !view || !view->parentWidget() )
        return;

    view->parentWidget()->setFocus();

    if( QGuardedPtr<KDockWidget> dockWidget = static_cast<KDockWidget*>(view->parentWidget()->qt_cast("KDockWidget")) ) {
        if( !dockWidget->isVisible() )
            makeDockVisible( dockWidget );
    }
}


void NewMainWindow::lowerView(QWidget *)
{
  // seems there's nothing to do here
}


void NewMainWindow::createGUI(KParts::Part *part) {
    if ( !part )
        setCaption( QString::null );
    KMdiMainFrm::createGUI(part);

    m_pMainWindowShare->slotGUICreated( part );
}


void NewMainWindow::loadSettings() {
    KConfig *config = kapp->config();

    ProjectManager::getInstance()->loadSettings();
    applyMainWindowSettings(config, "Mainwindow");
}

void NewMainWindow::setUserInterfaceMode( const QString & )
{
/*	
    // immediately switch the mode likely set in the uimode part
    if (uiMode == "Childframe") {
	switchToChildframeMode();
    }
    else if (uiMode == "TabPage") {
	switchToTabPageMode();
    }
    else if (uiMode == "Toplevel") {
	switchToToplevelMode();
    }
    else if (uiMode == "KMDI-IDEAl") {
        switchToIDEAlMode();
    }
*/	
}

void NewMainWindow::saveSettings() 
{
    KConfig *config = kapp->config();

    ProjectManager::getInstance()->saveSettings();
    saveMainWindowSettings(config, "Mainwindow");

	QString uimode = QString( "kdevuimode%1rc" ).arg( m_mdiMode );
	KConfig uiConfig( uimode );
	writeDockConfig( &uiConfig );

	QValueList<QWidget*> widgetList = m_pToolViews->keys();
	QValueList<QWidget*>::Iterator it = widgetList.begin();
	while( it != widgetList.end() )
	{
		rememberToolViewPosition( (*it)->name(), getDockWidgetDockingBorder( *it ) );	
		++it;
	}
}

void NewMainWindow::raiseEditor( )
{
	kdDebug() << k_funcinfo << endl;
	
	KDevPartController * partcontroller = API::getInstance()->partController();
	if ( partcontroller->activePart() && partcontroller->activePart()->widget() )
	{
		partcontroller->activePart()->widget()->setFocus();
	}
}

void NewMainWindow::setCaption( const QString & caption )
{
	KDevProject * project = API::getInstance()->project();
	if ( project /*&& !caption.isEmpty()*/ )
	{
		QString projectname = project->projectName();

		QString suffix(".kdevelop");
		if ( projectname.endsWith( suffix ) )
		{
			projectname.truncate( projectname.length() - suffix.length() );
		}

		KMdiMainFrm::setCaption( projectname + " - " + caption );
	}
	else
	{
		KMdiMainFrm::setCaption( caption );
	}
}

//@fixme - not currently connected - should it be?
void NewMainWindow::projectOpened() 
{
	// why would we reload mainwindow settings on project load?
	loadSettings();
}

void NewMainWindow::prepareToCloseViews()
{
  // seems there's nothing to do here
}

void NewMainWindow::guiRestoringFinished()
{
  // seems there's nothing to do here
}

void NewMainWindow::storeOutputViewTab( )
{
  // seems there's nothing to do here
}

void NewMainWindow::restoreOutputViewTab( )
{
  // seems there's nothing to do here
}

void NewMainWindow::lowerAllViews( )
{
  // seems there's nothing to do here
}

void NewMainWindow::slotPartURLChanged( KParts::ReadOnlyPart * ro_part )
{
	kdDebug() << k_funcinfo << endl;
	
	if ( QWidget * widget = EditorProxy::getInstance()->topWidgetForPart( ro_part ) )
	{
		KMdiChildView * childView = dynamic_cast<KMdiChildView*>( widget->parentWidget() );
		if ( childView )
		{
			childView->setMDICaption( ro_part->url().fileName() );
		}
	}
}

void NewMainWindow::documentChangedState( const KURL & url, DocumentState state )
{
	QWidget * widget = EditorProxy::getInstance()->topWidgetForPart( PartController::getInstance()->partForURL( url ) );
	if ( widget )
	{
		//calculate the icon size if showTabIcons is false
		//this is necessary to avoid tab resizing by setIcon() call
		int isize = 16;
		if (tabWidget() && !showTabIcons)
		{
			isize = tabWidget()->fontMetrics().height() - 1;
//			kdDebug() << "size: " << isize << endl;
			isize = isize > 16 ? 16 : isize;
		}
		switch( state )
		{
			// we should probably restore the original icon instead of just using "kdevelop",
			// but I have never seen any other icon in use so this should do for now
			case Clean:
				if (showTabIcons)
					widget->setIcon( SmallIcon("kdevelop", isize));
				else    
					widget->setIcon(QPixmap());
				break;
			case Modified:
				widget->setIcon( SmallIcon("filesave", isize));
				break;
			case Dirty:
				widget->setIcon( SmallIcon("revert", isize));
				break;
			case DirtyAndModified:
				widget->setIcon( SmallIcon("stop", isize));
				break;
		}
	}
}

void NewMainWindow::tabMoved( int from, int to )
{
    KMdiChildView *view = m_pDocumentViews->at(from);
    m_pDocumentViews->remove(from);
    m_pDocumentViews->insert(to, view);
}

#include "newmainwindow.moc"
