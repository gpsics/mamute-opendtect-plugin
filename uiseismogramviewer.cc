#include "uiseismogramviewer.h"
#include "bufstringset.h"
#include "file.h"
#include "filepath.h"
#include "od_istream.h"
#include "odjson.h"
#include "timer.h"
#include "uibutton.h"
#include "uifileinput.h"
#include "uigeninput.h"
#include "uigraphicsitemimpl.h"
#include "uigraphicsscene.h"
#include "uigraphicsview.h"
#include "uilabel.h"
#include "uilistbox.h"
#include "uimsg.h"
#include "uiprogressbar.h"
#include "uistrings.h"
#include "uitextedit.h"
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#ifndef __win__
#include <signal.h>
#endif

#include "ctxtioobj.h"
#include "ioman.h"
#include "ioobj.h"
#include "od_ostream.h"
#include "posinfo2d.h"
#include "seis2ddata.h"
#include "seis2dlineio.h"
#include "seistrc.h"
#include "survgeom2d.h"
#include "trckey.h"

#include "attribsel.h"
#include "flatview.h"
#include "genc.h"
#include "oscommand.h"
#include "survgeom.h"
#include "trckeyzsampling.h"
#include "uiattribpartserv.h"
#include "uiodapplmgr.h"
#include "uiodmain.h"
#include "uiodviewer2dmgr.h"
#include "uiodviewer2dposgrp.h"

using namespace Mamute;

static bool parseAxisRange(const OD::JSON::Object* axisobj,
                           int& first, int& last, int& delta) {
    if (!axisobj) return false;
    first = (int)axisobj->getIntValue("first");
    last = (int)axisobj->getIntValue("last");
    delta = (int)axisobj->getIntValue("delta");
    return delta > 0;
}

static bool readSrcRcvJson(const BufferString& jsonpath, OD::JSON::Object& root) {
    od_istream strm(jsonpath.buf());
    uiRetVal rv = root.read(strm);
    return rv.isOK();
}

static bool hasFatalLogError(const BufferString& logpath, BufferString& lastfatal) {
    if (logpath.isEmpty() || !File::exists(logpath))
        return false;

    std::ifstream strm(logpath.buf());
    if (!strm.good())
        return false;

    bool found = false;
    std::string line;
    std::string lastline;
    while (std::getline(strm, line)) {
        std::string low = line;
        for (char& c : low)
            c = (char)tolower((unsigned char)c);

        if (low.find("segmentation fault") != std::string::npos ||
            low.find("core dumped") != std::string::npos ||
            low.find("fatal") != std::string::npos ||
            low.find("exception") != std::string::npos ||
            low.find("traceback") != std::string::npos ||
            low.find("error") != std::string::npos) {
            found = true;
            lastline = line;
        }
    }

    if (found) {
        lastfatal = lastline.c_str();
        lastfatal.trimBlanks();
    }

    return found;
}

SeismogramMonitor::SeismogramMonitor(const BufferString& projdir, int nsrc, uiParent* parent, const BufferString& execid, PID_Type pid) : SeismogramMonitor(projdir, nsrc, parent, execid, pid, BufferString()) {
}

SeismogramMonitor::SeismogramMonitor(const BufferString& projdir, int nsrc,
                                     uiParent* parent, const BufferString& execid, PID_Type pid,
                                     const BufferString& runlogpath)
    : projdir_(projdir), nsrc_(nsrc), parent_(parent), pid_(pid), execution_id_(execid), runlogpath_(runlogpath) {
}

SeismogramMonitor::~SeismogramMonitor() {
    stopMonitoring();
    if (progressdlg_) {
        progressdlg_->close();
        progressdlg_ = nullptr;
    }
}

void SeismogramMonitor::createProgressDialog() {
    progressdlg_ = new uiDialog(parent_, uiDialog::Setup(toUiString("Modeling Monitor"), toUiString("Running seismic modeling..."), mNoHelpKey).modal(false).mainwidgcentered(true));
    progressdlg_->setDeleteOnClose(true);
    progressdlg_->setPopupArea(uiMainWin::Middle);
    progressdlg_->windowClosed.notify(mCB(this, SeismogramMonitor, monitorWindowClosed));
    progressdlg_->setCtrlStyle(uiDialog::CloseOnly);
    progressdlg_->setCancelText(uiString::emptyString());

    statuslabel_ = new uiLabel(progressdlg_, toUiString("Initializing modeling..."));
    statuslabel_->setHSzPol(uiObject::Wide);
    statuslabel_->setAlignment(Alignment::HCenter);

    progressbar_ = new uiProgressBar(progressdlg_, "Progress");
    progressbar_->setHSzPol(uiObject::WideMax);
    progressbar_->setStretch(2, 0);
    progressbar_->setTotalSteps(nsrc_);
    progressbar_->setProgress(0);
    progressbar_->attach(stretchedBelow, statuslabel_);

    elapsedlabel_ = new uiLabel(progressdlg_, toUiString("Elapsed: 00:00  |  Process running..."));
    elapsedlabel_->setHSzPol(uiObject::Wide);
    elapsedlabel_->setAlignment(Alignment::HCenter);
    elapsedlabel_->attach(centeredBelow, progressbar_);

    logstatuslabel_ = new uiLabel(progressdlg_, toUiString("Waiting for log file..."));
    logstatuslabel_->setHSzPol(uiObject::Wide);
    logstatuslabel_->setAlignment(Alignment::HCenter);
    logstatuslabel_->attach(alignedBelow, elapsedlabel_);

    logtextedit_ = new uiTextEdit(progressdlg_, "ModelingLogView", true);
    logtextedit_->setPrefWidthInChar(100);
    logtextedit_->setPrefHeightInChar(20);
    logtextedit_->attach(stretchedBelow, logstatuslabel_);

    terminatebtn_ = new uiPushButton(progressdlg_, toUiString("Terminate Process"),
                                     mCB(this, SeismogramMonitor, terminateProcess),
                                     true);
    terminatebtn_->attach(alignedBelow, logtextedit_);

    closebtn_ = new uiPushButton(progressdlg_, toUiString("Close"),
                                 mCB(this, SeismogramMonitor, closeMonitorWindow),
                                 true);
    closebtn_->attach(rightOf, terminatebtn_);
}

void SeismogramMonitor::updateProgress() {
    if (!progressbar_ || !statuslabel_ || !elapsedlabel_)
        return;

    progressbar_->setProgress(completed_shots_);

    float percentage = (float)completed_shots_ / nsrc_ * 100.0f;
    BufferString status("Shot ");
    status.add(completed_shots_).add(" of ").add(nsrc_);
    status.add(" completed (").add((int)percentage).add("%)");

    statuslabel_->setText(toUiString(status));

    const bool alive = isProcessRunning();
    const int elapsed = (!alive && final_elapsed_seconds_ >= 0) ? final_elapsed_seconds_ : (int)difftime(time(nullptr), start_time_);
    BufferString elapsed_str("Elapsed: ");
    elapsed_str.add(formatElapsed(elapsed));

    const char* spinner[] = {"|", "/", "-", "\\"};
    const char* spin = spinner[tick_count_ % 4];
    if (alive) {
        elapsed_str.add("  ").add(spin).add("  Process running...");
    } else {
        elapsed_str.add("  |  Process finished");
    }

    elapsedlabel_->setText(toUiString(elapsed_str));

    if (terminatebtn_)
        terminatebtn_->setSensitive(alive);
}

BufferString SeismogramMonitor::formatElapsed(int seconds) const {
    int hrs = seconds / 3600;
    int mins = (seconds % 3600) / 60;
    int secs = seconds % 60;
    BufferString result;
    if (hrs > 0)
        result.add(hrs).add("h ");
    if (mins < 10 && hrs > 0)
        result.add("0");
    result.add(mins).add("m ");
    if (secs < 10)
        result.add("0");
    result.add(secs).add("s");
    return result;
}

void SeismogramMonitor::startMonitoring() {
    start_time_ = time(nullptr);
    tick_count_ = 0;
    final_elapsed_seconds_ = -1;
    finish_handled_ = false;
    termination_requested_ = false;
    showMonitorWindow();
    updateProgress();
    loadLogContent();
    updateStatusText();

    timer_ = new Timer("Modeling Monitor");
    timer_->tick.notify(mCB(this, SeismogramMonitor, checkModelingStatus));
    timer_->start(1000, false);
}

void SeismogramMonitor::stopMonitoring() {
    if (timer_) {
        timer_->stop();
        delete timer_;
        timer_ = nullptr;
    }
}

void SeismogramMonitor::showMonitorWindow() {
    if (!progressdlg_) {
        last_log_file_size_ = 0;
        createProgressDialog();
        loadLogContent();
        updateStatusText();
        updateProgress();
    }

    progressdlg_->show();
    progressdlg_->raise();
}

bool SeismogramMonitor::isProcessRunning() {
    if (mIsUdf(pid_) || pid_ <= 0)
        return false;

    return isProcessAlive(pid_);
}

int SeismogramMonitor::countCompletedShots() {
    FilePath datadir(projdir_);
    datadir.add("data");

    int count = 0;
    for (int i = 0; i < nsrc_; i++) {
        FilePath fp(datadir);
        BufferString filename("dobs_");
        filename.add(i).add(".bin");
        fp.add(filename);

        if (File::exists(fp.fullPath()))
            count++;
    }
    return count;
}

void SeismogramMonitor::checkModelingStatus(CallBacker*) {
    tick_count_++;

    int current_completed = countCompletedShots();

    if (current_completed > completed_shots_) {
        completed_shots_ = current_completed;
    }

    loadLogContent();
    updateStatusText();

    updateProgress();

    if (!isProcessRunning() && !finish_handled_) {
        finish_handled_ = true;
        final_elapsed_seconds_ = (int)difftime(time(nullptr), start_time_);
        stopMonitoring();

        updateStatusText();
        updateProgress();

        if (termination_requested_)
            return;

        FilePath dobspath(projdir_);
        dobspath.add("data").add("dobs_0.bin");

        if (File::exists(dobspath.fullPath())) {
            showSeismogramViewer();
        } else {
            BufferString fatalline;
            const bool hasfatal = hasFatalLogError(runlogpath_, fatalline);

            BufferString msg;
            if (hasfatal)
                msg = "Modeling failed before generating seismic data.\n\n";
            else
                msg = "Modeling completed, but no seismic data found.\n\n";

            msg.add("Execution ID: ").add(execution_id_).add("\n");
            if (!runlogpath_.isEmpty())
                msg.add("Check log file at: ").add(runlogpath_);
            else
                msg.add("Check log file in project runs folder.");

            if (hasfatal && !fatalline.isEmpty()) {
                msg.add("\n\nLast error: ").add(fatalline);
                uiMSG().error(toUiString(msg));
            } else {
                uiMSG().warning(toUiString(msg));
            }
        }
    }
}

void SeismogramMonitor::showSeismogramViewer() {
    uiSeismogramViewerDlg* dlg = new uiSeismogramViewerDlg(parent_, projdir_, nsrc_);
    dlg->go();
}

void SeismogramMonitor::updateStatusText() {
    if (!logstatuslabel_)
        return;

    const bool logexists = !runlogpath_.isEmpty() && File::exists(runlogpath_);
    if (logexists)
        had_logfile_ = true;

    if (!logexists) {
        logstatuslabel_->setText(toUiString("Waiting for log file..."));
        return;
    }

    if (isProcessRunning())
        logstatuslabel_->setText(toUiString("Connected to execution log"));
    else if (termination_requested_)
        logstatuslabel_->setText(toUiString("Process terminated by user"));
    else
        logstatuslabel_->setText(toUiString("Process finished"));
}

void SeismogramMonitor::loadLogContent() {
    if (!logtextedit_ || runlogpath_.isEmpty() || !File::exists(runlogpath_))
        return;

    const od_int64 current_size = File::getFileSize(runlogpath_);
    if (current_size < 0)
        return;

    if (current_size < last_log_file_size_) {
        last_log_file_size_ = 0;
        logtextedit_->setEmpty();
    }

    if (current_size == last_log_file_size_)
        return;

    std::ifstream strm(runlogpath_.buf(), std::ios::in | std::ios::binary);
    if (!strm.good())
        return;

    if (last_log_file_size_ > 0)
        strm.seekg((std::streamoff)last_log_file_size_, std::ios::beg);

    const std::string delta((std::istreambuf_iterator<char>(strm)),
                            std::istreambuf_iterator<char>());
    if (!delta.empty()) {
        logtextedit_->append(delta.c_str());
        logtextedit_->scrollToBottom();
    }

    last_log_file_size_ = current_size;
}

void SeismogramMonitor::terminateProcess(CallBacker*) {
    if (!isProcessRunning()) {
        uiMSG().warning(toUiString("The modeling process is no longer running."));
        return;
    }

    if (!uiMSG().askGoOn(toUiString(
            "Terminate the current modeling process?")))
        return;

#ifdef __win__
    uiMSG().warning(toUiString(
        "Process termination is not implemented for this platform yet."));
#else
    errno = 0;
    const bool shellsignaled = (::kill((pid_t)pid_, SIGTERM) == 0);
    const int killerr = errno;

    OS::MachineCommand childkillcmd;
    childkillcmd.setProgram("pkill");
    childkillcmd.addArg("-TERM");
    childkillcmd.addArg("-P");
    childkillcmd.addArg(BufferString().add((int)pid_));
    OS::CommandExecPars childkillpars(OS::Wait4Finish);
    OS::CommandLauncher childkiller(childkillcmd);
    childkiller.execute(childkillpars);

    if (!shellsignaled && killerr != ESRCH) {
        uiMSG().error(toUiString("Failed to terminate the modeling process."));
        return;
    }

    termination_requested_ = true;
    if (terminatebtn_)
        terminatebtn_->setSensitive(false);
#endif

    updateStatusText();
    updateProgress();
}

void SeismogramMonitor::closeMonitorWindow(CallBacker*) {
    if (progressdlg_)
        progressdlg_->close();
}

void SeismogramMonitor::monitorWindowClosed(CallBacker*) {
    last_log_file_size_ = 0;
    progressdlg_ = nullptr;
    progressbar_ = nullptr;
    statuslabel_ = nullptr;
    elapsedlabel_ = nullptr;
    logstatuslabel_ = nullptr;
    logtextedit_ = nullptr;
    terminatebtn_ = nullptr;
    closebtn_ = nullptr;
}

uiLogMonitorDlg::uiLogMonitorDlg(uiParent* p, const BufferString& logpath, PID_Type pid)
    : uiDialog(p, uiDialog::Setup(toUiString("Modeling Log Monitor"),
                                  toUiString("Real-time log viewer"),
                                  mNoHelpKey)
                      .modal(false)),
      logpath_(logpath),
      pid_(pid) {
    statuslabel_ = new uiLabel(this, toUiString("Waiting for log file..."));

    logtextedit_ = new uiTextEdit(this, "LogView", true);
    logtextedit_->setPrefWidthInChar(100);
    logtextedit_->setPrefHeightInChar(30);
    logtextedit_->attach(stretchedBelow, statuslabel_);

    setCtrlStyle(uiDialog::CloseOnly);
}

uiLogMonitorDlg::~uiLogMonitorDlg() {
    stopMonitoring();
}

void uiLogMonitorDlg::startMonitoring() {
    loadLogContent();
    updateStatusText();

    timer_ = new Timer("Log Monitor Timer");
    timer_->tick.notify(mCB(this, uiLogMonitorDlg, updateLog));
    timer_->start(500, false);
}

void uiLogMonitorDlg::stopMonitoring() {
    if (timer_) {
        timer_->stop();
        delete timer_;
        timer_ = nullptr;
    }
}

void uiLogMonitorDlg::updateStatusText() {
    const bool logexists = File::exists(logpath_);
    if (logexists)
        had_logfile_ = true;

    if (!logexists) {
        statuslabel_->setText(toUiString("Waiting for log file..."));
        return;
    }

    if (!mIsUdf(pid_) && pid_ > 0 && !isProcessAlive(pid_)) {
        statuslabel_->setText(toUiString("Process finished"));
        return;
    }

    statuslabel_->setText(toUiString("Connected"));
}

void uiLogMonitorDlg::loadLogContent() {
    if (!File::exists(logpath_))
        return;

    const od_int64 current_size = File::getFileSize(logpath_);
    if (current_size < 0)
        return;

    if (current_size < last_file_size_) {
        last_file_size_ = 0;
        logtextedit_->setEmpty();
    }

    if (current_size == last_file_size_)
        return;

    std::ifstream strm(logpath_.buf(), std::ios::in | std::ios::binary);
    if (!strm.good())
        return;

    if (last_file_size_ > 0)
        strm.seekg((std::streamoff)last_file_size_, std::ios::beg);

    const std::string delta((std::istreambuf_iterator<char>(strm)),
                            std::istreambuf_iterator<char>());
    if (!delta.empty()) {
        logtextedit_->append(delta.c_str());
        logtextedit_->scrollToBottom();
    }

    last_file_size_ = current_size;
}

void uiLogMonitorDlg::updateLog(CallBacker*) {
    loadLogContent();
    updateStatusText();
}

bool uiLogMonitorDlg::acceptOK(CallBacker*) {
    stopMonitoring();
    return true;
}

uiImportConfigDlg::uiImportConfigDlg(uiParent* p, int nrcv, int ns, float dt_ms, const BufferString& datasetname, const BufferString& linename)
    : uiDialog(p, uiDialog::Setup(toUiString("Import Configuration"), toUiString("Configure import parameters"), mNoHelpKey).oktext(toUiString("View Seismogram")).canceltext(toUiString("Cancel"))) {
    nrcvfld_ = new uiGenInput(this, toUiString("Number of traces"), IntInpSpec(nrcv));

    nsfld_ = new uiGenInput(this, toUiString("Samples per trace"), IntInpSpec(ns));
    nsfld_->attach(alignedBelow, nrcvfld_);

    dtfld_ = new uiGenInput(this, toUiString("Sampling interval (ms)"), FloatInpSpec(dt_ms));
    dtfld_->attach(alignedBelow, nsfld_);

    datasetfld_ = new uiGenInput(this, toUiString("2D Dataset name"), StringInpSpec(datasetname));
    datasetfld_->attach(alignedBelow, dtfld_);

    linenamefld_ = new uiGenInput(this, toUiString("Line name"), StringInpSpec(linename));
    linenamefld_->attach(alignedBelow, datasetfld_);
}

uiImportConfigDlg::~uiImportConfigDlg() {
}

int uiImportConfigDlg::getNumTraces() const {
    return nrcvfld_->getIntValue();
}

int uiImportConfigDlg::getNumSamples() const {
    return nsfld_->getIntValue();
}

float uiImportConfigDlg::getSamplingInterval() const {
    return dtfld_->getFValue();
}

BufferString uiImportConfigDlg::getDatasetName() const {
    return datasetfld_->text();
}

BufferString uiImportConfigDlg::getLineName() const {
    return linenamefld_->text();
}

bool uiImportConfigDlg::acceptOK(CallBacker*) {
    if (getNumTraces() <= 0) {
        uiMSG().error(toUiString("Number of traces must be positive"));
        return false;
    }

    if (getNumSamples() <= 0) {
        uiMSG().error(toUiString("Number of samples must be positive"));
        return false;
    }

    if (getSamplingInterval() <= 0) {
        uiMSG().error(toUiString("Sampling interval must be positive"));
        return false;
    }

    BufferString dataset = getDatasetName();
    if (dataset.isEmpty()) {
        uiMSG().error(toUiString("Dataset name cannot be empty"));
        return false;
    }

    BufferString linename = getLineName();
    if (linename.isEmpty()) {
        uiMSG().error(toUiString("Line name cannot be empty"));
        return false;
    }

    return true;
}

uiSeismogramViewerDlg::uiSeismogramViewerDlg(uiParent* p, const BufferString& projdir, int nsrc)
    : uiDialog(p, uiDialog::Setup(toUiString("Seismogram Viewer"), toUiString("View Seismograms"), mNoHelpKey).oktext(toUiString("Import Seismogram"))), projdir_(projdir), nsrc_(nsrc) {
    createUI();
    populateSeismogramList();

    if (seismogramlist_->size() > 0)
        seismogramlist_->setCurrentItem(0);
}

uiSeismogramViewerDlg::~uiSeismogramViewerDlg() {
    
}

void uiSeismogramViewerDlg::createUI() {
    seismogramlist_ = new uiListBox(this, "Seismogram List");
    seismogramlist_->selectionChanged.notify(mCB(this, uiSeismogramViewerDlg, selectSeismogram));

    acquisitionview_ = new uiGraphicsView(this, "Shot Position");
    acquisitionview_->setPrefWidth(400);
    acquisitionview_->setPrefHeight(400);
    acquisitionview_->attach(rightOf, seismogramlist_);

    scene_ = new uiGraphicsScene("scene");
    acquisitionview_->setScene(*scene_);

    infobrowser_ = new uiTextEdit(this, "info", true);
    infobrowser_->attach(alignedBelow, acquisitionview_);
    infobrowser_->setPrefWidthInChar(50);
    infobrowser_->setPrefHeightInChar(10);
}

void uiSeismogramViewerDlg::populateSeismogramList() {
    seismogramlist_->setEmpty();

    FilePath datadir(projdir_);
    datadir.add("data");

    int dobidx = 0;
    while (true) {
        BufferString filename("dobs_");
        filename.add(dobidx).add(".bin");

        FilePath fp(datadir);
        fp.add(filename);

        if (File::exists(fp.fullPath())) {
            seismogramlist_->addItem(toUiString(filename));
            dobidx++;
        } else {
            break;
        }
    }

    if (seismogramlist_->isEmpty()) {
        seismogramlist_->addItem(toUiString("No seismic data found"));
    }
}

void uiSeismogramViewerDlg::selectSeismogram(CallBacker*) {
    BufferString filepath = getSelectedSeismogramPath();

    if (filepath.isEmpty() || !File::exists(filepath))
        return;

    BufferString filename = seismogramlist_->textOfItem(seismogramlist_->currentItem());
    int shotidx = -1;

    const char* underscore = strchr(filename.buf(), '_');
    if (underscore) {
        shotidx = atoi(underscore + 1);
    }

    if (shotidx < 0)
        return;

    BufferString info;

    info.add("File: ").add(filepath).add("\n\n");

    Coord3D srcpos;
    TypeSet<Coord3D> rcvpos;
    if (readSrcRcvPositions(shotidx, srcpos, rcvpos)) {
        info.add("=== Source-Receiver Position ===\n");
        info.add("Shot #").add(shotidx).add("\n\n");
        info.add("Source Coordinates: (").add(srcpos.x).add(", ").add(srcpos.y).add(", ").add(srcpos.z).add(") m\n\n");
        info.add("Number of receivers: ").add(rcvpos.size()).add(" traces\n\n");
    }

    infobrowser_->setText(info);

    plotAcquisitionGeometry(shotidx);
}

BufferString uiSeismogramViewerDlg::getSelectedSeismogramPath() {
    int selidx = seismogramlist_->currentItem();
    if (selidx < 0)
        return BufferString::empty();

    BufferString filename = seismogramlist_->textOfItem(selidx);

    FilePath fp(projdir_);
    fp.add("data").add(filename);

    return fp.fullPath();
}

bool uiSeismogramViewerDlg::readModelingParams(int& nrcv, int& ns, float& dt) {
    FilePath paramfp(projdir_);
    paramfp.add("modeling.toml");

    if (!File::exists(paramfp.fullPath())) {
        BufferString errmsg("modeling.toml file not found at: ");
        errmsg.add(paramfp.fullPath());
        uiMSG().error(toUiString(errmsg));
        return false;
    }

    od_istream strm(paramfp.fullPath());
    if (!strm.isOK()) {
        uiMSG().error(toUiString("Error opening modeling.toml"));
        return false;
    }

    BufferString line;
    ns = 0;
    dt = 0.0f;

    while (strm.getLine(line)) {

        line.trimBlanks();
        if (line.isEmpty() || line[0] == '#')
            continue;

        BufferStringSet parts;
        parts.unCat(line.buf(), "=");

        if (parts.size() < 2)
            continue;

        BufferString key = parts.get(0);
        BufferString value = parts.get(1);
        key.trimBlanks();
        value.trimBlanks();

        if (key == "ns")
            ns = value.toInt();
        else if (key == "dt")
            dt = value.toFloat();
    }

    if (ns == 0 || dt == 0.0f) {
        uiMSG().error(toUiString("Could not read ns and dt from modeling.toml"));
        return false;
    }

    FilePath jsonpath(projdir_);
    jsonpath.add("src_rcv.json");

    if (!File::exists(jsonpath.fullPath())) {
        BufferString errmsg("src_rcv.json file not found at: ");
        errmsg.add(jsonpath.fullPath());
        uiMSG().error(toUiString(errmsg));
        return false;
    }

    OD::JSON::Object root;
    if (!readSrcRcvJson(jsonpath.fullPath(), root)) {
        uiMSG().error(toUiString("Failed to parse src_rcv.json"));
        return false;
    }

    int rcv_x_first = 0, rcv_x_last = 0, rcv_x_delta = 1;
    int rcv_y_first = 0, rcv_y_last = 0, rcv_y_delta = 1;
    int rcv_z_first = 0, rcv_z_last = 0, rcv_z_delta = 1;

    const OD::JSON::Object* rcvobj = root.getObject("receivers");
    if (rcvobj) {
        parseAxisRange(rcvobj->getObject("x"), rcv_x_first, rcv_x_last, rcv_x_delta);
        parseAxisRange(rcvobj->getObject("y"), rcv_y_first, rcv_y_last, rcv_y_delta);
        parseAxisRange(rcvobj->getObject("z"), rcv_z_first, rcv_z_last, rcv_z_delta);
    }

    int nrcv_x = ((rcv_x_last - rcv_x_first) / rcv_x_delta) + 1;
    int nrcv_y = ((rcv_y_last - rcv_y_first) / rcv_y_delta) + 1;
    int nrcv_z = ((rcv_z_last - rcv_z_first) / rcv_z_delta) + 1;
    nrcv = nrcv_x * nrcv_y * nrcv_z;

    if (nrcv <= 0) {
        BufferString errmsg("Could not calculate number of receivers from src_rcv.json\n");
        errmsg.add("X: ").add(nrcv_x).add(" (").add(rcv_x_first).add(" to ").add(rcv_x_last).add(" step ").add(rcv_x_delta).add(")\n");
        errmsg.add("Y: ").add(nrcv_y).add(" (").add(rcv_y_first).add(" to ").add(rcv_y_last).add(" step ").add(rcv_y_delta).add(")\n");
        errmsg.add("Z: ").add(nrcv_z).add(" (").add(rcv_z_first).add(" to ").add(rcv_z_last).add(" step ").add(rcv_z_delta).add(")");
        uiMSG().error(toUiString(errmsg));
        return false;
    }

    return true;
}

bool uiSeismogramViewerDlg::importSeismogramToOpendTect(const BufferString& filepath, const BufferString& seisname) {
    int nrcv = 0, ns = 0;
    float dt = 0.0f;

    if (!readModelingParams(nrcv, ns, dt))
        return false;

    float dt_ms = dt * 1000.0f;

    FilePath fpath(filepath);
    BufferString filename = fpath.baseName();

    BufferString linename = filename;

    BufferString datasetname("Mamute_");
    FilePath projpath(projdir_);
    datasetname.add(projpath.fileName());

    uiImportConfigDlg configdlg(this, nrcv, ns, dt_ms, datasetname, linename);
    if (!configdlg.go())
        return false;

    nrcv = configdlg.getNumTraces();
    ns = configdlg.getNumSamples();
    dt_ms = configdlg.getSamplingInterval();
    datasetname = configdlg.getDatasetName();
    linename = configdlg.getLineName();

    const od_int64 expectedsize = (od_int64)nrcv * ns * sizeof(float);
    const od_int64 filesize = File::getFileSize(filepath);

    if (filesize != expectedsize) {
        BufferString errmsg("File size mismatch!\nExpected: ");
        errmsg.add(expectedsize).add(" bytes\nFound: ").add(filesize).add(" bytes");
        uiMSG().warning(toUiString(errmsg));
    }

    float* data = new float[nrcv * ns];

    FILE* fp = fopen(filepath.buf(), "rb");
    if (!fp) {
        delete[] data;
        BufferString errmsg("Could not open file: ");
        errmsg.add(filepath);
        uiMSG().error(toUiString(errmsg));
        return false;
    }
    const od_int64 nread = (od_int64)fread(data, sizeof(float), nrcv * ns, fp);
    fclose(fp);
    if (nread != (od_int64)nrcv * ns) {
        delete[] data;
        BufferString errmsg("Error reading binary data: expected ");
        errmsg.add(nrcv * ns).add(" floats, got ").add(nread);
        uiMSG().error(toUiString(errmsg));
        return false;
    }

    PtrMan<CtxtIOObj> ctio = mMkCtxtIOObj(SeisTrc2D);
    ctio->ctxt_.forread_ = false;
    ctio->setName(datasetname);

    const IOObj* existobj = IOM().get(datasetname, SeisTrcTranslatorGroup::sGroupName());
    if (existobj) {
        ctio->setObj(existobj->clone());
    } else {
        ctio->fillObj(false);
        if (!ctio->ioobj_) {
            delete[] data;
            uiMSG().error(toUiString("Could not create seismic dataset in OpendTect"));
            return false;
        }
        IOM().commitChanges(*ctio->ioobj_);
    }

    Pos::GeomID oldgeomid = Survey::GM().getGeomID(linename);
    if (!oldgeomid.isUdf())
        Survey::GMAdmin().removeGeometry(oldgeomid);

    auto* l2d = new PosInfo::Line2DData(linename);
    for (int itrc = 0; itrc < nrcv; itrc++) {
        PosInfo::Line2DPos pos;
        pos.nr_ = itrc + 1;
        pos.coord_.x = itrc;
        pos.coord_.y = 0;
        l2d->add(pos);
    }

    float dt_sec = dt_ms / 1000.0f;
    StepInterval<float> zrg(0, (ns - 1) * dt_sec, dt_sec);
    l2d->setZRange(zrg);

    RefMan<Survey::Geometry2D> newgeom = new Survey::Geometry2D(l2d);
    uiString geoerrmsg;
    Pos::GeomID geomid = Survey::GMAdmin().addNewEntry(newgeom.ptr(), geoerrmsg);

    if (!Survey::is2DGeom(geomid)) {
        delete[] data;
        BufferString msg("Error creating 2D geometry: ");
        msg.add(geoerrmsg.getString());
        uiMSG().error(toUiString(msg));
        return false;
    }

    Seis2DDataSet dataset(*ctio->ioobj_);

    if (!geomid.isUdf())
        dataset.remove(geomid);

    PtrMan<Seis2DLinePutter> putter = dataset.linePutter(geomid);

    if (!putter) {
        delete[] data;
        BufferString errmsg("Failed to create line putter for line: ");
        errmsg.add(linename);
        uiMSG().error(toUiString(errmsg));
        return false;
    }

    SamplingData<float> sd(0, dt_sec);

    for (int itrc = 0; itrc < nrcv; itrc++) {
        SeisTrc trc;
        if (!trc.reSize(ns, false)) {
            delete[] data;
            uiMSG().error(toUiString("Failed to resize trace"));
            return false;
        }
        trc.info().sampling = sd;
        TrcKey tk;
        tk.setGeomID(geomid);
        tk.setTrcNr(itrc + 1);
        trc.info().setTrcKey(tk);
        trc.info().coord.x = itrc;
        trc.info().coord.y = 0;
        for (int isamp = 0; isamp < ns; isamp++)
            trc.set(isamp, data[itrc * ns + isamp], 0);
        if (!putter->put(trc)) {
            delete[] data;
            BufferString errmsg("Error writing trace ");
            errmsg.add(itrc + 1);
            uiMSG().error(toUiString(errmsg));
            return false;
        }
    }

    putter->close();
    delete[] data;

    IOM().commitChanges(*ctio->ioobj_);

    MultiID datasetmid = ctio->ioobj_->key();

    BufferString successmsg("Successfully imported seismogram!\n\n");
    successmsg.add("Dataset: ").add(datasetname).add("\n");
    successmsg.add("Line: ").add(linename).add("\n");
    successmsg.add("Traces: ").add(nrcv).add("\n");
    successmsg.add("Samples: ").add(ns).add("\n\n");
    successmsg.add("Opening 2D Viewer...");

    uiMSG().message(toUiString(successmsg));

    openIn2DViewer(datasetname, datasetmid, geomid, nrcv, ns, dt_sec);

    return true;
}

bool uiSeismogramViewerDlg::openIn2DViewer(const BufferString& datasetname,
                                           const MultiID& datasetmid, const Pos::GeomID& geomid,
                                           int nrcv, int ns, float dt_sec) {
    const int ns_traces = nrcv;
    uiODMain* odmain = ODMainWin();
    if (!odmain)
        return false;

    uiAttribPartServer* attrserv = odmain->applMgr().attrServer();
    if (!attrserv)
        return false;

    Attrib::DescID descid = attrserv->getStoredID(datasetmid, true /*is2d*/);
    if (!descid.isValid())
        return false;

    Attrib::SelSpec selspec;
    selspec.set(nullptr, descid, false, "");
    selspec.setUserRef(datasetname.buf());
    selspec.set2DFlag(true);

    TrcKeyZSampling tkzs;
    tkzs.hsamp_.setGeomID(geomid);
    tkzs.hsamp_.setTrcRange(StepInterval<int>(1, ns_traces, 1));
    tkzs.zsamp_ = StepInterval<float>(0.f, (ns - 1) * dt_sec, dt_sec);

    Viewer2DPosDataSel posdatasel;
    posdatasel.postype_ = Viewer2DPosDataSel::Line2D;
    posdatasel.geomid_ = geomid;
    posdatasel.selspec_ = selspec;
    posdatasel.tkzs_ = tkzs;

    Viewer2DID vwrid = odmain->viewer2DMgr().displayIn2DViewer(
        posdatasel, FlatView::Viewer::VD);

    reject(nullptr);

    return vwrid.isValid();
}

void uiSeismogramViewerDlg::plotSeismogram(CallBacker*) {
    BufferString filepath = getSelectedSeismogramPath();

    if (filepath.isEmpty() || !File::exists(filepath)) {
        uiMSG().error(toUiString("Select a valid seismogram"));
        return;
    }

    BufferString filename = seismogramlist_->textOfItem(seismogramlist_->currentItem());
    int shotidx = -1;

    const char* underscore = strchr(filename.buf(), '_');
    if (underscore) {
        shotidx = atoi(underscore + 1);
    }

    BufferString seisname("Mamute_Shot_");
    seisname.add(shotidx >= 0 ? shotidx : seismogramlist_->currentItem());
    importSeismogramToOpendTect(filepath, seisname);
}

bool uiSeismogramViewerDlg::acceptOK(CallBacker* cb) {
    plotSeismogram(cb);
    return false;
}

bool uiSeismogramViewerDlg::readSrcRcvPositions(int shotidx, Coord3D& srcpos, TypeSet<Coord3D>& rcvpos) {
    FilePath jsonpath(projdir_);
    jsonpath.add("src_rcv.json");

    if (!File::exists(jsonpath.fullPath())) {
        return false;
    }

    OD::JSON::Object root;
    if (!readSrcRcvJson(jsonpath.fullPath(), root))
        return false;

    int rcv_x_first = 0, rcv_x_last = 0, rcv_x_delta = 1;
    int rcv_y_first = 0, rcv_y_last = 0, rcv_y_delta = 1;
    int rcv_z_first = 0, rcv_z_last = 0, rcv_z_delta = 1;

    const OD::JSON::Object* rcvobj = root.getObject("receivers");
    if (rcvobj) {
        parseAxisRange(rcvobj->getObject("x"), rcv_x_first, rcv_x_last, rcv_x_delta);
        parseAxisRange(rcvobj->getObject("y"), rcv_y_first, rcv_y_last, rcv_y_delta);
        parseAxisRange(rcvobj->getObject("z"), rcv_z_first, rcv_z_last, rcv_z_delta);
    }

    rcvpos.erase();
    if (rcv_x_delta > 0 && rcv_y_delta > 0 && rcv_z_delta > 0) {
        for (int ix = rcv_x_first; ix <= rcv_x_last; ix += rcv_x_delta) {
            for (int iy = rcv_y_first; iy <= rcv_y_last; iy += rcv_y_delta) {
                for (int iz = rcv_z_first; iz <= rcv_z_last; iz += rcv_z_delta) {
                    rcvpos += Coord3D(ix, iy, iz);
                }
            }
        }
    }

    int src_x_first = 0, src_x_last = 0, src_x_delta = 1;
    int src_y_first = 0, src_y_last = 0, src_y_delta = 1;
    int src_z_first = 0, src_z_last = 0, src_z_delta = 1;

    const OD::JSON::Object* srcobj = root.getObject("sources");
    if (srcobj) {
        parseAxisRange(srcobj->getObject("x"), src_x_first, src_x_last, src_x_delta);
        parseAxisRange(srcobj->getObject("y"), src_y_first, src_y_last, src_y_delta);
        parseAxisRange(srcobj->getObject("z"), src_z_first, src_z_last, src_z_delta);
    }

    TypeSet<Coord3D> allsources;
    if (src_x_delta > 0 && src_y_delta > 0 && src_z_delta > 0) {
        for (int iz = src_z_first; iz <= src_z_last; iz += src_z_delta) {
            for (int iy = src_y_first; iy <= src_y_last; iy += src_y_delta) {
                for (int ix = src_x_first; ix <= src_x_last; ix += src_x_delta) {
                    allsources += Coord3D(ix, iy, iz);
                }
            }
        }
    }

    if (shotidx >= 0 && shotidx < allsources.size())
        srcpos = allsources[shotidx];
    else
        return false;

    return true;
}

void uiSeismogramViewerDlg::plotAcquisitionGeometry(int shotidx) {
    if (!scene_)
        return;

    scene_->removeAllItems();

    Coord3D srcpos;
    TypeSet<Coord3D> rcvpos;

    if (!readSrcRcvPositions(shotidx, srcpos, rcvpos)) {
        scene_->addItem(new uiTextItem(toUiString("Error reading positions"),
                                       Alignment(Alignment::HCenter, Alignment::VCenter)));
        return;
    }

    if (rcvpos.isEmpty())
        return;

    float minx = srcpos.x;
    float maxx = srcpos.x;
    float miny = srcpos.y;
    float maxy = srcpos.y;

    for (int i = 0; i < rcvpos.size(); i++) {
        if (rcvpos[i].x < minx) minx = rcvpos[i].x;
        if (rcvpos[i].x > maxx) maxx = rcvpos[i].x;
        if (rcvpos[i].y < miny) miny = rcvpos[i].y;
        if (rcvpos[i].y > maxy) maxy = rcvpos[i].y;
    }

    float margin = 20;
    float topmargin = 90;
    float rangex = maxx - minx;
    float rangey = maxy - miny;

    if (rangex < 1) rangex = 100;
    if (rangey < 1) rangey = 100;

    float scenewidth = 360;
    float sceneheight = 360;

    float scalex = scenewidth / (rangex + 2 * margin);
    float scaley = (sceneheight - topmargin) / (rangey + 2 * margin);
    float scale = scalex < scaley ? scalex : scaley;

    auto toSceneX = [&](float x) { return margin + (x - minx) * scale; };
    auto toSceneY = [&](float y) { return sceneheight - (margin + (y - miny) * scale); };

    OD::Color rcvcolor = OD::Color::Green();

    std::map<std::pair<int, int>, TypeSet<int>> posmap;
    for (int i = 0; i < rcvpos.size(); i++) {
        std::pair<int, int> key(rcvpos[i].x, rcvpos[i].y);
        if (posmap.find(key) == posmap.end())
            posmap[key] = TypeSet<int>();
        posmap[key] += i;
    }

    for (auto it = posmap.begin(); it != posmap.end(); ++it) {
        int x = it->first.first;
        int y = it->first.second;
        const TypeSet<int>& indices = it->second;

        float sx = toSceneX(x);
        float sy = toSceneY(y);

        uiCircleItem* rcvpt = new uiCircleItem();
        rcvpt->setPos(uiPoint(sx, sy));
        rcvpt->setRadius(3);
        rcvpt->setFillColor(rcvcolor);
        rcvpt->setPenColor(rcvcolor);

        BufferString tooltip;
        if (indices.size() == 1) {
            int idx = indices[0];
            tooltip.add("Receiver #").add(idx + 1);
            tooltip.add(": (").add(rcvpos[idx].x).add(", ").add(rcvpos[idx].y);
            tooltip.add(", ").add(rcvpos[idx].z).add(") m");
        } else {
            tooltip.add(indices.size()).add(" receivers at (").add(x).add(", ").add(y).add("):\n");
            for (int j = 0; j < indices.size(); j++) {
                int idx = indices[j];
                tooltip.add("  #").add(idx + 1).add(": Z=").add(rcvpos[idx].z).add(" m");
                if (j < indices.size() - 1)
                    tooltip.add("\n");
            }
        }

        rcvpt->setToolTip(toUiString(tooltip));
        scene_->addItem(rcvpt);
    }

    OD::Color srccolor = OD::Color::Red();
    float sx = toSceneX(srcpos.x);
    float sy = toSceneY(srcpos.y);

    uiCircleItem* srcpt = new uiCircleItem();
    srcpt->setPos(uiPoint(sx, sy));
    srcpt->setRadius(6);
    srcpt->setFillColor(srccolor);
    srcpt->setPenColor(srccolor);

    BufferString srctooltip("Source: (");
    srctooltip.add(srcpos.x).add(", ").add(srcpos.y).add(", ").add(srcpos.z).add(") m");
    srcpt->setToolTip(toUiString(srctooltip));

    scene_->addItem(srcpt);

    BufferString title("Shot #");
    title.add(shotidx).add(" - Source: (").add(srcpos.x).add(", ").add(srcpos.y).add(", ").add(srcpos.z).add(")");
    uiTextItem* titleitem = new uiTextItem(toUiString(title),
                                           Alignment(Alignment::HCenter, Alignment::Top));
    titleitem->setPos(uiPoint(scenewidth / 2, 10));
    scene_->addItem(titleitem);

    uiRectItem* legendbox = new uiRectItem();
    legendbox->setRect(5, 45, 100, 55);
    legendbox->setFillColor(OD::Color(255, 255, 255, 200));
    legendbox->setPenColor(OD::Color::Black());
    legendbox->setPenStyle(OD::LineStyle(OD::LineStyle::Solid, 2));
    scene_->addItem(legendbox);

    uiTextItem* legendsrc = new uiTextItem(toUiString("● Source"),
                                           Alignment(Alignment::Left, Alignment::Top));
    legendsrc->setPos(uiPoint(10, 50));
    legendsrc->setTextColor(srccolor);
    scene_->addItem(legendsrc);

    uiTextItem* legendrcv = new uiTextItem(toUiString("● Receivers"),
                                           Alignment(Alignment::Left, Alignment::Top));
    legendrcv->setPos(uiPoint(10, 65));
    legendrcv->setTextColor(rcvcolor);
    scene_->addItem(legendrcv);
}
