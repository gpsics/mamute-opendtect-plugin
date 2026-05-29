#include "uiseismicsimulator.h"
#include "envvars.h"
#include "file.h"
#include "filepath.h"
#include "odplugin.h"
#include "settings.h"
#include "uibutton.h"
#include "uifiledlg.h"
#include "uifileinput.h"
#include "uigeninput.h"
#include "uilabel.h"
#include "uilineedit.h"
#include "uilistbox.h"
#include "uimain.h"
#include "uimamuteuiutils.h"
#include "uimsg.h"
#include "uiodmain.h"
#include "uiprogressbar.h"
#include "uiseparator.h"
#include "uistrings.h"
#include <cstring>
#include <string>

using namespace MamuteUI;

static BufferString getDefaultMamutePath() {
    FilePath fp(GetEnvVar("HOME"));
    fp.add("mamute");
    return fp.fullPath();
}

static BufferString getUserHomePath() {
    BufferString home = GetEnvVar("HOME");
    if (home.isEmpty())
        home = "/";

    return home;
}

static BufferString getRecentProjectParentPath(const BufferStringSet& recentprojects) {
    if (recentprojects.isEmpty())
        return getUserHomePath();

    FilePath fp(recentprojects.get(0));
    BufferString parent = fp.pathOnly();
    return parent.isEmpty() ? getUserHomePath() : parent;
}

static BufferString getProjectParentPath(const BufferString& projpath) {
    if (projpath.isEmpty())
        return getUserHomePath();

    FilePath fp(projpath);
    BufferString parent = fp.pathOnly();
    return parent.isEmpty() ? getUserHomePath() : parent;
}

class uiNewProjectSetupDlg : public uiDialog {
    mODTextTranslationClass(uiNewProjectSetupDlg);

  public:
    uiNewProjectSetupDlg(uiParent* p, const BufferString& defparent, const BufferString& defname)
        : uiDialog(p, uiDialog::Setup(tr("New Project"), tr("Create a new empty project"), mNoHelpKey)), namefld_(nullptr), pathfld_(nullptr) {
        namefld_ = new uiGenInput(this, tr("Project name"), StringInpSpec(defname));

        pathfld_ = new uiFileInput(this, tr("Destination"), uiFileInput::Setup().directories(true).defseldir(defparent));
        pathfld_->setFileName(defparent);
        pathfld_->attach(alignedBelow, namefld_);

        setOkText(tr("Create"));
    }

    BufferString destinationProjectPath() const {
        BufferString basedir;
        if (pathfld_)
            basedir = pathfld_->fileName();

        BufferString projname;
        if (namefld_)
            projname = namefld_->text();

        projname.trimBlanks();

        FilePath destfp(basedir);
        destfp.add(projname);
        return destfp.fullPath();
    }

  protected:
    bool acceptOK(CallBacker*) override {
        BufferString projname;
        if (namefld_)
            projname = namefld_->text();

        projname.trimBlanks();
        if (projname.isEmpty()) {
            uiMSG().error(tr("Please provide the new project name."));
            return false;
        }

        if (projname.find('/') || projname.find('\\')) {
            uiMSG().error(tr("Project name cannot contain path separators."));
            return false;
        }

        BufferString basedir;
        if (pathfld_)
            basedir = pathfld_->fileName();

        if (basedir.isEmpty() || !File::isDirectory(basedir)) {
            uiMSG().error(tr("Please select a valid destination parent path."));
            return false;
        }

        const BufferString dstpath = destinationProjectPath();
        if (File::exists(dstpath)) {
            uiMSG().error(tr("A project with this name already exists at the selected destination."));
            return false;
        }

        return true;
    }

  private:
    uiGenInput* namefld_;
    uiFileInput* pathfld_;
};

class uiCopyProjectSetupDlg : public uiDialog {
    mODTextTranslationClass(uiCopyProjectSetupDlg);

  public:
    uiCopyProjectSetupDlg(uiParent* p, const BufferString& srcproj, const BufferString& defparent, const BufferString& defname)
        : uiDialog(p, uiDialog::Setup(tr("Copy Project"), tr("Create a new project from a recent project"), mNoHelpKey)), srcproj_(srcproj), namefld_(nullptr), pathfld_(nullptr) {
        uiLabel* srcprojlbl = new uiLabel(this, tr("Source project: "));
        uiLabel* srcprojval = new uiLabel(this, toUiString(srcproj_));
        srcprojval->attach(rightOf, srcprojlbl);

        namefld_ = new uiGenInput(this, tr("Project name"), StringInpSpec(defname));
        namefld_->attach(alignedBelow, srcprojlbl);

        pathfld_ = new uiFileInput(this, tr("Destination"), uiFileInput::Setup().directories(true).defseldir(defparent));
        pathfld_->setFileName(defparent);
        pathfld_->attach(alignedBelow, namefld_);

        setOkText(tr("Create copy"));
    }

    BufferString sourceProject() const { return srcproj_; }

    BufferString destinationProjectPath() const {
        BufferString basedir;
        if (pathfld_)
            basedir = pathfld_->fileName();

        BufferString projname;
        if (namefld_)
            projname = namefld_->text();

        projname.trimBlanks();

        FilePath destfp(basedir);
        destfp.add(projname);
        return destfp.fullPath();
    }

  protected:
    bool acceptOK(CallBacker*) override {
        BufferString projname;
        if (namefld_)
            projname = namefld_->text();

        projname.trimBlanks();
        if (projname.isEmpty()) {
            uiMSG().error(tr("Please provide the new project name."));
            return false;
        }

        if (projname.find('/') || projname.find('\\')) {
            uiMSG().error(tr("Project name cannot contain path separators."));
            return false;
        }

        BufferString basedir;
        if (pathfld_)
            basedir = pathfld_->fileName();

        if (basedir.isEmpty() || !File::isDirectory(basedir)) {
            uiMSG().error(tr("Please select a valid destination parent path."));
            return false;
        }

        return true;
    }

  private:
    BufferString srcproj_;
    uiGenInput* namefld_;
    uiFileInput* pathfld_;
};

static const char* sKeyMamutePath = "Mamute.InstallPath";
static const char* sKeyRecentProjects = "Mamute.RecentProjects";

BufferString getMamuteInstallPath() {
    Settings& setts = Settings::fetch("Mamute");
    BufferString path;
    setts.get(sKeyMamutePath, path);
    if (path.isEmpty()) {
        BufferString envpath = GetEnvVar("MAMUTE_ROOT");
        if (!envpath.isEmpty())
            path = envpath;
        else
            path = getDefaultMamutePath();
    }

    return path;
}

void setMamuteInstallPath(const BufferString& path) {
    Settings& setts = Settings::fetch("Mamute");
    setts.set(sKeyMamutePath, path);
    setts.write();
}

uiProjectSetupDlg::uiProjectSetupDlg(uiParent* p)
    : uiDialog(p, uiDialog::Setup(tr("Mamute Project Launcher"), tr("Open recent"), HelpKey("mamute", "projectsetup"))) {
    setOkText(tr("Open"));

    loadRecentProjects();

    recentlistfld_ = new uiListBox(this, "Recent projects");
    recentlistfld_->setHSzPol(uiObject::Wide);
    recentlistfld_->setPrefWidthInChar(80);
    recentlistfld_->setPrefHeightInChar(14);
    recentlistfld_->selectionChanged.notify(mCB(this, uiProjectSetupDlg, recentSelectionChanged));
    recentlistfld_->doubleClicked.notify(mCB(this, uiProjectSetupDlg, openRecentProject));
    refreshRecentList();

    uiGroup* btngrp = new uiGroup(this, "Project launcher buttons");
    btngrp->attach(alignedBelow, recentlistfld_);

    newprojbtn_ = new uiPushButton(btngrp, tr("New project"), mCB(this, uiProjectSetupDlg, newProjectCB), true);

    copyprojbtn_ = new uiPushButton(btngrp, tr("Copy Project"), mCB(this, uiProjectSetupDlg, copyProjectCB), true);
    copyprojbtn_->attach(rightOf, newprojbtn_);

    browsebtn_ = new uiPushButton(btngrp, tr("Browse Project"), mCB(this, uiProjectSetupDlg, browseProjectCB), true);
    browsebtn_->attach(rightOf, copyprojbtn_);

    selectedlbl_ = new uiLabel(this, toUiString("Selected project: none"));
    selectedlbl_->setHSzPol(uiObject::Wide);
    selectedlbl_->setPrefWidthInChar(100);
    selectedlbl_->attach(alignedBelow, btngrp);
    updateSelectedLabel();
}

uiProjectSetupDlg::~uiProjectSetupDlg() {
}

void uiProjectSetupDlg::loadRecentProjects() {
    Settings& setts = Settings::fetch("Mamute");
    recent_projects_.setEmpty();
    setts.get(sKeyRecentProjects, recent_projects_);

    BufferStringSet filtered;
    for (int i = 0; i < recent_projects_.size(); i++) {
        const BufferString path = recent_projects_.get(i);
        if (!path.isEmpty() && File::isDirectory(path))
            filtered.addIfNew(path);
    }
    recent_projects_ = filtered;
}

void uiProjectSetupDlg::saveRecentProjects() {
    Settings& setts = Settings::fetch("Mamute");
    setts.set(sKeyRecentProjects, recent_projects_);
    setts.write();
}

void uiProjectSetupDlg::addRecentProject(const BufferString& projdir) {
    if (projdir.isEmpty())
        return;

    BufferStringSet updated;
    updated.add(projdir);
    for (int i = 0; i < recent_projects_.size(); i++) {
        const BufferString cur = recent_projects_.get(i);
        if (!cur.isEmpty() && cur != projdir)
            updated.add(cur);

        if (updated.size() >= 10)
            break;
    }
    recent_projects_ = updated;
    saveRecentProjects();
}

void uiProjectSetupDlg::refreshRecentList() {
    if (!recentlistfld_)
        return;

    recentlistfld_->setEmpty();
    for (int i = 0; i < recent_projects_.size(); i++) {
        const BufferString& fullpath = recent_projects_.get(i);
        const char* slash = strrchr(fullpath.str(), '/');
        const char* dirname = slash ? slash + 1 : fullpath.str();
        BufferString display(dirname);
        display.add("  \xe2\x80\x94  ").add(fullpath);
        recentlistfld_->addItem(toUiString(display));
    }
    if (!recent_projects_.isEmpty())
        recentlistfld_->setCurrentItem(0);
}

void uiProjectSetupDlg::updateSelectedLabel() {
    if (!selectedlbl_)
        return;

    BufferString txt("Selected project: ");
    if (selected_projdir_.isEmpty())
        txt.add("none");
    else
        txt.add(selected_projdir_);

    selectedlbl_->setText(toUiString(txt));
}

bool uiProjectSetupDlg::chooseDirectory(BufferString& outdir, const uiString& caption, const BufferString& startdir) {
    BufferString dlgstart = startdir;
    if (dlgstart.isEmpty()) {
        dlgstart = selected_projdir_.isEmpty() ? getUserHomePath() : selected_projdir_;
    }

    uiFileDialog dlg(this, uiFileDialog::DirectoryOnly, dlgstart, nullptr, caption);
    if (!dlg.go())
        return false;

    outdir = dlg.fileName();
    return !outdir.isEmpty();
}

bool uiProjectSetupDlg::copyProjectContents(const BufferString& srcproj, const BufferString& dstproj) {
    if (srcproj.isEmpty() || dstproj.isEmpty())
        return false;

    if (!File::isDirectory(srcproj)) {
        uiMSG().error(tr("The source project directory does not exist."));
        return false;
    }

    if (srcproj == dstproj) {
        uiMSG().error(tr("Source and destination directories must be different."));
        return false;
    }

    if (!File::exists(dstproj) && !File::createDir(dstproj)) {
        uiMSG().error(tr("Could not create the destination directory."));
        return false;
    }

    BufferStringSet existingentries;
    if (File::listDir(dstproj, File::DirListType::AllEntriesInDir, existingentries) && !existingentries.isEmpty()) {
        uiMSG().error(tr("The destination directory must be empty before copying a project."));
        return false;
    }

    BufferStringSet entries;
    if (!File::listDir(srcproj, File::DirListType::AllEntriesInDir, entries)) {
        uiMSG().error(tr("Failed to list the source project directory."));
        return false;
    }

    for (int i = 0; i < entries.size(); i++) {
        FilePath srcfp(srcproj);
        srcfp.add(entries.get(i));
        FilePath destfp(dstproj);
        destfp.add(entries.get(i));

        BufferString copyerr;
        const bool ok = File::isDirectory(srcfp.fullPath()) ? File::copyDir(srcfp.fullPath(), destfp.fullPath(), &copyerr) : File::copy(srcfp.fullPath(), destfp.fullPath(), &copyerr);

        if (!ok) {
            if (copyerr.isEmpty())
                uiMSG().error(tr("Failed to copy the selected project."));
            else
                uiMSG().error(toUiString(copyerr));

            return false;
        }
    }

    return true;
}

void uiProjectSetupDlg::recentSelectionChanged(CallBacker*) {
    if (!recentlistfld_)
        return;

    const int curidx = recentlistfld_->currentItem();
    if (curidx < 0 || curidx >= recent_projects_.size())
        return;

    selected_projdir_ = recent_projects_.get(curidx);
    project_type_ = ExistingProject;
    updateSelectedLabel();
}

void uiProjectSetupDlg::openRecentProject(CallBacker*) {
    if (recentlistfld_ && recentlistfld_->currentItem() >= 0) {
        recentSelectionChanged(nullptr);
        accept(nullptr);
    }
}

void uiProjectSetupDlg::newProjectCB(CallBacker*) {
    BufferString startdir = getProjectParentPath(selected_projdir_);
    if (startdir.isEmpty()) {
        const int curidx = recentlistfld_ ? recentlistfld_->currentItem() : -1;
        if (curidx >= 0 && curidx < recent_projects_.size())
            startdir = getProjectParentPath(recent_projects_.get(curidx));
    }
    if (startdir.isEmpty())
        startdir = getRecentProjectParentPath(recent_projects_);

    uiNewProjectSetupDlg setupdlg(this, startdir, BufferString("new_project"));
    if (!setupdlg.go())
        return;

    selected_projdir_ = setupdlg.destinationProjectPath();
    project_type_ = EmptyProject;

    if (!File::exists(selected_projdir_) && !File::createDir(selected_projdir_)) {
        uiMSG().error(tr("Could not create the project directory."));
        selected_projdir_.setEmpty();
        project_type_ = EmptyProject;
    }

    updateSelectedLabel();
}

void uiProjectSetupDlg::copyProjectCB(CallBacker*) {
    if (!recentlistfld_ || recent_projects_.isEmpty()) {
        uiMSG().warning(tr("No recent projects available as source. Open or browse a project first."));
        return;
    }

    const int curidx = recentlistfld_->currentItem();
    if (curidx < 0 || curidx >= recent_projects_.size()) {
        uiMSG().warning(tr("Select a source project from Recent projects before copying."));
        return;
    }

    BufferString srcproj = recent_projects_.get(curidx);
    if (!File::isDirectory(srcproj)) {
        uiMSG().error(tr("Selected source project does not exist."));
        return;
    }

    FilePath srcfp(srcproj);
    BufferString defname("copy_of_");
    defname.add(srcfp.fileName());
    BufferString defparent = getRecentProjectParentPath(recent_projects_);

    uiCopyProjectSetupDlg setupdlg(this, srcproj, defparent, defname);
    if (!setupdlg.go())
        return;

    BufferString dstproj = setupdlg.destinationProjectPath();

    if (!copyProjectContents(srcproj, dstproj))
        return;

    selected_projdir_ = dstproj;
    addRecentProject(selected_projdir_);
    refreshRecentList();

    FilePath modelfp(dstproj);
    modelfp.add("modeling.toml");
    project_type_ = File::exists(modelfp.fullPath()) ? ExistingProject : EmptyProject;
    updateSelectedLabel();

    uiMSG().message(tr("Project copied successfully. You can now open and edit the copied project."));
}

void uiProjectSetupDlg::browseProjectCB(CallBacker*) {
    BufferString projdir;
    if (!chooseDirectory(projdir, tr("Select Project Directory"), getUserHomePath()))
        return;

    selected_projdir_ = projdir;
    addRecentProject(selected_projdir_);
    refreshRecentList();

    FilePath modelfp(projdir);
    modelfp.add("modeling.toml");
    project_type_ = File::exists(modelfp.fullPath()) ? ExistingProject : EmptyProject;
    updateSelectedLabel();
}

BufferString uiProjectSetupDlg::getProjectDir() const {
    return selected_projdir_;
}

bool uiProjectSetupDlg::acceptOK(CallBacker*) {
    if (selected_projdir_.isEmpty()) {
        uiMSG().error(tr("Please select a project first (Recent, New Project, Copy Project, or Browse Project)."));
        return false;
    }

    if (project_type_ == ExistingProject) {
        BufferString projdir = selected_projdir_;
        if (!File::exists(projdir)) {
            uiMSG().error(tr("The specified project directory does not exist"));
            return false;
        }

        FilePath modelfp(projdir);
        modelfp.add("modeling.toml");
        if (!File::exists(modelfp.fullPath())) {
            uiMSG().warning(tr("The selected directory does not contain a modeling.toml file.\n"
                               "This may not be a valid Mamute modeling project."));
        }
    }

    addRecentProject(selected_projdir_);

    return true;
}