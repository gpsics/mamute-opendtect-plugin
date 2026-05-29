#include "mamuteworkflow.h"
#include "bufstring.h"
#include "bufstringset.h"
#include "file.h"
#include "filepath.h"
#include "od_istream.h"
#include "od_ostream.h"
#include "oscommand.h"
#include "uimsg.h"
#include "uiseismogramviewer.h"
#include "uistring.h"
#include <ctime>

using namespace Mamute;

MamuteService::MamuteService(const BufferString& mamutepath)
    : mamutepath_(mamutepath) {
}

MamuteService::~MamuteService() {
    if (monitor_) {
        monitor_->stopMonitoring();
        delete monitor_;
    }
}

BufferString MamuteService::buildBaseConfig(const MamuteParams& params) {
    BufferString content;
    content.add("nx = ").add(params.nx).add("\n");
    content.add("ny = ").add(params.ny).add("\n");
    content.add("nz = ").add(params.nz).add("\n");
    content.add("ns = ").add(params.ns).add("\n");
    content.add("border = ").add(params.border).add("\n");
    content.add("dx = ").add(params.dx).add("\n");
    content.add("dy = ").add(params.dy).add("\n");
    content.add("dz = ").add(params.dz).add("\n");
    content.add("velocity_filename = \"").add(params.velocity_filename).add("\"\n");
    if (!params.density_filename.isEmpty())
        content.add("density_filename = \"").add(params.density_filename).add("\"\n");
    content.add("fpeak = ").add(params.fpeak).add("\n");
    content.add("dt = ").add(params.dt).add("\n");
    content.add("amplitude = ").add(params.amplitude).add("\n");
    content.add("n_src = ").add(params.nsrc).add("\n");
    content.add("proj_dir = \"").add(params.proj_dir).add("\"\n");
    return content;
}

BufferString MamuteService::buildModelingConfig(const ModelingParams& params) {
    BufferString content = buildBaseConfig(params);
    content.add("qp_filename = \"").add(params.qp_filename).add("\"\n");
    return content;
}

bool MamuteService::writeConfigFile(const MamuteParams& params, uiParent* p) {
    BufferString content;
    switch (params.workflowType()) {
    case WorkflowType::Modeling:
        content = buildModelingConfig(static_cast<const ModelingParams&>(params));
        break;
    }

    FilePath fp(params.proj_dir);
    fp.add(params.configFileName());
    BufferString configpath = fp.fullPath();

    if (File::exists(configpath))
        File::remove(configpath);

    od_ostream strm(configpath);
    if (!strm.isOK()) {
        BufferString errmsg("Could not write config file: ");
        errmsg.add(configpath);
        uiMSG().error(toUiString(errmsg));
        return false;
    }

    strm << content;
    strm.close();
    return true;
}

bool MamuteService::executeMamuteScript(const BufferString& command, const BufferStringSet& args, const BufferString& workdir, uiParent* p, bool waitfinish) {
    last_result_ = CommandResult();
    last_result_.command = command;

    FilePath mamutescript(mamutepath_);
    mamutescript.add("scripts").add("mamute.py");
    BufferString scriptpath = mamutescript.fullPath();

    if (!File::exists(scriptpath)) {
        BufferString errmsg("Mamute script not found: ");
        errmsg.add(scriptpath);
        uiMSG().error(toUiString(errmsg));
        return false;
    }

    FilePath venvpython(mamutepath_);
    venvpython.add("install").add("scripts").add("_venv").add("bin").add("python3");
    const BufferString pythonpath = venvpython.fullPath();
    const bool venvok = File::exists(pythonpath);

    OS::MachineCommand mc(venvok ? pythonpath : BufferString("python3"));
    mc.addArg(scriptpath);
    mc.addArg(command);
    mc.addArg("--");
    for (int i = 0; i < args.size(); i++)
        mc.addArg(args.get(i));

    OS::CommandExecPars execpars(waitfinish ? OS::Wait4Finish : OS::RunInBG);
    execpars.workingdir_ = workdir.isEmpty() ? mamutepath_ : workdir;

    static int cmdcounter = 0;
    cmdcounter++;
    BufferString suffix;
    suffix.add((od_int64)time(nullptr)).add("_").add(cmdcounter);

    FilePath stdoutfp(File::getTempPath());
    stdoutfp.add(BufferString("mamute_stdout_").add(suffix).add(".txt"));
    FilePath stderrfp(File::getTempPath());
    stderrfp.add(BufferString("mamute_stderr_").add(suffix).add(".txt"));

    const BufferString stdoutfnm = stdoutfp.fullPath();
    const BufferString stderrfnm = stderrfp.fullPath();

    execpars.stdoutfnm_ = stdoutfnm;
    execpars.stderrfnm_ = stderrfnm;

    OS::CommandLauncher cl(mc);
    bool execok = cl.execute(execpars);
    int exitcode = cl.exitCode();

    if (File::exists(stdoutfnm)) {
        od_istream istdout(stdoutfnm);
        if (istdout.isOK()) {
            BufferString line;
            while (istdout.getLine(line))
                last_result_.stdout_output.add(line).add("\n");
        }
        File::remove(stdoutfnm);
    }

    if (File::exists(stderrfnm)) {
        od_istream istderr(stderrfnm);
        if (istderr.isOK()) {
            BufferString line;
            while (istderr.getLine(line))
                last_result_.stderr_output.add(line).add("\n");
        }
        File::remove(stderrfnm);
    }

    last_result_.exitcode = exitcode;
    last_result_.success = execok && exitcode == 0;
    last_result_.working_dir = execpars.workingdir_;

    if (!last_result_.success) {
        BufferString errmsg("=== EXECUTION FAILED ===\n");
        errmsg.add("Script: ").add(scriptpath).add("\n");
        errmsg.add("Command: ").add(command).add("\n");
        errmsg.add("Arguments: ");
        for (int i = 0; i < args.size(); i++)
            errmsg.add(args.get(i)).add(" ");
        errmsg.add("\nWorking directory: ").add(last_result_.working_dir).add("\n");
        errmsg.add("Exit code: ").add(exitcode);

        if (!last_result_.stderr_output.isEmpty())
            errmsg.add("\n\n=== STDERR ===\n").add(last_result_.stderr_output);
        if (!last_result_.stdout_output.isEmpty())
            errmsg.add("\n\n=== STDOUT ===\n").add(last_result_.stdout_output);

        uiMSG().error(toUiString(errmsg));
    }

    writeExecutionLog(last_result_, last_result_.working_dir);
    return last_result_.success;
}

bool MamuteService::executeMamuteBinary(const BufferString& subcommand,
                                        const BufferString& configpath,
                                        const MamuteParams& params,
                                        uiParent* p) {
    if (!File::exists(params.proj_dir)) {
        BufferString errmsg("Project directory not found. Please prepare the project first.\n\nExpected: ");
        errmsg.add(params.proj_dir);
        uiMSG().error(toUiString(errmsg));
        return false;
    }

    if (!File::exists(configpath)) {
        BufferString errmsg("Config file not found: ");
        errmsg.add(configpath);
        uiMSG().error(toUiString(errmsg));
        return false;
    }

    FilePath exefp(mamutepath_);
    exefp.add("install").add("bin").add("mamute");
    BufferString exepath = exefp.fullPath();

    if (!File::exists(exepath)) {
        BufferString errmsg("Mamute binary not found: ");
        errmsg.add(exepath);
        uiMSG().error(toUiString(errmsg));
        return false;
    }

    OS::MachineCommand mc;

    if (params.nprocs > 1) {
        mc.setProgram("mpirun");
        mc.addArg("-n");
        mc.addArg(BufferString().add(params.nprocs));
        mc.addArg(exepath);
    } else {
        mc.setProgram(exepath);
    }

    mc.addArg(subcommand);
    mc.addArg(configpath);

    OS::CommandExecPars execpars(OS::RunInBG);
    execpars.workingdir_ = mamutepath_;

    FilePath logfp(params.proj_dir);
    BufferString logname(subcommand);
    logname.add("_log.txt");
    logfp.add(logname);
    execpars.stdoutfnm_ = logfp.fullPath();
    execpars.stderrfnm_ = logfp.fullPath();

    OS::CommandLauncher cl(mc);

    if (!cl.execute(execpars)) {
        BufferString errmsg("Failed to start: mamute ");
        errmsg.add(subcommand);
        uiMSG().error(toUiString(errmsg));
        return false;
    }

    last_pid_ = cl.processID();
    return true;
}

bool MamuteService::generateSource(const MamuteParams& params, uiParent* p) {
    FilePath cfgfp(params.proj_dir);
    cfgfp.add(params.configFileName());

    BufferStringSet args;
    args.add("--config");
    args.add(cfgfp.fullPath());
    args.add("-a");
    args.add(BufferString().add((int)params.amplitude));

    return executeMamuteScript("data.generate_source", args, mamutepath_, p);
}

bool MamuteService::generateSrcRcv(const MamuteParams& params, uiParent* p) {
    FilePath cfgfp(params.proj_dir);
    cfgfp.add(params.configFileName());

    BufferStringSet args;
    args.add("--config");
    args.add(cfgfp.fullPath());

    return executeMamuteScript("data.generate_src_rcv", args, mamutepath_, p);
}

bool MamuteService::generateVelocityModel(const MamuteParams& params, uiParent* p) {
    FilePath cfgfp(params.proj_dir);
    cfgfp.add(params.configFileName());

    FilePath jsonfp(params.proj_dir);
    jsonfp.add("vel.json");

    BufferStringSet args;
    args.add("--config");
    args.add(cfgfp.fullPath());
    args.add("--json");
    args.add(jsonfp.fullPath());

    return executeMamuteScript("data.generate_model_velocity", args, mamutepath_, p);
}

bool MamuteService::generateQpModel(const ModelingParams& params, uiParent* p) {
    FilePath cfgfp(params.proj_dir);
    cfgfp.add(params.configFileName());

    BufferStringSet args;
    args.add("--config");
    args.add(cfgfp.fullPath());

    return executeMamuteScript("data.generate_model_qp", args, mamutepath_, p);
}

bool MamuteService::runMamute(const MamuteParams& params, uiParent* p) {
    const BufferString subcommand = workflowName(params.workflowType());

    if (!File::exists(params.proj_dir)) {
        BufferString errmsg("Project directory not found. Please run Build first.\n\nExpected: ");
        errmsg.add(params.proj_dir);
        uiMSG().error(toUiString(errmsg));
        return false;
    }

    FilePath cfgfp(params.proj_dir);
    cfgfp.add(params.configFileName());
    if (!File::exists(cfgfp.fullPath())) {
        BufferString errmsg("Config file not found. Please run Build before starting the simulation.\n\nExpected: ");
        errmsg.add(cfgfp.fullPath());
        uiMSG().error(toUiString(errmsg));
        return false;
    }

    FilePath exefp(mamutepath_);
    exefp.add("install").add("bin").add("mamute");
    const BufferString exepath = exefp.fullPath();
    if (!File::exists(exepath)) {
        BufferString errmsg("Mamute binary not found: ");
        errmsg.add(exepath);
        uiMSG().error(toUiString(errmsg));
        return false;
    }

    time_t now = time(nullptr);
    struct tm tstruct;
    localtime_r(&now, &tstruct);
    char execid[80];
    strftime(execid, sizeof(execid), "%Y%m%d%H%M%S", &tstruct);
    const BufferString execution_id(execid);

    FilePath runsdir(params.proj_dir);
    runsdir.add("runs");
    if (!File::exists(runsdir.fullPath()))
        File::createDir(runsdir.fullPath());

    FilePath runfp(runsdir.fullPath());
    runfp.add(BufferString("run_").add(execution_id));
    if (!File::exists(runfp.fullPath()))
        File::createDir(runfp.fullPath());

    FilePath runlogfp(runfp.fullPath());
    runlogfp.add("modeling.log");
    {
        od_ostream touchlog(runlogfp.fullPath());
        if (touchlog.isOK()) touchlog.close();
    }
    last_run_log_path_ = runlogfp.fullPath();

    FilePath runscriptfp(runfp.fullPath());
    runscriptfp.add("run_modeling.sh");
    {
        od_ostream scriptstrm(runscriptfp.fullPath());
        if (!scriptstrm.isOK()) {
            uiMSG().error(toUiString("Failed to create run script."));
            return false;
        }
        scriptstrm << "#!/bin/sh" << od_newline;
        scriptstrm << "child_pid=0" << od_newline;
        scriptstrm << "term_handler() {" << od_newline;
        scriptstrm << "  if [ \"$child_pid\" -gt 0 ] 2>/dev/null; then" << od_newline;
        scriptstrm << "    kill -TERM \"$child_pid\" 2>/dev/null" << od_newline;
        scriptstrm << "  fi" << od_newline;
        scriptstrm << "  exit 143" << od_newline;
        scriptstrm << "}" << od_newline;
        scriptstrm << "trap term_handler TERM INT" << od_newline;
        scriptstrm << "'" << exepath << "' " << subcommand
                   << " '" << cfgfp.fullPath()
                   << "' >> '" << last_run_log_path_ << "' 2>&1 &" << od_newline;
        scriptstrm << "child_pid=$!" << od_newline;
        scriptstrm << "wait \"$child_pid\"" << od_newline;
        scriptstrm << "exit $?" << od_newline;
        scriptstrm.close();
    }

    OS::MachineCommand mc;
    mc.setProgram("sh");
    mc.addArg(runscriptfp.fullPath());

    OS::CommandExecPars execpars(OS::RunInBG);
    execpars.workingdir_ = mamutepath_;

    OS::CommandLauncher cl(mc);
    if (!cl.execute(execpars)) {
        BufferString errmsg("Failed to start: mamute ");
        errmsg.add(subcommand);
        uiMSG().error(toUiString(errmsg));
        return false;
    }
    last_pid_ = cl.processID();

    FilePath metafp(runfp.fullPath());
    metafp.add("run.info");
    od_ostream metastrm(metafp.fullPath());
    if (metastrm.isOK()) {
        metastrm << "execution_id=" << execution_id << od_newline;
        metastrm << "pid=" << last_pid_ << od_newline;
        metastrm << "started_at=" << execid << od_newline;
        metastrm << "subcommand=" << subcommand << od_newline;
        metastrm << "log_file=" << last_run_log_path_ << od_newline;
        metastrm.close();
    }

    if (monitor_) {
        monitor_->stopMonitoring();
        delete monitor_;
    }
    monitor_ = new SeismogramMonitor(params.proj_dir, params.nsrc, p,
                                     execution_id, last_pid_,
                                     last_run_log_path_);
    monitor_->startMonitoring();

    return true;
}

void MamuteService::writeExecutionLog(const CommandResult& result,
                                      const BufferString& projdir) {
    if (projdir.isEmpty())
        return;

    FilePath logdir(projdir);
    logdir.add("logs");
    if (!File::exists(logdir.fullPath()))
        File::createDir(logdir.fullPath());

    time_t now = time(nullptr);
    struct tm tstruct;
    localtime_r(&now, &tstruct);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y%m%d_%H%M%S", &tstruct);

    BufferString logname(timebuf);
    logname.add("_").add(result.command).add(".log");

    FilePath logfp(logdir);
    logfp.add(logname);

    od_ostream logstrm(logfp.fullPath());
    if (!logstrm.isOK())
        return;

    logstrm << "=== Mamute Execution Log ===" << od_newline;
    logstrm << "Timestamp: " << timebuf << od_newline;
    logstrm << "Command: " << result.command << od_newline;
    logstrm << "Exit code: " << result.exitcode << od_newline;
    logstrm << "Success: " << (result.success ? "yes" : "no") << od_newline;
    logstrm << "Working dir: " << result.working_dir << od_newline;

    if (!result.stdout_output.isEmpty()) {
        logstrm << od_newline << "=== STDOUT ===" << od_newline;
        logstrm << result.stdout_output;
    }

    if (!result.stderr_output.isEmpty()) {
        logstrm << od_newline << "=== STDERR ===" << od_newline;
        logstrm << result.stderr_output;
    }

    logstrm.close();
}

void MamuteService::openLogMonitor(uiParent* p) {
    if (monitor_) {
        monitor_->showMonitorWindow();
        return;
    }

    if (last_run_log_path_.isEmpty()) {
        uiMSG().warning(toUiString(
            "No modeling run found yet.\n"
            "Click 'Run' first, then use 'Monitor' to view progress."));
        return;
    }

    uiMSG().warning(toUiString(
        "The monitor window is no longer active.\n"
        "Run the simulation again to start a new monitor session."));
}

MamuteWorkflow::MamuteWorkflow(MamuteService& service, uiParent* parent)
    : service_(service), parent_(parent) {
}

MamuteWorkflow::~MamuteWorkflow() {}

bool MamuteWorkflow::run(const MamuteParams& params) {
    return service_.runMamute(params, parent_);
}

ModelingWorkflow::ModelingWorkflow(MamuteService& service, uiParent* parent)
    : MamuteWorkflow(service, parent) {
}
