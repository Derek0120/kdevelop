/***************************************************************************
                          kdlgedit.cpp  -  description                              
                             -------------------                                         
    begin                : Thu Mar 18 1999                                           
    copyright            : (C) 1999 by Pascal Krahmer
    email                : pascal@beast.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   * 
 *                                                                         *
 ***************************************************************************/

#include <qdir.h>
#include <qfileinfo.h>
#include "kdlgedit.h"
#include "../ckdevelop.h"
#include "kdlgeditwidget.h"
#include "kdlgitems.h"
#include "kdlgpropwidget.h"
#include "items.h"
#include "kdlgotherdlgs.h"
#include "kdlgnewdialogdlg.h"
#include "kdlgdialogs.h"
#include "../cgeneratenewfile.h"

KDlgEdit::KDlgEdit(QObject *parentz, const char *name) : QObject(parentz,name)
{
   
   connect(((CKDevelop*)parent())->kdlg_get_dialogs_view(),SIGNAL(kdlgdialogsSelected(QString)),
	   SLOT(slotOpenDialog(QString)));
}

KDlgEdit::~KDlgEdit()
{
}


void KDlgEdit::slotFileNew(){
  CProject* prj = ((CKDevelop*)parent())->getProject(); 
  TDialogFileInfo info;
  if(prj != 0){
    KDlgNewDialogDlg dlg(((QWidget*) parent()),"I",prj);
    if( dlg.exec()){
      // get the location
      QString location = dlg.getLocation();
      if(location.right(1) != "/"){
	location = location + "/";
      }
      info.rel_name = prj->getSubDir() + dlg.getClassname().lower() + ".kdevdlg";
      info.dist = true;
      info.install = false;
      info.classname = dlg.getClassname();
      info.baseclass = dlg.getBaseClass();
      info.header_file = getRelativeName(location + dlg.getHeaderName());
      info.source_file = getRelativeName(location + dlg.getSourceName());
      info.data_file = getRelativeName(location + dlg.getDataName());
      info.is_toplevel_dialog = true;

      dialog_file = prj->getProjectDir() + info.rel_name;

      if(prj->addDialogFileToProject(info.rel_name,info)){
	((CKDevelop*)parent())->newSubDir();
      }
      ((CKDevelop*)parent())->kdlg_get_edit_widget()->saveToFile(dialog_file);
      
      // registrate the source files
      ((CKDevelop*)parent())->slotAddFileToProject(location + dlg.getHeaderName());
      ((CKDevelop*)parent())->slotAddFileToProject(location + dlg.getSourceName());
      ((CKDevelop*)parent())->slotAddFileToProject(location + dlg.getDataName());

      // generate the new files;
      // header
      generateInitialHeaderFile(info);
      generateInitialSourceFile(info);
      slotBuildGenerate();

      ((CKDevelop*)parent())->refreshTrees();
    }
  }
}

void KDlgEdit::slotFileOpen()
{
  ((CKDevelop*)parent())->kdlg_get_edit_widget()->openFromFile("/tmp/dialog.kdevdlg");
}
void KDlgEdit::slotOpenDialog(QString file){
  slotFileSave();
  dialog_file = file;
  ((CKDevelop*)parent())->kdlg_get_edit_widget()->openFromFile(file);
  ((CKDevelop*)parent())->setCaption(i18n("KDevelop Dialog Editor: ")+file); 
  ((CKDevelop*)parent())->kdlg_get_edit_widget()->mainWidget()->getProps()->setProp_Value("VarName","this");
}

void KDlgEdit::slotFileClose(){
}

void KDlgEdit::slotFileSave(){
  ((CKDevelop*)parent())->kdlg_get_edit_widget()->saveToFile(dialog_file);
}
	
void KDlgEdit::slotEditUndo(){
}

void KDlgEdit::slotEditRedo()
{
}

void KDlgEdit::slotEditCut(){
  ((CKDevelop*)parent())->kdlg_get_edit_widget()->slot_cutSelected();
}

void KDlgEdit::slotEditDelete(){
  ((CKDevelop*)parent())->kdlg_get_edit_widget()->slot_deleteSelected();
}

void KDlgEdit::slotEditCopy(){
  ((CKDevelop*)parent())->kdlg_get_edit_widget()->slot_copySelected();
}

void KDlgEdit::slotEditPaste(){
  ((CKDevelop*)parent())->kdlg_get_edit_widget()->slot_pasteSelected();
}

void KDlgEdit::slotEditProperties()
{
} 	

void KDlgEdit::slotViewRefresh()
{
  ((CKDevelop*)parent())->kdlg_get_items_view()->refreshList();

  KDlgItem_Base* sel = ((CKDevelop*)parent())->kdlg_get_edit_widget()->selectedWidget();

  if (sel)
    ((CKDevelop*)parent())->kdlg_get_prop_widget()->refillList(sel);

  ((CKDevelop*)parent())->kdlg_get_edit_widget()->mainWidget()->recreateItem();
}

void KDlgEdit::slotBuildGenerate(){
  CProject* prj = ((CKDevelop*)parent())->getProject(); 
  TDialogFileInfo info = prj->getDialogFileInfo(getRelativeName(dialog_file));

  ///////////////////////////// datafile/////////////////////////
  QFile file(prj->getProjectDir() + info.data_file);
  QTextStream stream( &file );
  if ( file.open(IO_WriteOnly) ){
    QFileInfo header_file_info(prj->getProjectDir()+info.header_file);
    stream << "#include \"" << header_file_info.fileName() << "\"\n\n";
    
    stream << "void  " << info.classname + "::initDialog(){\n";
    ((CKDevelop*)parent())->kdlg_get_edit_widget()->mainWidget()->getProps()->setProp_Value("VarName","this");
    variables.clear();
    includes.clear();
    generateWidget(((CKDevelop*)parent())->kdlg_get_edit_widget()->mainWidget(),&stream,"this");
    stream << "}\n";
  }
  file.close();

  ///////////////////////////headerfile////////////////////////
  
  if(((CKDevelop*)parent())->kdlg_get_edit_widget()->wasWidgetAdded() 
     || ((CKDevelop*)parent())->kdlg_get_edit_widget()->wasWidgetRemoved()){
    QString var;

    ((CKDevelop*)parent())->switchToFile(prj->getProjectDir() + info.header_file);
    ((CKDevelop*)parent())->slotFileSave();
    ((CKDevelop*)parent())->slotFileClose();

    file.setName(prj->getProjectDir() + info.header_file);
    QStrList list;
    bool ok = true;
    QString str;
    
    if(file.open(IO_ReadOnly)){ // read the header
      while(!stream.eof()){
	list.append(stream.readLine());
      }
    }
    file.close();
    
    if(file.open(IO_WriteOnly)){
      str = list.first();
      ////////////////////////////includes/////////////////////////////
      while(str != 0 && ok){
	if(str.contains("//Generated area. DO NOT EDIT!!!(begin)") != 0){
	  stream << str << "\n";
	  for(var = includes.first();var != 0;var = includes.next()){ // generate the includes
	    stream << var << endl;
	  }
	  ok = false; // finished
	}
	else{
	  stream << str << "\n";
	}
	str = list.next();
      }
      ok = true;
      while(str != 0 && ok){
	if(str.contains("//Generated area. DO NOT EDIT!!!(end)") != 0){
	  stream << str << "\n";
	  ok = false;
	}
	str = list.next();
      }
      ok = true;
      ////////////////////////////variables/////////////////////////////
      while(str != 0 && ok){
	if(str.contains("//Generated area. DO NOT EDIT!!!(begin)") !=0){
	  stream << str << "\n";
	  for(var = variables.first();var != 0;var = variables.next()){
	    stream << "\t" << var << endl;
	  }
	  ok = false; // finished
	}
	else{
	  stream << str << "\n";
	}
	str = list.next();
      }
      ok = true;
      while(str != 0 && ok){
	if(str.contains("//Generated area. DO NOT EDIT!!!(end)") != 0){
	  stream << str << "\n";
	  ok = false;
	}
	str = list.next();
      }
      while(str != 0){
	stream << str << "\n";
	str = list.next();
      }
    }
    file.close();
    ((CKDevelop*)parent())->switchToFile(prj->getProjectDir() + info.header_file);
  }

  ((CKDevelop*)parent())->kdlg_get_edit_widget()->setModified(false);
  ((CKDevelop*)parent())->kdlg_get_edit_widget()->setWidgetAdded(false);
  ((CKDevelop*)parent())->kdlg_get_edit_widget()->setWidgetRemoved(false);
  slotFileSave();
}

void KDlgEdit::slotViewGrid()
{
  KDlgGridDialog dlg((QWidget*)parent());

  if (dlg.exec())
    {
      ((CKDevelop*)parent())->kdlg_get_edit_widget()->setGridSize(dlg.getGridX(), dlg.getGridY());
      ((CKDevelop*)parent())->kdlg_get_edit_widget()->mainWidget()->repaintItem();
    }
}

QString KDlgEdit::getRelativeName(QString abs_filename){
  CProject* prj = ((CKDevelop*)parent())->getProject();
  // normalize it a little bit
  abs_filename.replace(QRegExp("///"),"/"); // remove ///
  abs_filename.replace(QRegExp("//"),"/"); // remove //
  abs_filename.replace(QRegExp(prj->getProjectDir()),"");
  return abs_filename;
}
  
void KDlgEdit::generateInitialHeaderFile(TDialogFileInfo info){
  CGenerateNewFile generator;
  CProject* prj = ((CKDevelop*)parent())->getProject(); 
  QString header_file = prj->getProjectDir() + info.header_file;
  generator.genHeaderFile(header_file,prj);


  // modify the header
  QStrList list;
  QFile file(header_file);
  QTextStream stream(&file);
  QString str;
  
  if(file.open(IO_ReadOnly)){ // 
    while(!stream.eof()){
      list.append(stream.readLine());
    }
  }
  file.close();
  
  if(file.open(IO_WriteOnly)){
    for(str = list.first();str != 0;str = list.next()){
      stream << str << "\n";
    }
    QFileInfo fileinfo(header_file);
    stream << "\n#ifndef " + fileinfo.baseName().upper() + "_H\n";
    stream << "#define "+ fileinfo.baseName().upper() + "_H\n\n";
    if(info.baseclass != "Custom"){
      stream << "//Generated area. DO NOT EDIT!!!(begin)\n";
      stream << "//Generated area. DO NOT EDIT!!!(end)\n";
    }
    stream << "\n#include <"+info.baseclass.lower() +".h>\n";
    
    stream << "\n/**\n";
    stream << "  *@author "+ prj->getAuthor() + "\n";
    stream << "  */\n\n";
    stream << "class " + info.classname;
    stream << " : ";
    stream << "public ";
    if(info.baseclass != "Custom"){
      stream << info.baseclass + " ";
    }
    stream << " {\n";
    stream << "   Q_OBJECT\n";
    
    stream << "public: \n";
    stream << "\t" + info.classname+"(";
    stream << "QWidget *parent=0, const char *name=0";
    stream << ");\n";
    stream << "\t~" + info.classname +"();\n\n";

    stream << "protected: \n";
    stream << "\tvoid initDialog();\n";
    stream << "\t//Generated area. DO NOT EDIT!!!(begin)\n";
    stream << "\t//Generated area. DO NOT EDIT!!!(end)\n\n";
    stream << "private: \n";
    stream << "};\n\n#endif\n";
  }
}
void KDlgEdit::generateInitialSourceFile(TDialogFileInfo info){
  CGenerateNewFile generator;
  CProject* prj = ((CKDevelop*)parent())->getProject(); 
  QString source_file = prj->getProjectDir() + info.source_file;
  generator.genCPPFile(source_file,prj);
  QFileInfo header_file_info(prj->getProjectDir() + info.header_file);
  

  // modify the source
  QStrList list;
  QFile file(source_file);
  QTextStream stream(&file);
  QString str;
  
  if(file.open(IO_ReadOnly)){ // 
    while(!stream.eof()){
      list.append(stream.readLine());
    }
  }
  file.close();
  
  if(file.open(IO_WriteOnly)){
    for(str = list.first();str != 0;str = list.next()){
      stream << str << "\n";
    }
    stream << "#include \"" << header_file_info.fileName() << "\"\n\n";


    // constructor
    if(info.baseclass == "QDialog"){
      stream << info.classname + "::" + info.classname 
	+ "(QWidget *parent, const char *name) : QDialog(parent,name,true){\n";
    }
    
    stream << "\tinitDialog();\n}\n\n";
    // destructor
    stream << info.classname + "::~" + info.classname +"(){\n}\n";
  }
}

void KDlgEdit::generateWidget(KDlgItem_Widget *wid, QTextStream *stream,QString parent){
  if ((!wid) || (!stream)){
    return;
  }
  //QPushButton
  if(wid->itemClass() == "QPushButton"){
    variables.append("QPushButton *"+wid->getProps()->getPropValue("VarName")+";");
    if(includes.contains("#include <qpushbutton.h>") == 0){
      includes.append("#include <qpushbutton.h>");
    }
    generateQPushButton(wid,stream,parent);
  }
  //QLineEdit
  if(wid->itemClass() == "QLineEdit"){
    variables.append("QLineEdit *"+wid->getProps()->getPropValue("VarName")+";");
    if(includes.contains("#include <qlineedit.h>") == 0){
      includes.append("#include <qlineedit.h>");
    }
    generateQLineEdit(wid,stream,parent);
  }
  //QLCDNumber
  if(wid->itemClass() == "QLCDNumber"){
    variables.append("QLCDNumber *"+wid->getProps()->getPropValue("VarName")+";");
    if(includes.contains("#include <qlcdnumber.h>") == 0){
      includes.append("#include <qlcdnumber.h>");
    }
    generateQLCDNumber(wid,stream,parent);
  }
  //QLabel
  if(wid->itemClass() == "QLabel"){
    variables.append("QLabel *"+wid->getProps()->getPropValue("VarName")+";");
    if(includes.contains("#include <qlabel.h>") == 0){
      includes.append("#include <qlabel.h>");
    }
    generateQLabel(wid,stream,parent);
  }
  //QRadiobutton
  if(wid->itemClass() == "QRadioButton"){
    variables.append("QRadioButton *"+wid->getProps()->getPropValue("VarName")+";");
    if(includes.contains("#include <qradiobutton.h>") == 0){
      includes.append("#include <qradiobutton.h>");
    }
    generateQRadioButton(wid,stream,parent);
  }
  //QCheckBox
  if(wid->itemClass() == "QCheckBox"){
    variables.append("QCheckBox *"+wid->getProps()->getPropValue("VarName")+";");
    if(includes.contains("#include <qcheckbox.h>") == 0){
      includes.append("#include <qcheckbox.h>");
    }
    generateQCheckBox(wid,stream,parent);
  }

  if (wid->itemClass().upper() == "QWIDGET"){
    if(wid->getProps()->getPropValue("VarName") != "this"){
      variables.append("QWidget *"+wid->getProps()->getPropValue("VarName")+";");
    }
    if(includes.contains("#include <qwidget.h>") == 0){
      includes.append("#include <qwidget.h>");
    }
    generateQWidget(wid,stream,parent);
    KDlgItemDatabase *cdb = wid->getChildDb();
    if (cdb)
      {
	KDlgItem_Base *cdit = cdb->getFirst();
	while (cdit)
	  {
	    generateWidget( (KDlgItem_Widget*)cdit, stream, wid->getProps()->getPropValue("VarName"));
	    cdit = cdb->getNext();
	  }
      }
  }
}

void KDlgEdit::generateQLCDNumber(KDlgItem_Widget *wid, QTextStream *stream,QString parent){
  KDlgPropertyBase* props = wid->getProps();
  *stream << "\t" + props->getPropValue("VarName") +" = new QLCDNumber(" + parent +",\"" 
    +props->getPropValue("Name") + "\");\n";
  generateQWidget(wid,stream,parent);

  QString varname_p = "\t"+props->getPropValue("VarName") + "->";
  //display
  *stream << varname_p + "display(" + props->getPropValue("Value") +");\n";
  
  //setNumDigits
  if(props->getPropValue("NumDigits") != ""){
    *stream << varname_p + "setNumDigits(" + props->getPropValue("NumDigits") + ");\n";
  }
  *stream << "\n";
}
void KDlgEdit::generateQLineEdit(KDlgItem_Widget *wid, QTextStream *stream,QString parent){
  KDlgPropertyBase* props = wid->getProps();
  *stream << "\t" + props->getPropValue("VarName") +" = new QLineEdit(" + parent +",\"" 
    +props->getPropValue("Name") + "\");\n";
  generateQWidget(wid,stream,parent);

  QString varname_p = "\t"+props->getPropValue("VarName") + "->";
  //setText
  *stream << varname_p + "setText(\""+ props->getPropValue("Text") +"\");\n";

  //setMaxLenght
  if(props->getPropValue("MaxLength") != ""){
    *stream << varname_p + "setMaxLength("+ props->getPropValue("MaxLength") +");\n";
  }
  //setFrame
  if(props->getPropValue("hasFrame") != "TRUE"){
    *stream << varname_p + "setFrame(false);\n";
  }
  //isTextSelected
  if(props->getPropValue("isTextSelected") != "FALSE"){
    *stream << varname_p + "selectAll();\n";
  }
  // CursorPosition
  if(props->getPropValue("CursorPosition") != ""){
    *stream << varname_p + "setCursorPosition(" + props->getPropValue("CursorPosition") +");\n";
  }
  *stream << "\n";
}
void KDlgEdit::generateQRadioButton(KDlgItem_Widget *wid, QTextStream *stream,QString parent){
  KDlgPropertyBase* props = wid->getProps();
  *stream << "\t" + props->getPropValue("VarName") +" = new QRadioButton(" + parent +",\"" 
    +props->getPropValue("Name") + "\");\n";
  generateQWidget(wid,stream,parent);

  QString varname_p = "\t"+props->getPropValue("VarName") + "->";
  //setText
  *stream << varname_p + "setText(\""+props->getPropValue("Text") +"\");\n";
  //  setChecked
  if(props->getPropValue("isChecked") == "TRUE"){
    *stream << varname_p + "setChecked(true);\n";
  }
  //isAutoResize
  if(props->getPropValue("isAutoResize") == "TRUE"){
    *stream << varname_p + "setAutoResize(true);\n";
  }
  //isAutoRepeat
  if(props->getPropValue("isAutoRepeat") == "TRUE"){
    *stream << varname_p + "setAutoRepeat(true);\n";
  }
  //Pixmap
  if(props->getPropValue("Pixmap") != ""){
    *stream << varname_p + "setPixmap(QPixmap(\""+props->getPropValue("Pixmap")+"\"));\n";
    if(includes.contains("#include <qpixmap.h>") == 0){
      includes.append("#include <qpixmap.h>");
    }
  }
  *stream << "\n";
  
}
void KDlgEdit::generateQCheckBox(KDlgItem_Widget *wid, QTextStream *stream,QString parent){
  KDlgPropertyBase* props = wid->getProps();
  *stream << "\t" + props->getPropValue("VarName") +" = new QCheckBox(" + parent +",\"" 
    +props->getPropValue("Name") + "\");\n";
  generateQWidget(wid,stream,parent);
  
  QString varname_p = "\t"+props->getPropValue("VarName") + "->";
  //setText
  *stream << varname_p + "setText(\""+props->getPropValue("Text") +"\");\n";
  //  setChecked
  if(props->getPropValue("isChecked") == "TRUE"){
    *stream << varname_p + "setChecked(true);\n";
  }
  //isAutoResize
  if(props->getPropValue("isAutoResize") == "TRUE"){
    *stream << varname_p + "setAutoResize(true);\n";
  }
  //isAutoRepeat
  if(props->getPropValue("isAutoRepeat") == "TRUE"){
    *stream << varname_p + "setAutoRepeat(true);\n";
  }
  //Pixmap
  if(props->getPropValue("Pixmap") != ""){
    *stream << varname_p + "setPixmap(QPixmap(\""+props->getPropValue("Pixmap")+"\"));\n";
    if(includes.contains("#include <qpixmap.h>") == 0){
      includes.append("#include <qpixmap.h>");
    }
  }
  *stream << "\n";
}
void KDlgEdit::generateQLabel(KDlgItem_Widget *wid, QTextStream *stream,QString parent){
  KDlgPropertyBase* props = wid->getProps();
  *stream << "\t" + props->getPropValue("VarName") +" = new QLabel(" + parent +",\"" 
    +props->getPropValue("Name") + "\");\n";
  generateQWidget(wid,stream,parent);

  QString varname_p = "\t"+props->getPropValue("VarName") + "->";
  //setText
  *stream << varname_p + "setText(\""+props->getPropValue("Text") +"\");\n";
  //isAutoResize
  if(props->getPropValue("isAutoResize") == "TRUE"){
    *stream << varname_p + "setAutoResize(true);\n";
  }
  //Margin
  if(props->getPropValue("Margin") != ""){
    *stream << varname_p + "setMargin("+props->getPropValue("Margin")+");\n";
  }
  *stream << "\n";
}

void KDlgEdit::generateQPushButton(KDlgItem_Widget *wid, QTextStream *stream,QString parent){
  KDlgPropertyBase* props = wid->getProps();
  *stream << "\t" + props->getPropValue("VarName") +" = new QPushButton(" + parent +",\"" 
    +props->getPropValue("Name") + "\");\n";
  generateQWidget(wid,stream,parent);

  QString varname_p = "\t"+props->getPropValue("VarName") + "->";
  //setText
  *stream << varname_p + "setText(\""+props->getPropValue("Text") +"\");\n";
  //isDefault
  if(props->getPropValue("isDefault") == "TRUE"){
    *stream << varname_p + "setDefault(true);\n";
  }
  //IsAutoDefaul
  if(props->getPropValue("isAutoDefault") == "TRUE"){
    *stream << varname_p + "setAutoDefault(true);\n";
  }
  //IsToogleButton
  if(props->getPropValue("isToggleButton") == "TRUE"){
    *stream << varname_p + "setToogleButton(true);\n";
  }
  //isToogledOn
  if(props->getPropValue("isToggledOn") == "TRUE"){
    *stream << varname_p + "setToogleOn(true);\n";
  }
  //IsMenuButton
  if(props->getPropValue("isMenuButton") == "TRUE"){
    *stream << varname_p + "setIsMenuButton(true);\n";
  }
  //isAutoResize
  if(props->getPropValue("isAutoResize") == "TRUE"){
    *stream << varname_p + "setAutoResize(true);\n";
  }
  //isAutoRepeat
  if(props->getPropValue("isAutoRepeat") == "TRUE"){
    *stream << varname_p + "setAutoRepeat(true);\n";
  }
  //Pixmap
  if(props->getPropValue("Pixmap") != ""){
    *stream << varname_p + "setPixmap(QPixmap(\""+props->getPropValue("Pixmap")+"\"));\n";
    if(includes.contains("#include <qpixmap.h>") == 0){
      includes.append("#include <qpixmap.h>");
    }
  }
  *stream << "\n";
  
}
void KDlgEdit::generateQWidget(KDlgItem_Widget *wid, QTextStream *stream,QString parent){
  KDlgPropertyBase* props = wid->getProps();
  QString varname_p;
  // new
  if(props->getPropValue("VarName") != "this" && wid->itemClass() == "QWidget"){
    *stream << "\t" + props->getPropValue("VarName") +" = new QWidget(" + parent +",\"" 
      +props->getPropValue("Name") + "\");\n";
  }
   
  varname_p = "\t"+props->getPropValue("VarName") + "->";
  ///////////////////////////////////////geometry////////////////////////////////////
  // setGeometry
  if(props->getPropValue("VarName") != "this"){
    *stream << varname_p + "setGeometry("+props->getPropValue("X")+","
      +props->getPropValue("Y")+","+props->getPropValue("Width")+","+props->getPropValue("Height")+");\n";
  }
  else {
    *stream << varname_p + "resize("+props->getPropValue("Width")+","+props->getPropValue("Height")+");\n";
  }

  //setMinimumSize
  if(props->getPropValue("MinWidth") != "0" || props->getPropValue("MinHeight") != "0"){
    *stream << varname_p + "setMinimumSize("+props->getPropValue("MinWidth")+","
    +props->getPropValue("MinHeight")+");\n";
  }

  //setMaximumSize
  if(props->getPropValue("MaxWidth") != "" && props->getPropValue("MaxHeight") != ""){
    *stream << varname_p + "setMaximumSize("+props->getPropValue("MaxWidth")+","
    +props->getPropValue("MaxHeight")+");\n";
  }
  //setFixedSize
  if(props->getPropValue("IsFixedSize") == "TRUE"){
    *stream << varname_p + "setFixedSize("+props->getPropValue("Width")
      +","+props->getPropValue("Height")+");\n";
  }
  //setSizeIncrement ( int w, int h )
  if(props->getPropValue("SizeIncX") != "" && props->getPropValue("SizeIncY") != ""){
    *stream << varname_p + "setSizeIncrement("+props->getPropValue("SizeIncX")
      +","+props->getPropValue("SizeIncY")+");\n";
  }
  /////////////////////////////////General///////////////////////////
  //IsHidden
  if(props->getPropValue("IsHidden") != "FALSE"){
    *stream << varname_p + "hide();\n";
  }
  //isEnabled
  if(props->getPropValue("IsEnabled") != "TRUE"){
    *stream << varname_p + "setEnabled(false);\n";
  }
  ////////////////////////////////C++ Code//////////////////////////
  
  
  ////////////////////////////////Appearance/////////////////////////
  //BgPixmap
  if(props->getPropValue("BgPixmap") != ""){
    *stream << varname_p + "setBackgroundPixmap(QPixmap(\""+props->getPropValue("BgPixmap")+"\"));\n";
    if(includes.contains("#include <qpixmap.h>") == 0){
      includes.append("#include <qpixmap.h>");
    }
  }

}
