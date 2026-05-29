#include "uimamutemainwindow.h"
#include "uiseismicsimulator.h"
#include "uiseismogramviewer.h"

#include "file.h"
#include "filepath.h"
#include "od_istream.h"
#include "uibutton.h"
#include "uifileinput.h"
#include "uigeninput.h"
#include "uigroup.h"
#include "uilabel.h"
#include "uilineedit.h"
#include "uilistbox.h"
#include "uimain.h"
#include "uimamuteuiutils.h"
#include "uimsg.h"
#include "uiseparator.h"
#include "uitabbar.h"
#include "uitabstack.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>

using namespace Mamute;
using namespace MamuteUI;

namespace {
BufferString makeDialogTitle(const BufferString& projdir) {
    BufferString title("Mamute Seismic Tools");
    FilePath fp(projdir);
    BufferString projname = fp.fileName();
    if (projname.isEmpty())
        projname = projdir;

    if (!projname.isEmpty()) {
        title.add(" - ");
        title.add(projname);
    }

    return title;
}

int readIntParamFromFile(const BufferString& filepath, const char* key, int fallback) {
    if (!File::exists(filepath))
        return fallback;

    od_istream strm(filepath);
    if (!strm.isOK())
        return fallback;

    BufferString line;
    while (strm.getLine(line)) {
        line.trimBlanks();
        if (line.isEmpty() || line[0] == '#')
            continue;

        BufferStringSet parts;
        parts.unCat(line.buf(), "=");
        if (parts.size() < 2)
            continue;

        BufferString parsed_key = parts.get(0);
        BufferString parsed_value = parts.get(1);
        parsed_key.trimBlanks();
        parsed_value.trimBlanks();

        if (parsed_key == key) {
            const int val = parsed_value.toInt();
            return val > 0 ? val : fallback;
        }
    }

    return fallback;
}

float readFloatParamFromFile(const BufferString& filepath, const char* key, float fallback) {
    if (!File::exists(filepath))
        return fallback;

    od_istream strm(filepath);
    if (!strm.isOK())
        return fallback;

    BufferString line;
    while (strm.getLine(line)) {
        line.trimBlanks();
        if (line.isEmpty() || line[0] == '#')
            continue;

        BufferStringSet parts;
        parts.unCat(line.buf(), "=");
        if (parts.size() < 2)
            continue;

        BufferString parsed_key = parts.get(0);
        BufferString parsed_value = parts.get(1);
        parsed_key.trimBlanks();
        parsed_value.trimBlanks();

        if (parsed_key == key) {
            const float val = parsed_value.toFloat();
            return val > 0.0f ? val : fallback;
        }
    }

    return fallback;
}
}

uiMamuteMainWindow::uiMamuteMainWindow(uiParent* p, const BufferString& projdir)
    : uiDialog(p, uiDialog::Setup(toUiString(makeDialogTitle(projdir)), toUiString("Workflow Navigator"), mODHelpKey("Mamute"))), hasbuiltparams_(false), proj_dir_(projdir) {
    service_ = new MamuteService(getMamuteInstallPath());
    workflow_ = new ModelingWorkflow(*service_, this);
    createUI();
    setCtrlStyle(CloseOnly);
}

uiMamuteMainWindow::~uiMamuteMainWindow() {
    detachAllNotifiers();
    delete workflow_;
    delete service_;
}

void uiMamuteMainWindow::createUI() {
    sidebar_ = new uiListBox(this, "MamuteSidebar");
    sidebar_->setNrLines(16);
    sidebar_->setFieldWidth(24);
    sidebar_->setPrefWidth(180);
    sidebar_->addItem(toUiString("Acquisition Geometry"));
    sidebar_->addItem(toUiString("Velocity Model"));
    sidebar_->addItem(toUiString("Visco-Acoustic Modeling"));

    tabstack_ = new uiHiddenTabStack(this, "MamutePages");
    sidebar_->setStretch(1, 2);
    tabstack_->setStretch(4, 2);
    tabstack_->attach(rightOf, sidebar_);

    createGeometryPage();
    createVelocityPage();
    createModelingPage();

    sidebar_->selectionChanged.notify(mCB(this, uiMamuteMainWindow, sidebarSelectionChanged));
    sidebar_->setCurrentItem(0);
    tabstack_->setCurrentPage(0);

    sourceModeChangedCB(nullptr);
    velocityModeChangedCB(nullptr);
    qpModeChangedCB(nullptr);

    sidebar_->setCurrentItem(2);
    tabstack_->setCurrentPage(2);
}

void uiMamuteMainWindow::createModelingPage() {
    modelingpage_ = new uiGroup(tabstack_->tabGroup(), "ModelingPage");

    uiLabel* title = new uiLabel(modelingpage_, toUiString("Visco-Acoustic Modeling"));
    title->setHSzPol(uiObject::Wide);
    title->setAlignment(Alignment::HCenter);

    uiSeparator* titlesep = new uiSeparator(modelingpage_, "titlesep");
    titlesep->attach(stretchedBelow, title);

    BufferString projmsg("Selected project: ");
    projmsg.add(proj_dir_);
    uiLabel* projlbl = new uiLabel(modelingpage_, toUiString(projmsg));
    projlbl->setHSzPol(uiObject::Wide);
    projlbl->attach(alignedBelow, titlesep);

    uiGroup* row1 = new uiGroup(modelingpage_, "row1");
    row1->attach(alignedBelow, projlbl);
    nxfld_ = new uiGenInput(row1, tr("Grid size X (m)"), FloatInpSpec(1000.0f));
    fieldmsglabels_[nxfld_] = createFieldMessageLabel(row1, nxfld_);
    nyfld_ = new uiGenInput(row1, tr("Grid size Y (m)"), FloatInpSpec(1000.0f));
    nyfld_->attach(rightOf, nxfld_);
    fieldmsglabels_[nyfld_] = createFieldMessageLabel(row1, nyfld_);
    nzfld_ = new uiGenInput(row1, tr("Grid size Z (m)"), FloatInpSpec(1000.0f));
    nzfld_->attach(rightOf, nyfld_);
    fieldmsglabels_[nzfld_] = createFieldMessageLabel(row1, nzfld_);

    uiSeparator* sep1 = new uiSeparator(modelingpage_, "sep1");
    sep1->attach(stretchedBelow, row1);

    uiGroup* row2 = new uiGroup(modelingpage_, "row2");
    row2->attach(alignedBelow, sep1);
    borderfld_ = new uiGenInput(row2, tr("Border (grid cells)"), IntInpSpec(50));
    fieldmsglabels_[borderfld_] = createFieldMessageLabel(row2, borderfld_);
    dxfld_ = new uiGenInput(row2, tr("dx (m)"), FloatInpSpec(10.0f));
    dxfld_->attach(rightOf, borderfld_);
    fieldmsglabels_[dxfld_] = createFieldMessageLabel(row2, dxfld_);
    dyfld_ = new uiGenInput(row2, tr("dy (m)"), FloatInpSpec(10.0f));
    dyfld_->attach(rightOf, dxfld_);
    fieldmsglabels_[dyfld_] = createFieldMessageLabel(row2, dyfld_);
    dzfld_ = new uiGenInput(row2, tr("dz (m)"), FloatInpSpec(10.0f));
    dzfld_->attach(rightOf, dyfld_);
    fieldmsglabels_[dzfld_] = createFieldMessageLabel(row2, dzfld_);

    uiSeparator* sep_time = new uiSeparator(modelingpage_, "sep_time");
    sep_time->attach(stretchedBelow, row2);

    uiGroup* row_time = new uiGroup(modelingpage_, "row_time");
    row_time->attach(alignedBelow, sep_time);
    nsfld_ = new uiGenInput(row_time, tr("Total simulation time (s)"), FloatInpSpec(0.2005f));
    fieldmsglabels_[nsfld_] = createFieldMessageLabel(row_time, nsfld_);
    dtfld_ = new uiGenInput(row_time, tr("Sampling interval (s)"), FloatInpSpec(0.0005f));
    dtfld_->attach(rightOf, nsfld_);
    fieldmsglabels_[dtfld_] = createFieldMessageLabel(row_time, dtfld_);

    uiSeparator* sep2 = new uiSeparator(modelingpage_, "sep2");
    sep2->attach(stretchedBelow, row_time);

    uiGroup* row3 = new uiGroup(modelingpage_, "row3");
    row3->attach(alignedBelow, sep2);
    fpeakfld_ = new uiGenInput(row3, tr("Peak frequency (Hz)"), FloatInpSpec(20.0f));
    fieldmsglabels_[fpeakfld_] = createFieldMessageLabel(row3, fpeakfld_);
    amplitudefld_ = new uiGenInput(row3, tr("Source amplitude (a.u.)"), FloatInpSpec(1.0f));
    amplitudefld_->attach(rightOf, fpeakfld_);
    fieldmsglabels_[amplitudefld_] = createFieldMessageLabel(row3, amplitudefld_);
    nsrcfld_ = new uiGenInput(row3, tr("Number of sources (count)"), IntInpSpec(27));
    nsrcfld_->attach(rightOf, amplitudefld_);
    fieldmsglabels_[nsrcfld_] = createFieldMessageLabel(row3, nsrcfld_);

    uiSeparator* sep3 = new uiSeparator(modelingpage_, "sep3");
    sep3->attach(stretchedBelow, row3);

    uiGroup* row4 = new uiGroup(modelingpage_, "row4");
    row4->attach(alignedBelow, sep3);
    useexistingsourcefld_ = new uiGenInput(row4, tr("Use existing source signature"), BoolInpSpec(false));
    sourceexistingfilefld_ = new uiFileInput(row4, tr("Source (.bin) path"), uiFileInput::Setup().filter("Binary files (*.bin)"));
    sourceexistingfilefld_->attach(rightOf, useexistingsourcefld_);
    sourceexistingfilefld_->display(false);

    uiGroup* row4b = new uiGroup(modelingpage_, "row4b");
    row4b->attach(alignedBelow, row4);
    velexistingfilefld_ = new uiFileInput(row4b, tr("Velocity model file"), uiFileInput::Setup().filter("Binary files (*.bin)"));

    uiGroup* row4c = new uiGroup(modelingpage_, "row4c");
    row4c->attach(alignedBelow, row4b);
    useexistingqpfld_ = new uiGenInput(row4c, tr("Use existing Qp model"), BoolInpSpec(false));
    qpexistingfilefld_ = new uiFileInput(row4c, tr("Qp model (.bin) file"), uiFileInput::Setup().filter("Binary files (*.bin)"));
    qpexistingfilefld_->attach(rightOf, useexistingqpfld_);
    qpexistingfilefld_->display(false);

    qpinfolbl_ = new uiLabel(modelingpage_, toUiString("Qp will be generated from the velocity model using Gardner's equation and saved as qp_model.bin."));
    qpinfolbl_->setHSzPol(uiObject::Wide);
    qpinfolbl_->attach(alignedBelow, row4c);

    uiSeparator* sep4 = new uiSeparator(modelingpage_, "sep4");
    sep4->attach(stretchedBelow, qpinfolbl_);

    uiGroup* rowbtn = new uiGroup(modelingpage_, "rowbtn");
    rowbtn->attach(alignedBelow, sep4);
    buildbtn_ = new uiPushButton(rowbtn, tr("Build"), mCB(this, uiMamuteMainWindow, buildModelingCB), true);
    runbtn_ = new uiPushButton(rowbtn, tr("Run"), mCB(this, uiMamuteMainWindow, runModelingCB), true);
    runbtn_->attach(rightOf, buildbtn_);
    monitorbtn_ = new uiPushButton(rowbtn, tr("Monitor"), mCB(this, uiMamuteMainWindow, monitorCB), true);
    monitorbtn_->attach(rightOf, runbtn_);
    openseismobtn_ = new uiPushButton(rowbtn, tr("Open Seismogram Viewer"), mCB(this, uiMamuteMainWindow, openSeismogramCB), true);
    openseismobtn_->attach(rightOf, monitorbtn_);

    statuslbl_ = new uiLabel(modelingpage_, toUiString("Ready."));
    statuslbl_->setHSzPol(uiObject::Wide);
    statuslbl_->setPrefWidthInChar(90);
    statuslbl_->attach(alignedBelow, rowbtn);

    nxfld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, paramsChangedCB));
    nyfld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, paramsChangedCB));
    nzfld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, paramsChangedCB));
    nsfld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, paramsChangedCB));
    borderfld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, paramsChangedCB));
    dxfld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, paramsChangedCB));
    dyfld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, paramsChangedCB));
    dzfld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, paramsChangedCB));
    fpeakfld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, paramsChangedCB));
    dtfld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, paramsChangedCB));
    amplitudefld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, paramsChangedCB));
    nsrcfld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, paramsChangedCB));
    useexistingsourcefld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, sourceModeChangedCB));
    useexistingqpfld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, qpModeChangedCB));

    loadModelingValues();
    validateInputs(false);
    tabstack_->addTab(modelingpage_, toUiString("Modeling"));
}

void uiMamuteMainWindow::loadModelingValues() {
    ModelingParams def;

    FilePath fp(proj_dir_);
    fp.add("modeling.toml");
    const BufferString modelingfp = fp.fullPath();

    const int nx = readIntParamFromFile(modelingfp, "nx", def.nx);
    const int ny = readIntParamFromFile(modelingfp, "ny", def.ny);
    const int nz = readIntParamFromFile(modelingfp, "nz", def.nz);
    const int ns = readIntParamFromFile(modelingfp, "ns", def.ns);
    borderfld_->setValue(readIntParamFromFile(modelingfp, "border", def.border));

    const float dx = readFloatParamFromFile(modelingfp, "dx", def.dx);
    const float dy = readFloatParamFromFile(modelingfp, "dy", def.dy);
    const float dz = readFloatParamFromFile(modelingfp, "dz", def.dz);
    dxfld_->setValue(dx);
    dyfld_->setValue(dy);
    dzfld_->setValue(dz);

    const float sizex = (nx > 0 && dx > 0.0f) ? (nx - 1) * dx : 0.0f;
    const float sizey = (ny > 0 && dy > 0.0f) ? (ny - 1) * dy : 0.0f;
    const float sizez = (nz > 0 && dz > 0.0f) ? (nz - 1) * dz : 0.0f;
    const float simtime = (ns > 0) ? ns * readFloatParamFromFile(modelingfp, "dt", def.dt) : 0.0f;

    nxfld_->setValue(sizex);
    nyfld_->setValue(sizey);
    nzfld_->setValue(sizez);
    nsfld_->setValue(simtime);

    fpeakfld_->setValue(readFloatParamFromFile(modelingfp, "fpeak", def.fpeak));
    dtfld_->setValue(readFloatParamFromFile(modelingfp, "dt", def.dt));
    amplitudefld_->setValue(readFloatParamFromFile(modelingfp, "amplitude", def.amplitude));
    nsrcfld_->setValue(readIntParamFromFile(modelingfp, "n_src", def.nsrc));

    hasbuiltparams_ = buildParamsFromUI(lastbuiltparams_, false);
}

bool uiMamuteMainWindow::buildParamsFromUI(ModelingParams& params, bool showmsg) const {
    params = ModelingParams();
    params.proj_dir = proj_dir_;

    const float sizex = nxfld_->getFValue();
    const float sizey = nyfld_->getFValue();
    const float sizez = nzfld_->getFValue();
    const float simtime = nsfld_->getFValue();

    params.dx = dxfld_->getFValue();
    params.dy = dyfld_->getFValue();
    params.dz = dzfld_->getFValue();

    if (params.dx <= 0.0f || params.dy <= 0.0f || params.dz <= 0.0f) {
        if (showmsg) uiMSG().error(toUiString("dx, dy, and dz must be positive before conversion."));
        return false;
    }

    params.nx = (int)std::ceil(sizex / params.dx) + 1;
    params.ny = (int)std::ceil(sizey / params.dy) + 1;
    params.nz = (int)std::ceil(sizez / params.dz) + 1;

    params.dt = dtfld_->getFValue();
    if (params.dt <= 0.0f) {
        if (showmsg) uiMSG().error(toUiString("dt must be positive before conversion."));
        return false;
    }
    params.ns = (int)std::ceil(simtime / params.dt);
    params.border = borderfld_->getIntValue();

    params.fpeak = fpeakfld_->getFValue();
    params.amplitude = amplitudefld_->getFValue();
    params.nsrc = nsrcfld_->getIntValue();

    params.velocity_filename = "data/velocity_model.bin";
    params.qp_filename = "data/qp_model.bin";

    const bool useexistingsource = useexistingsourcefld_ && useexistingsourcefld_->getBoolValue();
    const bool useexistingvel = velexistingfilefld_ && !BufferString(velexistingfilefld_->fileName()).isEmpty();
    const bool useexistingqp = useexistingqpfld_ && useexistingqpfld_->getBoolValue();

    params.generate_source_signature = !useexistingsource;
    params.generate_geometry = true;
    params.generate_velocity_model = !useexistingvel;
    params.generate_qp_model = !useexistingqp;

    if (useexistingsource)
        params.source_signature_file = sourceexistingfilefld_ ? sourceexistingfilefld_->fileName() : "";

    if (params.proj_dir.isEmpty()) {
        if (showmsg) uiMSG().error(toUiString("Project directory is empty."));
        return false;
    }

    if (params.nx <= 0 || params.ny <= 0 || params.nz <= 0 || params.ns <= 0 || params.nsrc <= 0) {
        if (showmsg) uiMSG().error(toUiString("nx, ny, nz, ns, and n_src must be positive."));
        return false;
    }

    return true;
}

bool uiMamuteMainWindow::paramsDifferFromLastBuild(const ModelingParams& cur) const {
    return !hasbuiltparams_ || cur != lastbuiltparams_;
}

bool uiMamuteMainWindow::validateInputs(bool showmsg) {
    const float sizex = nxfld_->getFValue();
    const float sizey = nyfld_->getFValue();
    const float sizez = nzfld_->getFValue();
    const float simtime = nsfld_->getFValue();
    const int border = borderfld_->getIntValue();
    const int nsrc = nsrcfld_->getIntValue();

    const float dx = dxfld_->getFValue();
    const float dy = dyfld_->getFValue();
    const float dz = dzfld_->getFValue();
    const float fpeak = fpeakfld_->getFValue();
    const float dt = dtfld_->getFValue();
    const float amplitude = amplitudefld_->getFValue();

    const bool useexistingsource = useexistingsourcefld_ && useexistingsourcefld_->getBoolValue();
    const bool useexistingvel = velexistingfilefld_ && !BufferString(velexistingfilefld_->fileName()).isEmpty();
    const bool useexistingqp = useexistingqpfld_ && useexistingqpfld_->getBoolValue();

    bool allok = true;

    const bool sizexok = sizex > 0.0f;
    setFieldValidationState(nxfld_, sizexok, "Size X must be > 0");
    allok = allok && sizexok;

    const bool sizeyok = sizey > 0.0f;
    setFieldValidationState(nyfld_, sizeyok, "Size Y must be > 0");
    allok = allok && sizeyok;

    const bool sizezok = sizez > 0.0f;
    setFieldValidationState(nzfld_, sizezok, "Size Z must be > 0");
    allok = allok && sizezok;

    const bool simtimeok = simtime > 0.0f;
    setFieldValidationState(nsfld_, simtimeok, "Simulation time must be > 0");
    allok = allok && simtimeok;

    const int nx = (dx > 0.0f) ? ((int)std::ceil(sizex / dx) + 1) : 0;
    const int ny = (dy > 0.0f) ? ((int)std::ceil(sizey / dy) + 1) : 0;
    const int nz = (dz > 0.0f) ? ((int)std::ceil(sizez / dz) + 1) : 0;
    const bool borderok = border > 0 && border < nx && border < ny && border < nz;
    setFieldValidationState(borderfld_, borderok, border <= 0 ? "Border (grid cells) must be > 0" : "Border must be < grid size (cells)");
    allok = allok && borderok;

    const bool dxok = dx > 0.0f;
    setFieldValidationState(dxfld_, dxok, "dx must be > 0");
    allok = allok && dxok;

    const bool dyok = dy > 0.0f;
    setFieldValidationState(dyfld_, dyok, "dy must be > 0");
    allok = allok && dyok;

    const bool dzok = dz > 0.0f;
    setFieldValidationState(dzfld_, dzok, "dz must be > 0");
    allok = allok && dzok;

    const bool fpeakok = fpeak > 0.0f;
    setFieldValidationState(fpeakfld_, fpeakok, "Hz must be > 0");
    allok = allok && fpeakok;

    if (useexistingsource) {
        setFieldValidationState(amplitudefld_, true, "");
        const BufferString srcpath = sourceexistingfilefld_ ? sourceexistingfilefld_->fileName() : "";
        const bool srcok = !srcpath.isEmpty() && File::exists(srcpath);
        allok = allok && srcok;
        if (showmsg && !srcok) {
            if (srcpath.isEmpty())
                uiMSG().error(toUiString("Select an existing source signature file (.bin)."));
            else
                uiMSG().error(toUiString("Selected source signature file does not exist."));
        }
    } else {
        const bool ampok = amplitude > 0.0f;
        setFieldValidationState(amplitudefld_, ampok, "Amplitude must be > 0");
        allok = allok && ampok;
    }

    const bool dtok = dt > 0.0f;
    setFieldValidationState(dtfld_, dtok, "Sampling must be > 0");
    allok = allok && dtok;

    const bool nsrcok = nsrc > 0;
    setFieldValidationState(nsrcfld_, nsrcok, "Sources must be > 0");
    allok = allok && nsrcok;

    if (useexistingvel) {
        BufferString velpath = velexistingfilefld_ ? velexistingfilefld_->fileName() : "";
        if (!velpath.isEmpty() && !File::exists(velpath)) {
            FilePath fp(proj_dir_);
            fp.add("data").add(velpath);
            if (File::exists(fp.fullPath())) velpath = fp.fullPath();
        }
        const bool velpathok = !velpath.isEmpty() && File::exists(velpath);
        allok = allok && velpathok;
        if (showmsg && !velpathok) {
            if (velpath.isEmpty())
                uiMSG().error(toUiString("Select an existing velocity model file (.bin)."));
            else
                uiMSG().error(toUiString("Selected velocity model file does not exist."));
        }
    }

    if (useexistingqp) {
        const BufferString qppath = qpexistingfilefld_ ? qpexistingfilefld_->fileName() : "";
        const bool qpfileok = !qppath.isEmpty() && File::exists(qppath);
        allok = allok && qpfileok;

        if (showmsg && !qpfileok) {
            if (qppath.isEmpty())
                uiMSG().error(toUiString("Select an existing Qp model file (.bin)."));
            else
                uiMSG().error(toUiString("Selected Qp model file does not exist."));
        }
    }

    if (buildbtn_)
        buildbtn_->setSensitive(allok);
    if (runbtn_)
        runbtn_->setSensitive(allok && hasbuiltparams_);

    if (statuslbl_) {
        if (!allok) {
            statuslbl_->setText(toUiString("Fix the highlighted fields before Build/Run."));
            statuslbl_->setTextColor(OD::Color::Red());
        } else if (!hasbuiltparams_) {
            statuslbl_->setText(toUiString("Parameters valid. Click Build to prepare inputs."));
            statuslbl_->setTextColor(OD::Color::Black());
        } else {
            ModelingParams cur;
            buildParamsFromUI(cur, false);
            if (paramsDifferFromLastBuild(cur)) {
                statuslbl_->setText(toUiString("Parameters changed since last Build."));
                statuslbl_->setTextColor(OD::Color(170, 120, 0));
            } else {
                statuslbl_->setText(toUiString("Parameters valid. Ready to run."));
                statuslbl_->setTextColor(OD::Color::Black());
            }
        }
    }

    return allok;
}

void uiMamuteMainWindow::setFieldValidationState(uiGenInput* fld, bool isvalid, const char* msg) {
    if (!fld)
        return;

    const uiString uitip = (!isvalid && msg && *msg) ? toUiString(msg) : uiString::emptyString();
    fld->setToolTip(uitip);

    mDynamicCastGet(uiLineEdit*, le, fld->element(0));
    if (le) {
        le->setBackgroundColor(isvalid ? OD::Color::White() : OD::Color(255, 230, 230));
        le->setToolTip(uitip);
    }

    auto it = fieldmsglabels_.find(fld);
    if (it != fieldmsglabels_.end() && it->second) {
        if (isvalid || !msg || !*msg) {
            it->second->setText(toUiString(" "));
            it->second->setTextColor(OD::Color::Black());
        } else {
            it->second->setText(toUiString(msg));
            it->second->setTextColor(OD::Color::Red());
        }
    }
}

void uiMamuteMainWindow::paramsChangedCB(CallBacker*) {
    normalizeDecimalSeparator(dxfld_);
    normalizeDecimalSeparator(dyfld_);
    normalizeDecimalSeparator(dzfld_);
    normalizeDecimalSeparator(nxfld_);
    normalizeDecimalSeparator(nyfld_);
    normalizeDecimalSeparator(nzfld_);
    normalizeDecimalSeparator(nsfld_);
    normalizeDecimalSeparator(fpeakfld_);
    normalizeDecimalSeparator(dtfld_);
    normalizeDecimalSeparator(amplitudefld_);
    validateInputs(false);
}

void uiMamuteMainWindow::sourceModeChangedCB(CallBacker*) {
    const bool useexistingsource = useexistingsourcefld_ && useexistingsourcefld_->getBoolValue();

    if (fpeakfld_) {
        fpeakfld_->display(true);
        fpeakfld_->setSensitive(true);
        fpeakfld_->setToolTip(uiString::emptyString());
    }

    if (amplitudefld_) {
        amplitudefld_->display(!useexistingsource);
        amplitudefld_->setSensitive(!useexistingsource);
        amplitudefld_->setToolTip(useexistingsource ? toUiString("Not used when using an existing source signature") : uiString::emptyString());
    }

    if (sourceexistingfilefld_) {
        if (!useexistingsource)
            sourceexistingfilefld_->setFileName("");

        sourceexistingfilefld_->display(useexistingsource);
        sourceexistingfilefld_->setSensitive(useexistingsource);
    }

    validateInputs(false);
}

void uiMamuteMainWindow::velocityModeChangedCB(CallBacker*) {
    validateInputs(false);
}

void uiMamuteMainWindow::qpModeChangedCB(CallBacker*) {
    const bool useexistingqp = useexistingqpfld_ && useexistingqpfld_->getBoolValue();

    if (qpexistingfilefld_) {
        if (!useexistingqp)
            qpexistingfilefld_->setFileName("");

        qpexistingfilefld_->display(useexistingqp);
        qpexistingfilefld_->setSensitive(useexistingqp);
    }

    if (qpinfolbl_)
        qpinfolbl_->display(!useexistingqp);

    validateInputs(false);
}

void uiMamuteMainWindow::sidebarSelectionChanged(CallBacker*) {
    if (!sidebar_ || !tabstack_)
        return;

    const int cur = sidebar_->currentItem();
    if (cur < 0 || cur >= tabstack_->size())
        return;

    tabstack_->setCurrentPage(cur);
}

int uiMamuteMainWindow::readNSrcFromModeling() const {
    FilePath fp(proj_dir_);
    fp.add("modeling.toml");
    return readIntParamFromFile(fp.fullPath(), "n_src", 27);
}

void uiMamuteMainWindow::buildModelingCB(CallBacker*) {
    if (!validateInputs(true))
        return;

    ModelingParams params;
    if (!buildParamsFromUI(params, true))
        return;

    const bool useexistingsource = useexistingsourcefld_ && useexistingsourcefld_->getBoolValue();
    const bool useexistingvel = velexistingfilefld_ && !BufferString(velexistingfilefld_->fileName()).isEmpty();
    const bool useexistingqp = useexistingqpfld_ && useexistingqpfld_->getBoolValue();

    if (!hasProjectGeometryFiles(params.proj_dir)) {
        uiMSG().error(toUiString(
            "Geometry binaries not found in project (coords/src_coord.bin and rcv_coord_*.bin).\nGenerate them first in the Acquisition Geometry tab."));
        return;
    }

    if (!useexistingvel && !hasProjectVelocityModel(params.proj_dir)) {
        uiMSG().error(toUiString(
            "Project velocity model not found (data/velocity_model.bin).\nGenerate it first in the Velocity Model tab, or switch to existing velocity model file."));
        return;
    }

    BufferStringSet previousbuildouts;
    collectBuildOutputFiles(params.proj_dir, true, false, true, previousbuildouts);
    if (!previousbuildouts.isEmpty()) {
        BufferString msg(
            "This Build will overwrite existing model files.\n\nFiles to overwrite:\n");

        for (int i = 0; i < previousbuildouts.size(); i++)
            msg.add(" - ").add(previousbuildouts.get(i)).add("\n");

        msg.add("\n"
                "Do you want to continue?");

        if (!uiMSG().askGoOn(toUiString(msg)))
            return;

        if (!clearOutputFiles(previousbuildouts)) {
            uiMSG().error(toUiString(
                "Failed to remove one or more previous build output files. Please check project directory permissions."));
            return;
        }
    }

    service_->setMamutePath(getMamuteInstallPath());

    const int totalsteps = 4;
    const auto setbuildstep = [&](int step, const char* msg) {
        if (!statuslbl_)
            return;

        BufferString txt("Build: Step ");
        txt.add(step).add("/").add(totalsteps).add(" - ").add(msg);
        statuslbl_->setText(toUiString(txt));
        statuslbl_->setTextColor(OD::Color(190, 140, 0));
        uiMain::processEvents();
    };

    setbuildstep(1, "Saving modeling parameters");
    if (!service_->writeConfigFile(params, this)) return;

    setbuildstep(2, useexistingsource ? "Copying source signature" : "Generating source signature");
    if (useexistingsource) {
        const BufferString srcpath = sourceexistingfilefld_ ? sourceexistingfilefld_->fileName() : "";
        if (srcpath.isEmpty() || !File::exists(srcpath)) {
            uiMSG().error(toUiString("Selected source signature file does not exist."));
            return;
        }

        FilePath dstfp(params.proj_dir);
        dstfp.add("data").add("source.bin");
        if (File::exists(dstfp.fullPath()))
            File::remove(dstfp.fullPath());

        BufferString copyerr;
        if (!File::copy(srcpath, dstfp.fullPath(), &copyerr)) {
            BufferString errmsg("Failed to copy source signature file: ");
            errmsg.add(copyerr.isEmpty() ? srcpath.buf() : copyerr.buf());
            uiMSG().error(toUiString(errmsg));
            return;
        }
    } else {
        if (!service_->generateSource(params, this)) return;
    }

    setbuildstep(3, useexistingvel ? "Copying velocity model" : "Verifying project velocity model");
    if (useexistingvel) {
        BufferString velpath = velexistingfilefld_ ? velexistingfilefld_->fileName() : "";
        if (velpath.isEmpty() || !File::exists(velpath)) {
            uiMSG().error(toUiString("Selected velocity model file does not exist."));
            return;
        }

        FilePath dstfp(params.proj_dir);
        dstfp.add("data").add("velocity_model.bin");

        if (FilePath(velpath).fullPath() != dstfp.fullPath()) {
            if (File::exists(dstfp.fullPath()))
                File::remove(dstfp.fullPath());

            BufferString copyerr;
            if (!File::copy(velpath, dstfp.fullPath(), &copyerr)) {
                BufferString errmsg("Failed to copy velocity model file: ");
                errmsg.add(copyerr.isEmpty() ? velpath.buf() : copyerr.buf());
                uiMSG().error(toUiString(errmsg));
                return;
            }
        }
    }

    setbuildstep(4, useexistingqp ? "Copying Qp model" : "Generating Qp model");
    if (useexistingqp) {
        const BufferString srcpath = qpexistingfilefld_ ? qpexistingfilefld_->fileName() : "";
        if (srcpath.isEmpty() || !File::exists(srcpath)) {
            uiMSG().error(toUiString("Selected Qp model file does not exist."));
            return;
        }

        FilePath dstfp(params.proj_dir);
        dstfp.add("data").add(params.qp_filename);
        if (File::exists(dstfp.fullPath()))
            File::remove(dstfp.fullPath());

        BufferString copyerr;
        if (!File::copy(srcpath, dstfp.fullPath(), &copyerr)) {
            BufferString errmsg("Failed to copy Qp model file: ");
            errmsg.add(copyerr.isEmpty() ? srcpath.buf() : copyerr.buf());
            uiMSG().error(toUiString(errmsg));
            return;
        }
    } else {
        if (!service_->generateQpModel(params, this)) return;
    }

    lastbuiltparams_ = params;
    hasbuiltparams_ = true;
    validateInputs(false);

    if (statuslbl_) {
        statuslbl_->setText(toUiString("Build completed successfully."));
        statuslbl_->setTextColor(OD::Color(0, 140, 0));
    }
}

void uiMamuteMainWindow::runModelingCB(CallBacker*) {
    if (!validateInputs(true))
        return;

    ModelingParams params;
    if (!buildParamsFromUI(params, true))
        return;

    if (!hasbuiltparams_) {
        uiMSG().warning(toUiString("Run blocked: build is required first."));
        return;
    }

    const bool changed = paramsDifferFromLastBuild(params);
    ModelingParams paramstorun = changed ? lastbuiltparams_ : params;

    if (changed) {
        BufferString msg(
            "Current parameters differ from the last built modeling.toml.\n\n"
            "If you want to run with the new parameters, run Build first.\n"
            "Do you want to continue Run using the last built parameters?");
        if (!uiMSG().askGoOn(toUiString(msg)))
            return;
    }

    BufferStringSet previousdobs;
    collectRunOutputFiles(paramstorun.proj_dir, previousdobs);
    if (!previousdobs.isEmpty()) {
        BufferString msg(
            "This project already contains seismic output files from a previous run\n(found ");
        msg.add(previousdobs.size()).add(" dobs_*.bin file");
        if (previousdobs.size() != 1) msg.add("s");
        msg.add(").\n\n"
                "They will be overwritten if you continue.\n\nDo you want to continue?");

        if (!uiMSG().askGoOn(toUiString(msg)))
            return;

        if (!clearOutputFiles(previousdobs)) {
            uiMSG().error(toUiString(
                "Failed to remove one or more previous output files. Please check project directory permissions."));
            return;
        }
    }

    service_->setMamutePath(getMamuteInstallPath());
    if (!workflow_->run(paramstorun))
        return;

    if (statuslbl_)
        statuslbl_->setText(toUiString("Modeling started."));
}

void uiMamuteMainWindow::monitorCB(CallBacker*) {
    service_->openLogMonitor(this);
}

void uiMamuteMainWindow::openSeismogramCB(CallBacker*) {
    if (proj_dir_.isEmpty() || !File::isDirectory(proj_dir_)) {
        uiMSG().error(toUiString("Project directory is invalid."));
        return;
    }

    int nsrc = nsrcfld_ ? nsrcfld_->getIntValue() : readNSrcFromModeling();
    if (nsrc <= 0)
        nsrc = readNSrcFromModeling();

    uiSeismogramViewerDlg* dlg = new uiSeismogramViewerDlg(this, proj_dir_, nsrc);
    dlg->go();
}

bool uiMamuteMainWindow::acceptOK(CallBacker*) {
    return true;
}