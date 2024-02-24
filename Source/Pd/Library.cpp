/*
 // Copyright (c) 2021-2022 Timothy Schoen.
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Utility/Config.h"

#include <BinaryData.h>

#include "Utility/OSUtils.h"

extern "C" {
#include <m_pd.h>
#include <g_canvas.h>
#include <m_imp.h>
#include <s_stuff.h>
#include <z_libpd.h>
}

#include <utility>
#include "Library.h"
#include "Instance.h"
#include "Pd/Interface.h"

struct _canvasenvironment {
    t_symbol* ce_dir;    /* directory patch lives in */
    int ce_argc;         /* number of "$" arguments */
    t_atom* ce_argv;     /* array of "$" arguments */
    int ce_dollarzero;   /* value of "$0" */
    t_namelist* ce_path; /* search path */
};

namespace pd {

void Library::updateLibrary()
{
    auto settingsTree = ValueTree::fromXml(ProjectInfo::appDataDir.getChildFile(".settings").loadFileAsString());
    auto pathTree = settingsTree.getChildWithName("Paths");

    sys_lock();

    // Get available objects directly from pd
    t_class* o = pd_objectmaker;

    auto* mlist = static_cast<t_methodentry*>(libpd_get_class_methods(o));
    t_methodentry* m;

    allObjects.clear();

    int i;
    for (i = o->c_nmethod, m = mlist; i--; m++) {
        if (!m || !m->me_name)
            continue;

        auto newName = String::fromUTF8(m->me_name->s_name);
        if (!(newName.startsWith("else/") || newName.startsWith("cyclone/") || newName.endsWith("_aliased"))) {
            allObjects.add(newName);
        }
    }

    // Find patches in our search tree
    for (auto path : pathTree) {
        auto filePath = path.getProperty("Path").toString();

        auto file = File(filePath);
        if (!file.exists() || !file.isDirectory())
            continue;

        for (auto const& file : OSUtils::iterateDirectory(file, false, true)) {
            if (file.hasFileExtension("pd")) {
                auto filename = file.getFileNameWithoutExtension();
                if (!filename.startsWith("help-") || filename.endsWith("-help")) {
                    allObjects.add(filename);
                }
            }
        }
    }

    // These can't be created by name in Pd, but plugdata allows it
    allObjects.add("graph");
    allObjects.add("garray");
    

    // These aren't in there but should be
    allObjects.add("float");
    allObjects.add("symbol");
    allObjects.add("list");

    sys_unlock();
}

Library::Library(pd::Instance* instance)
{
    MemoryInputStream instream(BinaryData::Documentation_bin, BinaryData::Documentation_binSize, false);
    documentationTree = ValueTree::readFromStream(instream);

    watcher.addFolder(ProjectInfo::appDataDir);
    watcher.addListener(this);

    // This is unfortunately necessary to make Windows LV2 turtle dump work
    // Let's hope its not harmful
    MessageManager::callAsync([this, instance = juce::WeakReference(instance)]() {
        if (instance.get()) {
            instance->setThis();
            updateLibrary();
        }
    });
}

StringArray Library::autocomplete(String const& query, File const& patchDirectory) const
{
    StringArray result;
    result.ensureStorageAllocated(20);

    if (patchDirectory.isDirectory()) {
        for (auto const& file : OSUtils::iterateDirectory(patchDirectory, false, true, 20)) {
            auto filename = file.getFileNameWithoutExtension();
            if (file.hasFileExtension("pd") && filename.startsWith(query) && !filename.startsWith("help-") && !filename.endsWith("-help")) {
                result.add(filename);
            }
        }
    }

    for (auto const& str : allObjects) {
        if (result.size() >= 20)
            break;

        if (str.startsWith(query)) {
            result.addIfNotAlreadyThere(str);
        }
    }

    return result;
}

void Library::getExtraSuggestions(int currentNumSuggestions, String const& query, std::function<void(StringArray)> const& callback)
{

    int const maxSuggestions = 20;
    if (currentNumSuggestions > maxSuggestions)
        return;

    objectSearchThread.addJob([this, callback, query]() mutable {
        StringArray result;
        StringArray matches;

        for (const auto& object : getAllObjects()) {
            auto info = getObjectInfo(object);

            auto description = info.getProperty("description").toString();

            auto iolets = info.getChildWithName("iolets");
            auto arguments = info.getChildWithName("arguments");

            if (description.contains(query) || object.contains(query)) {
                matches.addIfNotAlreadyThere(object);
            }

            for (auto arg : arguments) {
                auto argDescription = arg.getProperty("description").toString();
                if (argDescription.contains(query)) {
                    matches.addIfNotAlreadyThere(object);
                }
            }

            for (auto iolet : iolets) {
                auto ioletDescription = iolet.getProperty("description").toString();
                if (description.contains(query)) {
                    matches.addIfNotAlreadyThere(object);
                }
            }
        }

        matches.sort(true);
        result.addArray(matches);
        matches.clear();

        MessageManager::callAsync([callback, result]() {
            callback(result);
        });
    });
}

ValueTree Library::getObjectInfo(String const& name)
{
    return documentationTree.getChildWithProperty("name", name.fromLastOccurrenceOf("/", false, false));
}

std::array<StringArray, 2> Library::parseIoletTooltips(ValueTree const& iolets, String const& name, int numIn, int numOut)
{
    std::array<StringArray, 2> result;
    Array<std::pair<String, bool>> inlets;
    Array<std::pair<String, bool>> outlets;

    auto args = StringArray::fromTokens(name.fromFirstOccurrenceOf(" ", false, false), true);

    for (auto iolet : iolets) {
        auto isVariable = iolet.getProperty("variable").toString() == "1";
        auto tooltip = iolet.getProperty("tooltip");
        if (iolet.getType() == Identifier("inlet")) {
            inlets.add({ tooltip, isVariable });
        }

        if (iolet.getType() == Identifier("outlet")) {
            outlets.add({ tooltip, isVariable });
        }
    }

    for (int type = 0; type < 2; type++) {
        int total = type ? numOut : numIn;
        auto& descriptions = type ? outlets : inlets;
        // if the amount of inlets is not equal to the amount in the spec, look for repeating iolets
        if (descriptions.size() < total) {
            for (int i = 0; i < descriptions.size(); i++) {
                if (descriptions[i].second) { // repeating inlet found
                    for (int j = 0; j < (total - descriptions.size()) + 1; j++) {

                        auto description = descriptions[i].first;
                        description = description.replace("$mth", String(j));
                        description = description.replace("$nth", String(j + 1));

                        if (isPositiveAndBelow(j, args.size())) {
                            description = description.replace("$arg", args[j]);
                        }

                        result[type].add(description);
                    }
                } else {
                    result[type].add(descriptions[i].first);
                }
            }
        } else {
            for (auto&& description : descriptions) {
                result[type].add(description.first);
            }
        }
    }

    return result;
}

StringArray Library::getAllObjects()
{
    return allObjects;
}

void Library::filesystemChanged()
{
    updateLibrary();
}

File Library::findHelpfile(t_gobj* obj, File const& parentPatchFile)
{
    String helpName;
    String helpDir;

    auto* pdclass = pd_class(reinterpret_cast<t_pd*>(obj));

    if (pdclass == canvas_class && canvas_isabstraction(reinterpret_cast<t_canvas*>(obj))) {
        char namebuf[MAXPDSTRING];
        t_object* ob = pd::Interface::checkObject(obj);
        int ac = binbuf_getnatom(ob->te_binbuf);
        t_atom* av = binbuf_getvec(ob->te_binbuf);
        if (ac < 1)
            return {};

        atom_string(av, namebuf, MAXPDSTRING);
        helpName = String::fromUTF8(namebuf);
    } else {
        helpDir = class_gethelpdir(pdclass);
        helpName = class_gethelpname(pdclass);
        helpName = helpName.upToLastOccurrenceOf(".pd", false, false);
    }

    auto patchHelpPaths = Array<File>();

    // Add abstraction dir to search paths
    if (pd_class(reinterpret_cast<t_pd*>(obj)) == canvas_class && canvas_isabstraction(reinterpret_cast<t_canvas*>(obj))) {
        auto* cnv = reinterpret_cast<t_canvas*>(obj);
        patchHelpPaths.add(File(String::fromUTF8(canvas_getenv(cnv)->ce_dir->s_name)));
        if (helpDir.isNotEmpty()) {
            patchHelpPaths.add(File(String::fromUTF8(canvas_getenv(cnv)->ce_dir->s_name)).getChildFile(helpDir));
        }
    }

    // Add parent patch dir to search paths
    if (parentPatchFile.existsAsFile()) {
        patchHelpPaths.add(parentPatchFile.getParentDirectory());
        if (helpDir.isNotEmpty()) {
            patchHelpPaths.add(parentPatchFile.getParentDirectory().getChildFile(helpDir));
        }
    }

    for (auto path : helpPaths) {
        patchHelpPaths.add(helpDir.isNotEmpty() ? path.getChildFile(helpDir) : path);
    }

    String firstName = helpName + "-help.pd";
    String secondName = "help-" + helpName + ".pd";

    auto findHelpPatch = [&firstName, &secondName](File const& searchDir) -> File {
        for (const auto& file : OSUtils::iterateDirectory(searchDir, false, true)) {
            auto pathName = file.getFullPathName().replace("\\", "/").trimCharactersAtEnd("/");
            // Hack to make it find else/cyclone helpfiles...
            pathName = pathName.replace("/else", "/9.else");
            pathName = pathName.replace("/cyclone", "/10.else");
            
            if (pathName.endsWith("/" + firstName) || pathName.endsWith("/" + secondName)) {
                return file;
            }
        }
        return {};
    };

    for (auto& path : patchHelpPaths) {

        if (!path.exists())
            continue;

        auto file = findHelpPatch(path);
        if (file.existsAsFile()) {
            return file;
        }
    }

    auto* rawHelpDir = class_gethelpdir(pd_class(&reinterpret_cast<t_gobj*>(obj)->g_pd));
    helpDir = String::fromUTF8(rawHelpDir);

    if (helpDir.isNotEmpty() && File(helpDir).exists()) {
        // Search for files in the patch directory
        auto file = findHelpPatch(helpDir);
        if (file.existsAsFile()) {
            return file;
        }
    }

    return {};
}

} // namespace pd
