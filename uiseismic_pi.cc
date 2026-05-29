#include "filepath.h"
#include "odplugin.h"
#include "uidialog.h"
#include "uifileinput.h"
#include "uigeninput.h"
#include "uihelpview.h"
#include "uimamutemainwindow.h"
#include "uimenu.h"
#include "uimsg.h"
#include "uiodmenumgr.h"
#include "uiseismicsimulator.h"
#include "uitoolbar.h"

#ifdef __win__
#define Export_uiMamuteModeling __declspec(dllexport)
#else
#define Export_uiMamuteModeling
#endif

mDefODPluginInfo(uiMamuteModeling) {
    static PluginInfo retpi("Mamute Seismic Tools", "Mamute Seismic Tools integration plugin");
    return &retpi;
}

class uiMamutePIMgr : public uiPluginInitMgr {
    mODTextTranslationClass(uiMamutePIMgr) public : uiMamutePIMgr();

  private:
    uiDialog* dlg_ = nullptr;

    void init() override;
    void dTectToolbarChanged() override;
    void cleanup() override;

    void showDlgCB(CallBacker*);
    void showSettingsCB(CallBacker*);
};

uiMamutePIMgr::uiMamutePIMgr() : uiPluginInitMgr() {
    init();
}

void uiMamutePIMgr::init() {
    uiPluginInitMgr::init();

    uiMenu* mamutemnu = new uiMenu(tr("Mamute Seismic Tools"));
    mamutemnu->insertAction(new uiAction(tr("Mamute Tools"), mCB(this, uiMamutePIMgr, showDlgCB)));
    mamutemnu->insertSeparator();
    mamutemnu->insertAction(new uiAction(tr("Settings"), mCB(this, uiMamutePIMgr, showSettingsCB)));
    appl().menuMgr().toolsMnu()->addMenu(mamutemnu);
}

void uiMamutePIMgr::dTectToolbarChanged() {
    const int btnid = appl().menuMgr().dtectTB()->addButton("../../plugins/uiMamuteModeling/images/mamute", tr("Mamute Seismic Tools"), CallBack());

    uiMenu* tbmnu = new uiMenu(tr("Mamute Seismic Tools"));
    tbmnu->insertAction(new uiAction(tr("Mamute Tools"), mCB(this, uiMamutePIMgr, showDlgCB)));
    tbmnu->insertSeparator();
    tbmnu->insertAction(new uiAction(tr("Settings"), mCB(this, uiMamutePIMgr, showSettingsCB)));

    appl().menuMgr().dtectTB()->setButtonMenu(btnid, tbmnu, uiToolButton::InstantPopup);
}

void uiMamutePIMgr::cleanup() {
    closeAndNullPtr(dlg_);
    uiPluginInitMgr::cleanup();
}

void uiMamutePIMgr::showDlgCB(CallBacker*) {

    uiProjectSetupDlg setupdlg(&appl());
    if (!setupdlg.go())
        return;

    BufferString projdir = setupdlg.getProjectDir();

    closeAndNullPtr(dlg_);
    dlg_ = new uiMamuteMainWindow(&appl(), projdir);
    dlg_->show();
}

void uiMamutePIMgr::showSettingsCB(CallBacker*) {
    uiDialog dlg(&appl(), uiDialog::Setup(toUiString("Mamute Settings"), toUiString("Configure Mamute installation path"), mNoHelpKey));

    BufferString curpath = getMamuteInstallPath();
    uiFileInput* pathfld = new uiFileInput(&dlg, toUiString("Mamute install path"), uiFileInput::Setup().directories(true).defseldir(curpath));
    pathfld->setFileName(curpath);

    if (dlg.go()) {
        BufferString newpath = pathfld->fileName();
        if (!newpath.isEmpty()) {
            setMamuteInstallPath(newpath);
            FilePath binpath(newpath);
            binpath.add("install").add("bin").add("mamute");
            if (!File::exists(binpath.fullPath())) {
                BufferString warnmsg("Path saved, but mamute binary not found at:\n");
                warnmsg.add(binpath.fullPath());
                warnmsg.add("\n\nPlease verify the path.");
                uiMSG().warning(toUiString(warnmsg));
            } else {
                uiMSG().message(toUiString("Mamute path updated successfully."));
            }
        }
    }
}

class MamuteHelpProvider : public SimpleHelpProvider {
  public:
    MamuteHelpProvider() : SimpleHelpProvider("https://lappsufrn.gitlab.io/mamute/dev/") {
        addKeyLink("simulator", "features/parameters-modeling/");
    }

    static void initClass() {
        factory().addCreator(&MamuteHelpProvider::createInstance, "mamute");
    }

    static HelpProvider* createInstance() {
        return new MamuteHelpProvider();
    }
};

mDefODInitPlugin(uiMamuteModeling) {
    mDefineStaticLocalObject(PtrMan<uiMamutePIMgr>, theinst_, = new uiMamutePIMgr());
    if (!theinst_) {
        return "Cannot instantiate Mamute Simulator plugin";
    }
    MamuteHelpProvider::initClass();

    return nullptr;
}