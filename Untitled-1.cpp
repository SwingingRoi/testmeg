/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2019 Tianjin KYLIN Information Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include <QWidget>

#include "about.h"
#include "trialdialog.h"

#include <KFormat>
#include <unistd.h>
#include <QFile>
#include <QGridLayout>
#include <QPluginLoader>
#include <QEvent>
#include <QMessageBox>

#ifdef Q_OS_LINUX
#include <sys/sysinfo.h>
#elif defined(Q_OS_FREEBSD)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include <QProcess>
#include <QFile>
#include <QDebug>
#include <QStorageInfo>
#include <QtMath>
#include <QSvgRenderer>
#include <QSqlQuery>
#include <QSqlRecord>

const QString kAboutFile = "/usr/share/applications/kylin-user-guide.desktop";
const QString kHPFile = "/usr/share/applications/hp-document.desktop";
QStringList mIpList;

About::About() : mFirstLoad(true)
{
#ifdef MAVIS
    pluginName = tr("About and Support");
#else
    pluginName = tr("About");
#endif
    pluginType = SYSTEM;
}

About::~About()
{
    if (!mFirstLoad) {

    }
}

QString About::plugini18nName()
{
    return pluginName;
}

int About::pluginTypes()
{
    return pluginType;
}

QWidget *About::pluginUi()
{
    if (mFirstLoad) {
        mFirstLoad = false;

        mAboutWidget = new AboutUi;
        mAboutDBus = new QDBusInterface("org.ukui.ukcc.session",
                                        "/About",
                                        "org.ukui.ukcc.session.About",
                                        QDBusConnection::sessionBus(), this);
        if (!mAboutDBus->isValid()) {
            qCritical() << "org.ukui.ukcc.session.About DBus error:" << mAboutDBus->lastError();
        } else {
            QDBusConnection::sessionBus().connect("org.ukui.ukcc.session",
                                                  "/About",
                                                  "org.ukui.ukcc.session.About",
                                                  "changed",
                                                  this,
                                                  SLOT(keyChangedSlot(QString)));
            mAboutWidget->getEditHost()->installEventFilter(this);
            mAboutWidget->getSequenceContent()->installEventFilter(this);
            setConnect();
            setupVersionCompenent();
            setVersionNumCompenent();
            setupDesktopComponent();
            setHostNameCompenet();
            setupKernelCompenent();
            initActiveDbus();
            setupSerialComponent();
            setPrivacyCompent();
            setupDiskCompenet();
            securityControl();
            setupSysInstallComponent();
            setupUpgradeComponent();
        }
    }
    return mAboutWidget;
}

const QString About::name() const
{
    return QStringLiteral("About");
}

bool About::isShowOnHomePage() const
{
    return true;
}

QIcon About::icon() const
{
    if (QIcon::hasThemeIcon("preferences-system-details-symbolic")) {
        return QIcon::fromTheme("preferences-system-details-symbolic");
    } else {
        return QIcon();
    }
}

bool About::isEnable() const
{
    return true;
}

/* 初始化DBus对象 */
void About::initActiveDbus()
{
    activeInterface = QSharedPointer<QDBusInterface>(
        new QDBusInterface("org.freedesktop.activation",
                           "/org/freedesktop/activation",
                           "org.freedesktop.activation.interface",
                           QDBusConnection::systemBus()));
    if (activeInterface.get()->isValid()) {
        connect(activeInterface.get(), SIGNAL(activation_result(int)), this, SLOT(activeSlot(int)));
    }
}

void About::initCopyRightName()
{
#ifdef OPENKYLIN
    mCopyRightName = tr("Openkylin");
#else
    mCopyRightName = tr("KylinSoft");
#endif
}

void About::setConnect()
{
    connect(mAboutWidget->getHpBtn(),&QPushButton::clicked,this,[=](){
        openIntelSlot(kHPFile);
    });

    connect(mAboutWidget->getEducateBtn(), &QPushButton::clicked, this, [=]{
        openIntelSlot(kAboutFile);
    });

    connect(mAboutWidget->getActivationBtn(), &QPushButton::clicked, this, &About::runActiveWindow);

    connect(mAboutWidget->getTrialBtn(), &KBorderlessButton::clicked, this, [=]() {
        Common::buriedSettings(name(), "show trial exemption agreement", QString("clicked"));
        TrialDialog *dialog = new TrialDialog(mAboutWidget);
        dialog->exec();
    });

    connect(mAboutWidget->getAgreeBtn(), &KBorderlessButton::clicked, this, [=]() {
        Common::buriedSettings(name(), "show user privacy agreement", QString("clicked"));
        PrivacyDialog *dialog = new PrivacyDialog(mAboutWidget);
        dialog->exec();
    });
}

/* 获取激活信息 */
void About::setupSerialComponent()
{
    if (!activeInterface.get()->isValid()) {
        qDebug() << "Create active Interface Failed When Get active info: " <<
            QDBusConnection::systemBus().lastError();
        return;
    }

    int status = 0;
    QDBusMessage activeReply = activeInterface.get()->call("status");
    if (activeReply.type() == QDBusMessage::ReplyMessage) {
        status = activeReply.arguments().at(0).toInt();
    }
    int trial_status = 0;
    QDBusMessage trialReply = activeInterface.get()->call("trial_status");
    if (trialReply.type() == QDBusMessage::ReplyMessage) {
        trial_status = trialReply.arguments().at(0).toInt();
    }

    QString serial;
    QDBusReply<QString> serialReply;
    serialReply = activeInterface.get()->call("serial_number");
    if (!serialReply.isValid()) {
        qDebug()<<"serialReply is invalid"<<endl;
    } else {
        serial = serialReply.value();
    }

    QDBusMessage dateReply = activeInterface.get()->call("date");
    if (dateReply.type() == QDBusMessage::ReplyMessage) {
        dateRes = dateReply.arguments().at(0).toString();
    }

    QDBusMessage trial_dateReply = activeInterface.get()->call("trial_date");
    QString trial_dateRes;
    if (trial_dateReply.type() == QDBusMessage::ReplyMessage) {
        trial_dateRes = trial_dateReply.arguments().at(0).toString();
    }
    mAboutWidget->getSequenceContent()->setText(serial);
    mAboutWidget->getSequenceContent()->setStyleSheet("color : #2FB3E8");
    mTimeText = tr("DateRes");
    if (dateRes.isEmpty()) {  //未激活
         if (!trial_dateRes.isEmpty()) {  //试用期
            mAboutWidget->getActiveStatus()->setText(tr("Inactivated"));
            mAboutWidget->getActiveStatus()->setStyleSheet("color : red ");
            mTimeText = tr("Trial expiration time");
            dateRes = trial_dateRes;
            mAboutWidget->getActivationBtn()->setText(tr("Active"));
        } else {
             mAboutWidget->getActiveStatus()->setText(tr("Inactivated"));
             mAboutWidget->getActiveStatus()->setStyleSheet("color : red ");
             mAboutWidget->getActivationBtn()->setText(tr("Active"));
         }
         activestatus = false;
    }  else {    //已激活
        mAboutWidget->getActivationBtn()->hide();
        mAboutWidget->getTrialBtn()->hide();
        mAboutWidget->getAndLabel()->hide();
        mAboutWidget->getActiveStatus()->setStyleSheet("");
        mAboutWidget->getActiveStatus()->setText(tr("Activated"));
        mAboutWidget->getActivationBtn()->setText(tr("Extend"));
    }
}

/* 获取内部版本号 */
void About::setVersionNumCompenent()
{
    QStringList list = mAboutDBus->property("build").toStringList();
    mAboutWidget->getBuild()->setText(list.at(0));
    mAboutWidget->getPatchVersion()->setText(list.at(1));
    mAboutWidget->getBuildFrame()->setHidden(mAboutWidget->getBuild()->text().isEmpty());
    mAboutWidget->getPatchFrame()->setHidden(mAboutWidget->getPatchVersion()->text().isEmpty());
}

/* 获取logo图片和版本名称 */
void About::setupVersionCompenent()
{
    QString versionID;
    QString version;

    QStringList list = mAboutDBus->property("versionInfo").toStringList();
    if (list.count() < 2) {
        return;
    } else {
        versionID = list.at(0);
        version = list.at(1);
    }

    if (QString(kdk_system_get_systemCategory()).compare("MaxTablet") == 0) {
        version = tr("Kylin Linux Desktop (Touch Screen) V10 (SP1)");
    } else if (QString(kdk_system_get_systemCategory()).compare("Tablet") == 0) {
        version = tr("Kylin Linux Desktop (Tablet) V10 (SP1)");
    }

    if (!version.isEmpty()) {
        mAboutWidget->getVersion()->setText(version);
    } else {
        mAboutWidget->getVersion()->setText(tr("Kylin Linux Desktop V10 (SP1)"));
    }

    if (!versionID.compare(vTen, Qt::CaseInsensitive) ||
        !versionID.compare(vTenEnhance, Qt::CaseInsensitive) ||
        !versionID.compare(vFour, Qt::CaseInsensitive)) {

        if (mAboutDBus->property("themeMode").toString() == UKUI_DARK) {
            mThemePixmap = loadSvg("://img/plugins/about/logo-dark.svg", 130, 50);
        } else {
            mThemePixmap = loadSvg("://img/plugins/about/logo-light.svg", 130, 50);
        }
    } else {
        mAboutWidget->getActivaFrame()->setVisible(false);
        mAboutWidget->getTrialBtn()->setVisible(false);
        mAboutWidget->getAndLabel()->setVisible(false);
        mThemePixmap = loadSvg("://img/plugins/about/logoukui.svg", 130, 50);
    }
    mAboutWidget->getLogo()->setPixmap(mThemePixmap);
}

/* 获取桌面信息 */
void About::setupDesktopComponent()
{
    QString desktop = mAboutDBus->property("desktop").toString();
    mAboutWidget->getDesktopEnv()->setText(desktop);
    changedSlot();
    QDBusConnection::systemBus().connect(QString(), QString("/org/freedesktop/Accounts/User1000"),
                                         QString("org.freedesktop.Accounts.User"), "Changed",this,
                                         SLOT(changedSlot()));
}

/* 获取CPU信息 */
void About::setupKernelCompenent()
{
    QString memorySize("0GB");
    QString cpuType;
    QString kernal = QSysInfo::kernelType() + " " + QSysInfo::kernelVersion();
    mUkccDbus = new QDBusInterface("com.control.center.qt.systemdbus",
                                   "/",
                                   "com.control.center.interface",
                                   QDBusConnection::systemBus(), this);
   if (mUkccDbus->isValid()) {
       QDBusReply<QString>  result = mUkccDbus->call("getMemory");
       qDebug()<<"memory :"<<result;
       if (result != "0") {
            memorySize.clear();
            memorySize.append(result + "GB" + mMemAvaliable);
       }
   }
   if (memorySize == "0GB")
       memorySize = mAboutDBus->property("memory").toString();

    mAboutWidget->getKerner()->setText(kernal);
    mAboutWidget->getMemeory()->setText(memorySize);

    cpuType = Common::getCpuInfo();
    mAboutWidget->getCpuInfo()->setText(cpuType);
}


/* 获取硬盘信息 */
void About::setupDiskCompenet()
{
    QString diskSize = mAboutDBus->property("blockInfo").toString();
    foreach (QString diskResult, diskSize.split("\n")) {
        if (diskResult == NULL)
            continue;
        diskResult.replace(QRegExp("[\\s]+"), " ");
        diskInfo = diskResult.split(" ");

        if (diskInfo.count() >= 6) {
            if (diskInfo.at(5).contains("part"))
                mDiskParts.append(diskInfo.at(0));
        }
        if (diskInfo.count() >= 6 && (!diskInfo.at(0).contains("fd")) &&
               (diskInfo.at(2) !="1")) { //过滤掉可移动硬盘
            if (diskInfo.at(5) == "disk") {
                QStringList totalSize;
                totalSize.append(diskInfo.at(3));
                disk2.insert(diskInfo.at(0),totalSize); //硬盘信息分类存储，用以兼容多硬盘电脑
            }

        }
    }
    QString diskSize2 = mAboutDBus->property("diskInfo").toString();
    double availSize=0;
    QStringList diskInfo2;
    QMap<QString, QStringList>::iterator iter;
    for(iter=disk2.begin();iter!=disk2.end();iter++)
    {
        foreach (QString diskResult, diskSize2.split("\n")) {
            if (diskResult == NULL)
                continue;
            diskResult.replace(QRegExp("[\\s]+"), " ");
            diskInfo2 = diskResult.split(" ");
            if(diskInfo2.at(1).contains("overlay"))
                continue;
            if(diskInfo2.at(0).contains(iter.key())){
                availSize += diskInfo2.at(4).toInt();
            }
        }
        QString diskAvailable = QString::number((availSize/1024/1024), 'f', 1) + "G";
        iter.value().append(diskAvailable);
        availSize = 0;
    }
    if (mAboutWidget->getDiskFrame()->isHidden())
        return;
    int count = 0;
    for(iter=disk2.begin();iter!=disk2.end();iter++){
        if (disk2.size() == 1) {
            mAboutWidget->getDiskLabel()->show();
            mAboutWidget->getDiskContent()->show();
            mAboutWidget->getDiskContent()->setText(iter.value().at(0) + "B (" + iter.value().at(1) + "B "+ tr("avaliable") +")");
        }
        else {
            mAboutWidget->getDiskLabel()->hide();
            mAboutWidget->getDiskContent()->hide();
            QHBoxLayout * layout = new QHBoxLayout;
            QLabel *label = new FixLabel;
            label->setText(tr("Disk") + QString::number(count + 1));
            QLabel *diskLabel = new FixLabel;
            diskLabel->setText(iter.value().at(0) + "B (" + iter.value().at(1) + "B "+ tr("avaliable") +")");

            layout->addWidget(label);
            layout->addWidget(diskLabel);
            layout->addStretch();
            mAboutWidget->getDiskLayout()->addLayout(layout);

        }
    }
}

void About::setHostNameCompenet()
{
    mAboutWidget->getHostName()->setText(Common::getHostName());
}

void About::setPrivacyCompent()
{
    if (Common::isWayland())
        return;
    QDBusInterface *PriDBus = new QDBusInterface("com.kylin.daq",
                                                 "/com/kylin/daq",
                                                 "com.kylin.daq.interface",
                                                 QDBusConnection::systemBus(), this);
    if (!PriDBus->isValid()) {
        return;
    }
    QDBusReply<int> reply = PriDBus->call("GetUploadState");
    mAboutWidget->getPriBtn()->blockSignals(true);
    mAboutWidget->getPriBtn()->setChecked(reply == 0 ? false : true);
    mAboutWidget->getPriBtn()->blockSignals(false);

    connect(mAboutWidget->getPriBtn(),&KSwitchButton::stateChanged ,this ,[=](bool status){
        Common::buriedSettings(name(), "Send optional diagnostic data", QString("settings"), status ? "true": "false");
        PriDBus->call("SetUploadState" , (status ? 1 : 0));
    } );
}

void About::showExtend(QString dateres)
{
    dateRes = dateres + QString("(%1)").arg(tr("expired"));
    mAboutWidget->getActivationBtn()->setText(tr("Extend"));
}

int About::getMonth(QString month)
{
    if (month == "Jan") {
        return 1;
    } else if (month == "Feb") {
        return 2;
    } else if (month == "Mar") {
        return 3;
    } else if (month == "Apr") {
        return 4;
    } else if (month == "May") {
        return 5;
    } else if (month == "Jun") {
        return 6;
    } else if (month == "Jul") {
        return 7;
    } else if (month == "Aug") {
        return 8;
    } else if (month == "Sep" || month == "Sept") {
        return 9;
    } else if (month == "Oct") {
        return 10;
    } else if (month == "Nov") {
        return 11;
    } else if (month == "Dec") {
        return 12;
    }else {
        return 0;
    }
}

void About::reboot()
{
    QDBusInterface *rebootDbus = new QDBusInterface("org.gnome.SessionManager",
                                                    "/org/gnome/SessionManager",
                                                    "org.gnome.SessionManager",
                                                    QDBusConnection::sessionBus());

    rebootDbus->call("reboot");
    delete rebootDbus;
    rebootDbus = nullptr;
}

/* 处理窗口缩放时的文本显示 */
bool About::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == mAboutWidget->getEditHost()) {
        if (event->type() == QEvent::MouseButtonPress){
            QMouseEvent * mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton ){            
                QString str = Common::getHostName();
                HostNameDialog *dialog = new HostNameDialog(mAboutWidget);
                QWidget *widget = qApp->activeWindow(); // 记录mainwindow的地址，exec之后，activeWindow会变成空值
                dialog->exec();
                if (str !=  Common::getHostName()) {
                    QMessageBox *mReboot = new QMessageBox(widget);
                    mReboot->setIcon(QMessageBox::Warning);
                    mReboot->setText(tr("The system needs to be restarted to set the HostName, whether to reboot"));
                    mReboot->addButton(tr("Reboot Now"), QMessageBox::AcceptRole);
                    mReboot->addButton(tr("Reboot Later"), QMessageBox::RejectRole);
                    int ret = mReboot->exec();
                    switch (ret) {
                    case QMessageBox::AcceptRole:
                        sleep(1);
                        reboot();
                        break;
                    }
                    mAboutWidget->getHostName()->setText(Common::getHostName());
                    Common::buriedSettings(name(), "change hostname", QString("settings"), Common::getHostName());
                }
            }
        }
    } else if ( obj == mAboutWidget->getSequenceContent()) {
        if (event->type() == QEvent::MouseButtonPress){
            QMouseEvent * mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton  && !mAboutWidget->getSequenceContent()->text().isEmpty()){
                Common::buriedSettings(name(), "show activation info", QString("clicked"));
                if (!dateRes.isEmpty())
                    compareTime(dateRes);
                StatusDialog *dialog = new StatusDialog(mAboutWidget);
                dialog->mLogoLabel->setPixmap(mThemePixmap);
                connect(this,&About::changeTheme,[=](){
                    dialog->mLogoLabel->setPixmap(mThemePixmap);
                });
                dialog->mVersionLabel_1->setText(tr("Version"));
                dialog->mVersionLabel_2->setText(mAboutWidget->getVersion()->text());
                dialog->mStatusLabel_1->setText(tr("Status"));
                dialog->mStatusLabel_2->setText(mAboutWidget->getActiveStatus()->text());
                dialog->mSerialLabel_1->setText(tr("Serial"));
                dialog->mSerialLabel_2->setText(mAboutWidget->getSequenceContent()->text());
                dialog->mTimeLabel_1->setText(mTimeText);
                dialog->mTimeLabel_2->setText(dateRes);
                if (dialog->mTimeLabel_2->text().contains(tr("expired"))) {
                    dialog->mTimeLabel_2->setStyleSheet("color : red ");
                } else {
                    dialog->mTimeLabel_2->setStyleSheet("");
                }

                if (!activestatus) {
                    dialog->mTimeLabel_1->parentWidget()->hide();
                }
                dialog->mExtentBtn->setText(mAboutWidget->getActivationBtn()->text());
                connect(dialog->mExtentBtn, &QPushButton::clicked, this, &About::runActiveWindow);
                dialog->exec();
                return true;
            }
        }
    }
    return QObject::eventFilter(obj, event);
}


QStringList About::getUserDefaultLanguage()
{
    QString formats;
    QString language;
    QStringList result;

    unsigned int uid = getuid();
    QString objpath = "/org/freedesktop/Accounts/User"+QString::number(uid);

    QDBusInterface iproperty("org.freedesktop.Accounts",
                             objpath,
                             "org.freedesktop.DBus.Properties",
                             QDBusConnection::systemBus());
    QDBusReply<QMap<QString, QVariant> > reply = iproperty.call("GetAll", "org.freedesktop.Accounts.User");
    if (reply.isValid()) {
        QMap<QString, QVariant> propertyMap;
        propertyMap = reply.value();
        if (propertyMap.keys().contains("FormatsLocale")) {
            formats = propertyMap.find("FormatsLocale").value().toString();
        }
        if(language.isEmpty() && propertyMap.keys().contains("Language")) {
            language = propertyMap.find("Language").value().toString();
        }
    }
    qDebug()<<formats<<"---"<<language;
    result.append(formats);
    result.append(language);
    return result;
}

void About::setupSysInstallComponent()
{
    if (mAboutWidget->getInstallDateFrame()->isHidden())
        return;
    for (QString part : mDiskParts) {
        part = part.mid(2);
        if (mUkccDbus->isValid()) {
            QDBusReply<QString>  result = mUkccDbus->call("getSysInstallTime", part);
            if (result != "") {
                QStringList list = QString(result).split(" ");
                if (list.count() >= 5) {
                    QString date = list.at(2);
                    date = date.toInt() < 10 ? QString("0%1").arg(date) : date;
                    mAboutWidget->getInstallDate()->setText(QString("%1-%2-%3").arg(list.at(4)).arg(QString::number(getMonth(list.at(1)))).arg(date));
                    return;
                }
            }
        }
    }
    mAboutWidget->getInstallDateFrame()->hide();
}

void About::setupUpgradeComponent()
{
    if (mAboutWidget->getUpgradeDateFrame()->isHidden())
        return;

    QString upgradeDate = mAboutDBus->property("upgradeDate").toString();
    if (upgradeDate.isEmpty())
        mAboutWidget->getUpgradeDateFrame()->hide();
    else
        mAboutWidget->getUpgradeDate()->setText(upgradeDate);
}

void About::securityControl()
{
    mAboutWidget->getInstallDateFrame()->hide();
    mAboutWidget->getUpgradeDateFrame()->hide();
    // 安全管控 安装时间及系统更新时间的显示与隐藏
    QVariantMap ModuleMap = Common::getModuleHideStatus();
    QString moduleSettings = ModuleMap.value(name().toLower() + "Settings").toString();
    QStringList setItems = moduleSettings.split(",");

    foreach (QString setItem, setItems) {
        QStringList item = setItem.split(":");
        qDebug() << "set item Name: " << item.at(0);
        if (item.at(0) == "installedDateFrame") {
            mAboutWidget->getInstallDateFrame()->setVisible(item.at(1) == "true");
        }
        if (item.at(0) == "upgradeDateFrame") {
            mAboutWidget->getUpgradeDateFrame()->setVisible(item.at(1) == "true");
        }
    }
}

void About::compareTime(QString date)
{
    QString s1 = mAboutDBus->property("netDate").toString();
    QStringList list_1;
    QStringList list_2 = date.split("-");
    int year;
    int mouth;
    int day;
    if (s1.isNull()) {    //未连接上网络, 和系统时间作对比
        QString currenttime =  QDateTime::currentDateTime().toString("yyyy-MM-dd");
        qDebug()<<currenttime;
         list_1 = currenttime.split("-");
         year = QString(list_1.at(0)).toInt();
         mouth = QString(list_1.at(1)).toInt();
         day = QString(list_1.at(2)).toInt();
    } else {    //获取到网络时间
        s1.remove(QChar('\n'), Qt::CaseInsensitive);
        s1.replace(QRegExp("[\\s]+"), " ");   //把所有的多余的空格转为一个空格
        qDebug()<<"网络时间 : "<<s1;
        list_1 = s1.split(" ");
        year = QString(list_1.at(list_1.count() -1)).toInt();
        mouth = getMonth(list_1.at(1));
        day = QString(list_1.at(2)).toInt();
    }
    if (QString(list_2.at(0)).toInt() > year) { //未到服务到期时间
    } else if (QString(list_2.at(0)).toInt() == year) {
        if (QString(list_2.at(1)).toInt() > mouth) {
        } else if (QString(list_2.at(1)).toInt() == mouth) {
            if (QString(list_2.at(2)).toInt() > day) {
            } else {   // 已过服务到期时间
                showExtend(date);
            }
        } else {
            showExtend(date);
        }
    } else {
        showExtend(date);
    }
}

void About::activeSlot(int activeSignal)
{
    if (!activeSignal) {
        setupSerialComponent();
    }
}

/* 打开激活窗口 */
void About::runActiveWindow()
{
    Common::buriedSettings(name(), "Activate the system or extend the service", QString("settings"));
    mAboutDBus->call("openActivation");
}

/* 获取用户昵称 */
void About::changedSlot()
{
    qlonglong uid = getuid();
    QDBusInterface user("org.freedesktop.Accounts",
                        "/org/freedesktop/Accounts",
                        "org.freedesktop.Accounts",
                        QDBusConnection::systemBus());
    QDBusMessage result = user.call("FindUserById", uid);
    QString userpath = result.arguments().value(0).value<QDBusObjectPath>().path();
    QDBusInterface *userInterface = new QDBusInterface ("org.freedesktop.Accounts",
                                          userpath,
                                        "org.freedesktop.Accounts.User",
                                        QDBusConnection::systemBus());
    QString userName = userInterface->property("RealName").value<QString>();
    mAboutWidget->getUserName()->setText(userName);
}

void About::openIntelSlot(const QString &desktopFile)
{
    QDBusInterface ifc("com.kylin.AppManager",
                       "/com/kylin/AppManager",
                       "com.kylin.AppManager",
                       QDBusConnection::sessionBus());
    ifc.call("LaunchApp", desktopFile);
}

void About::keyChangedSlot(const QString &key)
{
    if (key == "styleName") {
        if (mAboutDBus->property("themeMode").toString() == UKUI_DARK) {
            mThemePixmap = loadSvg("://img/plugins/about/logo-dark.svg", 130, 50);
        } else {
            mThemePixmap = loadSvg("://img/plugins/about/logo-light.svg", 130, 50);
        }
        mAboutWidget->getLogo()->setPixmap(mThemePixmap);
        emit changeTheme();
    }
}

QPixmap About::loadSvg(const QString &path, int width, int height) {
    const auto ratio = qApp->devicePixelRatio();
    if (ratio >= 2) {
        width += width;
        height += height;
    } else {
        height *= ratio;
        width *= ratio;
    }
    QPixmap pixmap(width, height);
    QSvgRenderer renderer(path);
    pixmap.fill(Qt::transparent);

    QPainter painter;
    painter.begin(&pixmap);
    renderer.render(&painter);
    painter.end();

    pixmap.setDevicePixelRatio(ratio);
    return pixmap;
}
