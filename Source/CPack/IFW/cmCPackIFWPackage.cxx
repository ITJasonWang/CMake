/*============================================================================
  CMake - Cross Platform Makefile Generator
  Copyright 2000-2009 Kitware, Inc., Insight Software Consortium

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/

#include "cmCPackIFWPackage.h"

#include "cmCPackIFWGenerator.h"

#include <CPack/cmCPackLog.h>

#include <cmGeneratedFileStream.h>
#include <cmTimestamp.h>
#include <cmXMLWriter.h>

//----------------------------------------------------------------- Logger ---
#ifdef cmCPackLogger
#undef cmCPackLogger
#endif
#define cmCPackLogger(logType, msg)                                           \
  do {                                                                        \
    std::ostringstream cmCPackLog_msg;                                        \
    cmCPackLog_msg << msg;                                                    \
    if (Generator) {                                                          \
      Generator->Logger->Log(logType, __FILE__, __LINE__,                     \
                             cmCPackLog_msg.str().c_str());                   \
    }                                                                         \
  } while (0)

//---------------------------------------------------------- CompareStruct ---
cmCPackIFWPackage::CompareStruct::CompareStruct()
  : Type(CompareNone)
{
}

//------------------------------------------------------- DependenceStruct ---
cmCPackIFWPackage::DependenceStruct::DependenceStruct()
{
}

cmCPackIFWPackage::DependenceStruct::DependenceStruct(
  const std::string& dependence)
{
  // Search compare section
  size_t pos = std::string::npos;
  if ((pos = dependence.find("<=")) != std::string::npos) {
    Compare.Type = CompareLessOrEqual;
    Compare.Value = dependence.substr(pos + 2);
  } else if ((pos = dependence.find(">=")) != std::string::npos) {
    Compare.Type = CompareGreaterOrEqual;
    Compare.Value = dependence.substr(pos + 2);
  } else if ((pos = dependence.find('<')) != std::string::npos) {
    Compare.Type = CompareLess;
    Compare.Value = dependence.substr(pos + 1);
  } else if ((pos = dependence.find('=')) != std::string::npos) {
    Compare.Type = CompareEqual;
    Compare.Value = dependence.substr(pos + 1);
  } else if ((pos = dependence.find('>')) != std::string::npos) {
    Compare.Type = CompareGreater;
    Compare.Value = dependence.substr(pos + 1);
  }
  Name = pos == std::string::npos ? dependence : dependence.substr(0, pos);
}

std::string cmCPackIFWPackage::DependenceStruct::NameWithCompare() const
{
  if (Compare.Type == CompareNone) {
    return Name;
  }

  std::string result = Name;

  if (Compare.Type == CompareLessOrEqual) {
    result += "<=";
  } else if (Compare.Type == CompareGreaterOrEqual) {
    result += ">=";
  } else if (Compare.Type == CompareLess) {
    result += "<";
  } else if (Compare.Type == CompareEqual) {
    result += "=";
  } else if (Compare.Type == CompareGreater) {
    result += ">";
  }

  result += Compare.Value;

  return result;
}

//------------------------------------------------------ cmCPackIFWPackage ---
cmCPackIFWPackage::cmCPackIFWPackage()
  : Generator(CM_NULLPTR)
  , Installer(CM_NULLPTR)
{
}

const char* cmCPackIFWPackage::GetOption(const std::string& op) const
{
  const char* option = Generator ? Generator->GetOption(op) : CM_NULLPTR;
  return option && *option ? option : CM_NULLPTR;
}

bool cmCPackIFWPackage::IsOn(const std::string& op) const
{
  return Generator ? Generator->IsOn(op) : false;
}

bool cmCPackIFWPackage::IsVersionLess(const char* version)
{
  return Generator ? Generator->IsVersionLess(version) : false;
}

bool cmCPackIFWPackage::IsVersionGreater(const char* version)
{
  return Generator ? Generator->IsVersionGreater(version) : false;
}

bool cmCPackIFWPackage::IsVersionEqual(const char* version)
{
  return Generator ? Generator->IsVersionEqual(version) : false;
}

std::string cmCPackIFWPackage::GetComponentName(cmCPackComponent* component)
{
  if (!component) {
    return "";
  }
  const char* option =
    GetOption("CPACK_IFW_COMPONENT_" +
              cmsys::SystemTools::UpperCase(component->Name) + "_NAME");
  return option ? option : component->Name;
}

void cmCPackIFWPackage::DefaultConfiguration()
{
  DisplayName = "";
  Description = "";
  Version = "";
  ReleaseDate = "";
  Script = "";
  Licenses.clear();
  SortingPriority = "";
  Default = "";
  Essential = "";
  Virtual = "";
  ForcedInstallation = "";
}

// Defaul configuration (all in one package)
int cmCPackIFWPackage::ConfigureFromOptions()
{
  // Restore defaul configuration
  DefaultConfiguration();

  // Name
  Name = Generator->GetRootPackageName();

  // Display name
  if (const char* option = this->GetOption("CPACK_PACKAGE_NAME")) {
    DisplayName = option;
  } else {
    DisplayName = "Your package";
  }

  // Description
  if (const char* option =
        this->GetOption("CPACK_PACKAGE_DESCRIPTION_SUMMARY")) {
    Description = option;
  } else {
    Description = "Your package description";
  }

  // Version
  if (const char* option = GetOption("CPACK_PACKAGE_VERSION")) {
    Version = option;
  } else {
    Version = "1.0.0";
  }

  ForcedInstallation = "true";

  return 1;
}

int cmCPackIFWPackage::ConfigureFromComponent(cmCPackComponent* component)
{
  if (!component) {
    return 0;
  }

  // Restore defaul configuration
  DefaultConfiguration();

  std::string prefix = "CPACK_IFW_COMPONENT_" +
    cmsys::SystemTools::UpperCase(component->Name) + "_";

  // Display name
  DisplayName = component->DisplayName;

  // Description
  Description = component->Description;

  // Version
  if (const char* optVERSION = GetOption(prefix + "VERSION")) {
    Version = optVERSION;
  } else if (const char* optPACKAGE_VERSION =
               GetOption("CPACK_PACKAGE_VERSION")) {
    Version = optPACKAGE_VERSION;
  } else {
    Version = "1.0.0";
  }

  // Script
  if (const char* option = GetOption(prefix + "SCRIPT")) {
    Script = option;
  }

  // CMake dependencies
  if (!component->Dependencies.empty()) {
    std::vector<cmCPackComponent*>::iterator dit;
    for (dit = component->Dependencies.begin();
         dit != component->Dependencies.end(); ++dit) {
      Dependencies.insert(Generator->ComponentPackages[*dit]);
    }
  }

  // QtIFW dependencies
  if (const char* option = this->GetOption(prefix + "DEPENDS")) {
    std::vector<std::string> deps;
    cmSystemTools::ExpandListArgument(option, deps);
    for (std::vector<std::string>::iterator dit = deps.begin();
         dit != deps.end(); ++dit) {
      DependenceStruct dep(*dit);
      if (!Generator->Packages.count(dep.Name)) {
        bool hasDep = Generator->DependentPackages.count(dep.Name) > 0;
        DependenceStruct& depRef = Generator->DependentPackages[dep.Name];
        if (!hasDep) {
          depRef = dep;
        }
        AlienDependencies.insert(&depRef);
      }
    }
  }

  // Licenses
  if (const char* option = this->GetOption(prefix + "LICENSES")) {
    Licenses.clear();
    cmSystemTools::ExpandListArgument(option, Licenses);
    if (Licenses.size() % 2 != 0) {
      cmCPackLogger(
        cmCPackLog::LOG_WARNING, prefix
          << "LICENSES"
          << " should contain pairs of <display_name> and <file_path>."
          << std::endl);
      Licenses.clear();
    }
  }

  // Priority
  if (const char* option = this->GetOption(prefix + "PRIORITY")) {
    SortingPriority = option;
  }

  // Default
  Default = component->IsDisabledByDefault ? "false" : "true";

  // Essential
  if (this->IsOn(prefix + "ESSENTIAL")) {
    Essential = "true";
  }

  // Virtual
  Virtual = component->IsHidden ? "true" : "";

  // ForcedInstallation
  ForcedInstallation = component->IsRequired ? "true" : "false";

  return 1;
}

int cmCPackIFWPackage::ConfigureFromGroup(cmCPackComponentGroup* group)
{
  if (!group) {
    return 0;
  }

  // Restore defaul configuration
  DefaultConfiguration();

  std::string prefix = "CPACK_IFW_COMPONENT_GROUP_" +
    cmsys::SystemTools::UpperCase(group->Name) + "_";

  DisplayName = group->DisplayName;
  Description = group->Description;

  // Version
  if (const char* optVERSION = GetOption(prefix + "VERSION")) {
    Version = optVERSION;
  } else if (const char* optPACKAGE_VERSION =
               GetOption("CPACK_PACKAGE_VERSION")) {
    Version = optPACKAGE_VERSION;
  } else {
    Version = "1.0.0";
  }

  // Script
  if (const char* option = GetOption(prefix + "SCRIPT")) {
    Script = option;
  }

  // Licenses
  if (const char* option = this->GetOption(prefix + "LICENSES")) {
    Licenses.clear();
    cmSystemTools::ExpandListArgument(option, Licenses);
    if (Licenses.size() % 2 != 0) {
      cmCPackLogger(
        cmCPackLog::LOG_WARNING, prefix
          << "LICENSES"
          << " should contain pairs of <display_name> and <file_path>."
          << std::endl);
      Licenses.clear();
    }
  }

  // Priority
  if (const char* option = this->GetOption(prefix + "PRIORITY")) {
    SortingPriority = option;
  }

  return 1;
}

int cmCPackIFWPackage::ConfigureFromGroup(const std::string& groupName)
{
  // Group configuration

  cmCPackComponentGroup group;
  std::string prefix =
    "CPACK_COMPONENT_GROUP_" + cmsys::SystemTools::UpperCase(groupName) + "_";

  if (const char* option = GetOption(prefix + "DISPLAY_NAME")) {
    group.DisplayName = option;
  } else {
    group.DisplayName = group.Name;
  }

  if (const char* option = GetOption(prefix + "DESCRIPTION")) {
    group.Description = option;
  }
  group.IsBold = IsOn(prefix + "BOLD_TITLE");
  group.IsExpandedByDefault = IsOn(prefix + "EXPANDED");

  // Package configuration

  group.Name = groupName;

  if (Generator) {
    Name = Generator->GetGroupPackageName(&group);
  } else {
    Name = group.Name;
  }

  return ConfigureFromGroup(&group);
}

void cmCPackIFWPackage::GeneratePackageFile()
{
  // Lazy directory initialization
  if (Directory.empty()) {
    if (Installer) {
      Directory = Installer->Directory + "/packages/" + Name;
    } else if (Generator) {
      Directory = Generator->toplevel + "/packages/" + Name;
    }
  }

  // Output stream
  cmGeneratedFileStream fout((Directory + "/meta/package.xml").data());
  cmXMLWriter xout(fout);

  xout.StartDocument();

  WriteGeneratedByToStrim(xout);

  xout.StartElement("Package");

  xout.Element("DisplayName", DisplayName);
  xout.Element("Description", Description);
  xout.Element("Name", Name);
  xout.Element("Version", Version);

  if (!ReleaseDate.empty()) {
    xout.Element("ReleaseDate", ReleaseDate);
  } else {
    xout.Element("ReleaseDate", cmTimestamp().CurrentTime("%Y-%m-%d", true));
  }

  // Script (copy to meta dir)
  if (!Script.empty()) {
    std::string name = cmSystemTools::GetFilenameName(Script);
    std::string path = Directory + "/meta/" + name;
    cmsys::SystemTools::CopyFileIfDifferent(Script.data(), path.data());
    xout.Element("Script", name);
  }

  // Dependencies
  std::set<DependenceStruct> compDepSet;
  for (std::set<DependenceStruct*>::iterator ait = AlienDependencies.begin();
       ait != AlienDependencies.end(); ++ait) {
    compDepSet.insert(*(*ait));
  }
  for (std::set<cmCPackIFWPackage*>::iterator it = Dependencies.begin();
       it != Dependencies.end(); ++it) {
    compDepSet.insert(DependenceStruct((*it)->Name));
  }
  // Write dependencies
  if (!compDepSet.empty()) {
    std::ostringstream dependencies;
    std::set<DependenceStruct>::iterator it = compDepSet.begin();
    dependencies << it->NameWithCompare();
    ++it;
    while (it != compDepSet.end()) {
      dependencies << "," << it->NameWithCompare();
      ++it;
    }
    xout.Element("Dependencies", dependencies.str());
  }

  // Licenses (copy to meta dir)
  std::vector<std::string> licenses = Licenses;
  for (size_t i = 1; i < licenses.size(); i += 2) {
    std::string name = cmSystemTools::GetFilenameName(licenses[i]);
    std::string path = Directory + "/meta/" + name;
    cmsys::SystemTools::CopyFileIfDifferent(licenses[i].data(), path.data());
    licenses[i] = name;
  }
  if (!licenses.empty()) {
    xout.StartElement("Licenses");
    for (size_t i = 0; i < licenses.size(); i += 2) {
      xout.StartElement("License");
      xout.Attribute("name", licenses[i]);
      xout.Attribute("file", licenses[i + 1]);
      xout.EndElement();
    }
    xout.EndElement();
  }

  if (!ForcedInstallation.empty()) {
    xout.Element("ForcedInstallation", ForcedInstallation);
  }

  if (!Virtual.empty()) {
    xout.Element("Virtual", Virtual);
  } else if (!Default.empty()) {
    xout.Element("Default", Default);
  }

  // Essential
  if (!Essential.empty()) {
    xout.Element("Essential", Essential);
  }

  // Priority
  if (!SortingPriority.empty()) {
    xout.Element("SortingPriority", SortingPriority);
  }

  xout.EndElement();
  xout.EndDocument();
}

void cmCPackIFWPackage::WriteGeneratedByToStrim(cmXMLWriter& xout)
{
  if (Generator) {
    Generator->WriteGeneratedByToStrim(xout);
  }
}
