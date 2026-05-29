#pragma once

#include "bufstring.h"
#include "mamuteworkflow.h"
#include "uidialog.h"
#include "uiodmainmod.h"

#include <map>

class uiListBox;
class uiGroup;
class uiPushButton;
class uiTabStack;
class uiLabel;
class uiGenInput;
class uiFileInput;

mExpClass(uiODMain) uiMamuteMainWindow : public uiDialog {
    mODTextTranslationClass(uiMamuteMainWindow);

  public:
    uiMamuteMainWindow(uiParent*, const BufferString& projdir);
    ~uiMamuteMainWindow();

  protected:
    bool acceptOK(CallBacker*) override;

  private:
    void createUI();
    void createModelingPage();
    void createVelocityPage();
    void createGeometryPage();
    void loadModelingValues();
    void loadVelocityJson();
    void loadGeometryJson();
    bool buildParamsFromUI(Mamute::ModelingParams&, bool showmsg) const;
    bool validateInputs(bool showmsg);
    bool paramsDifferFromLastBuild(const Mamute::ModelingParams&) const;
    void setFieldValidationState(uiGenInput*, bool, const char*);
    void updateVelocityTypeUI();
    bool buildVelocityJson(BufferString & jsonout, BufferString & errmsg) const;
    void updateGeometryTypeUI();
    void updateGeometrySummary();
    bool buildGeometryJson(BufferString & jsonout, BufferString & errmsg) const;
    void sidebarSelectionChanged(CallBacker*);
    void paramsChangedCB(CallBacker*);
    void velocityTypeChangedCB(CallBacker*);
    void velocityParamsChangedCB(CallBacker*);
    void validateVelocityParams();
    void validateGeometryParams();
    void geometryTypeChangedCB(CallBacker*);
    void geometryParamsChangedCB(CallBacker*);
    void buildModelingCB(CallBacker*);
    void runModelingCB(CallBacker*);
    void saveVelocityJsonCB(CallBacker*);
    void saveGeometryJsonCB(CallBacker*);
    void sourceModeChangedCB(CallBacker*);
    void velocityModeChangedCB(CallBacker*);
    void qpModeChangedCB(CallBacker*);
    void monitorCB(CallBacker*);
    void openSeismogramCB(CallBacker*);
    int readNSrcFromModeling() const;

    uiListBox* sidebar_ = nullptr;
    uiTabStack* tabstack_ = nullptr;
    uiGroup* modelingpage_ = nullptr;
    uiGroup* velocitypage_ = nullptr;
    uiGroup* geometrypage_ = nullptr;

    uiGenInput* nxfld_ = nullptr;
    uiGenInput* nyfld_ = nullptr;
    uiGenInput* nzfld_ = nullptr;
    uiGenInput* nsfld_ = nullptr;
    uiGenInput* borderfld_ = nullptr;
    uiGenInput* dxfld_ = nullptr;
    uiGenInput* dyfld_ = nullptr;
    uiGenInput* dzfld_ = nullptr;
    uiGenInput* fpeakfld_ = nullptr;
    uiGenInput* dtfld_ = nullptr;
    uiGenInput* amplitudefld_ = nullptr;
    uiGenInput* nsrcfld_ = nullptr;
    uiGenInput* useexistingsourcefld_ = nullptr;
    uiFileInput* sourceexistingfilefld_ = nullptr;

    uiFileInput* velexistingfilefld_ = nullptr;
    uiGenInput* useexistingqpfld_ = nullptr;
    uiFileInput* qpexistingfilefld_ = nullptr;
    uiGenInput* veltypefld_ = nullptr;

    uiGenInput* velvfld_ = nullptr;
    uiGenInput* velsigmafld_ = nullptr;
    uiGenInput* velvlistfld_ = nullptr;
    uiGenInput* velvinfld_ = nullptr;
    uiGenInput* velvoutfld_ = nullptr;
    uiGenInput* velsidefld_ = nullptr;
    uiGenInput* velzposfld_ = nullptr;
    uiGenInput* velvinfld_circ_ = nullptr;
    uiGenInput* velvoutfld_circ_ = nullptr;
    uiGenInput* velrfld_ = nullptr;
    uiGenInput* velvstartfld_ = nullptr;
    uiGenInput* velvendfld_ = nullptr;
    uiFileInput* velinputvfld_ = nullptr;
    uiGenInput* velsigmafld_smooth_ = nullptr;
    uiGenInput* velv1fld_ = nullptr;
    uiGenInput* velv2fld_ = nullptr;
    uiGenInput* veloperationfld_ = nullptr;
    uiFileInput* velinputvfld_slow_ = nullptr;
    uiTabStack* velparams_stack_ = nullptr;

    uiGenInput* srcgeotypefld_ = nullptr;
    uiGenInput* rcvgeotypefld_ = nullptr;
    
    uiGenInput* srcxfirstfld_ = nullptr;
    uiGenInput* srcxlastfld_ = nullptr;
    uiGenInput* srcxdeltafld_ = nullptr;
    uiGenInput* srcyfirstfld_ = nullptr;
    uiGenInput* srcylastfld_ = nullptr;
    uiGenInput* srcydeltafld_ = nullptr;
    uiGenInput* srczfirstfld_ = nullptr;
    uiGenInput* srczlastfld_ = nullptr;
    uiGenInput* srczdeltafld_ = nullptr;
    uiGenInput* srccenterxfld_ = nullptr;
    uiGenInput* srccenteryfld_ = nullptr;
    uiGenInput* srcradiusfld_ = nullptr;
    uiGenInput* srcnpointsfld_ = nullptr;
    uiGenInput* srcdepthfld_ = nullptr;
    uiGenInput* rcvxfirstfld_ = nullptr;
    uiGenInput* rcvxlastfld_ = nullptr;
    uiGenInput* rcvxdeltafld_ = nullptr;
    uiGenInput* rcvyfirstfld_ = nullptr;
    uiGenInput* rcvylastfld_ = nullptr;
    uiGenInput* rcvydeltafld_ = nullptr;
    uiGenInput* rcvzfirstfld_ = nullptr;
    uiGenInput* rcvzlastfld_ = nullptr;
    uiGenInput* rcvzdeltafld_ = nullptr;
    uiGenInput* rcvcenterxfld_ = nullptr;
    uiGenInput* rcvcenteryfld_ = nullptr;
    uiGenInput* rcvradiusfld_ = nullptr;
    uiGenInput* rcvnpointsfld_ = nullptr;
    uiGenInput* rcvdepthfld_ = nullptr;
    uiTabStack* srcparams_stack_ = nullptr;
    uiTabStack* rcvparams_stack_ = nullptr;

    uiPushButton* buildbtn_ = nullptr;
    uiPushButton* runbtn_ = nullptr;
    uiPushButton* monitorbtn_ = nullptr;
    uiPushButton* openseismobtn_ = nullptr;
    uiPushButton* saveveljsonbtn_ = nullptr;
    uiPushButton* savesrcjsonbtn_ = nullptr;
    uiLabel* statuslbl_ = nullptr;
    uiLabel* velstatuslbl_ = nullptr;
    uiLabel* geostatuslbl_ = nullptr;
    uiLabel* geosummarylbl_ = nullptr;
    uiLabel* qpinfolbl_ = nullptr;
    std::map<uiGenInput*, uiLabel*> fieldmsglabels_;
    Mamute::MamuteService* service_ = nullptr;
    Mamute::ModelingWorkflow* workflow_ = nullptr;
    Mamute::ModelingParams lastbuiltparams_;
    bool hasbuiltparams_;

    BufferString proj_dir_;
};
