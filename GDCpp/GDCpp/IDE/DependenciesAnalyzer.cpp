#if defined(GD_IDE_ONLY)
#include "GDCore/Events/Event.h"
#include "GDCore/PlatformDefinition/ExternalEvents.h"
#include "GDCore/PlatformDefinition/Layout.h"
#include "GDCore/PlatformDefinition/Project.h"
#include "GDCore/PlatformDefinition/SourceFile.h"
#include "GDCore/Events/Builtin/LinkEvent.h"
#include "GDCpp/CppCodeEvent.h"
#include "DependenciesAnalyzer.h"

DependenciesAnalyzer::DependenciesAnalyzer(gd::Project & project_, gd::Layout & layout_) : 
    project(project_), 
    layout(&layout_), 
    externalEvents(NULL) 
{
    parentScenes.push_back(layout->GetName());
}

DependenciesAnalyzer::DependenciesAnalyzer(gd::Project & project_, gd::ExternalEvents & externalEvents_) : 
    project(project_), 
    layout(NULL), 
    externalEvents(&externalEvents_) 
{
    parentExternalEvents.push_back(externalEvents->GetName());
}

DependenciesAnalyzer::DependenciesAnalyzer(const DependenciesAnalyzer & parent) :
    parentScenes(parent.parentScenes),
    parentExternalEvents(parent.parentExternalEvents),
    project(parent.project),
    layout(NULL),
    externalEvents(NULL)
{
}

bool DependenciesAnalyzer::Analyze()
{
    if (!layout && !externalEvents) 
    {
        std::cout << "ERROR: DependenciesAnalyzer called without any layout or external events.";
        return false;
    }

    if (layout)
        return Analyze(layout->GetEvents(), true);
    else if (externalEvents)
        return Analyze(externalEvents->GetEvents(), true);
}

DependenciesAnalyzer::~DependenciesAnalyzer()
{
}

bool DependenciesAnalyzer::Analyze(std::vector< boost::shared_ptr<gd::BaseEvent> > & events, bool isOnTopLevel)
{
    for (unsigned int i = 0;i<events.size();++i)
    {
        boost::shared_ptr<gd::LinkEvent> linkEvent = boost::dynamic_pointer_cast<gd::LinkEvent>(events[i]);
        boost::shared_ptr<CppCodeEvent> cppCodeEvent = boost::dynamic_pointer_cast<CppCodeEvent>(events[i]);
        if ( linkEvent != boost::shared_ptr<gd::LinkEvent>() )
        {
            DependenciesAnalyzer analyzer(*this);

            std::string linked = linkEvent->GetTarget();
            if ( project.HasExternalEventsNamed(linked) )
            {
                if ( std::find(parentExternalEvents.begin(), parentExternalEvents.end(), linked) != parentExternalEvents.end() )
                    return false; //Circular dependency!

                externalEventsDependencies.insert(linked); //There is a direct dependency
                if ( !isOnTopLevel ) notTopLevelExternalEventsDependencies.insert(linked); 
                analyzer.AddParentExternalEvents(linked);
                if ( !analyzer.Analyze(project.GetExternalEvents(linked).GetEvents(), isOnTopLevel) )
                    return false;

            }
            else if ( project.HasLayoutNamed(linked) )
            {
                if ( std::find(parentScenes.begin(), parentScenes.end(), linked) != parentScenes.end() )
                    return false; //Circular dependency!

                scenesDependencies.insert(linked); //There is a direct dependency
                if ( !isOnTopLevel ) notTopLevelScenesDependencies.insert(linked); 
                analyzer.AddParentScene(linked);
                if ( !analyzer.Analyze(project.GetLayout(linked).GetEvents(), isOnTopLevel) )
                    return false;
            }

            //Update with indirect dependencies.
            scenesDependencies.insert(analyzer.GetScenesDependencies().begin(), analyzer.GetScenesDependencies().end());
            externalEventsDependencies.insert(analyzer.GetExternalEventsDependencies().begin(), analyzer.GetExternalEventsDependencies().end());
            sourceFilesDependencies.insert(analyzer.GetSourceFilesDependencies().begin(), analyzer.GetSourceFilesDependencies().end());
            notTopLevelScenesDependencies.insert(analyzer.GetNotTopLevelScenesDependencies().begin(), analyzer.GetNotTopLevelScenesDependencies().end());
            notTopLevelExternalEventsDependencies.insert(analyzer.GetNotTopLevelExternalEventsDependencies().begin(), analyzer.GetNotTopLevelExternalEventsDependencies().end());

            if ( !isOnTopLevel )
            {
                notTopLevelScenesDependencies.insert(analyzer.GetScenesDependencies().begin(), analyzer.GetScenesDependencies().end());
                notTopLevelExternalEventsDependencies.insert(analyzer.GetExternalEventsDependencies().begin(), analyzer.GetExternalEventsDependencies().end());
            }
        }
        else if ( cppCodeEvent != boost::shared_ptr<CppCodeEvent>() )
        {
            const std::vector<std::string> & dependencies = cppCodeEvent->GetDependencies();
            sourceFilesDependencies.insert(dependencies.begin(), dependencies.end());
            sourceFilesDependencies.insert(cppCodeEvent->GetAssociatedGDManagedSourceFile(project));
        }
        else if ( events[i]->CanHaveSubEvents() ) 
        {
            if ( !Analyze(events[i]->GetSubEvents(), false) )
                return false;
        }
    }

    return true;
}

std::string DependenciesAnalyzer::ExternalEventsCanBeCompiledForAScene()
{
    if ( !externalEvents )
    {
        std::cout << "ERROR: ExternalEventsCanBeCompiledForAScene called without external events set!" << std::endl;
        return "";
    }

    std::string sceneName;
    for (unsigned int i = 0;i<project.GetLayoutCount();++i)
    {
        //For each layout, compute the dependencies and the dependencies which are not coming from a top level event.
        DependenciesAnalyzer analyzer(project, project.GetLayout(i));
        if ( !analyzer.Analyze() ) continue; //Analyze failed -> Cyclic dependencies
        const std::set <std::string > & dependencies = analyzer.GetExternalEventsDependencies();
        const std::set <std::string > & notTopLevelDependencies = analyzer.GetNotTopLevelExternalEventsDependencies();

        //Check if the external events is a dependency, and that is is only present as a link on the top level.
        if ( dependencies.find(externalEvents->GetName()) != dependencies.end() &&
             notTopLevelDependencies.find(externalEvents->GetName()) == notTopLevelDependencies.end() )
        {
            if (!sceneName.empty())
                return ""; //External events can be compiled only if one scene is including them.
            else
                sceneName = project.GetLayout(i).GetName();
        }
    }

    return sceneName; //External events can be compiled and used for the scene.
}
#endif
