#pragma once

#include "bufstring.h"
#include "coord.h"
#include "genc.h"
#include "multiid.h"
#include "posgeomid.h"
#include "timer.h"
#include "uidialog.h"

class uiGenInput;
class uiFileInput;
class uiPushButton;
class uiListBox;
class uiTextEdit;
class uiGraphicsView;
class uiGraphicsScene;
class uiProgressBar;
class uiLabel;

namespace Mamute {

class Coord3D {
  public:
    Coord3D() : x(0), y(0), z(0) {}
    Coord3D(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    double x;
    double y;
    double z;

    bool operator==(const Coord3D& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const Coord3D& other) const {
        return !(*this == other);
    }
};

class SeismogramMonitor : public CallBacker {
  public:
    SeismogramMonitor(const BufferString& projdir, int nsrc, uiParent* parent, const BufferString& execid, PID_Type pid);
    SeismogramMonitor(const BufferString& projdir, int nsrc, uiParent* parent, const BufferString& execid, PID_Type pid, const BufferString& runlogpath);
    ~SeismogramMonitor();

    void startMonitoring();
    void stopMonitoring();
    void showMonitorWindow();
    int getCompletedShots() const { return completed_shots_; }
    int getTotalShots() const { return nsrc_; }

  private:
    BufferString projdir_;
    int nsrc_;
    uiParent* parent_;
    Timer* timer_ = nullptr;
    PID_Type pid_;
    BufferString execution_id_;
    int completed_shots_ = 0;
    uiDialog* progressdlg_ = nullptr;
    uiProgressBar* progressbar_ = nullptr;
    uiLabel* statuslabel_ = nullptr;
    uiLabel* elapsedlabel_ = nullptr;
    uiLabel* logstatuslabel_ = nullptr;
    uiTextEdit* logtextedit_ = nullptr;
    uiPushButton* terminatebtn_ = nullptr;
    uiPushButton* closebtn_ = nullptr;
    time_t start_time_ = 0;
    int tick_count_ = 0;
    int final_elapsed_seconds_ = -1;
    BufferString runlogpath_;
    od_int64 last_log_file_size_ = 0;
    bool had_logfile_ = false;
    bool finish_handled_ = false;
    bool termination_requested_ = false;

    void checkModelingStatus(CallBacker*);
    bool isProcessRunning();
    void showSeismogramViewer();
    int countCompletedShots();
    void createProgressDialog();
    void updateProgress();
    BufferString formatElapsed(int seconds) const;
    void updateStatusText();
    void loadLogContent();
    void terminateProcess(CallBacker*);
    void closeMonitorWindow(CallBacker*);
    void monitorWindowClosed(CallBacker*);
};

class uiLogMonitorDlg : public uiDialog {
  public:
    uiLogMonitorDlg(uiParent*, const BufferString& logpath, PID_Type pid);
    ~uiLogMonitorDlg();

    void startMonitoring();
    void stopMonitoring();

  protected:
    bool acceptOK(CallBacker*) override;

  private:
    BufferString logpath_;
    PID_Type pid_;
    uiTextEdit* logtextedit_ = nullptr;
    uiLabel* statuslabel_ = nullptr;
    Timer* timer_ = nullptr;
    od_int64 last_file_size_ = 0;
    bool had_logfile_ = false;

    void updateLog(CallBacker*);
    void loadLogContent();
    void updateStatusText();
};

class uiSeismogramViewerDlg : public uiDialog {
  public:
    uiSeismogramViewerDlg(uiParent*, const BufferString& projdir, int nsrc);
    ~uiSeismogramViewerDlg();

  protected:
    void createUI();
    void loadSeismogram(CallBacker*);
    void plotSeismogram(CallBacker*);
    void selectSeismogram(CallBacker*);
    bool acceptOK(CallBacker*) override;

  private:
    BufferString projdir_;
    int nsrc_;

    uiListBox* seismogramlist_ = nullptr;
    uiGenInput* selectedfld_ = nullptr;
    uiTextEdit* infobrowser_ = nullptr;
    uiGraphicsView* acquisitionview_ = nullptr;
    uiGraphicsScene* scene_ = nullptr;

    void populateSeismogramList();
    BufferString getSelectedSeismogramPath();
    bool importSeismogramToOpendTect(const BufferString& filepath, const BufferString& seisname);
    bool readModelingParams(int& nrcv, int& ns, float& dt);
    void plotAcquisitionGeometry(int shotidx);
    bool readSrcRcvPositions(int shotidx, Coord3D& srcpos, TypeSet<Coord3D>& rcvpos);
    bool openIn2DViewer(const BufferString& datasetname, const MultiID& datasetmid, const Pos::GeomID& geomid, int nrcv, int ns, float dt_sec);
};

class uiImportConfigDlg : public uiDialog {
  public:
    uiImportConfigDlg(uiParent*, int nrcv, int ns, float dt_ms, const BufferString& datasetname, const BufferString& linename);
    ~uiImportConfigDlg();

    int getNumTraces() const;
    int getNumSamples() const;
    float getSamplingInterval() const;
    BufferString getDatasetName() const;
    BufferString getLineName() const;

  protected:
    bool acceptOK(CallBacker*) override;

  private:
    uiGenInput* nrcvfld_ = nullptr;
    uiGenInput* nsfld_ = nullptr;
    uiGenInput* dtfld_ = nullptr;
    uiGenInput* datasetfld_ = nullptr;
    uiGenInput* linenamefld_ = nullptr;
};
}
