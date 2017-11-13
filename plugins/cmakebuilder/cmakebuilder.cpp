/* KDevelop CMake Support
 *
 * Copyright 2006-2007 Andreas Pakulat <apaku@gmx.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "cmakebuilder.h"
#include "debug.h"

#include <cmakebuilderconfig.h>

#include <project/projectmodel.h>

#include <interfaces/iproject.h>
#include <interfaces/icore.h>
#include <interfaces/iplugincontroller.h>
#include <project/builderjob.h>
#include <makebuilder/imakebuilder.h>

#include <kpluginfactory.h>
#include <KLocalizedString>
#include <kjob.h>
#include <QUrl>

#include "cmakejob.h"
#include "prunejob.h"
#include "cmakebuilderpreferences.h"
#include "cmakeutils.h"
#include <cmakemodelitems.h>

K_PLUGIN_FACTORY_WITH_JSON(CMakeBuilderFactory, "kdevcmakebuilder.json", registerPlugin<CMakeBuilder>(); )

class ErrorJob : public KJob
{
public:
    ErrorJob(QObject* parent, const QString& error)
        : KJob(parent)
        , m_error(error)
    {}

    void start() override {
        setError(!m_error.isEmpty());
        setErrorText(m_error);
        emitResult();
    }

private:
    QString m_error;
};

CMakeBuilder::CMakeBuilder(QObject *parent, const QVariantList &)
    : KDevelop::IPlugin(QStringLiteral("kdevcmakebuilder"), parent)
{
    addBuilder(QStringLiteral("Makefile"), QStringList(QStringLiteral("Unix Makefiles"))
                                                    << QStringLiteral("NMake Makefiles")
                                                    << QStringLiteral("MinGW Makefiles"),
               core()->pluginController()->pluginForExtension(QStringLiteral("org.kdevelop.IMakeBuilder")));
    addBuilder(QStringLiteral("build.ninja"), QStringList(QStringLiteral("Ninja")), core()->pluginController()->pluginForExtension(QStringLiteral("org.kdevelop.IProjectBuilder"), QStringLiteral("KDevNinjaBuilder")));
}

CMakeBuilder::~CMakeBuilder()
{
}

void CMakeBuilder::addBuilder(const QString& neededfile, const QStringList& generators, KDevelop::IPlugin* i)
{
    if( i )
    {
        IProjectBuilder* b = i->extension<KDevelop::IProjectBuilder>();
        if( b )
        {
            m_builders[neededfile] = b;
            foreach(const QString& gen, generators) {
                m_buildersForGenerator[gen] = b;
            }
            // can't use new signal/slot syntax here, IProjectBuilder is not a QObject
            connect(i, SIGNAL(built(KDevelop::ProjectBaseItem*)), this, SIGNAL(built(KDevelop::ProjectBaseItem*)));
            connect(i, SIGNAL(failed(KDevelop::ProjectBaseItem*)), this, SIGNAL(failed(KDevelop::ProjectBaseItem*)));
            connect(i, SIGNAL(cleaned(KDevelop::ProjectBaseItem*)), this, SIGNAL(cleaned(KDevelop::ProjectBaseItem*)));
            connect(i, SIGNAL(installed(KDevelop::ProjectBaseItem*)), this, SIGNAL(installed(KDevelop::ProjectBaseItem*)));

            qCDebug(KDEV_CMAKEBUILDER) << "Added builder " << i->metaObject()->className() << "for" << neededfile;
        }
        else
            qCWarning(KDEV_CMAKEBUILDER) << "Couldn't add" << i->metaObject()->className();
    }
}

KJob* CMakeBuilder::build(KDevelop::ProjectBaseItem *dom)
{
    KDevelop::IProject* p = dom->project();
    IProjectBuilder* builder = builderForProject(p);
    if( builder )
    {
        bool valid;
        KJob* configure = checkConfigureJob(dom->project(), valid);

        KJob* build = nullptr;
        if(dom->file())
        {
            IMakeBuilder* makeBuilder = dynamic_cast<IMakeBuilder*>(builder);
            if (!makeBuilder) {
                return new ErrorJob(this, i18n("Could not find the make builder. Check your installation"));
            }
            KDevelop::ProjectFileItem* file = dom->file();
            int lastDot = file->text().lastIndexOf('.');
            QString target = file->text().mid(0, lastDot)+".o";
            build = makeBuilder->executeMakeTarget(dom->parent(), target);
            qCDebug(KDEV_CMAKEBUILDER) << "create build job for target" << build << dom << target;
        }
        qCDebug(KDEV_CMAKEBUILDER) << "Building with" << builder;
        if (!build)
        {
            build = builder->build(dom);
        }
        if( configure )
        {
            qCDebug(KDEV_CMAKEBUILDER) << "creating composite job";
            KDevelop::BuilderJob* builderJob = new KDevelop::BuilderJob;
            builderJob->addCustomJob( KDevelop::BuilderJob::Configure, configure, dom );
            builderJob->addCustomJob( KDevelop::BuilderJob::Build, build, dom );
            builderJob->updateJobName();
            build = builderJob;
        }
        return build;
    }
    return new ErrorJob(this, i18n("Could not find a builder for %1", p->name()));
}

KJob* CMakeBuilder::clean(KDevelop::ProjectBaseItem *dom)
{
    IProjectBuilder* builder = builderForProject(dom->project());
    if( builder )
    {
        bool valid;
        KJob* configure = checkConfigureJob(dom->project(), valid);

        KDevelop::ProjectBaseItem* item = dom;
        if(dom->file()) //It doesn't work to compile a file
            item=(KDevelop::ProjectBaseItem*) dom->parent();

        qCDebug(KDEV_CMAKEBUILDER) << "Cleaning with" << builder;
        KJob* clean = builder->clean(item);
        if( configure ) {
            KDevelop::BuilderJob* builderJob = new KDevelop::BuilderJob;
            builderJob->addCustomJob( KDevelop::BuilderJob::Configure, configure, item );
            builderJob->addCustomJob( KDevelop::BuilderJob::Clean, clean, item );
            builderJob->updateJobName();
            clean = builderJob;
        }
        return clean;
    }
    return new ErrorJob(this, i18n("Could not find a builder for %1", dom->project()->name()));
}

KJob* CMakeBuilder::install(KDevelop::ProjectBaseItem *dom, const QUrl &installPrefix)
{
    IProjectBuilder* builder = builderForProject(dom->project());
    if( builder )
    {
        bool valid;
        KJob* configure = checkConfigureJob(dom->project(), valid);

        KDevelop::ProjectBaseItem* item = dom;
        if(dom->file())
            item=(KDevelop::ProjectBaseItem*) dom->parent();

        qCDebug(KDEV_CMAKEBUILDER) << "Installing with" << builder;
        KJob* install = builder->install(item, installPrefix);
        if( configure ) {
            KDevelop::BuilderJob* builderJob = new KDevelop::BuilderJob;
            builderJob->addCustomJob( KDevelop::BuilderJob::Configure, configure, item );
            builderJob->addCustomJob( KDevelop::BuilderJob::Install, install, item );
            builderJob->updateJobName();
            install = builderJob;
        }
        return install;

    }
    return new ErrorJob(this, i18n("Could not find a builder for %1", dom->project()->name()));
}

KJob* CMakeBuilder::checkConfigureJob(KDevelop::IProject* project, bool& valid)
{
    valid = false;
    KJob* configure = nullptr;
    if( CMake::checkForNeedingConfigure(project) )
    {
        configure = this->configure(project);
    }
    else if( CMake::currentBuildDir(project).isEmpty() )
    {
        return new ErrorJob(this, i18n("No Build Directory configured, cannot install"));
    }
    valid = true;
    return configure;
}

KJob* CMakeBuilder::configure( KDevelop::IProject* project )
{
    if( CMake::currentBuildDir( project ).isEmpty() )
    {
        return new ErrorJob(this, i18n("No Build Directory configured, cannot configure"));
    }
    CMakeJob* job = new CMakeJob(this);
    job->setProject(project);
    connect(job, &KJob::result, this, [this, project] {
        emit configured(project);
    });
    return job;
}

KJob* CMakeBuilder::prune( KDevelop::IProject* project )
{
    return new PruneJob(project);
}

KDevelop::IProjectBuilder* CMakeBuilder::builderForProject(KDevelop::IProject* p) const
{
    QString builddir = CMake::currentBuildDir( p ).toLocalFile();
    QMap<QString, IProjectBuilder*>::const_iterator it = m_builders.constBegin(), itEnd = m_builders.constEnd();
    for(; it!=itEnd; ++it) {
        if(QFile::exists(builddir+'/'+it.key()))
            return it.value();
    }
    //It means that it still has to be generated, so use the builder for
    //the generator we use
    return m_buildersForGenerator[CMake::defaultGenerator()];
}

QList< KDevelop::IProjectBuilder* > CMakeBuilder::additionalBuilderPlugins( KDevelop::IProject* project  ) const
{
    IProjectBuilder* b = builderForProject( project );
    QList< KDevelop::IProjectBuilder* > ret;
    if(b)
        ret << b;
    return ret;
}

int CMakeBuilder::configPages() const
{
    return 1;
}

KDevelop::ConfigPage* CMakeBuilder::configPage(int number, QWidget* parent)
{
    if (number == 0) {
        return new CMakeBuilderPreferences(this, parent);
    }
    return nullptr;
}


#include "cmakebuilder.moc"