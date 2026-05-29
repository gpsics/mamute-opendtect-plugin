#pragma once

#include "bufstring.h"
#include "bufstringset.h"
#include "genc.h"
#include "uiodmainmod.h"

#include <cmath>

class uiParent;

namespace Mamute {

inline bool isNearlyEqual(float a, float b) {
    return std::fabs(a - b) < 1e-6f;
}

enum class WorkflowType {
    Modeling
};

inline const char* workflowName(WorkflowType wf) {
    switch (wf) {
    case WorkflowType::Modeling: return "modeling";
    }
    return "unknown";
}

inline const char* workflowDisplayName(WorkflowType wf) {
    switch (wf) {
    case WorkflowType::Modeling: return "Viscoacoustic Modeling";
    }
    return "Unknown";
}

struct CommandResult {
    bool success;
    int exitcode;
    BufferString stdout_output;
    BufferString stderr_output;
    BufferString command;
    BufferString working_dir;

    CommandResult()
        : success(false), exitcode(-1) {}

    BufferString summary() const {
        BufferString msg;
        if (success) {
            msg.add("OK [").add(command).add("]");
        } else {
            msg.add("FAILED [").add(command).add("] exit=").add(exitcode);
            if (!stderr_output.isEmpty())
                msg.add("\nstderr: ").add(stderr_output);
        }
        return msg;
    }
};

struct MamuteParams {
    int nx, ny, nz;
    int ns;
    int border;
    int nsrc;

    float dx, dy, dz;

    float fpeak;
    float dt;
    float amplitude;

    BufferString velocity_filename;
    BufferString density_filename;

    BufferString proj_dir;

    int nprocs;

    MamuteParams()
        : nx(101), ny(101), nz(101), ns(401), border(50), nsrc(27), dx(10.0f), dy(10.0f), dz(10.0f), fpeak(20.0f), dt(0.0005f), amplitude(1.0f), velocity_filename("data/velocity_model.bin"), density_filename("data/density_model.bin"), proj_dir("projects/example"), nprocs(0) {}

    virtual ~MamuteParams() {}

    virtual WorkflowType workflowType() const { return WorkflowType::Modeling; }
    virtual const char* configFileName() const { return "modeling.toml"; }

    bool operator==(const MamuteParams& other) const {
        return nx == other.nx && ny == other.ny && nz == other.nz && ns == other.ns && border == other.border && nsrc == other.nsrc && isNearlyEqual(dx, other.dx) && isNearlyEqual(dy, other.dy) && isNearlyEqual(dz, other.dz) && isNearlyEqual(fpeak, other.fpeak) && isNearlyEqual(dt, other.dt) && isNearlyEqual(amplitude, other.amplitude) && velocity_filename == other.velocity_filename;
    }
    bool operator!=(const MamuteParams& other) const { return !(*this == other); }
};

struct ModelingParams : public MamuteParams {
    BufferString qp_filename;
    bool generate_velocity_model;
    bool generate_qp_model;
    bool generate_source_signature;
    bool generate_geometry;

    BufferString source_signature_file;

    ModelingParams()
        : MamuteParams(), qp_filename("data/qp_model.bin"), generate_velocity_model(true), generate_qp_model(true), generate_source_signature(true), generate_geometry(true), source_signature_file("source_signature.bin") {}

    WorkflowType workflowType() const override { return WorkflowType::Modeling; }
    const char* configFileName() const override { return "modeling.toml"; }

    bool operator==(const ModelingParams& other) const {
        return MamuteParams::operator==(other) && qp_filename == other.qp_filename && generate_velocity_model == other.generate_velocity_model && generate_qp_model == other.generate_qp_model && generate_source_signature == other.generate_source_signature && generate_geometry == other.generate_geometry && source_signature_file == other.source_signature_file;
    }
    bool operator!=(const ModelingParams& other) const { return !(*this == other); }
};

class SeismogramMonitor;

class MamuteService {
  public:
    MamuteService(const BufferString& mamutepath);
    ~MamuteService();

    void setMamutePath(const BufferString& path) { mamutepath_ = path; }
    const BufferString& getMamutePath() const { return mamutepath_; }

    bool writeConfigFile(const MamuteParams& params, uiParent* p);

    bool generateSource(const MamuteParams& params, uiParent* p);
    bool generateSrcRcv(const MamuteParams& params, uiParent* p);
    bool generateVelocityModel(const MamuteParams& params, uiParent* p);
    bool generateQpModel(const ModelingParams& params, uiParent* p);

    bool runMamute(const MamuteParams& params, uiParent* p);

    const CommandResult& lastResult() const { return last_result_; }
    PID_Type lastProcessPID() const { return last_pid_; }

    SeismogramMonitor* monitor() { return monitor_; }

    void openLogMonitor(uiParent* p);

  private:
    BufferString mamutepath_;
    CommandResult last_result_;
    PID_Type last_pid_ = mUdf(PID_Type);
    SeismogramMonitor* monitor_ = nullptr;
    BufferString last_run_log_path_;

    bool executeMamuteScript(const BufferString& command,
                             const BufferStringSet& args,
                             const BufferString& workdir,
                             uiParent* p,
                             bool waitfinish = true);

    bool executeMamuteBinary(const BufferString& subcommand,
                             const BufferString& configpath,
                             const MamuteParams& params,
                             uiParent* p);

    BufferString buildModelingConfig(const ModelingParams& params);
    BufferString buildBaseConfig(const MamuteParams& params);

    void writeExecutionLog(const CommandResult& result,
                           const BufferString& projdir);
};

class MamuteWorkflow {
  public:
    MamuteWorkflow(MamuteService& service, uiParent* parent);
    virtual ~MamuteWorkflow();

    virtual WorkflowType type() const = 0;
    const char* name() const { return workflowDisplayName(type()); }

    virtual bool run(const MamuteParams& params);

    MamuteService& service() { return service_; }

  protected:
    MamuteService& service_;
    uiParent* parent_;
};

class ModelingWorkflow : public MamuteWorkflow {
  public:
    ModelingWorkflow(MamuteService& service, uiParent* parent);

    WorkflowType type() const override { return WorkflowType::Modeling; }
};

}
