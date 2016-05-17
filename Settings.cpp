#include "Settings.h"
#include "logging.h"

#include <QAbstractButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QQuickItem>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

using namespace qtredmine;
using namespace redtimer;
using namespace std;

Settings::Settings( SimpleRedmineClient* redmine, QQuickView* parent )
    : Window( "qrc:/Settings.qml", parent ),
      redmine_( redmine ),
      settings_( QSettings::IniFormat, QSettings::UserScope, "Thomssen IT", "RedTimer", this )
{
    ENTER();

    // Settings window initialisation
    setResizeMode( QQuickView::SizeRootObjectToView );
    setModality( Qt::ApplicationModal );
    setFlags( Qt::Dialog );
    setTitle( "Settings" );

    // Set the models
    ctx_->setContextProperty( "issueStatusModel", &issueStatusModel_ );

    profilesProxyModel_.setSourceModel( &profilesModel_ );
    profilesProxyModel_.setSortRole( SimpleModel::NameRole );
    profilesProxyModel_.setDynamicSortFilter( true );
    ctx_->setContextProperty( "profilesModel", &profilesProxyModel_ );

    // Connect the profile selector
    connect( qml("profiles"), SIGNAL(activated(int)), this, SLOT(profileSelected(int)) );

    // Connect the create profile button
    connect( qml("createProfile"), SIGNAL(clicked()), this, SLOT(createProfile()) );

    // Connect the delete profile button
    connect( qml("deleteProfile"), SIGNAL(clicked()), this, SLOT(deleteProfile()) );

    // Connect the rename profile button
    connect( qml("renameProfile"), SIGNAL(clicked()), this, SLOT(renameProfile()) );

    // Connect the cancel button
    connect( qml("cancel"), SIGNAL(clicked()), this, SLOT(cancel()) );

    // Connect the apply button
    connect( qml("apply"), SIGNAL(clicked()), this, SLOT(apply()) );

    // Connect the save button
    connect( qml("save"), SIGNAL(clicked()), this, SLOT(applyAndClose()) );

    RETURN();
}

void
Settings::apply()
{
    ENTER();

    QString oldUrl = data.url;

    data.apiKey            = qml("apikey")->property("text").toString();
    data.checkConnection   = qml("checkConnection")->property("checked").toBool();
    data.ignoreSslErrors   = qml("ignoreSslErrors")->property("checked").toBool();
    data.numRecentIssues   = qml("numRecentIssues")->property("text").toInt();
    data.url               = qml("url")->property("text").toString();
    data.useSystemTrayIcon = qml("useSystemTrayIcon")->property("checked").toBool();

    data.shortcutCreateIssue = qml("shortcutCreateIssue")->property("text").toString();
    data.shortcutSelectIssue = qml("shortcutSelectIssue")->property("text").toString();
    data.shortcutStartStop   = qml("shortcutStartStop")->property("text").toString();
    data.shortcutToggle      = qml("shortcutToggle")->property("text").toString();

    if( oldUrl == data.url )
    {
        int workedOnIndex = qml("workedOn")->property("currentIndex").toInt();
        if( issueStatusModel_.rowCount() && workedOnIndex )
            data.workedOnId = issueStatusModel_.at(workedOnIndex).id();
    }
    else
    {
        data.activityId = NULL_ID;
        data.issueId    = NULL_ID;
        data.projectId  = NULL_ID;
        data.workedOnId = NULL_ID;

        while( !data.recentIssues.isEmpty() )
            data.recentIssues.removeLast();
    }

    DEBUG("Changed settings to")(data.apiKey)(data.checkConnection)(data.ignoreSslErrors)
                                (data.numRecentIssues)(data.url)(data.useSystemTrayIcon)(data.workedOnId);

    save();

    DEBUG() << "Emitting applied() signal";
    applied();

    RETURN();
}

void
Settings::applyAndClose()
{
    ENTER();

    apply();
    hide();

    RETURN();
}

void
Settings::cancel()
{
    ENTER();

    applied();
    hide();

    RETURN();
}

bool
Settings::createProfile()
{
    ENTER();

    int maxProfileNameId = 0;
    int maxId = 0;
    for( const auto& profile : profilesModel_.data() )
    {
        QRegularExpressionMatch match;
        if( profile.name().contains(QRegularExpression("Profile (\\d+)"), &match) && match.hasMatch()
            && match.captured(1).toInt() > maxProfileNameId )
            maxProfileNameId = match.captured(1).toInt();

        if( profile.id() > maxId )
            maxId = profile.id();
    }

    QString name;
    if( !getProfileName( name, tr("Create new profile"), QString("Profile %1").arg(maxProfileNameId+1) ) )
        RETURN( false );

    profilesModel_.push_back( SimpleItem(maxId+1, name) );

    QModelIndex modelIndex = profilesModel_.index( profilesModel_.rowCount() - 1 );
    QModelIndex proxyIndex = profilesProxyModel_.mapFromSource( modelIndex );
    profileSelected( proxyIndex.row() );

    RETURN( true );
}

void
Settings::deleteProfile()
{
    ENTER();

    int index = qml("profiles")->property("currentIndex").toInt();
    QModelIndex proxyIndex = profilesProxyModel_.index( index, 0 );
    QString profileName = proxyIndex.data(SimpleModel::NameRole).toString();

    int ret = QMessageBox::question( qobject_cast<QWidget*>(this), tr("Delete profile"),
                                     tr("Do you really want to delete profile '%1'?").arg(profileName) );

    if( ret != QMessageBox::Yes )
        RETURN();

    QModelIndex modelIndex = profilesProxyModel_.mapToSource( proxyIndex );
    profilesModel_.removeRow( modelIndex.row() );

    // If no profile exists, ask to create a new one until a profile was successfully created
    if( !profilesModel_.rowCount() )
        while( !createProfile() );

    proxyIndex = profilesProxyModel_.index( 0, 0 );
    if( proxyIndex.isValid() )
        profileSelected( proxyIndex.row() );

    RETURN();
}

void
Settings::display( bool loadData )
{
    ENTER();

    if( loadData )
        load();

    // Select current profile
    {
        QModelIndexList indices = profilesProxyModel_.match( profilesProxyModel_.index(0, 0),
                                                             SimpleModel::IdRole, profileId_ );
        qml("profiles")->setProperty( "currentIndex", -1 );
        if( indices.size() )
            qml("profiles")->setProperty( "currentIndex", indices[0].row() );
    }

    qml("apikey")->setProperty( "text", data.apiKey );
    qml("checkConnection")->setProperty( "checked", data.checkConnection );
    qml("ignoreSslErrors")->setProperty( "checked", data.ignoreSslErrors );
    qml("numRecentIssues")->setProperty( "text", data.numRecentIssues );
    qml("url")->setProperty( "text", data.url );
    qml("useSystemTrayIcon")->setProperty( "checked", data.useSystemTrayIcon );

    qml("shortcutCreateIssue")->setProperty( "text", data.shortcutCreateIssue );
    qml("shortcutSelectIssue")->setProperty( "text", data.shortcutSelectIssue );
    qml("shortcutStartStop")->setProperty( "text", data.shortcutStartStop );
    qml("shortcutToggle")->setProperty( "text", data.shortcutToggle );

    updateIssueStatuses();

    showNormal();
    raise();

    RETURN();
}

bool
Settings::getProfileName( QString& name, QString title, QString initText )
{
    ENTER();

    bool ok;
    name = QInputDialog::getText( qobject_cast<QWidget*>(this), title, tr("Profile name:"),
                                  QLineEdit::Normal, initText, &ok );

    if( !ok )
        RETURN( false );

    if( name.isEmpty() )
    {
        QMessageBox::critical( qobject_cast<QWidget*>(this), tr("Create new profile"),
                               tr("No profile name specified. Aborting.") );
        RETURN( false );
    }

    if( profilesModel_.match(profilesModel_.index(0), SimpleModel::NameRole, name).size() )
    {
        QMessageBox::critical( qobject_cast<QWidget*>(this), title,
                               tr("Profile '%1' already exists. Aborting.").arg(name) );
        RETURN( false );
    }

    RETURN( true );
}


void
Settings::load()
{
    ENTER();

    // First, load general settings
    {
        data.position = settings_.value("position").toPoint();

        if( !settings_.value("profileId").isNull() )
            profileId_ = settings_.value("profileId").toInt();
    }

    // Then, load profiles
    {
        loadedProfiles_.clear();
        profilesModel_.clear();
        bool foundProfileId = false;

        int size = settings_.beginReadArray( "profiles" );        
        for( int i = 0; i < size; ++i )
        {
            settings_.setArrayIndex( i );
            profilesModel_.push_back( SimpleItem(settings_.value("id").toInt(),
                                                 settings_.value("name").toString()) );
            if( settings_.value("id").toInt() == profileId_ )
                foundProfileId = true;
        }
        settings_.endArray();

        // If no profile exists, ask to create a new one until a profile was successfully created
        if( profilesModel_.rowCount() == 0 )
            while( !createProfile() );
        else
            profilesModel_.sort( SimpleModel::NameRole );

        if( !foundProfileId )
            profileId_ = profilesModel_.at(0).id();

        for( const auto& profile : profilesModel_.data() )
            loadedProfiles_.insert( profile.id() );
    }

    loadProfileData();

    DEBUG("Loaded settings from file:")
            (data.apiKey)(data.checkConnection)(data.ignoreSslErrors)(data.numRecentIssues)(data.url)
            (data.useSystemTrayIcon)(data.workedOnId)
            (data.activityId)(data.issueId)(data.position)(data.projectId)(data.recentIssues);

    applied();

    RETURN();
}

void
Settings::loadProfileData()
{
    ENTER();

    profileHash_ = QString("profile-%1").arg(profileId_);

    // Then, load general settings for the profile
    settings_.beginGroup( profileHash_ );

    {
        // Settings GUI
        data.apiKey = settings_.value("apikey").toString();
        data.url = settings_.value("url").toString();
        data.checkConnection = settings_.value("checkConnection").toBool();
        data.ignoreSslErrors = settings_.value("ignoreSslErrors").toBool();

        data.useSystemTrayIcon = settings_.value("useSystemTrayIcon").isValid()
                                 ? settings_.value("useSystemTrayIcon").toBool()
                                 : true;

        data.workedOnId = settings_.value("workedOnId").isValid()
                          ? settings_.value("workedOnId").toInt()
                          : NULL_ID;

        data.numRecentIssues = settings_.value("numRecentIssues").isValid()
                               ? settings_.value("numRecentIssues").toInt()
                               : 10;

        // Shortcuts
        data.shortcutCreateIssue = settings_.value("shortcutCreateIssue").isValid()
                                   ? settings_.value("shortcutCreateIssue").toString()
                                   : "Ctrl+Alt+C";

        data.shortcutSelectIssue = settings_.value("shortcutSelectIssue").isValid()
                                   ? settings_.value("shortcutSelectIssue").toString()
                                   : "Ctrl+Alt+L";

        data.shortcutStartStop = settings_.value("shortcutStartStop").isValid()
                                 ? settings_.value("shortcutStartStop").toString()
                                 : "Ctrl+Alt+S";

        data.shortcutToggle = settings_.value("shortcutToggle").isValid()
                              ? settings_.value("shortcutToggle").toString()
                              : "Ctrl+Alt+R";

        // Other GUIs
        data.activityId  = settings_.value("activity").isValid()
                           ? settings_.value("activity").toInt()
                           : NULL_ID;
        data.issueId = settings_.value("issue").isValid()
                       ? settings_.value("issue").toInt()
                       : NULL_ID;
        data.projectId = settings_.value("project").isValid()
                         ? settings_.value("project").toInt()
                         : NULL_ID;
    }

    // Last, load recently used issues for the profile
    {
        int size = settings_.beginReadArray( "recentIssues" );
        for( int i = 0; i < size; ++i )
        {
            settings_.setArrayIndex( i );
            Issue issue;
            issue.id      = settings_.value("id").toInt();
            issue.subject = settings_.value("subject").toString();
            data.recentIssues.append( issue );
        }
        settings_.endArray();
    }

    settings_.endGroup();

    RETURN();
}

void
Settings::profileSelected( int profileIndex )
{
    ENTER();

    QModelIndex proxyIndex = profilesProxyModel_.index( profileIndex, 0 );

    profileId_ = proxyIndex.data(SimpleModel::IdRole).toInt();
    loadProfileData();
    applied();
    display( false );

    RETURN();
}

void
Settings::renameProfile()
{
    ENTER();

    int index = qml("profiles")->property("currentIndex").toInt();
    QModelIndex proxyIndex = profilesProxyModel_.index( index, 0 );
    QString profileName = proxyIndex.data(SimpleModel::NameRole).toString();

    QString newProfileName;
    if( !getProfileName( newProfileName, tr("Rename profile"), profileName ) )
        RETURN();

    QModelIndex modelIndex = profilesProxyModel_.mapToSource( proxyIndex );
    profilesModel_.setData( modelIndex, newProfileName, SimpleModel::NameRole );

    RETURN();
}

void
Settings::save()
{
    ENTER();

    // First, save general settings
    {
        settings_.setValue( "position", data.position );
        settings_.setValue( "profileId", profileId_ );
    }

    // Then, save profiles
    {
        settings_.beginWriteArray( "profiles" );
        int i = 0;
        for( const auto& profile : profilesModel_.data() )
        {
            loadedProfiles_.remove( profile.id() );

            settings_.setArrayIndex( i );
            settings_.setValue( "id",   profile.id() );
            settings_.setValue( "name", profile.name() );
            ++i;
        }
        settings_.endArray();
    }

    settings_.beginGroup( profileHash_ );

    // Then, save general settings for the profile
    {
        // From Settings GUI
        settings_.setValue( "apikey",            data.apiKey );
        settings_.setValue( "checkConnection",   data.checkConnection );
        settings_.setValue( "ignoreSslErrors",   data.ignoreSslErrors );
        settings_.setValue( "numRecentIssues",   data.numRecentIssues );
        settings_.setValue( "url",               data.url );
        settings_.setValue( "useSystemTrayIcon", data.useSystemTrayIcon );
        settings_.setValue( "workedOnId",        data.workedOnId );

        settings_.setValue("shortcutCreateIssue", data.shortcutCreateIssue );
        settings_.setValue("shortcutSelectIssue", data.shortcutSelectIssue );
        settings_.setValue("shortcutStartStop",   data.shortcutStartStop );
        settings_.setValue("shortcutToggle",      data.shortcutToggle );

        // From other GUIs
        settings_.setValue( "activity", data.activityId );
        settings_.setValue( "issue",    data.issueId );
        settings_.setValue( "project",  data.projectId );
    }

    // Last, save recently used issues for the profile
    {
        settings_.beginWriteArray( "recentIssues" );
        for( int i = 0; i < data.recentIssues.size(); ++i )
        {
            settings_.setArrayIndex( i );
            settings_.setValue( "id",      data.recentIssues.at(i).id );
            settings_.setValue( "subject", data.recentIssues.at(i).subject );
        }
        settings_.endArray();
    }

    settings_.endGroup();

    for( const auto& profile : loadedProfiles_ )
    {
        settings_.beginGroup( QString("profile-%1").arg(profile) );
        settings_.remove( "" );
        settings_.endGroup();
    }

    settings_.sync();

    RETURN();
}

void
Settings::updateIssueStatuses()
{
    ENTER();

    if( data.apiKey.isEmpty() || data.url.isEmpty() )
    {
        issueStatusModel_.clear();
        issueStatusModel_.push_back( SimpleItem(NULL_ID, "Currently not available") );

        qml("workedOn")->setProperty( "enabled", false );
        qml("workedOn")->setProperty( "currentIndex", -1 );
        qml("workedOn")->setProperty( "currentIndex", 0 );

        RETURN();
    }

    redmine_->retrieveIssueStatuses( [&]( IssueStatuses issueStatuses, RedmineError redmineError,
                                          QStringList errors )
    {
        ENTER();

        if( redmineError != NO_ERROR )
        {
            QString errorMsg = tr("Could not load issue statuses.");
            for( const auto& error : errors )
                errorMsg.append("\n").append(error);

            message( errorMsg, QtCriticalMsg );
            RETURN();
        }

        int currentIndex = 0;

        // Sort issues ascending by ID
        qSort( issueStatuses.begin(), issueStatuses.end(),
               []( const IssueStatus& a, const IssueStatus& b ){ return a.id < b.id; } );

        issueStatusModel_.clear();
        issueStatusModel_.push_back( SimpleItem(NULL_ID, "Choose issue status") );
        for( const auto& issueStatus : issueStatuses )
        {
            if( issueStatus.id == data.workedOnId )
                currentIndex = issueStatusModel_.rowCount();

            issueStatusModel_.push_back( SimpleItem(issueStatus) );
        }

        DEBUG()(issueStatusModel_)(data.workedOnId)(currentIndex);

        qml("workedOn")->setProperty( "enabled", true );
        qml("workedOn")->setProperty( "currentIndex", -1 );
        qml("workedOn")->setProperty( "currentIndex", currentIndex );

        RETURN();
    } );

    RETURN();
}
