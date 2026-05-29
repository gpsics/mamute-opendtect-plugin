#pragma once

#include "bufstring.h"
#include "bufstringset.h"
#include "uidialog.h"
#include "uiodmainmod.h"
#include <map>

class uiGenInput;
class uiFileInput;
class uiPushButton;
class uiGroup;
class uiODMain;
class uiListBox;
class uiLabel;

BufferString getMamuteInstallPath();
void setMamuteInstallPath(const BufferString&);

mExpClass(uiODMain) uiProjectSetupDlg : public uiDialog {
    mODTextTranslationClass(uiProjectSetupDlg);

  public:
    uiProjectSetupDlg(uiParent*);
    ~uiProjectSetupDlg();

    enum ProjectType {
        ExistingProject,
        EmptyProject
    };

    ProjectType getProjectType() const { return project_type_; }
    BufferString getProjectDir() const;

  protected:
    bool acceptOK(CallBacker*) override;

  private:
    uiListBox* recentlistfld_ = nullptr;
    uiPushButton* newprojbtn_ = nullptr;
    uiPushButton* copyprojbtn_ = nullptr;
    uiPushButton* browsebtn_ = nullptr;
    uiLabel* selectedlbl_ = nullptr;

    BufferStringSet recent_projects_;
    BufferString selected_projdir_;
    ProjectType project_type_ = EmptyProject;

    void loadRecentProjects();
    void saveRecentProjects();
    void addRecentProject(const BufferString&);
    void refreshRecentList();
    void updateSelectedLabel();
    bool chooseDirectory(BufferString&, const uiString&, const BufferString& startdir = BufferString());
    bool copyProjectContents(const BufferString& srcproj, const BufferString& dstproj);

    void recentSelectionChanged(CallBacker*);
    void openRecentProject(CallBacker*);
    void newProjectCB(CallBacker*);
    void copyProjectCB(CallBacker*);
    void browseProjectCB(CallBacker*);
};
