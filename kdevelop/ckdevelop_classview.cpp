/***************************************************************************
                          cclassview.cpp  -  implementation
                             -------------------
    begin                : Fri Mar 19 1999
    copyright            : (C) 1999 by Jonas Nordin
    email                : jonas.nordin@cenacle.se
    based on             : ckdevelop_classview.cpp by Sandy Meier
   
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   * 
 *                                                                         *
 ***************************************************************************/

#include "ckdevelop.h"

#include "cclassview.h"
#include "caddclassmethoddlg.h"
#include "caddclassattributedlg.h"
#include <ceditwidget.h>
#include "classparser/ProgrammingByContract.h"

#include <kcombobox.h>
#include <klocale.h>
#include <kmessagebox.h>

/*********************************************************************
 *                                                                   *
 *                              SLOTS                                *
 *                                                                   *
 ********************************************************************/

/*--------------------------------- CKDevelop::slotClassbrowserViewTree()
 * slotClassbrowserViewTree()
 *   Event when the user clicks the graphical view toolbar button.
 *
 * Parameters:
 *   -
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::slotClassbrowserViewTree()
{
  class_tree->viewGraphicalTree();
}

/*--------------------------------- CKDevelop::slotClassChoiceCombo()
 * slotClassChoiceCombo()
 *   Event when the user selects an item in the classcombo.
 *
 * Parameters:
 *   index           Index of the selected item.
 *
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::slotClassChoiceCombo(int index)
{
  CParsedClass *aClass;
  KComboBox* classCombo = toolBar(ID_BROWSER_TOOLBAR)->getCombo(ID_CV_TOOLBAR_CLASS_CHOICE);
  QString classname = classCombo->text( index );

  if ( !classname.isEmpty() )
  {
    aClass = class_tree->store->getClassByName( classname );
    CVRefreshMethodCombo( aClass );
  }
}

/*-------------------------------- CKDevelop::slotMethodChoiceCombo()
 * slotMethodChoiceCombo()
 *   Event when the user selects an item in the methodcombo.
 *
 * Parameters:
 *   index           Index of the selected item.
 *
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::slotMethodChoiceCombo(int index)
{
  KComboBox* classCombo = toolBar(ID_BROWSER_TOOLBAR)->getCombo(ID_CV_TOOLBAR_CLASS_CHOICE);
  KComboBox* methodCombo = toolBar(ID_BROWSER_TOOLBAR)->getCombo(ID_CV_TOOLBAR_METHOD_CHOICE);
  QString classname = classCombo->currentText();
  QString methodname = methodCombo->text( index );

  // Only bother if the methodname is non-empty.
  if( !methodname.isEmpty() )
  {
    // Make sure the next click on the wiz-button switch to declaration.
    cv_decl_or_impl = true;

    // Switch to the method defintin
    CVGotoDefinition( classname, methodname, THCLASS, THPUBLIC_METHOD );
  }
}

/*-------------------------------- CKDevelop::slotCVViewDeclaration()
 * slotCVViewDeclaration()
 *   Event when the user wants a declaration for an item in the tree.
 *
 * Parameters:
 *   index           Index of the item.
 *
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::slotCVViewDeclaration( const char *parentPath, 
                                       const char *itemName, 
                                       THType parentType,
                                       THType itemType )
{
  REQUIRE( "Valid parent path", parentPath != NULL );
  REQUIRE( "Valid item name", itemName != NULL );
  
  CVGotoDeclaration( parentPath, itemName, parentType, itemType );
  CVMethodSelected( itemName );
}

/*-------------------------------- CKDevelop::slotCVViewDefinition()
 * slotCVViewDefinition()
 *   Event when the user wants a definition for an item in the tree.
 *
 * Parameters:
 *   index           Index of the item.
 *
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::slotCVViewDefinition( const char *parentPath, 
                                      const char *itemName, 
                                      THType parentType,
                                      THType itemType )
{
  REQUIRE( "Valid parent path", parentPath != NULL );
  REQUIRE( "Valid item name", itemName != NULL );

  CVGotoDefinition( parentPath, itemName, parentType, itemType );
  CVMethodSelected( itemName );
}

/*---------------------------- CClassView::slotViewClassDeclaration()
 * viewClassDefinition()
 *   Views a declaration of a specified class.
 *
 * Parameters:
 *
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::slotSwitchToFile(const QString & toFile, int toLine)
{
  if (!toFile.isEmpty() && toLine!=-1)
  {
    switchToFile( toFile, toLine );
  }
}

/*-------------------------------------- CKDevelop::slotCVAddMethod()
 * slotCVAddMethod()
 *   Event when the user adds a method to a class. Brings up a dialog
 *   and lets the user fill it out.
 *
 * Parameters:
 *   aClassName      The class to add the method to.
 *
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::slotCVAddMethod( const char *aClassName )
{
  REQUIRE( "Valid class name", aClassName != NULL );

  CAddClassMethodDlg dlg( class_tree, this, "methodDlg");
  
  if (bAutosave)
    saveTimer->stop();
  // Show the dialog and let the user fill it out.
  if( dlg.exec() )
  {
    CParsedMethod *aMethod = dlg.asSystemObj();
    aMethod->setDeclaredInScope( aClassName );

    slotCVAddMethod( aClassName, aMethod );

    delete aMethod;
  }
  if (bAutosave)
    saveTimer->start(saveTimeout);
}

/*-------------------------------------- CKDevelop::slotCVAddMethod()
 * slotCVAddMethod()
 *   Event when the user adds a method to a class. 
 *
 * Parameters:
 *   aClassName      The class to add the method to.
 *   aMethod         The method to add to the class.
 *
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::slotCVAddMethod( const char *aClassName, 
                                 CParsedMethod *aMethod )
{
  REQUIRE( "Valid class name", aClassName != NULL );
  REQUIRE( "Valid method", aMethod != NULL );

  CParsedClass *aClass;
  QString toAdd;
  QString headerCode;
  int atLine = -1;
  CParsedMethod *meth = NULL;

  // Fetch the current class.
  aClass = class_tree->store->getClassByName( aClassName );

  if( aMethod->isSignal )     // Signals
  {
    // Search for a signal with the same export as the one being added.
    for( aClass->signalIterator.toFirst();
         aClass->signalIterator.current();
         ++aClass->signalIterator )
    {
      meth = aClass->signalIterator.current();
      if( meth->exportScope == aMethod->exportScope && 
          atLine < meth->declarationEndsOnLine )
        atLine = meth->declarationEndsOnLine;
    }
  }
  else if( aMethod->isSlot )  // Slots
  {
    // Search for a slot with the same export as the one being added.
    for( aClass->slotIterator.toFirst();
         aClass->slotIterator.current();
         ++aClass->slotIterator )
    {
      meth = aClass->slotIterator.current();
      if( meth->exportScope == aMethod->exportScope && 
          atLine < meth->declarationEndsOnLine )
        atLine = meth->declarationEndsOnLine;
    }
  }
  else                        // Methods
  {
    // Search for a method with the same export as the one being added.
    for( aClass->methodIterator.toFirst();
         aClass->methodIterator.current();
         ++aClass->methodIterator )
    {
      meth = aClass->methodIterator.current();
      if( meth->exportScope == aMethod->exportScope && 
          atLine < meth->declarationEndsOnLine )
        atLine = meth->declarationEndsOnLine;
    }
  }

  // Switch to the .h file.
  CVGotoDeclaration( aClassName, "", THCLASS, THCLASS );  

  aMethod->asHeaderCode( headerCode );

  if( atLine == -1 )
  {
    if( aMethod->isSignal )
      toAdd = "signals: // Signals\n" + headerCode;
    else // Methods and slots
    {
      switch( aMethod->exportScope )
      {
        case PIE_PUBLIC:
          toAdd.sprintf( "public%s: // Public %s\n%s", 
                         aMethod->isSlot ? " slots" : "",
                         aMethod->isSlot ? "slots" : "methods", 
                         headerCode.data() );
          break;
        case PIE_PROTECTED:
          toAdd.sprintf( "protected%s: // Protected %s\n%s", 
                         aMethod->isSlot ? " slots" : "",
                         aMethod->isSlot ? "slots" : "methods", 
                         headerCode.data() );
          break;
        case PIE_PRIVATE:
          toAdd.sprintf( "private%s: // Private %s\n%s", 
                         aMethod->isSlot ? " slots" : "",
                         aMethod->isSlot ? "slots" : "methods", 
                         headerCode.data() );
          break;
        default:
          break;
      }
    }

    atLine = aClass->declarationEndsOnLine;
  }
  else
  {
    toAdd = headerCode;
    atLine++;
  }

  // Add the declaration.
  edit_widget->insertAtLine( toAdd, atLine );
  edit_widget->setCursorPosition( atLine, 0 );
  edit_widget->toggleModified( true );

  // Get the code for the .cpp file.
  aMethod->asCppCode( toAdd );

  // Switch to the .cpp file and add the code if some code was generated.
  if( !toAdd.isEmpty() )
  {
    switchToFile( aClass->definedInFile );

    // Add the code to the file.
    aMethod->asCppCode( toAdd );
    edit_widget->append( toAdd );
    edit_widget->setCursorPosition( edit_widget->lines() - 1, 0 );
    edit_widget->toggleModified( true );
  }
}

/*------------------------------------- CKDevelop::slotCVAddAttribute()
 * slotCVAddAttribute()
 *   Event when the user adds an attribute to a class.
 *
 * Parameters:
 *   aAttr           The attribute to add.
 *
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::slotCVAddAttribute( const char *aClassName )
{
  REQUIRE( "Valid class name", aClassName != NULL );

  CParsedClass *aClass;
  CParsedAttribute *attr = NULL;
  QString toAdd;
  int atLine = -1;
  CAddClassAttributeDlg dlg(this, "attrDlg" );
  CParsedAttribute *aAttr;

  if (bAutosave)
    saveTimer->stop();

  if( !dlg.exec() )
    return;

  aAttr = dlg.asSystemObj();
  aAttr->setDeclaredInScope( aClassName );

  if (bAutosave)
    saveTimer->start(saveTimeout);

  // Fetch the current class.
  aClass = class_tree->store->getClassByName( aClassName );
  
  for( aClass->attributeIterator.toFirst();
       aClass->attributeIterator.current();
       ++aClass->attributeIterator )
  {
    attr = aClass->attributeIterator.current();
    if( attr->exportScope == aAttr->exportScope && 
        atLine < attr->declarationEndsOnLine )
      atLine = attr->declarationEndsOnLine + 1;
  }

  // Switch to the .h file.
  CVGotoDeclaration( aClass->name, "", THCLASS, THCLASS );  

  // Get the code for the new attribute
  aAttr->asHeaderCode( toAdd );

  // If we found an attribute with the same export we don't need to output
  // the label as well.
  if( atLine == -1 )
  {
    switch( aAttr->exportScope )
    {
      case PIE_PUBLIC:
        toAdd = "public: // Public attributes\n" + toAdd;
        break;
      case PIE_PROTECTED:
        toAdd = "protected: // Protected attributes\n" + toAdd;
        break;
      case PIE_PRIVATE:
        toAdd = "private: // Private attributes\n" + toAdd;
        break;
      default:
        break;
    }

    atLine = aClass->declarationEndsOnLine;
  }

  // Add the code to the file.
  edit_widget->insertAtLine( toAdd, atLine );
  edit_widget->setCursorPosition( atLine, 0 );

  // Delete the genererated attribute
  delete aAttr;
}

/*------------------------------------- CKDevelop::slotCVDeleteMethod()
 * slotCVDeleteMethod()
 *   Event when the user wants to delete a method.
 *
 * Parameters:
 *   aClassName      Name of the class that holds the method. NULL
 *                   for global functions.
 *   aMethod         The method to delete.
 *
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::slotCVDeleteMethod( const char *aClassName,
                                    const char *aMethodName )
{
  CParsedClass *aClass;
  CParsedMethod *aMethod;
  int line;

  aClass = class_tree->store->getClassByName( aClassName );

  if( aClass != NULL )
  {
    aMethod = aClass->getMethodByNameAndArg( aMethodName );

    // If we don't find the method, we try to find a slot with the same name.
    if( aMethod == NULL )
      aMethod = aClass->getSlotByNameAndArg( aMethodName );

    // If it isn't a method or a slot we go for a signal. 
    if( aMethod == NULL )
      aMethod = aClass->getSignalByNameAndArg( aMethodName );

    // If everything fails notify the user.
    if( aMethod != NULL )
    {
      if( KMessageBox::questionYesNo( this,
                          i18n("Are you sure you want to delete this method?"),
                          i18n("Delete method")) == KMessageBox::Yes )
      {
        // Start by deleting the declaration.
        switchToFile( aMethod->declaredInFile, aMethod->declaredOnLine );
        edit_widget->deleteInterval( aMethod->declaredOnLine, 
                                     aMethod->declarationEndsOnLine );

        // Comment out the definition if it isn't a signal.
        if( !aMethod->isSignal )
        {
          switchToFile( aMethod->definedInFile, aMethod->definedOnLine );
          for( line = aMethod->definedOnLine; 
               line <= aMethod->definitionEndsOnLine;
               line++ )
            edit_widget->insertAtLine( i18n("//Del by KDevelop: "), line );
        }
      }
    }
    else
        KMessageBox::error( NULL, i18n( "Couldn't find the method to delete." ),
                                i18n( "Method missing" ) );
  }
  else
    KMessageBox::error( NULL, i18n( "Couldn't find the class which has the method to delete." ),
                                i18n( "Class missing" ) );

}

/*********************************************************************
 *                                                                   *
 *                          PRIVATE METHODS                          *
 *                                                                   *
 ********************************************************************/

/*-------------------------------------- CKDevelop::CVClassSelected()
 * CVClassSelected()
 *   The class has been selected, make sure the classcombo updates.
 *
 * Parameters:
 *   aName          Name of the class.
 *
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::CVClassSelected( const char *aName )
{
  KComboBox* classCombo = toolBar(ID_BROWSER_TOOLBAR)->getCombo(ID_CV_TOOLBAR_CLASS_CHOICE);
  bool found = false;
  int i;

  // Only bother if the text has changed.
  if( strcmp( classCombo->currentText(), aName ) != 0 )
  {

    for( i=0; i< classCombo->count() && !found; i++ )
      found = ( strcmp( classCombo->text( i ), aName ) == 0 );
    
    if( found )
    {
      i--;
      classCombo->setCurrentItem( i );
      slotClassChoiceCombo( i );
    }
  }
}

/*-------------------------------------- CKDevelop::CVMethodSelected()
 * CVMethodSelected()
 *   A method has been selected, make sure the methodcombo updates.
 *
 * Parameters:
 *   aName          Name of the method.
 *
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::CVMethodSelected( const char *aName )
{
  KComboBox* methodCombo = toolBar(ID_BROWSER_TOOLBAR)->getCombo(ID_CV_TOOLBAR_METHOD_CHOICE);
  bool found = false;
  int i;

  // Only bother if the text has changed.
  for( i=0; i< methodCombo->count() && !found; i++ )
    found = ( strcmp( methodCombo->text( i ), aName ) == 0 );
    
  if( found )
  {
    i--;
    methodCombo->setCurrentItem( i );
  }
}

/*-------------------------------------- CKDevelop::CVGotoDefinition()
 * CVGotoDefinition()
 *   Goto the definition of the item with the index in the tree.
 *
 * Parameters:
 *   index          Index in the tree.
 *
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::CVGotoDefinition( const char *parentPath, 
                                  const char *itemName, 
                                  THType parentType,
                                  THType itemType )
{
  CParsedContainer *aContainer = NULL;
  CParsedMethod *aMethod = NULL;

  aContainer = CVGetContainer( parentPath, parentType );

  // Get the type of declaration at the index.
  switch( itemType )
  {
    case THPUBLIC_SLOT:
    case THPROTECTED_SLOT:
    case THPRIVATE_SLOT:
      if( aContainer )
        aMethod = ((CParsedClass *)aContainer)->getSlotByNameAndArg( itemName );      
      break;
    case THPUBLIC_METHOD:
    case THPROTECTED_METHOD:
    case THPRIVATE_METHOD:
      if( aContainer )
        aMethod = aContainer->getMethodByNameAndArg( itemName );
      break;
    case THGLOBAL_FUNCTION:
      aMethod = class_tree->store->globalContainer.getMethodByNameAndArg( itemName );
      break;
    default:
      debug( "Unknown type %d in CVGotoDefinition.", itemType );
  }

  if( aMethod )
    switchToFile( aMethod->definedInFile, aMethod->definedOnLine );
  else
    debug( "Couldn't find method %s::%s", parentPath, itemName );
}

/*-------------------------------------- CKDevelop::CVGotoDeclaration()
 * CVGotoDeclaration()
 *   Goto the declaration of the item with the index in the tree.
 *
 * Parameters:
 *   index          Index in the tree.
 *
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::CVGotoDeclaration( const char *parentPath, 
                                   const char *itemName,
                                   THType parentType,
                                   THType itemType )
{
  CParsedContainer *aContainer = NULL;
  CParsedAttribute *aAttr = NULL;
  QString toFile;
  int toLine = -1;

  aContainer = CVGetContainer( parentPath, parentType );

  switch( itemType )
  {
    case THCLASS:
    case THSTRUCT:
    case THSCOPE:
      if( aContainer != NULL )
      {
        toFile = aContainer->declaredInFile;
        toLine = aContainer->declaredOnLine;
      }
      break;
    case THPUBLIC_ATTR:
    case THPROTECTED_ATTR:
    case THPRIVATE_ATTR:
      if( aContainer != NULL )
        aAttr = aContainer->getAttributeByName( itemName );
      break;
    case THPUBLIC_METHOD:
    case THPROTECTED_METHOD:
    case THPRIVATE_METHOD:
      if( aContainer != NULL )
        aAttr = aContainer->getMethodByNameAndArg( itemName );
      break;
    case THPUBLIC_SLOT:
    case THPROTECTED_SLOT:
    case THPRIVATE_SLOT:
      if( aContainer != NULL )
        aAttr = ((CParsedClass *)aContainer)->getSlotByNameAndArg( itemName );
      break;
    case THSIGNAL:
      if( aContainer != NULL )
        aAttr = ((CParsedClass *)aContainer)->getSignalByNameAndArg( itemName );
      break;
    case THGLOBAL_FUNCTION:
      aAttr = class_tree->store->globalContainer.getMethodByNameAndArg( itemName );
      break;
    case THGLOBAL_VARIABLE:
      aAttr = class_tree->store->globalContainer.getAttributeByName( itemName );
      break;
    default:
      debug( "Unknown type %d in CVGotoDeclaration.", itemType );
      break;
  }
  
  // Fetch the line and file from the attribute if the value is set.
  if( aAttr != NULL )
  {
    toFile = aAttr->declaredInFile;
    toLine = aAttr->declaredOnLine;
  }

  if( toLine != -1 )
  {
    debug( "  Switching to file %s @ line %d", toFile.data(), toLine );
    switchToFile( toFile, toLine );
  }
}

/*----------------------------------- CKDevelop::CVRefreshClassCombo()
 * CVRefreshClassCombo()
 *   Update the class combo with all classes.
 *
 * Parameters:
 *   -
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::CVRefreshClassCombo()
{
  CParsedClass *aClass;
  QList<CParsedClass> *classList;
  KComboBox* classCombo = toolBar(ID_BROWSER_TOOLBAR)->getCombo(ID_CV_TOOLBAR_CLASS_CHOICE);
  KComboBox* methodCombo = toolBar(ID_BROWSER_TOOLBAR)->getCombo(ID_CV_TOOLBAR_METHOD_CHOICE);
  QString savedClass;
  int savedIdx = -1;
  int i;

  savedClass = classCombo->currentText();

  // Clear the combos.
  classCombo->clear();

  // Add all classes.
  classList = class_tree->store->getSortedClassList();
  for( aClass = classList->first(),i=0;
       aClass != NULL;
       aClass = classList->next(), i++ )
  {
    classCombo->insertItem(SmallIcon("CVclass"), aClass->name );
    if( aClass->name == savedClass )
      savedIdx = i;
  }
  delete classList;
	
  if (!savedClass.isEmpty())
  {

		// Update the method combo with the class from the classcombo.
		aClass = class_tree->store->getClassByName( savedClass );
		if( aClass && savedIdx != -1 )
		{
			classCombo->setCurrentItem( savedIdx );
			CVRefreshMethodCombo( aClass );
			return;
		}
	}
	methodCombo->clear();
}

/*----------------------------------- CKDevelop::CVRefreshMethodCombo()
 * CVRefreshMethodCombo()
 *   Update the method combo with the methods from the selected
 *   class.
 *
 * Parameters:
 *   aClass           The selected class.
 *
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::CVRefreshMethodCombo( CParsedClass *aClass )
{
  QListBox *lb;
  KComboBox* methodCombo = toolBar(ID_BROWSER_TOOLBAR)->getCombo(ID_CV_TOOLBAR_METHOD_CHOICE);
  QString str;
  QString savedMethod;

  // Save the current value.
  if(methodCombo->count() > 0)
    savedMethod = methodCombo->currentText();

  methodCombo->clear();
  lb = methodCombo->listBox();
  lb->setAutoUpdate( false );

  // Add all methods, slots and signals of this class.
  for( aClass->methodIterator.toFirst(); 
       aClass->methodIterator.current();
       ++aClass->methodIterator )
  {
    aClass->methodIterator.current()->asString( str );
    lb->inSort( str );
  }
  
  for( aClass->slotIterator.toFirst(); 
       aClass->slotIterator.current();
       ++aClass->slotIterator )
  {
    aClass->slotIterator.current()->asString( str );
    lb->inSort( str );
  }

  lb->setAutoUpdate( true );

  // Try to restore the saved value.
  for(int i=0; i<methodCombo->count(); i++ )
  {
    if( savedMethod == methodCombo->text( i ) )
      methodCombo->setCurrentItem( i );
  }
}

/*----------------------------------- CKDevelop::CVRefreshMethodCombo()
 * CVRefreshMethodCombo()
 *   Returns the class for the supplied classname. 
 *
 * Parameters:
 *   className        Name of the class to fetch.
 *
 * Returns:
 *   Pointer to the class or NULL if not found.
 *-----------------------------------------------------------------*/
CParsedContainer *CKDevelop::CVGetContainer( const char *containerPath,
                                             THType containerType )
{
  REQUIRE1( "Valid container path", containerPath != NULL, NULL );
  REQUIRE1( "Valid container path length", strlen( containerPath ) > 0, NULL );

  CParsedContainer *aContainer;

  switch( containerType )
  {
    case THCLASS:
      // Try to fetch the class.
      aContainer = class_tree->store->getClassByName( containerPath );
      
      // If we found the class and it isn't a subclass we update the combo.
      if( aContainer != NULL && aContainer->declaredInScope.isEmpty() )
        CVClassSelected( containerPath );
      break;
    case THSTRUCT:
      aContainer = class_tree->store->globalContainer.getStructByName( containerPath );
      break;
    case THSCOPE:
      aContainer = class_tree->store->globalContainer.getScopeByName( containerPath );
      break;
    default:
      debug( "Didn't find class/struct/scope %s[%d]", containerPath, containerType );
      aContainer = NULL;
      break;
  }

  return aContainer;
}
