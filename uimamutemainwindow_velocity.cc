#include "file.h"
#include "filepath.h"
#include "od_istream.h"
#include "uibutton.h"
#include "uifileinput.h"
#include "uigeninput.h"
#include "uigroup.h"
#include "uilabel.h"
#include "uimamutemainwindow.h"
#include "uimamuteuiutils.h"
#include "uimsg.h"
#include "uiseismicsimulator.h"
#include "uiseparator.h"
#include <fstream>
#include <sstream>
#include <vector>
using namespace Mamute;
using namespace MamuteUI;

void uiMamuteMainWindow::createVelocityPage() {
    velocitypage_ = new uiGroup(tabstack_->tabGroup(), "VelocityPage");

    uiLabel* title = new uiLabel(velocitypage_, toUiString("Velocity Model"));
    title->setHSzPol(uiObject::Wide);
    title->setAlignment(Alignment::HCenter);

    uiSeparator* titlesep = new uiSeparator(velocitypage_, "veltitlesep");
    titlesep->attach(stretchedBelow, title);

    BufferString projmsg("Selected project: ");
    projmsg.add(proj_dir_);
    uiLabel* projlbl = new uiLabel(velocitypage_, toUiString(projmsg));
    projlbl->setHSzPol(uiObject::Wide);
    projlbl->attach(alignedBelow, titlesep);

    BufferStringSet veltypes;
    veltypes.add("constant");
    veltypes.add("gaussian_perturbation");
    veltypes.add("layers");
    veltypes.add("square");
    veltypes.add("circle");
    veltypes.add("gradient_vertical");
    veltypes.add("smooth");
    veltypes.add("reduction");
    veltypes.add("slowness_squared");
    veltypes.add("slowness_squared_inverse");

    uiGroup* typerow = new uiGroup(velocitypage_, "veltyperow");
    typerow->attach(alignedBelow, projlbl);
    veltypefld_ = new uiGenInput(typerow, tr("Model type"), StringListInpSpec(veltypes));

    uiSeparator* sep1 = new uiSeparator(velocitypage_, "velsep1");
    sep1->attach(stretchedBelow, typerow);

    velparams_stack_ = new uiHiddenTabStack(velocitypage_, "velparamsstack");
    velparams_stack_->attach(alignedBelow, sep1);

    uiGroup* pg0 = new uiGroup(velparams_stack_->tabGroup(), "velpage0");
    velvfld_ = new uiGenInput(pg0, tr("v (m/s)"), FloatInpSpec(2500.0f));
    velsigmafld_ = new uiGenInput(pg0, tr("sigma (cells)"), FloatInpSpec(5.0f));
    velsigmafld_->attach(rightOf, velvfld_);
    velparams_stack_->addTab(pg0, toUiString("simple_v"));

    uiGroup* pg1_layers = new uiGroup(velparams_stack_->tabGroup(), "velpage1");
    velvlistfld_ = new uiGenInput(pg1_layers, tr("v list (comma separated)"), StringInpSpec("1500,2000,2500"));
    velparams_stack_->addTab(pg1_layers, toUiString("layers"));

    uiGroup* pg1 = new uiGroup(velparams_stack_->tabGroup(), "velpage2");
    velvinfld_ = new uiGenInput(pg1, tr("v_in (m/s)"), FloatInpSpec(3000.0f));
    velvoutfld_ = new uiGenInput(pg1, tr("v_out (m/s)"), FloatInpSpec(2000.0f));
    velvoutfld_->attach(rightOf, velvinfld_);
    velsidefld_ = new uiGenInput(pg1, tr("side (m)"), IntInpSpec(20));
    velsidefld_->attach(rightOf, velvoutfld_);
    velzposfld_ = new uiGenInput(pg1, tr("z_position (cells)"), IntInpSpec(20));
    velzposfld_->attach(rightOf, velsidefld_);
    velparams_stack_->addTab(pg1, toUiString("square"));

    uiGroup* pg2 = new uiGroup(velparams_stack_->tabGroup(), "velpage3");
    velvinfld_circ_ = new uiGenInput(pg2, tr("v_in (m/s)"), FloatInpSpec(3000.0f));
    velvoutfld_circ_ = new uiGenInput(pg2, tr("v_out (m/s)"), FloatInpSpec(2000.0f));
    velvoutfld_circ_->attach(rightOf, velvinfld_circ_);
    velrfld_ = new uiGenInput(pg2, tr("r (m)"), FloatInpSpec(30.0f));
    velrfld_->attach(rightOf, velvoutfld_circ_);
    velparams_stack_->addTab(pg2, toUiString("circle"));

    uiGroup* pg3 = new uiGroup(velparams_stack_->tabGroup(), "velpage4");
    velvstartfld_ = new uiGenInput(pg3, tr("v_start (m/s)"), FloatInpSpec(1000.0f));
    velvendfld_ = new uiGenInput(pg3, tr("v_end (m/s)"), FloatInpSpec(2000.0f));
    velvendfld_->attach(rightOf, velvstartfld_);
    velparams_stack_->addTab(pg3, toUiString("gradient"));

    uiGroup* pg4 = new uiGroup(velparams_stack_->tabGroup(), "velpage5");
    FilePath datadir_smooth(proj_dir_);
    datadir_smooth.add("data");
    velinputvfld_ = new uiFileInput(pg4, tr("Input velocity model (.bin)"), uiFileInput::Setup().filter("Binary files (*.bin)").defseldir(datadir_smooth.fullPath()));
    velinputvfld_->setFileName("velocity_model.bin");
    velsigmafld_smooth_ = new uiGenInput(pg4, tr("sigma (cells)"), FloatInpSpec(5.0f));
    velsigmafld_smooth_->attach(rightOf, velinputvfld_);
    velparams_stack_->addTab(pg4, toUiString("smooth"));

    uiGroup* pg5 = new uiGroup(velparams_stack_->tabGroup(), "velpage6");
    velv1fld_ = new uiGenInput(pg5, tr("v1"), StringInpSpec("velocity_model.bin"));
    velv2fld_ = new uiGenInput(pg5, tr("v2"), StringInpSpec("velocity_model_2.bin"));
    velv2fld_->attach(rightOf, velv1fld_);
    BufferStringSet operations;
    operations.add("add");
    operations.add("mult");
    operations.add("div");
    operations.add("sub");
    operations.add("slowness_squared");
    operations.add("slowness");
    veloperationfld_ = new uiGenInput(pg5, tr("operation"), StringListInpSpec(operations));
    veloperationfld_->attach(rightOf, velv2fld_);
    velparams_stack_->addTab(pg5, toUiString("reduction"));

    uiGroup* pg6 = new uiGroup(velparams_stack_->tabGroup(), "velpage7");
    FilePath datadir_slow(proj_dir_);
    datadir_slow.add("data");
    velinputvfld_slow_ = new uiFileInput(pg6, tr("Input velocity model (.bin)"), uiFileInput::Setup().filter("Binary files (*.bin)").defseldir(datadir_slow.fullPath()));
    velinputvfld_slow_->setFileName("velocity_model.bin");
    velparams_stack_->addTab(pg6, toUiString("slowness"));

    uiSeparator* sep2 = new uiSeparator(velocitypage_, "velsep2");
    sep2->attach(stretchedBelow, velparams_stack_);

    uiGroup* rowbtn = new uiGroup(velocitypage_, "velbtnrow");
    rowbtn->attach(alignedBelow, sep2);
    saveveljsonbtn_ = new uiPushButton(rowbtn, tr("Generate velocity binary"), mCB(this, uiMamuteMainWindow, saveVelocityJsonCB), true);

    velstatuslbl_ = new uiLabel(velocitypage_, toUiString("Select a model type and generate velocity binary."));
    velstatuslbl_->setHSzPol(uiObject::Wide);
    velstatuslbl_->setPrefWidthInChar(90);
    velstatuslbl_->attach(alignedBelow, rowbtn);

    veltypefld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, velocityTypeChangedCB));

    const auto notifyVel = [&](uiGenInput* fld) {
        if (fld) fld->valuechanged.notify(mCB(this, uiMamuteMainWindow, velocityParamsChangedCB));
    };
    notifyVel(velvfld_);
    notifyVel(velsigmafld_);
    notifyVel(velvlistfld_);
    notifyVel(velvinfld_);
    notifyVel(velvoutfld_);
    notifyVel(velsidefld_);
    notifyVel(velzposfld_);
    notifyVel(velvinfld_circ_);
    notifyVel(velvoutfld_circ_);
    notifyVel(velrfld_);
    notifyVel(velvstartfld_);
    notifyVel(velvendfld_);
    notifyVel(velsigmafld_smooth_);
    notifyVel(velv1fld_);
    notifyVel(velv2fld_);
    notifyVel(veloperationfld_);

    updateVelocityTypeUI();

    loadVelocityJson();

    tabstack_->addTab(velocitypage_, toUiString("Velocity"));
}

void uiMamuteMainWindow::updateVelocityTypeUI() {
    if (!veltypefld_ || !velparams_stack_)
        return;

    const BufferString type = veltypefld_->text();

    if (type == "constant" || type == "gaussian_perturbation") {
        velparams_stack_->setCurrentPage(0);
        const bool isgauss = (type == "gaussian_perturbation");
        if (velvfld_) {
            velvfld_->display(true);
            velvfld_->setSensitive(true);
        }
        if (velsigmafld_) {
            velsigmafld_->display(isgauss);
            velsigmafld_->setSensitive(isgauss);
        }
    } else if (type == "layers")
        velparams_stack_->setCurrentPage(1);
    else if (type == "square")
        velparams_stack_->setCurrentPage(2);
    else if (type == "circle")
        velparams_stack_->setCurrentPage(3);
    else if (type == "gradient_vertical")
        velparams_stack_->setCurrentPage(4);
    else if (type == "smooth")
        velparams_stack_->setCurrentPage(5);
    else if (type == "reduction")
        velparams_stack_->setCurrentPage(6);
    else
        velparams_stack_->setCurrentPage(7);

    validateVelocityParams();
}

void uiMamuteMainWindow::velocityParamsChangedCB(CallBacker*) {
    validateVelocityParams();
}

void uiMamuteMainWindow::validateVelocityParams() {
    BufferString json, errmsg;
    const bool ok = buildVelocityJson(json, errmsg);
    if (saveveljsonbtn_)
        saveveljsonbtn_->setSensitive(ok);
    if (velstatuslbl_) {
        if (ok) {
            velstatuslbl_->setText(toUiString("Parameters valid. Click Generate velocity binary."));
            velstatuslbl_->setTextColor(OD::Color::Black());
        } else {
            velstatuslbl_->setText(toUiString(errmsg));
            velstatuslbl_->setTextColor(OD::Color::Red());
        }
    }
}

bool uiMamuteMainWindow::buildVelocityJson(BufferString& jsonout, BufferString& errmsg) const {
    if (!veltypefld_) {
        errmsg = "Velocity page is not initialized.";
        return false;
    }

    const std::string type = veltypefld_->text() ? veltypefld_->text() : "";
    if (type.empty()) {
        errmsg = "Model type is required.";
        return false;
    }

    const auto reqPosFloat = [&](uiGenInput* fld, const char* fieldname, float& out) -> bool {
        if (!fld) {
            errmsg = BufferString("Missing field: ").add(fieldname);
            return false;
        }
        out = fld->getFValue();
        if (out <= 0.0f) {
            errmsg = BufferString(fieldname).add(" must be > 0.");
            return false;
        }
        return true;
    };

    const auto reqPosInt = [&](uiGenInput* fld, const char* fieldname, int& out) -> bool {
        if (!fld) {
            errmsg = BufferString("Missing field: ").add(fieldname);
            return false;
        }
        out = fld->getIntValue();
        if (out <= 0) {
            errmsg = BufferString(fieldname).add(" must be > 0.");
            return false;
        }
        return true;
    };

    const auto reqString = [&](uiGenInput* fld, const char* fieldname, std::string& out) -> bool {
        out = fld && fld->text() ? fld->text() : "";
        const std::size_t start = out.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            errmsg = BufferString(fieldname).add(" is required.");
            return false;
        }
        const std::size_t end = out.find_last_not_of(" \t\r\n");
        out = out.substr(start, end - start + 1);
        return true;
    };

    const auto reqFileName = [&](uiFileInput* fld, const char* fieldname, std::string& out) -> bool {
        out = fld && fld->fileName() ? fld->fileName() : "";
        const std::size_t start = out.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            errmsg = BufferString(fieldname).add(" is required.");
            return false;
        }

        const std::size_t end = out.find_last_not_of(" \t\r\n");
        out = out.substr(start, end - start + 1);

        FilePath filepath(out.c_str());
        BufferString basename = filepath.fileName();
        if (!basename.isEmpty())
            out = basename.buf();

        return true;
    };

    std::ostringstream json_stream;
    json_stream << "{\n";
    json_stream << "    \"type\": \"" << type << "\"";

    if (type == "constant") {
        float velocity = 0.0f;
        if (!reqPosFloat(velvfld_, "v", velocity)) return false;
        json_stream << ",\n    \"v\": " << velocity;
    } else if (type == "gaussian_perturbation") {
        float velocity = 0.0f, sigma = 0.0f;
        if (!reqPosFloat(velvfld_, "v", velocity)) return false;
        if (!reqPosFloat(velsigmafld_, "sigma", sigma)) return false;
        json_stream << ",\n    \"v\": " << velocity;
        json_stream << ",\n    \"sigma\": " << sigma;
    } else if (type == "layers") {
        std::string velocity_list_str;
        if (!reqString(velvlistfld_, "v list", velocity_list_str)) return false;

        std::vector<float> velocities;
        std::stringstream token_stream(velocity_list_str);
        std::string token;
        while (std::getline(token_stream, token, ',')) {
            const std::size_t start = token.find_first_not_of(" \t\r\n");
            if (start == std::string::npos)
                continue;
            const std::size_t end = token.find_last_not_of(" \t\r\n");
            token = token.substr(start, end - start + 1);
            const float velocity = static_cast<float>(atof(token.c_str()));
            if (velocity <= 0.0f) {
                errmsg = "All layers velocities must be > 0.";
                return false;
            }
            velocities.push_back(velocity);
        }

        if (velocities.empty()) {
            errmsg = "Provide at least one velocity in v list.";
            return false;
        }

        json_stream << ",\n    \"v\": [";
        for (size_t i = 0; i < velocities.size(); ++i) {
            if (i) json_stream << ", ";
            json_stream << velocities[i];
        }
        json_stream << "]";
    } else if (type == "square") {
        float vel_in = 0.0f, vel_out = 0.0f;
        int side = 0, z_position = 0;
        if (!reqPosFloat(velvinfld_, "v_in", vel_in)) return false;
        if (!reqPosFloat(velvoutfld_, "v_out", vel_out)) return false;
        if (!reqPosInt(velsidefld_, "side", side)) return false;
        if (!reqPosInt(velzposfld_, "z_position", z_position)) return false;
        json_stream << ",\n    \"v_in\": " << vel_in;
        json_stream << ",\n    \"v_out\": " << vel_out;
        json_stream << ",\n    \"side\": " << side;
        json_stream << ",\n    \"z_position\": " << z_position;
    } else if (type == "circle") {
        float vel_in = 0.0f, vel_out = 0.0f, radius = 0.0f;
        if (!reqPosFloat(velvinfld_circ_, "v_in", vel_in)) return false;
        if (!reqPosFloat(velvoutfld_circ_, "v_out", vel_out)) return false;
        if (!reqPosFloat(velrfld_, "r", radius)) return false;
        json_stream << ",\n    \"v_in\": " << vel_in;
        json_stream << ",\n    \"v_out\": " << vel_out;
        json_stream << ",\n    \"r\": " << radius;
    } else if (type == "gradient_vertical") {
        float vel_start = 0.0f, vel_end = 0.0f;
        if (!reqPosFloat(velvstartfld_, "v_start", vel_start)) return false;
        if (!reqPosFloat(velvendfld_, "v_end", vel_end)) return false;
        json_stream << ",\n    \"v_start\": " << vel_start;
        json_stream << ",\n    \"v_end\": " << vel_end;
    } else if (type == "smooth") {
        std::string input_filename;
        float sigma = 0.0f;
        if (!reqFileName(velinputvfld_, "input model file (v)", input_filename)) return false;
        if (!reqPosFloat(velsigmafld_smooth_, "sigma", sigma)) return false;
        json_stream << ",\n    \"v\": \"" << input_filename << "\"";
        json_stream << ",\n    \"sigma\": " << sigma;
    } else if (type == "reduction") {
        std::string vel1_filename, vel2_filename, operation;
        if (!reqString(velv1fld_, "v1", vel1_filename)) return false;
        if (!reqString(velv2fld_, "v2", vel2_filename)) return false;
        if (!reqString(veloperationfld_, "operation", operation)) return false;

        if (operation != "add" && operation != "mult" && operation != "div" && operation != "sub" && operation != "slowness_squared" && operation != "slowness") {
            errmsg = "Invalid operation. Use add, mult, div, sub, slowness_squared or slowness.";
            return false;
        }

        json_stream << ",\n    \"v1\": \"" << vel1_filename << "\"";
        json_stream << ",\n    \"v2\": \"" << vel2_filename << "\"";
        json_stream << ",\n    \"operation\": \"" << operation << "\"";
    } else if (type == "slowness_squared" || type == "slowness_squared_inverse") {
        std::string input_filename;
        if (!reqFileName(velinputvfld_slow_, "input model file (v)", input_filename)) return false;
        json_stream << ",\n    \"v\": \"" << input_filename << "\"";
    } else {
        errmsg = "Unsupported model type.";
        return false;
    }

    json_stream << "\n}";

    jsonout = BufferString(json_stream.str().c_str());
    return true;
}

void uiMamuteMainWindow::velocityTypeChangedCB(CallBacker*) {
    updateVelocityTypeUI();
}

void uiMamuteMainWindow::saveVelocityJsonCB(CallBacker*) {
    BufferString json;
    BufferString errmsg;
    if (!buildVelocityJson(json, errmsg)) {
        if (velstatuslbl_) {
            velstatuslbl_->setText(toUiString(errmsg));
            velstatuslbl_->setTextColor(OD::Color::Red());
        }
        uiMSG().error(toUiString(errmsg));
        return;
    }

    FilePath fp(proj_dir_);
    fp.add("vel.json");
    const BufferString outpath = fp.fullPath();

    FilePath binpath(proj_dir_);
    binpath.add("data").add("velocity_model.bin");
    if (File::exists(binpath.fullPath())) {
        BufferString msg("This will overwrite the existing velocity model binary:\n");
        msg.add(" - data/velocity_model.bin\n\n");
        msg.add("Do you want to continue?");
        if (!uiMSG().askGoOn(toUiString(msg)))
            return;
    }

    if (!File::exists(proj_dir_) && !File::createDir(proj_dir_)) {
        const BufferString msg = BufferString("Could not create project directory: ").add(proj_dir_);
        if (velstatuslbl_) {
            velstatuslbl_->setText(toUiString(msg));
            velstatuslbl_->setTextColor(OD::Color::Red());
        }
        uiMSG().error(toUiString(msg));
        return;
    }

    std::ofstream ofs(outpath.buf(), std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        const BufferString msg = BufferString("Could not write ").add(outpath);
        if (velstatuslbl_) {
            velstatuslbl_->setText(toUiString(msg));
            velstatuslbl_->setTextColor(OD::Color::Red());
        }
        uiMSG().error(toUiString(msg));
        return;
    }

    ofs << json.buf();
    ofs.close();

    service_->setMamutePath(getMamuteInstallPath());

    ModelingParams writeparams;
    if (!buildParamsFromUI(writeparams, false) || !service_->writeConfigFile(writeparams, this)) {
        const BufferString msg("Could not prepare modeling.toml required to generate velocity binary. Check Modeling parameters and run Build once.");
        if (velstatuslbl_) {
            velstatuslbl_->setText(toUiString(msg));
            velstatuslbl_->setTextColor(OD::Color::Red());
        }
        return;
    }

    ModelingParams genparams;
    genparams.proj_dir = proj_dir_;
    if (!service_->generateVelocityModel(genparams, this))
        return;

    const BufferString okmsg = BufferString("Generated data/velocity_model.bin (updated ").add(outpath).add(")");
    if (velstatuslbl_) {
        velstatuslbl_->setText(toUiString(okmsg));
        velstatuslbl_->setTextColor(OD::Color(0, 140, 0));
    }
}

void uiMamuteMainWindow::loadVelocityJson() {
    FilePath fp(proj_dir_);
    fp.add("vel.json");
    if (!File::exists(fp.fullPath()))
        return;

    od_istream strm(fp.fullPath());
    if (!strm.isOK())
        return;

    std::string json;
    BufferString line;
    while (strm.getLine(line)) {
        json += line.buf();
        json += '\n';
    }

    const std::string type = jsonStringVal(json, "type");
    if (type.empty())
        return;

    if (veltypefld_) {
        veltypefld_->setText(type.c_str());
        updateVelocityTypeUI();
    }

    if (type == "constant") {
        if (velvfld_) velvfld_->setValue(jsonFloatVal(json, "v", 2500.0f));
    } else if (type == "gaussian_perturbation") {
        if (velvfld_) velvfld_->setValue(jsonFloatVal(json, "v", 2500.0f));
        if (velsigmafld_) velsigmafld_->setValue(jsonFloatVal(json, "sigma", 5.0f));
    } else if (type == "layers") {
        const std::string vlist = jsonStringVal(json, "v");
        if (!vlist.empty() && velvlistfld_) velvlistfld_->setText(vlist.c_str());
    } else if (type == "square") {
        if (velvinfld_) velvinfld_->setValue(jsonFloatVal(json, "v_in", 3000.0f));
        if (velvoutfld_) velvoutfld_->setValue(jsonFloatVal(json, "v_out", 2000.0f));
        if (velsidefld_) velsidefld_->setValue(jsonIntVal(json, "side", 20));
        if (velzposfld_) velzposfld_->setValue(jsonIntVal(json, "z_position", 20));
    } else if (type == "circle") {
        if (velvinfld_circ_) velvinfld_circ_->setValue(jsonFloatVal(json, "v_in", 3000.0f));
        if (velvoutfld_circ_) velvoutfld_circ_->setValue(jsonFloatVal(json, "v_out", 2000.0f));
        if (velrfld_) velrfld_->setValue(jsonFloatVal(json, "r", 30.0f));
    } else if (type == "gradient_vertical") {
        if (velvstartfld_) velvstartfld_->setValue(jsonFloatVal(json, "v_start", 1000.0f));
        if (velvendfld_) velvendfld_->setValue(jsonFloatVal(json, "v_end", 2000.0f));
    } else if (type == "smooth") {
        const std::string input_filename = jsonStringVal(json, "v");
        if (!input_filename.empty() && velinputvfld_) velinputvfld_->setFileName(input_filename.c_str());
        if (velsigmafld_smooth_) velsigmafld_smooth_->setValue(jsonFloatVal(json, "sigma", 5.0f));
    } else if (type == "reduction") {
        const std::string vel1_filename = jsonStringVal(json, "v1");
        const std::string vel2_filename = jsonStringVal(json, "v2");
        const std::string operation = jsonStringVal(json, "operation");
        if (!vel1_filename.empty() && velv1fld_) velv1fld_->setText(vel1_filename.c_str());
        if (!vel2_filename.empty() && velv2fld_) velv2fld_->setText(vel2_filename.c_str());
        if (!operation.empty() && veloperationfld_) veloperationfld_->setText(operation.c_str());
    } else if (type == "slowness_squared" || type == "slowness_squared_inverse") {
        const std::string input_filename = jsonStringVal(json, "v");
        if (!input_filename.empty() && velinputvfld_slow_) velinputvfld_slow_->setFileName(input_filename.c_str());
    }

    if (velstatuslbl_) {
        velstatuslbl_->setText(toUiString("Loaded parameters from existing vel.json."));
    }
}
