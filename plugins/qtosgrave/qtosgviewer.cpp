// -*- coding: utf-8 -*-
// Copyright (C) 2012-2014 Gustavo Puche, Rosen Diankov, OpenGrasp Team
//
// OpenRAVE Qt/OpenSceneGraph Viewer is licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "qtosgviewer.h"

#include <boost/lexical_cast.hpp>
#include <iostream>

#include <osg/ArgumentParser>

#include "osgviewerwidget.h"


namespace qtosgrave {

#define ITEM_DELETER boost::bind(DeleteItemCallbackSafe,weak_viewer(),_1)

void DeleteItemCallbackSafe(QtOSGViewerWeakPtr wpt, Item* pItem)
{
    QtOSGViewerPtr pviewer = wpt.lock();
    if( !!pviewer ) {
        pviewer->_DeleteItemCallback(pItem);
    }
}

class ItemSelectionCallbackData : public UserData
{
public:
    ItemSelectionCallbackData(const ViewerBase::ItemSelectionCallbackFn& callback, boost::shared_ptr<QtOSGViewer> pviewer) : _callback(callback), _pweakviewer(pviewer) {
    }
    virtual ~ItemSelectionCallbackData() {
        boost::shared_ptr<QtOSGViewer> pviewer = _pweakviewer.lock();
        if( !!pviewer ) {
            boost::mutex::scoped_lock lock(pviewer->_mutexCallbacks);
            pviewer->_listRegisteredItemSelectionCallbacks.erase(_iterator);
        }
    }

    list<UserDataWeakPtr>::iterator _iterator;
    ViewerBase::ItemSelectionCallbackFn _callback;
protected:
    boost::weak_ptr<QtOSGViewer> _pweakviewer;
};
typedef boost::shared_ptr<ItemSelectionCallbackData> ItemSelectionCallbackDataPtr;

QtOSGViewer::QtOSGViewer(EnvironmentBasePtr penv, std::istream& sinput) : QMainWindow(NULL, Qt::Window), ViewerBase(penv)
{
    _userdatakey = std::string("qtosg") + boost::lexical_cast<std::string>(this);
    int qtosgbuild = 0;
    bool bCreateStatusBar = true, bCreateMenu = true;
    int nAlwaysOnTopFlag = 0; // 1 - add on top flag (keep others), 2 - add on top flag (remove others)
    sinput >> qtosgbuild >> bCreateStatusBar >> bCreateMenu >> nAlwaysOnTopFlag;

    _name = str(boost::format("OpenRAVE %s")%OPENRAVE_VERSION_STRING);
    if( (OPENRAVE_VERSION_MINOR%2) || (OPENRAVE_VERSION_PATCH%2) ) {
        _name += " (Development Version)";
    }
    else {
        _name += " (Stable Release)";
    }

    setWindowTitle(_name.c_str());
    if(  bCreateStatusBar ) {
        statusBar()->showMessage(tr("Status Bar"));
    }

    __description = ":Interface Author: Gustavo Puche, Rosen Diankov\n\nProvides a viewer based on Open Scene Graph.";
//    RegisterCommand("SetFiguresInCamera",boost::bind(&QtOSGViewer::_SetFiguresInCamera,this,_1,_2),
//                    "Accepts 0/1 value that decides whether to render the figure plots in the camera image through GetCameraImage");
//    RegisterCommand("SetFeedbackVisibility",boost::bind(&QtOSGViewer::_SetFeedbackVisibility,this,_1,_2),
//                    "Accepts 0/1 value that decides whether to render the cross hairs");
//    RegisterCommand("ShowWorldAxes",boost::bind(&QtOSGViewer::_SetFeedbackVisibility,this,_1,_2),
//                    "Accepts 0/1 value that decides whether to render the cross hairs");
//    RegisterCommand("Resize",boost::bind(&QtOSGViewer::_CommandResize,this,_1,_2),
//                    "Accepts width x height to resize internal video frame");
//    RegisterCommand("SaveBodyLinkToVRML",boost::bind(&QtOSGViewer::_SaveBodyLinkToVRMLCommand,this,_1,_2),
//                    "Saves a body and/or a link to VRML. Format is::\n\n  bodyname linkindex filename\\n\n\nwhere linkindex >= 0 to save for a specific link, or < 0 to save all links");
//    RegisterCommand("SetNearPlane", boost::bind(&QtOSGViewer::_SetNearPlaneCommand, this, _1, _2),
//                    "Sets the near plane for rendering of the image. Useful when tweaking rendering units");
//    RegisterCommand("StartViewerLoop", boost::bind(&QtOSGViewer::_StartViewerLoopCommand, this, _1, _2),
//                    "starts the viewer sync loop and shows the viewer. expects someone else will call the qapplication exec fn");
//    RegisterCommand("Show", boost::bind(&QtOSGViewer::_ShowCommand, this, _1, _2),
//                    "executs the show directly");
//    RegisterCommand("TrackLink", boost::bind(&QtOSGViewer::_TrackLinkCommand, this, _1, _2),
//                    "camera tracks the link maintaining a specific relative transform: robotname, manipname, _fTrackingRadius");
//    RegisterCommand("TrackManipulator", boost::bind(&QtOSGViewer::_TrackManipulatorCommand, this, _1, _2),
//                    "camera tracks the manipulator maintaining a specific relative transform: robotname, manipname, _fTrackingRadius");
//    RegisterCommand("SetTrackingAngleToUp", boost::bind(&QtOSGViewer::_SetTrackingAngleToUpCommand, this, _1, _2),
//                    "sets a new angle to up");

    _bLockEnvironment = true;
    _InitGUI(bCreateStatusBar);
}

void QtOSGViewer::_InitGUI(bool bCreateStatusBar)
{
    osg::ArgumentParser arguments(0, NULL);

//    _qCentralWidget.reset(new QWidget());
//    _qCentralWidget->adjustSize();
//    _qCentralLayout = new QGridLayout;
//    _qCentralLayout->addWidget(_posgWidget,0,0);
//    _qCentralWidget->setLayout(_qCentralLayout);
//    setCentralWidget(_qCentralWidget);

    _posgWidget = new ViewerWidget(GetEnv());
    setCentralWidget(_posgWidget);

    // initialize the environment
    _ivRoot = new osg::Group();
    _ivRoot->ref();
    
    _qtree = new QTreeView;

    _CreateActions();
    _CreateMenus();
    _CreateToolsBar();
    if( bCreateStatusBar ) {
        _CreateStatusBar();
    }
    _CreateDockWidgets();
    
    resize(1024, 750);

    // toggle switches
    _bDisplayGrid = false;
    _bDisplayIK = false;
    _bDisplayFPS = false;
    _bJointHilit = true;
    _bDynamicReplan = false;
    _bVelPredict = true;
    _bDynSim = false;
    _bControl = true;
    _bGravity = true;
    _bTimeElapsed = false;
    _bSensing = false;
    _bMemory = true;
    _bHardwarePlan = false;
    _bShareBitmap = true;
    _bManipTracking = false;
    _bAntialiasing = false;
    _viewGeometryMode = VG_RenderOnly;
}

bool QtOSGViewer::_ForceUpdatePublishedBodies()
{
    {
        boost::mutex::scoped_lock lockupdating(_mutexUpdating);
        if( !_bUpdateEnvironment )
            return false;
    }

    boost::mutex::scoped_lock lock(_mutexUpdateModels);
    EnvironmentMutex::scoped_lock lockenv(GetEnv()->GetMutex());
    GetEnv()->UpdatePublishedBodies();

    _bModelsUpdated = false;
    _bLockEnvironment = false; // notify UpdateFromModel to update without acquiring the lock
    _condUpdateModels.wait(lock);
    _bLockEnvironment = true; // reste
    return _bModelsUpdated;
}

void QtOSGViewer::_CreateActions()
{
    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(tr("Ctrl+Q"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

    loadAct = new QAction(QIcon(":/images/open.png"),tr("L&oad..."), this);
    loadAct->setShortcut(tr("Ctrl+L"));
    connect(loadAct, SIGNAL(triggered()), this, SLOT(LoadEnvironment()));

    importAct = new QAction(QIcon(":/images/Import.png"),tr("I&mport..."), this);
    importAct->setShortcut(tr("Ctrl+I"));
    connect(importAct, SIGNAL(triggered()), this, SLOT(ImportEnvironment()));

    saveAct = new QAction(QIcon(":/images/save.png"),tr("S&ave..."), this);
    saveAct->setShortcut(tr("Ctrl+S"));
    connect(saveAct, SIGNAL(triggered()), this, SLOT(SaveEnvironment()));

    viewCamAct = new QAction(tr("View Camera Params"), this);

    viewColAct = new QAction(tr("View Collision Word"), this);

    pubilshAct = new QAction(tr("Pubilsh Bodies Anytimes"), this);

    printAct = new QAction(tr("Print Debug Output"), this);

    showAct = new QAction(tr("Show Framerate"), this);

    playAct = new QAction(QIcon(":/images/play.png"),tr("Play"), this);

    stopAct = new QAction(QIcon(":/images/stop.png"),tr("Stop"), this);

    recordAct = new QAction(tr("Record V&ideo"), this);

    odeAct = new QAction(tr("ODE Dynamic Simulation"), this);
    odeAct->setCheckable(true);

    selfAct = new QAction(tr("Self Collision"), this);
    selfAct->setCheckable(true);

    applyAct = new QAction(tr("Apply Gravity"), this);
    applyAct->setCheckable(true);
    applyAct->setChecked(true);

    aboutAct = new QAction(tr("About"), this);

    pauseAct = new QAction(QIcon(":/images/pause.png"),tr("Pause"), this);

    puntAct = new QAction(QIcon(":/images/no_edit.png"),tr("Pointer"), this);

    AxesAct = new QAction(QIcon(":/images/axes.png"),tr("Axes"), this);
    AxesAct->setCheckable(true);
    connect(AxesAct, SIGNAL(triggered()), this, SLOT(axes()));

    houseAct = new QAction(QIcon(":/images/house.png"),tr("Home"), this);
    connect(houseAct, SIGNAL(triggered()),this,SLOT(ResetViewToHome()));

    smoothAct = new QAction(QIcon(":/images/smooth.png"),tr("Smooth"), this);
    connect(smoothAct, SIGNAL(triggered()), this, SLOT(polygonMode()));

    flatAct = new QAction(QIcon(":/images/flat.png"),tr("Flat"), this);
    connect(flatAct, SIGNAL(triggered()), this, SLOT(polygonMode()));

    lightAct = new QAction(QIcon(":/images/lighton.png"),tr("Light"), this);
    lightAct->setCheckable(true);
    connect(lightAct, SIGNAL(triggered()), this, SLOT(_ProcessLightChange()));

    wireAct = new QAction(QIcon(":/images/wire.png"),tr("Wire"), this);
    connect(wireAct, SIGNAL(triggered()), this, SLOT(polygonMode()));

    facesAct = new QAction(QIcon(":/images/faces.png"),tr("Cull face"), this);
    facesAct->setCheckable(true);
    connect(facesAct, SIGNAL(triggered()), this, SLOT(_ProcessFacesModeChange()));

    bboxAct = new QAction(QIcon(":/images/bbox.png"),tr("Poligon"), this);
    bboxAct->setCheckable(true);
    connect(bboxAct, SIGNAL(triggered()), this, SLOT(_ProcessBoundingBox()));

    connect(&_timer, SIGNAL(timeout()), this, SLOT(_Refresh()));
    _timer.start(10);
}

void QtOSGViewer::_Refresh()
{
    UpdateFromModel();
    //  _posgWidget->update();
}

void QtOSGViewer::_Reset()
{
    _Deselect();
    UpdateFromModel();
    _condUpdateModels.notify_all();

    FOREACH(itbody, _mapbodies)
    {
        BOOST_ASSERT( itbody->first->GetUserData(_userdatakey) == itbody->second );
        itbody->first->RemoveUserData(_userdatakey);
    }

    _mapbodies.clear();
    objectTree->clear();

    {
        boost::mutex::scoped_lock lock(_mutexItems);
        FOREACH(it,_listRemoveItems) {
            delete *it;
        }
        _listRemoveItems.clear();
    }
}

void QtOSGViewer::_CreateMenus()
{
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(loadAct);
    fileMenu->addAction(importAct);
    fileMenu->addAction(saveAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    viewMenu = menuBar()->addMenu(tr("&View"));

    viewMenu->addSeparator();
    viewMenu->addAction(viewCamAct);

    viewMenu->addAction(viewColAct);
    viewMenu->addAction(pubilshAct);
    viewMenu->addAction(printAct);
    viewMenu->addAction(showAct);

//	animation = menuBar()->addMenu(tr("&Animation"));
//	animation->addAction(playAct);
//	animation->addAction(stopAct);
//
//	options = menuBar()->addMenu(tr("&Options"));
//	options->addAction(recordAct);
//
//	dynamics = menuBar()->addMenu(tr("D&ynamics"));
//	dynamics->addAction(odeAct);
//	dynamics->addAction(selfAct);
//	dynamics->addAction(applyAct);

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAct);
}

void QtOSGViewer::LoadEnvironment()
{
    QString s = QFileDialog::getOpenFileName( this, "Load Environment", NULL,
                                              "Env Files (*.xml);;COLLADA Files (*.dae)");

    if( s.length() == 0 )
        return;

    _Reset();
    GetEnv()->Reset();

    GetEnv()->Load(s.toAscii().data());

    RAVELOG_INFO("\n---------Refresh--------\n");

    //  Refresh the screen.
    UpdateFromModel();

    RAVELOG_INFO("----> set home <----\n");

    //  Center object in window viewer
    _posgWidget->SetHome();
}

void QtOSGViewer::ImportEnvironment()
{
    QString s = QFileDialog::getOpenFileName( this, "Import Environment", NULL,
                                              "Env Files (*.xml);;COLLADA Files (*.dae)");

    if( s.length() == 0 )
        return;

    GetEnv()->Load(s.toAscii().data());

    //  Refresh the screen.
    UpdateFromModel();
}

void QtOSGViewer::SaveEnvironment()
{
    QString s = QFileDialog::getSaveFileName( this, "Save Environment", NULL,
                                              "Env Files (*.xml);;COLLADA Files (*.dae)");

    if( s.length() == 0 )
        return;

    GetEnv()->Save(s.toAscii().data());
}

void QtOSGViewer::ResetViewToHome()
{
    _posgWidget->ResetViewToHome();
}

void QtOSGViewer::_ProcessLightChange()
{
    if (lightAct->isChecked()) {
        lightAct->setIcon(QIcon(":/images/lightoff.png"));
    }
    else {
        lightAct->setIcon(QIcon(":/images/lighton.png"));
    }
    _posgWidget->SetLight(!lightAct->isChecked());
}

void QtOSGViewer::_ProcessFacesModeChange()
{
    _posgWidget->SetFacesMode(!facesAct->isChecked());
}

void QtOSGViewer::polygonMode()
{
    if (smoothAct->isChecked())
    {
        _posgWidget->setPolygonMode(0);
    }
    else if (flatAct->isChecked())
    {
        _posgWidget->setPolygonMode(1);
    }
    else
    {
        _posgWidget->setPolygonMode(2);
    }
}

void QtOSGViewer::_ProcessBoundingBox()
{
    _posgWidget->DrawBoundingBox(bboxAct->isChecked());
}

void QtOSGViewer::axes()
{
    _posgWidget->DrawAxes(AxesAct->isChecked());
}

void QtOSGViewer::_ProcessPointerGroupClicked(int button)
{
    switch (button) {
    case -2:
        _posgWidget->DrawTrackball(false);
        _posgWidget->SelectActive(true);
        break;
    case -3:
        _posgWidget->DrawTrackball(true);
        _posgWidget->SelectActive(false);
        break;
    case -4:
        _posgWidget->DrawBoundingBox(true);
        _posgWidget->SelectActive(false);
        break;
    case -5:
        _posgWidget->DrawAxes(true);
        _posgWidget->SelectActive(false);
        break;
    default:
        RAVELOG_ERROR_FORMAT("pointerGroupClicked failure. Button %d pushed", button);
        _posgWidget->SelectActive(false);
        break;
    }
}

void QtOSGViewer::_ProcessDraggerGroupClicked(int button)
{
    RAVELOG_INFO_FORMAT("Dragger button clicked %d", button);
}

void QtOSGViewer::_RepaintWidgets(osg::ref_ptr<osg::Group> group)
{
    _posgWidget->SetSceneData(group);
}

void QtOSGViewer::_CreateToolsBar()
{
    fileToolBar = addToolBar(tr("File Bar"));
    fileToolBar->addAction(loadAct);
    fileToolBar->addAction(importAct);
    fileToolBar->addAction(saveAct);

//  actionToolBar = addToolBar(tr("Action Bar"));
//  actionToolBar->addAction(playAct);
//  actionToolBar->addAction(stopAct);
//  actionToolBar->addAction(pauseAct);

//  physicsToolBar = addToolBar(tr("Physics Engine Bar"));
//  physicsComboBox = new QComboBox;
//
//  physicsComboBox->addItem(tr("Physics Engine"));
//  physicsComboBox->addItem(tr("Bullet"));
//  physicsComboBox->addItem(tr("ODE"));
//  physicsComboBox->addItem(tr("PAL"));
//  physicsComboBox->addItem(tr("PhysX"));
//
//
//  physicsToolBar->addWidget(physicsComboBox);

    toolsBar = addToolBar(tr("Tools Bar"));
    QToolButton *pointerButton = new QToolButton;
    pointerButton->setCheckable(true);
    pointerButton->setChecked(true);
    pointerButton->setIcon(QIcon(":/images/pointer.png"));

    QToolButton *handButton = new QToolButton;
    handButton->setCheckable(true);
    handButton->setIcon(QIcon(":/images/hand.png"));

    QToolButton *boundButton = new QToolButton;
    boundButton->setCheckable(true);
    boundButton->setIcon(QIcon(":/images/bbox.png"));

    QToolButton *axesButton = new QToolButton;
    axesButton->setCheckable(true);
    axesButton->setIcon(QIcon(":/images/axes.png"));

    pointerTypeGroup = new QButtonGroup;
    pointerTypeGroup->addButton(pointerButton);
    pointerTypeGroup->addButton(handButton);
    pointerTypeGroup->addButton(boundButton);
    pointerTypeGroup->addButton(axesButton);

    connect(pointerTypeGroup, SIGNAL(buttonClicked(int)), this, SLOT(_ProcessPointerGroupClicked(int)));

    shapeGroup = new QActionGroup(this);
    smoothAct->setCheckable(true);
    flatAct->setCheckable(true);
    wireAct->setCheckable(true);
    shapeGroup->addAction(smoothAct);
    shapeGroup->addAction(flatAct);
    shapeGroup->addAction(wireAct);
    smoothAct->setChecked(true);


    toolsBar->addWidget(pointerButton);
    toolsBar->addWidget(handButton);
    toolsBar->addWidget(boundButton);
    toolsBar->addWidget(axesButton);
    toolsBar->addAction(houseAct);
    toolsBar->addAction(lightAct);
    toolsBar->addAction(smoothAct);
    toolsBar->addAction(flatAct);
    toolsBar->addAction(wireAct);
    toolsBar->addAction(facesAct);
}

void QtOSGViewer::_CreateStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void QtOSGViewer::_CreateDockWidgets()
{
    QDockWidget *dock = new QDockWidget(tr("Objects Tree"), this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    objectTree = _CreateObjectTree();

    dock->setWidget(objectTree);

    addDockWidget(Qt::RightDockWidgetArea, dock);
    viewMenu->addAction(dock->toggleViewAction());


    dock = new QDockWidget(tr("Details"), this);

    //  QListWidget *sensorList = new QListWidget(dock);

    detailsTree = new QTreeWidget();
    detailsTree->setHeaderLabel(QString("Properties"));
    dock->setWidget(detailsTree);

    addDockWidget(Qt::RightDockWidgetArea, dock);
    viewMenu->addAction(dock->toggleViewAction());
}

void QtOSGViewer::_OnObjectTreeClick(QTreeWidgetItem* item,int num)
{
    RobotBasePtr robot;
    KinBodyPtr kinbody;
    KinBody::LinkPtr link;

    std::string mass;

    //  Select robot in Viewers
    _posgWidget->SelectRobot(item->text(0).toAscii().data());

    //  Clears details
    detailsTree->clear();

    QList<QTreeWidgetItem*> items;

    if (!!item->parent()) {
        if (item->parent()->text(0) == "Links") {
            std::ostringstream strs;

            //  Set Title
            detailsTree->setHeaderLabel(item->text(0).toAscii().data());

            robot = GetEnv()->GetRobot(item->parent()->parent()->text(0).toAscii().data());
            link  = robot->GetLink(item->text(0).toAscii().data());

            //  Clears output string
            strs.clear();

            strs << link->GetMass();

            mass = string(" Mass= ") + strs.str();

            items.append(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString(mass.c_str()))));
        }
        else {
            //  Set Title
            detailsTree->setHeaderLabel(item->text(0).toAscii().data());
        }
    }
    else {
        //  Set Title
        detailsTree->setHeaderLabel(item->text(0).toAscii().data());
        kinbody = GetEnv()->GetKinBody(item->text(0).toAscii().data());
        for (size_t i=0; i<kinbody->GetLinks().size(); i++) {
            std::ostringstream strs;
            link = kinbody->GetLinks()[i];
            strs << link->GetMass();
            mass = link->GetName() + string(" Mass= ") + strs.str();
            items.append(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString(mass.c_str()))));
        }
    }

    //  Add Items to details panel
    detailsTree->insertTopLevelItems(0, items);
}

QTreeWidget* QtOSGViewer::_CreateObjectTree()
{
    QTreeWidget *treeWidget = new QTreeWidget();
    treeWidget->setColumnCount(1);

    treeWidget->setHeaderLabel(QString("Scene"));

    //  Connect scene list clicked
    connect(treeWidget,SIGNAL(itemClicked(QTreeWidgetItem*,int)), this, SLOT(_OnObjectTreeClick(QTreeWidgetItem*,int)));

    return treeWidget;
}

void QtOSGViewer::_FillObjectTree(QTreeWidget *treeWidget)
{
    RAVELOG_DEBUG("Begin _FillObjectTree....\n");

    RobotBase* robot;
    vector<KinBodyPtr> kinbodies;
    QList<QTreeWidgetItem*> items;

    //  Clears tree
    treeWidget->clear();
    GetEnv()->GetBodies(kinbodies);
    for (size_t i = 0; i < kinbodies.size(); i++) {
        items.append(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString(kinbodies[i]->GetName().c_str()))));

        //  Number of child to add
        size_t nchild = 0;

        vector<KinBody::LinkPtr> links = kinbodies[i]->GetLinks();
        vector<KinBody::JointPtr>  joints = kinbodies[i]->GetJoints();

        //  Header 'Links'
        items[i]->addChild(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString("Links"))));

        for (size_t j = 0; j < links.size(); j++) {
            items[i]->child(nchild)->addChild(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString(links[j]->GetName().c_str()))));
        }

        if (joints.size() > 0) {
            //  Header 'Joints'
            items[i]->addChild(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString("Joints"))));

            nchild++;
        }

        for (size_t j = 0; j < joints.size(); j++) {
            items[i]->child(nchild)->addChild(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString(joints[j]->GetName().c_str()))));
            
            //  Adds links of joints
            items[i]->child(nchild)->child(j)->addChild(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString(joints[j]->GetFirstAttached()->GetName().c_str()))));
            items[i]->child(nchild)->child(j)->addChild(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString(joints[j]->GetSecondAttached()->GetName().c_str()))));
        }

        if (kinbodies[i]->IsRobot()) {
            robot = (RobotBase*)kinbodies[i].get();

            vector<RobotBase::ManipulatorPtr>     manipulators;
            vector<RobotBase::AttachedSensorPtr>  sensors;
            ControllerBasePtr controller;

            sensors = robot->GetAttachedSensors();
            manipulators  = robot->GetManipulators();
            controller  = robot->GetController();

            RAVELOG_DEBUG("Sensors....\n");

            if (sensors.size() > 0) {
                //  Header 'Sensors'
                items[i]->addChild(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString("Sensors"))));

                nchild++;
            }

            RAVELOG_DEBUG("Sensors number=%d\n",sensors.size());

            for (size_t j = 0; j < sensors.size(); j++) {
                RAVELOG_INFO("Sensor name=%s\n",sensors[j]->GetName().c_str());

                items[i]->child(nchild)->addChild(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString(sensors[j]->GetName().c_str()))));

                RAVELOG_WARN("Sensor link=%s\n",sensors[j]->GetAttachingLink()->GetName().c_str());

                items[i]->child(nchild)->child(j)->addChild(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString(sensors[j]->GetAttachingLink()->GetName().c_str()))));
            }

            RAVELOG_DEBUG("Manipulators....\n");

            if (manipulators.size() > 0) {
                //  Header 'Manipulators'
                items[i]->addChild(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString("Manipulators"))));

                nchild++;
            }

            for (size_t j = 0; j < manipulators.size(); j++) {
                items[i]->child(nchild)->addChild(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString(manipulators[j]->GetName().c_str()))));
            }

            RAVELOG_DEBUG("Controller....\n");

            if (!!controller) {
                //  Header 'Controller'
                items[i]->addChild(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString("Controller"))));

                nchild++;

                items[i]->child(nchild)->addChild(new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString(controller->GetXMLFilename().c_str()))));
            }
        }
    }

    treeWidget->insertTopLevelItems(0, items);

    RAVELOG_DEBUG("End _FillObjectTree....\n");
}

void QtOSGViewer::mouseDoubleClickEvent(QMouseEvent *e)
{
    std::cout << "Press mouse: doubleClick" << std::endl;
    std::cout << e->button() << std::endl;
}

void QtOSGViewer::_UpdateCameraTransform(float fTimeElapsed)
{
    boost::mutex::scoped_lock lock(_mutexMessages);

//    SbVec3f pos = GetCamera()->position.getValue();
//    _Tcamera.trans = RaveVector<float>(pos[0], pos[1], pos[2]);
//
//    SbVec3f axis;
//    float fangle;
//    GetCamera()->orientation.getValue(axis, fangle);
//    _Tcamera.rot = quatFromAxisAngle(RaveVector<float>(axis[0],axis[1],axis[2]),fangle);
//
//    if( fTimeElapsed > 0 ) {
//        // animate the camera if necessary
//        bool bTracking=false;
//        Transform tTrack;
//        KinBody::LinkPtr ptrackinglink = _ptrackinglink;
//        if( !!ptrackinglink ) {
//            bTracking = true;
//            tTrack = ptrackinglink->GetTransform();
//            tTrack.trans = ptrackinglink->ComputeAABB().pos;
//        }
//        RobotBase::ManipulatorPtr ptrackingmanip=_ptrackingmanip;
//        if( !!ptrackingmanip ) {
//            bTracking = true;
//            tTrack = ptrackingmanip->GetTransform();
//        }
//
//        if( bTracking ) {
//            RaveVector<float> vup(0,0,1); // up vector that camera should always be oriented to
//            RaveVector<float> vlookatdir = _Tcamera.trans - tTrack.trans;
//            vlookatdir -= vup*vup.dot3(vlookatdir);
//            float flookatlen = sqrtf(vlookatdir.lengthsqr3());
//            vlookatdir = vlookatdir*cosf(_fTrackAngleToUp) + flookatlen*sinf(_fTrackAngleToUp)*vup; // flookatlen shouldn't change
//            if( flookatlen > g_fEpsilon ) {
//                vlookatdir *= 1/flookatlen;
//            }
//            else {
//                vlookatdir = Vector(1,0,0);
//            }
//            vup -= vlookatdir*vlookatdir.dot3(vup);
//            vup.normalize3();
//
//            //RaveVector<float> vcameradir = ExtractAxisFromQuat(_Tcamera.rot, 2);
//            //RaveVector<float> vToDesiredQuat = quatRotateDirection(vcameradir, vlookatdir);
//            //RaveVector<float> vDestQuat = quatMultiply(vToDesiredQuat, _Tcamera.rot);
//            //vDestQuat = quatMultiply(quatRotateDirection(ExtractAxisFromQuat(vDestQuat,1), vup), vDestQuat);
//            float angle = normalizeAxisRotation(vup, _Tcamera.rot).first;
//            RaveVector<float> vDestQuat = quatMultiply(quatFromAxisAngle(vup, -angle), quatRotateDirection(RaveVector<float>(0,1,0), vup));
//            //transformLookat(tTrack.trans, _Tcamera.trans, vup);
//
//            RaveVector<float> vDestPos = tTrack.trans + ExtractAxisFromQuat(vDestQuat,2)*_fTrackingRadius;
//
//            if(1) {
//                // PID animation
//                float pconst = 0.02;
//                float dconst = 0.2;
//                RaveVector<float> newtrans = _Tcamera.trans + fTimeElapsed*_tTrackingCameraVelocity.trans;
//                newtrans += pconst*(vDestPos - _Tcamera.trans); // proportional term
//                newtrans -= dconst*_tTrackingCameraVelocity.trans*fTimeElapsed; // derivative term (target is zero velocity)
//
//                pconst = 0.001;
//                dconst = 0.04;
//                RaveVector<float> newquat = _Tcamera.rot + fTimeElapsed*_tTrackingCameraVelocity.rot;
//                newquat += pconst*(vDestQuat - _Tcamera.rot);
//                newquat -= dconst*_tTrackingCameraVelocity.rot*fTimeElapsed;
//                newquat.normalize4();
//                // have to make sure newquat's y vector aligns with vup
//
//                _tTrackingCameraVelocity.trans = (newtrans-_Tcamera.trans)*(2/fTimeElapsed) - _tTrackingCameraVelocity.trans;
//                _tTrackingCameraVelocity.rot = (newquat-_Tcamera.rot)*(2/fTimeElapsed) - _tTrackingCameraVelocity.rot;
//                _Tcamera.trans = newtrans;
//                _Tcamera.rot = newquat;
//            }
//            else {
//                _Tcamera.trans = vDestPos;
//                _Tcamera.rot = vDestQuat;
//            }
//            GetCamera()->position.setValue(_Tcamera.trans.x, _Tcamera.trans.y, _Tcamera.trans.z);
//            GetCamera()->orientation.setValue(_Tcamera.rot.y, _Tcamera.rot.z, _Tcamera.rot.w, _Tcamera.rot.x);
//        }
//    }
//
//    int width = centralWidget()->size().width();
//    int height = centralWidget()->size().height();
//    _ivCamera->aspectRatio = (float)view1->size().width() / (float)view1->size().height();
//
//    _camintrinsics.fy = 0.5*height/RaveTan(0.5f*GetCamera()->heightAngle.getValue());
//    _camintrinsics.fx = (float)width*_camintrinsics.fy/((float)height*GetCamera()->aspectRatio.getValue());
//    _camintrinsics.cx = (float)width/2;
//    _camintrinsics.cy = (float)height/2;
//    _camintrinsics.focal_length = GetCamera()->nearDistance.getValue();
//    _camintrinsics.distortion_model = "";
}

int QtOSGViewer::main(bool bShow)
{
    if( !QApplication::instance() ) {
        throw OPENRAVE_EXCEPTION_FORMAT0("need a valid QApplication before viewer loop is run", ORE_InvalidState);
    }
    if (bShow) {
        this->show();
    }

    UpdateFromModel();
    _posgWidget->SetHome();
    
    QApplication::instance()->exec();
    return 0;
}

void QtOSGViewer::quitmainloop()
{

}

bool QtOSGViewer::GetFractionOccluded(KinBodyPtr pbody, int width, int height, float nearPlane, float farPlane, const RaveTransform<float>& extrinsic, const float* pKK, double& fracOccluded)
{
    return false;
}

bool QtOSGViewer::GetCameraImage(std::vector<uint8_t>& memory, int width, int height, const RaveTransform<float>& t, const SensorBase::CameraIntrinsics& KK)
{
    return false;
}
bool QtOSGViewer::WriteCameraImage(int width, int height, const RaveTransform<float>& t, const SensorBase::CameraIntrinsics& KK, const std::string& filename, const std::string& extension)
{
    return false;
}
void QtOSGViewer::SetCamera(const RaveTransform<float>& trans, float focalDistance)
{
//  RAVELOG_INFO("[SetCamera]\n");
}
//void QtOSGViewer::SetCameraLookAt(const RaveVector<float>& lookat, const RaveVector<float>& campos, const RaveVector<float>& camup)
//{
//  RAVELOG_INFO("[SetCameraLookAt]\n");
//}

RaveTransform<float> QtOSGViewer::GetCameraTransform() const
{
    boost::mutex::scoped_lock lock(_mutexMessages);
    // have to flip Z axis
    RaveTransform<float> trot; trot.rot = quatFromAxisAngle(RaveVector<float>(1,0,0),(float)PI);
    return _Tcamera*trot;
}

geometry::RaveCameraIntrinsics<float> QtOSGViewer::GetCameraIntrinsics() const
{
    boost::mutex::scoped_lock lock(_mutexMessages);
    return _camintrinsics;
}

SensorBase::CameraIntrinsics QtOSGViewer::GetCameraIntrinsics2() const
{
    boost::mutex::scoped_lock lock(_mutexMessages);
    SensorBase::CameraIntrinsics intr;
    intr.fx = _camintrinsics.fx;
    intr.fy = _camintrinsics.fy;
    intr.cx = _camintrinsics.cx;
    intr.cy = _camintrinsics.cy;
    intr.distortion_model = _camintrinsics.distortion_model;
    intr.distortion_coeffs.resize(_camintrinsics.distortion_coeffs.size());
    std::copy(_camintrinsics.distortion_coeffs.begin(), _camintrinsics.distortion_coeffs.end(), intr.distortion_coeffs.begin());
    intr.focal_length = _camintrinsics.focal_length;
    return intr;
}

GraphHandlePtr QtOSGViewer::plot3(const float* ppoints, int numPoints, int stride, float fPointSize, const RaveVector<float>& color, int drawstyle)
{
    return GraphHandlePtr();
}

GraphHandlePtr QtOSGViewer::plot3(const float* ppoints, int numPoints, int stride, float fPointSize, const float* colors, int drawstyle, bool bhasalpha)
{
    return GraphHandlePtr();
}

GraphHandlePtr QtOSGViewer::drawlinestrip(const float* ppoints, int numPoints, int stride, float fwidth, const RaveVector<float>& color)
{
    return GraphHandlePtr();
}
GraphHandlePtr QtOSGViewer::drawlinestrip(const float* ppoints, int numPoints, int stride, float fwidth, const float* colors)
{
    return GraphHandlePtr();
}

GraphHandlePtr QtOSGViewer::drawlinelist(const float* ppoints, int numPoints, int stride, float fwidth, const RaveVector<float>& color)
{
    return GraphHandlePtr();
}
GraphHandlePtr QtOSGViewer::drawlinelist(const float* ppoints, int numPoints, int stride, float fwidth, const float* colors)
{
    return GraphHandlePtr();
}

GraphHandlePtr QtOSGViewer::drawarrow(const RaveVector<float>& p1, const RaveVector<float>& p2, float fwidth, const RaveVector<float>& color)
{
    return GraphHandlePtr();
}

GraphHandlePtr QtOSGViewer::drawbox(const RaveVector<float>& vpos, const RaveVector<float>& vextents)
{
    return GraphHandlePtr();
}

GraphHandlePtr QtOSGViewer::drawplane(const RaveTransform<float>& tplane, const RaveVector<float>& vextents, const boost::multi_array<float,3>& vtexture)
{
//    void* pret = new SoSeparator();
//    EnvMessagePtr pmsg(new DrawPlaneMessage(shared_viewer(), (SoSeparator*)pret, tplane,vextents,vtexture));
//    pmsg->callerexecute();

    return GraphHandlePtr();
}


GraphHandlePtr QtOSGViewer::drawtrimesh(const float* ppoints, int stride, const int* pIndices, int numTriangles, const RaveVector<float>& color)
{
    return GraphHandlePtr();
}

GraphHandlePtr QtOSGViewer::drawtrimesh(const float* ppoints, int stride, const int* pIndices, int numTriangles, const boost::multi_array<float,2>& colors)
{


    //    void* pret = new SoSeparator();
    //    EnvMessagePtr pmsg(new DrawTriMeshColorMessage(shared_viewer(), (SoSeparator*)pret, ppoints, stride, pIndices, numTriangles, colors));
    //    pmsg->callerexecute();

    return GraphHandlePtr();
}

void QtOSGViewer::closegraph(void* handle)
{

}

void QtOSGViewer::_Deselect()
{

}

void QtOSGViewer::Reset()
{

}

void QtOSGViewer::SetBkgndColor(const RaveVector<float>& color)
{

}

void QtOSGViewer::StartPlaybackTimer()
{

}
void QtOSGViewer::StopPlaybackTimer()
{

}

void QtOSGViewer::SetSize(int w, int h)
{
    RAVELOG_VERBOSE("----->>>> ViewerSetSize(int w, int h)\n");

}
void QtOSGViewer::Move(int x, int y)
{
    RAVELOG_VERBOSE("----->>>> ViewerMove(int x, int y)\n");
}

void QtOSGViewer::SetName(const string& ptitle)
{
    setWindowTitle(ptitle.c_str());
}

bool QtOSGViewer::LoadModel(const string& filename)
{
    //    Debug
//  RAVELOG_WARN("QtOSGViewer::LoadModel(pfilename)\n");

    if( filename == "")
        return false;

    osg::ref_ptr<osg::Node> loadedModel = osgDB::readNodeFile(filename);
    osg::Node *node = loadedModel.get();

    if (node != NULL) {
        GetRoot()->addChild(node);
        return true;
    }

    return false;
}

void QtOSGViewer::UpdateFromModel()
{
    {
        boost::mutex::scoped_lock lock(_mutexItems);
        FOREACH(it,_listRemoveItems) {
            delete *it;
        }
        _listRemoveItems.clear();
    }

    bool newdata;

    _listRemoveItems.clear();

    vector<KinBody::BodyState> vecbodies;

    GetEnv()->GetPublishedBodies(vecbodies);

    newdata = false;

    FOREACH(it, _mapbodies) {
        it->second->SetUserData(0);
    }

    FOREACH(itbody, vecbodies) {
        BOOST_ASSERT( !!itbody->pbody );
        KinBodyPtr pbody = itbody->pbody; // try to use only as an id, don't call any methods!
        KinBodyItemPtr pitem = boost::dynamic_pointer_cast<KinBodyItem>(pbody->GetUserData(_userdatakey));

        //  if pitem is FALSE
        if( !pitem ) {
            // create a new body
            // make sure pbody is actually present
            if( GetEnv()->GetBodyFromEnvironmentId(itbody->environmentid) == pbody ) {

                // check to make sure the real GUI data is also NULL
                if( !pbody->GetUserData(_userdatakey) ) {
                    if( _mapbodies.find(pbody) != _mapbodies.end() ) {
                        continue;
                    }

                    if( pbody->IsRobot() ) {
                        pitem = boost::shared_ptr<RobotItem>(new RobotItem(shared_viewer(), boost::static_pointer_cast<RobotBase>(pbody), _viewGeometryMode));
                    }
                    else {
                        pitem = boost::shared_ptr<KinBodyItem>(new KinBodyItem(shared_viewer(), pbody, _viewGeometryMode));
                    }
                    newdata = true;

                    //  Adds pitem to list of items for remove
                    _listRemoveItems.push_back(pitem.get());

                    pitem->Load();

                    //  Modified for OpenRAVE 0.5v
                    pbody->SetUserData(_userdatakey, pitem);

                    _mapbodies[pbody] = pitem;
                }
                else {
                    pitem = boost::static_pointer_cast<KinBodyItem>(pbody->GetUserData(_userdatakey));
                    BOOST_ASSERT( _mapbodies.find(pbody) != _mapbodies.end() && _mapbodies[pbody] == pitem );
                }
            }
            else {
                // body is gone
                continue;
            }
        }

        map<KinBodyPtr, KinBodyItemPtr>::iterator itmap = _mapbodies.find(pbody);

        //  There are NO mapbodies
        if( itmap == _mapbodies.end() ) {
            continue;
        }

        //  TODO : Revise
        BOOST_ASSERT( pitem->GetBody() == pbody);
        BOOST_ASSERT( itmap->second == pitem );

        pitem->SetUserData(1);

        //  Update viewer with core transforms
        pitem->UpdateFromModel(itbody->jointvalues,itbody->vectrans);
    }

    //  Repaint the scene created
    _RepaintWidgets(GetRoot());

    if (newdata)
    {
        //  Fill tree widget with robot and joints
        _FillObjectTree(objectTree);
    }
    
    FOREACH_NOINC(it, _mapbodies) {
        if(!it->second->GetUserData()) {
            //  Modified for OpenRAVE 0.5v
            it->first->RemoveUserData(_userdatakey);
            _mapbodies.erase(it++);
        }
        else {
            ++it;
        }
    }
}

boost::shared_ptr<EnvironmentMutex::scoped_try_lock> QtOSGViewer::LockEnvironment(uint64_t timeout,bool bUpdateEnvironment)
{
    // try to acquire the lock
#if BOOST_VERSION >= 103500
    boost::shared_ptr<EnvironmentMutex::scoped_try_lock> lockenv(new EnvironmentMutex::scoped_try_lock(GetEnv()->GetMutex(),boost::defer_lock_t()));
#else
    boost::shared_ptr<EnvironmentMutex::scoped_try_lock> lockenv(new EnvironmentMutex::scoped_try_lock(GetEnv()->GetMutex(),false));
#endif
    uint64_t basetime = utils::GetMicroTime();
    while(utils::GetMicroTime()-basetime<timeout ) {
        if( lockenv->try_lock() ) {
            break;
        }
        if( bUpdateEnvironment ) {
            _UpdateEnvironment(0);
        }
    }

    if( !*lockenv ) {
        lockenv.reset();
    }
    return lockenv;
}

void QtOSGViewer::_UpdateEnvironment(float fTimeElapsed)
{
    boost::mutex::scoped_lock lockupd(_mutexUpdating);

    if( _bUpdateEnvironment ) {
        // process all messages
//        list<EnvMessagePtr> listmessages;
//        {
//            boost::mutex::scoped_lock lockmsg(_mutexMessages);
//            listmessages.swap(_listMessages);
//            BOOST_ASSERT( _listMessages.size() == 0 );
//        }
//
//        FOREACH(itmsg, listmessages) {
//            (*itmsg)->viewerexecute();
//        }

        // have to update model after messages since it can lock the environment
        UpdateFromModel();
        _UpdateCameraTransform(fTimeElapsed);
    }
}

////////////////////////////////////////////////////////////////////////////////
/// Environment message
////////////////////////////////////////////////////////////////////////////////
//QtOSGViewer::EnvMessage::EnvMessage(QtOSGViewerPtr pviewer, void** ppreturn, bool bWaitForMutex)
//    : _pviewer(pviewer), _ppreturn(ppreturn)
//{
//    // get a mutex
//    if( bWaitForMutex ) {
//        _plock.reset(new boost::mutex::scoped_lock(_mutex));
//    }
//}
//
//QtOSGViewer::EnvMessage::~EnvMessage()
//{
//    _plock.reset();
//}
//
///// execute the command in the caller
//void QtOSGViewer::EnvMessage::callerexecute()
//{
//    bool bWaitForMutex = !!_plock;
//
//    {
//        boost::mutex::scoped_lock lock(_pviewer->_mutexMessages);
//        _pviewer->_listMessages.push_back(shared_from_this());
//    }
//
//    if( bWaitForMutex )
//        boost::mutex::scoped_lock lock(_mutex);
//}
//
///// execute the command in the viewer
//void QtOSGViewer::EnvMessage::viewerexecute()
//{
//    _plock.reset();
//}

void QtOSGViewer::SetEnvironmentSync(bool bUpdate)
{
    boost::mutex::scoped_lock lockupdating(_mutexUpdating);
    boost::mutex::scoped_lock lock(_mutexUpdateModels);
    _bUpdateEnvironment = bUpdate;
    _condUpdateModels.notify_all();

    if( !bUpdate ) {
        // remove all messages in order to release the locks
        boost::mutex::scoped_lock lockmsg(_mutexMessages);
//        FOREACH(it,_listMessages) {
//            (*it)->releasemutex();
//        }
//        _listMessages.clear();
    }
}

void QtOSGViewer::_DeleteItemCallback(Item* pItem)
{
    boost::mutex::scoped_lock lock(_mutexItems);
    pItem->PrepForDeletion();
    _listRemoveItems.push_back(pItem);
}

void QtOSGViewer::EnvironmentSync()
{
    {
        boost::mutex::scoped_lock lockupdating(_mutexUpdating);
        if( !_bUpdateEnvironment ) {
            RAVELOG_WARN("cannot update models from environment sync\n");
            return;
        }
    }

    boost::mutex::scoped_lock lock(_mutexUpdateModels);
    _bModelsUpdated = false;
    _condUpdateModels.wait(lock);
    if( !_bModelsUpdated ) {
        RAVELOG_WARNA("failed to update models from environment sync\n");
    }
}

UserDataPtr QtOSGViewer::RegisterItemSelectionCallback(const ItemSelectionCallbackFn& fncallback)
{
    ItemSelectionCallbackDataPtr pdata(new ItemSelectionCallbackData(fncallback,shared_viewer()));
    pdata->_iterator = _listRegisteredItemSelectionCallbacks.insert(_listRegisteredItemSelectionCallbacks.end(),pdata);
    return pdata;
}

ViewerBasePtr CreateQtOSGViewer(EnvironmentBasePtr penv, std::istream& sinput)
{
    return ViewerBasePtr(new QtOSGViewer(penv, sinput));
}

} // end namespace qtosgviewer
