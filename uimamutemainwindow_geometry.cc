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
using namespace Mamute;
using namespace MamuteUI;

void uiMamuteMainWindow::updateGeometryTypeUI() {
    const BufferString srctype = srcgeotypefld_ ? BufferString(srcgeotypefld_->text()) : BufferString("grid");
    const BufferString rcvtype = rcvgeotypefld_ ? BufferString(rcvgeotypefld_->text()) : BufferString("grid");

    if (srcparams_stack_)
        srcparams_stack_->setCurrentPage(srctype == "circular" ? 1 : 0);

    if (rcvparams_stack_)
        rcvparams_stack_->setCurrentPage(rcvtype == "circular" ? 1 : 0);

    updateGeometrySummary();
    validateGeometryParams();
}

void uiMamuteMainWindow::validateGeometryParams() {
    BufferString json, errmsg;
    const bool ok = buildGeometryJson(json, errmsg);
    if (savesrcjsonbtn_)
        savesrcjsonbtn_->setSensitive(ok);
    if (geostatuslbl_) {
        if (ok) {
            geostatuslbl_->setText(toUiString("Parameters valid. Click Generate geometry binaries."));
            geostatuslbl_->setTextColor(OD::Color::Black());
        } else {
            geostatuslbl_->setText(toUiString(errmsg));
            geostatuslbl_->setTextColor(OD::Color::Red());
        }
    }
}

void uiMamuteMainWindow::updateGeometrySummary() {
    if (!geosummarylbl_)
        return;

    const auto countgrid = [](float first, float last, float delta) -> int {
        if (delta <= 0.0f || last <= first)
            return -1;

        const float span = last - first;
        const int n = static_cast<int>(std::ceil(span / delta));
        return n > 0 ? n : -1;
    };

    int nsrc = -1;
    int nrcv = -1;

    const BufferString srctype = srcgeotypefld_ ? BufferString(srcgeotypefld_->text()) : BufferString("grid");
    if (srctype == "circular") {
        const int n = srcnpointsfld_ ? srcnpointsfld_->getIntValue() : -1;
        nsrc = n > 0 ? n : -1;
    } else {
        const int nx = countgrid(srcxfirstfld_->getFValue(), srcxlastfld_->getFValue(), srcxdeltafld_->getFValue());
        const int ny = countgrid(srcyfirstfld_->getFValue(), srcylastfld_->getFValue(), srcydeltafld_->getFValue());
        const int nz = countgrid(srczfirstfld_->getFValue(), srczlastfld_->getFValue(), srczdeltafld_->getFValue());
        if (nx > 0 && ny > 0 && nz > 0)
            nsrc = nx * ny * nz;
    }

    const BufferString rcvtype = rcvgeotypefld_ ? BufferString(rcvgeotypefld_->text()) : BufferString("grid");
    if (rcvtype == "circular") {
        const int n = rcvnpointsfld_ ? rcvnpointsfld_->getIntValue() : -1;
        nrcv = n > 0 ? n : -1;
    } else {
        const int nx = countgrid(rcvxfirstfld_->getFValue(), rcvxlastfld_->getFValue(), rcvxdeltafld_->getFValue());
        const int ny = countgrid(rcvyfirstfld_->getFValue(), rcvylastfld_->getFValue(), rcvydeltafld_->getFValue());
        const int nz = countgrid(rcvzfirstfld_->getFValue(), rcvzlastfld_->getFValue(), rcvzdeltafld_->getFValue());
        if (nx > 0 && ny > 0 && nz > 0)
            nrcv = nx * ny * nz;
    }

    BufferString txt("Sources=");
    txt.add(nsrc > 0 ? BufferString(nsrc) : BufferString("invalid"));
    txt.add(", Receivers=");
    txt.add(nrcv > 0 ? BufferString(nrcv) : BufferString("invalid"));
    geosummarylbl_->setText(toUiString(txt));
}

bool uiMamuteMainWindow::buildGeometryJson(BufferString& jsonout, BufferString& errmsg) const {
    if (!srcgeotypefld_ || !rcvgeotypefld_) {
        errmsg = "Geometry page is not initialized.";
        return false;
    }

    const std::string srctype = srcgeotypefld_->text() ? srcgeotypefld_->text() : "";
    const std::string rcvtype = rcvgeotypefld_->text() ? rcvgeotypefld_->text() : "";

    const auto validategrid = [&](const char* prefix,
                                  uiGenInput* xfirst, uiGenInput* xlast, uiGenInput* xdelta,
                                  uiGenInput* yfirst, uiGenInput* ylast, uiGenInput* ydelta,
                                  uiGenInput* zfirst, uiGenInput* zlast, uiGenInput* zdelta) -> bool {
        const float xf = xfirst->getFValue();
        const float xl = xlast->getFValue();
        const float xd = xdelta->getFValue();
        const float yf = yfirst->getFValue();
        const float yl = ylast->getFValue();
        const float yd = ydelta->getFValue();
        const float zf = zfirst->getFValue();
        const float zl = zlast->getFValue();
        const float zd = zdelta->getFValue();

        if (xd <= 0.0f || yd <= 0.0f || zd <= 0.0f) {
            errmsg = BufferString(prefix).add(" delta must be > 0.");
            return false;
        }
        if (xl <= xf || yl <= yf || zl <= zf) {
            errmsg = BufferString(prefix).add(" requires last > first on x/y/z.");
            return false;
        }
        return true;
    };

    const auto validatecircular = [&](const char* prefix, uiGenInput* radiusfld, uiGenInput* npointsfld, uiGenInput* depthfld) -> bool {
        const float radius = radiusfld->getFValue();
        const int npoints = npointsfld->getIntValue();
        const float depth = depthfld->getFValue();

        if (radius <= 0.0f) {
            errmsg = BufferString(prefix).add(" radius must be > 0.");
            return false;
        }
        if (npoints <= 0) {
            errmsg = BufferString(prefix).add(" number of points must be > 0.");
            return false;
        }
        if (depth < 0.0f) {
            errmsg = BufferString(prefix).add(" depth must be >= 0.");
            return false;
        }
        return true;
    };

    if (srctype == "grid") {
        if (!validategrid("Sources", srcxfirstfld_, srcxlastfld_, srcxdeltafld_, srcyfirstfld_, srcylastfld_, srcydeltafld_, srczfirstfld_, srczlastfld_, srczdeltafld_))
            return false;
    } else if (srctype == "circular") {
        if (!validatecircular("Sources", srcradiusfld_, srcnpointsfld_, srcdepthfld_))
            return false;
    } else {
        errmsg = "Invalid sources geometry type.";
        return false;
    }

    if (rcvtype == "grid") {
        if (!validategrid("Receivers", rcvxfirstfld_, rcvxlastfld_, rcvxdeltafld_, rcvyfirstfld_, rcvylastfld_, rcvydeltafld_, rcvzfirstfld_, rcvzlastfld_, rcvzdeltafld_))
            return false;
    } else if (rcvtype == "circular") {
        if (!validatecircular("Receivers", rcvradiusfld_, rcvnpointsfld_, rcvdepthfld_))
            return false;
    } else {
        errmsg = "Invalid receivers geometry type.";
        return false;
    }

    std::ostringstream json_stream;
    json_stream << "{\n";

    if (srctype == "grid") {
        json_stream << "  \"sources\": {\n";
        json_stream << "    \"x\": {\"first\": " << srcxfirstfld_->getFValue()
                    << ", \"last\": " << srcxlastfld_->getFValue()
                    << ", \"delta\": " << srcxdeltafld_->getFValue() << "},\n";
        json_stream << "    \"y\": {\"first\": " << srcyfirstfld_->getFValue()
                    << ", \"last\": " << srcylastfld_->getFValue()
                    << ", \"delta\": " << srcydeltafld_->getFValue() << "},\n";
        json_stream << "    \"z\": {\"first\": " << srczfirstfld_->getFValue()
                    << ", \"last\": " << srczlastfld_->getFValue()
                    << ", \"delta\": " << srczdeltafld_->getFValue() << "}\n";
        json_stream << "  },\n";
    } else {
        json_stream << "  \"sources\": {\n";
        json_stream << "    \"circular\": {\n";
        json_stream << "      \"center\": [" << srccenterxfld_->getFValue() << ", " << srccenteryfld_->getFValue() << "],\n";
        json_stream << "      \"radius\": " << srcradiusfld_->getFValue() << ",\n";
        json_stream << "      \"n_receivers\": " << srcnpointsfld_->getIntValue() << ",\n";
        json_stream << "      \"depth\": " << srcdepthfld_->getFValue() << "\n";
        json_stream << "    }\n";
        json_stream << "  },\n";
    }

    if (rcvtype == "grid") {
        json_stream << "  \"receivers\": {\n";
        json_stream << "    \"x\": {\"first\": " << rcvxfirstfld_->getFValue()
                    << ", \"last\": " << rcvxlastfld_->getFValue()
                    << ", \"delta\": " << rcvxdeltafld_->getFValue() << "},\n";
        json_stream << "    \"y\": {\"first\": " << rcvyfirstfld_->getFValue()
                    << ", \"last\": " << rcvylastfld_->getFValue()
                    << ", \"delta\": " << rcvydeltafld_->getFValue() << "},\n";
        json_stream << "    \"z\": {\"first\": " << rcvzfirstfld_->getFValue()
                    << ", \"last\": " << rcvzlastfld_->getFValue()
                    << ", \"delta\": " << rcvzdeltafld_->getFValue() << "}\n";
        json_stream << "  }\n";
    } else {
        json_stream << "  \"receivers\": {\n";
        json_stream << "    \"circular\": {\n";
        json_stream << "      \"center\": [" << rcvcenterxfld_->getFValue() << ", " << rcvcenteryfld_->getFValue() << "],\n";
        json_stream << "      \"radius\": " << rcvradiusfld_->getFValue() << ",\n";
        json_stream << "      \"n_receivers\": " << rcvnpointsfld_->getIntValue() << ",\n";
        json_stream << "      \"depth\": " << rcvdepthfld_->getFValue() << "\n";
        json_stream << "    }\n";
        json_stream << "  }\n";
    }

    json_stream << "}\n";

    jsonout = BufferString(json_stream.str().c_str());
    return true;
}

void uiMamuteMainWindow::geometryTypeChangedCB(CallBacker*) {
    updateGeometryTypeUI();
}

void uiMamuteMainWindow::geometryParamsChangedCB(CallBacker*) {
    updateGeometrySummary();
    validateGeometryParams();
}

void uiMamuteMainWindow::saveGeometryJsonCB(CallBacker*) {
    BufferString json;
    BufferString errmsg;
    if (!buildGeometryJson(json, errmsg)) {
        if (geostatuslbl_) {
            geostatuslbl_->setText(toUiString(errmsg));
            geostatuslbl_->setTextColor(OD::Color::Red());
        }
        uiMSG().error(toUiString(errmsg));
        return;
    }

    FilePath fp(proj_dir_);
    fp.add("src_rcv.json");
    const BufferString outpath = fp.fullPath();

    FilePath coordsdir(proj_dir_);
    coordsdir.add("coords");
    FilePath srcbin(coordsdir.fullPath());
    srcbin.add("src_coord.bin");
    const bool srcexists = File::exists(srcbin.fullPath());

    BufferStringSet rcvfiles;
    if (File::isDirectory(coordsdir.fullPath())) {
        BufferStringSet allfiles;
        File::listDir(coordsdir.fullPath(), File::DirListType::FilesInDir, allfiles);
        for (int i = 0; i < allfiles.size(); i++) {
            const BufferString& filename = allfiles.get(i);
            if (filename.startsWith("rcv_coord_") && filename.endsWith(".bin"))
                rcvfiles.add(filename);
        }
    }
    const bool rcvexists = !rcvfiles.isEmpty();

    if (srcexists || rcvexists) {
        BufferString msg("This will overwrite existing geometry binaries:\n");
        if (srcexists)
            msg.add(" - coords/src_coord.bin\n");
        if (rcvexists)
            msg.add(" - coords/rcv_coord_*.bin\n");
        msg.add("\nDo you want to continue?");
        if (!uiMSG().askGoOn(toUiString(msg)))
            return;
    }

    if (!File::exists(proj_dir_) && !File::createDir(proj_dir_)) {
        const BufferString msg = BufferString("Could not create project directory: ").add(proj_dir_);
        if (geostatuslbl_) {
            geostatuslbl_->setText(toUiString(msg));
            geostatuslbl_->setTextColor(OD::Color::Red());
        }
        uiMSG().error(toUiString(msg));
        return;
    }

    std::ofstream ofs(outpath.buf(), std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        const BufferString msg = BufferString("Could not write ").add(outpath);
        if (geostatuslbl_) {
            geostatuslbl_->setText(toUiString(msg));
            geostatuslbl_->setTextColor(OD::Color::Red());
        }
        uiMSG().error(toUiString(msg));
        return;
    }

    ofs << json.buf();
    ofs.close();

    service_->setMamutePath(getMamuteInstallPath());

    ModelingParams writeparams;
    if (!buildParamsFromUI(writeparams, false) || !service_->writeConfigFile(writeparams, this)) {
        const BufferString msg(
            "Could not prepare modeling.toml required to generate geometry binaries. Check Modeling parameters and run Build once.");
        if (geostatuslbl_) {
            geostatuslbl_->setText(toUiString(msg));
            geostatuslbl_->setTextColor(OD::Color::Red());
        }
        return;
    }

    ModelingParams genparams;
    genparams.proj_dir = proj_dir_;
    if (!service_->generateSrcRcv(genparams, this))
        return;

    const BufferString okmsg = BufferString("Generated geometry binaries (updated ")
                                   .add(outpath)
                                   .add(")");
    if (geostatuslbl_) {
        geostatuslbl_->setText(toUiString(okmsg));
        geostatuslbl_->setTextColor(OD::Color(0, 140, 0));
    }
}

void uiMamuteMainWindow::createGeometryPage() {
    geometrypage_ = new uiGroup(tabstack_->tabGroup(), "GeometryPage");

    uiLabel* title = new uiLabel(geometrypage_, toUiString("Acquisition Geometry"));
    title->setHSzPol(uiObject::Wide);
    title->setAlignment(Alignment::HCenter);

    uiSeparator* titlesep = new uiSeparator(geometrypage_, "geotitlesep");
    titlesep->attach(stretchedBelow, title);

    BufferString projmsg("Selected project: ");
    projmsg.add(proj_dir_);
    uiLabel* projlbl = new uiLabel(geometrypage_, toUiString(projmsg));
    projlbl->setHSzPol(uiObject::Wide);
    projlbl->attach(alignedBelow, titlesep);

    BufferStringSet geotypes;
    geotypes.add("grid");
    geotypes.add("circular");

    uiLabel* srclbl = new uiLabel(geometrypage_, toUiString("Sources"));
    srclbl->setHSzPol(uiObject::Wide);
    srclbl->attach(alignedBelow, projlbl);

    uiGroup* srcrow0 = new uiGroup(geometrypage_, "srcrow0");
    srcrow0->attach(alignedBelow, srclbl);
    srcgeotypefld_ = new uiGenInput(srcrow0, tr("Sources type"), StringListInpSpec(geotypes));

    srcparams_stack_ = new uiHiddenTabStack(geometrypage_, "srcparamsstack");
    srcparams_stack_->attach(alignedBelow, srcrow0);

    uiGroup* srcgrid = new uiGroup(srcparams_stack_->tabGroup(), "srcgridpage");

    uiGroup* src_r1 = new uiGroup(srcgrid, "srcrow1");
    srcxfirstfld_ = new uiGenInput(src_r1, tr("X first (m)"), FloatInpSpec(0.0f));
    srcxlastfld_ = new uiGenInput(src_r1, tr("X last (m)"), FloatInpSpec(1000.0f));
    srcxlastfld_->attach(rightOf, srcxfirstfld_);
    srcxdeltafld_ = new uiGenInput(src_r1, tr("\u0394X (m)"), FloatInpSpec(100.0f));
    srcxdeltafld_->attach(rightOf, srcxlastfld_);

    uiGroup* src_r2 = new uiGroup(srcgrid, "srcrow2");
    src_r2->attach(alignedBelow, src_r1);
    srcyfirstfld_ = new uiGenInput(src_r2, tr("Y first (m)"), FloatInpSpec(0.0f));
    srcylastfld_ = new uiGenInput(src_r2, tr("Y last (m)"), FloatInpSpec(1.0f));
    srcylastfld_->attach(rightOf, srcyfirstfld_);
    srcydeltafld_ = new uiGenInput(src_r2, tr("\u0394Y (m)"), FloatInpSpec(1.0f));
    srcydeltafld_->attach(rightOf, srcylastfld_);

    uiGroup* src_r3 = new uiGroup(srcgrid, "srcrow3");
    src_r3->attach(alignedBelow, src_r2);
    srczfirstfld_ = new uiGenInput(src_r3, tr("Z first (m)"), FloatInpSpec(10.0f));
    srczlastfld_ = new uiGenInput(src_r3, tr("Z last (m)"), FloatInpSpec(11.0f));
    srczlastfld_->attach(rightOf, srczfirstfld_);
    srczdeltafld_ = new uiGenInput(src_r3, tr("\u0394Z (m)"), FloatInpSpec(1.0f));
    srczdeltafld_->attach(rightOf, srczlastfld_);
    srcparams_stack_->addTab(srcgrid, toUiString("grid"));

    uiGroup* srccirc = new uiGroup(srcparams_stack_->tabGroup(), "srccircpage");

    uiGroup* src_r4 = new uiGroup(srccirc, "srcrow4");
    srccenterxfld_ = new uiGenInput(src_r4, tr("center X (m)"), FloatInpSpec(500.0f));
    srccenteryfld_ = new uiGenInput(src_r4, tr("center Y (m)"), FloatInpSpec(500.0f));
    srccenteryfld_->attach(rightOf, srccenterxfld_);
    srcradiusfld_ = new uiGenInput(src_r4, tr("radius (m)"), FloatInpSpec(100.0f));
    srcradiusfld_->attach(rightOf, srccenteryfld_);

    uiGroup* src_r4b = new uiGroup(srccirc, "srcrow4b");
    src_r4b->attach(alignedBelow, src_r4);
    srcnpointsfld_ = new uiGenInput(src_r4b, tr("n points"), IntInpSpec(8));
    srcdepthfld_ = new uiGenInput(src_r4b, tr("depth (m)"), FloatInpSpec(10.0f));
    srcdepthfld_->attach(rightOf, srcnpointsfld_);
    srcparams_stack_->addTab(srccirc, toUiString("circular"));

    uiSeparator* midsep = new uiSeparator(geometrypage_, "geomidsep");
    midsep->attach(stretchedBelow, srcparams_stack_);

    uiLabel* rcvlbl = new uiLabel(geometrypage_, toUiString("Receivers"));
    rcvlbl->setHSzPol(uiObject::Wide);
    rcvlbl->attach(alignedBelow, midsep);

    uiGroup* rcvrow0 = new uiGroup(geometrypage_, "rcvrow0");
    rcvrow0->attach(alignedBelow, rcvlbl);
    rcvgeotypefld_ = new uiGenInput(rcvrow0, tr("Receivers type"), StringListInpSpec(geotypes));

    rcvparams_stack_ = new uiHiddenTabStack(geometrypage_, "rcvparamsstack");
    rcvparams_stack_->attach(alignedBelow, rcvrow0);

    uiGroup* rcvgrid = new uiGroup(rcvparams_stack_->tabGroup(), "rcvgridpage");

    uiGroup* rcv_r1 = new uiGroup(rcvgrid, "rcvrow1");
    rcvxfirstfld_ = new uiGenInput(rcv_r1, tr("X first (m)"), FloatInpSpec(0.0f));
    rcvxlastfld_ = new uiGenInput(rcv_r1, tr("X last (m)"), FloatInpSpec(1000.0f));
    rcvxlastfld_->attach(rightOf, rcvxfirstfld_);
    rcvxdeltafld_ = new uiGenInput(rcv_r1, tr("\u0394X (m)"), FloatInpSpec(10.0f));
    rcvxdeltafld_->attach(rightOf, rcvxlastfld_);

    uiGroup* rcv_r2 = new uiGroup(rcvgrid, "rcvrow2");
    rcv_r2->attach(alignedBelow, rcv_r1);
    rcvyfirstfld_ = new uiGenInput(rcv_r2, tr("Y first (m)"), FloatInpSpec(0.0f));
    rcvylastfld_ = new uiGenInput(rcv_r2, tr("Y last (m)"), FloatInpSpec(1.0f));
    rcvylastfld_->attach(rightOf, rcvyfirstfld_);
    rcvydeltafld_ = new uiGenInput(rcv_r2, tr("\u0394Y (m)"), FloatInpSpec(1.0f));
    rcvydeltafld_->attach(rightOf, rcvylastfld_);

    uiGroup* rcv_r3 = new uiGroup(rcvgrid, "rcvrow3");
    rcv_r3->attach(alignedBelow, rcv_r2);
    rcvzfirstfld_ = new uiGenInput(rcv_r3, tr("Z first (m)"), FloatInpSpec(10.0f));
    rcvzlastfld_ = new uiGenInput(rcv_r3, tr("Z last (m)"), FloatInpSpec(11.0f));
    rcvzlastfld_->attach(rightOf, rcvzfirstfld_);
    rcvzdeltafld_ = new uiGenInput(rcv_r3, tr("\u0394Z (m)"), FloatInpSpec(1.0f));
    rcvzdeltafld_->attach(rightOf, rcvzlastfld_);
    rcvparams_stack_->addTab(rcvgrid, toUiString("grid"));

    uiGroup* rcvcirc = new uiGroup(rcvparams_stack_->tabGroup(), "rcvcircpage");

    uiGroup* rcv_r4 = new uiGroup(rcvcirc, "rcvrow4");
    rcvcenterxfld_ = new uiGenInput(rcv_r4, tr("center X (m)"), FloatInpSpec(500.0f));
    rcvcenteryfld_ = new uiGenInput(rcv_r4, tr("center Y (m)"), FloatInpSpec(500.0f));
    rcvcenteryfld_->attach(rightOf, rcvcenterxfld_);
    rcvradiusfld_ = new uiGenInput(rcv_r4, tr("radius (m)"), FloatInpSpec(300.0f));
    rcvradiusfld_->attach(rightOf, rcvcenteryfld_);

    uiGroup* rcv_r4b = new uiGroup(rcvcirc, "rcvrow4b");
    rcv_r4b->attach(alignedBelow, rcv_r4);
    rcvnpointsfld_ = new uiGenInput(rcv_r4b, tr("n points"), IntInpSpec(128));
    rcvdepthfld_ = new uiGenInput(rcv_r4b, tr("depth (m)"), FloatInpSpec(10.0f));
    rcvdepthfld_->attach(rightOf, rcvnpointsfld_);
    rcvparams_stack_->addTab(rcvcirc, toUiString("circular"));

    uiSeparator* bottomsep = new uiSeparator(geometrypage_, "geobottomsep");
    bottomsep->attach(stretchedBelow, rcvparams_stack_);

    uiGroup* rowbtn = new uiGroup(geometrypage_, "geobtnrow");
    rowbtn->attach(alignedBelow, bottomsep);
    savesrcjsonbtn_ = new uiPushButton(rowbtn, tr("Generate geometry binaries"), mCB(this, uiMamuteMainWindow, saveGeometryJsonCB), true);

    geosummarylbl_ = new uiLabel(geometrypage_, toUiString("Sources=invalid, Receivers=invalid"));
    geosummarylbl_->setHSzPol(uiObject::Wide);
    geosummarylbl_->setPrefWidthInChar(90);
    geosummarylbl_->attach(alignedBelow, rowbtn);

    geostatuslbl_ = new uiLabel(geometrypage_, toUiString("Set source/receiver parameters and generate geometry binaries."));
    geostatuslbl_->setHSzPol(uiObject::Wide);
    geostatuslbl_->setPrefWidthInChar(90);
    geostatuslbl_->attach(alignedBelow, geosummarylbl_);

    srcgeotypefld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, geometryTypeChangedCB));
    rcvgeotypefld_->valuechanged.notify(mCB(this, uiMamuteMainWindow, geometryTypeChangedCB));

    const auto notifyGeo = [&](uiGenInput* fld) {
        if (fld) fld->valuechanged.notify(mCB(this, uiMamuteMainWindow, geometryParamsChangedCB));
    };
    notifyGeo(srcxfirstfld_);
    notifyGeo(srcxlastfld_);
    notifyGeo(srcxdeltafld_);
    notifyGeo(srcyfirstfld_);
    notifyGeo(srcylastfld_);
    notifyGeo(srcydeltafld_);
    notifyGeo(srczfirstfld_);
    notifyGeo(srczlastfld_);
    notifyGeo(srczdeltafld_);
    notifyGeo(srccenterxfld_);
    notifyGeo(srccenteryfld_);
    notifyGeo(srcradiusfld_);
    notifyGeo(srcnpointsfld_);
    notifyGeo(srcdepthfld_);
    notifyGeo(rcvxfirstfld_);
    notifyGeo(rcvxlastfld_);
    notifyGeo(rcvxdeltafld_);
    notifyGeo(rcvyfirstfld_);
    notifyGeo(rcvylastfld_);
    notifyGeo(rcvydeltafld_);
    notifyGeo(rcvzfirstfld_);
    notifyGeo(rcvzlastfld_);
    notifyGeo(rcvzdeltafld_);
    notifyGeo(rcvcenterxfld_);
    notifyGeo(rcvcenteryfld_);
    notifyGeo(rcvradiusfld_);
    notifyGeo(rcvnpointsfld_);
    notifyGeo(rcvdepthfld_);

    updateGeometryTypeUI();

    loadGeometryJson();

    tabstack_->addTab(geometrypage_, toUiString("Geometry"));
}

void uiMamuteMainWindow::loadGeometryJson() {
    FilePath fp(proj_dir_);
    fp.add("src_rcv.json");
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

    const auto loadGrid = [&](const std::string& section, uiGenInput* xf, uiGenInput* xl, uiGenInput* xd, uiGenInput* yf, uiGenInput* yl, uiGenInput* yd, uiGenInput* zf, uiGenInput* zl, uiGenInput* zd) {
        const std::string xs = jsonObjectSection(section, "x");
        const std::string ys = jsonObjectSection(section, "y");
        const std::string zs = jsonObjectSection(section, "z");
        if (!xs.empty()) {
            if (xf) xf->setValue(jsonFloatVal(xs, "first", 0.0f));
            if (xl) xl->setValue(jsonFloatVal(xs, "last", 1000.0f));
            if (xd) xd->setValue(jsonFloatVal(xs, "delta", 100.0f));
        }
        if (!ys.empty()) {
            if (yf) yf->setValue(jsonFloatVal(ys, "first", 0.0f));
            if (yl) yl->setValue(jsonFloatVal(ys, "last", 1.0f));
            if (yd) yd->setValue(jsonFloatVal(ys, "delta", 1.0f));
        }
        if (!zs.empty()) {
            if (zf) zf->setValue(jsonFloatVal(zs, "first", 10.0f));
            if (zl) zl->setValue(jsonFloatVal(zs, "last", 11.0f));
            if (zd) zd->setValue(jsonFloatVal(zs, "delta", 1.0f));
        }
    };

    const auto loadCircular = [&](const std::string& section,
                                  uiGenInput* center_x_fld, uiGenInput* center_y_fld,
                                  uiGenInput* radius_fld, uiGenInput* npoints_fld, uiGenInput* depth_fld) {
        const std::string circ = jsonObjectSection(section, "circular");
        if (circ.empty()) return;
        const std::string center = jsonObjectSection(circ, "center");
        if (!center.empty()) {
            const std::size_t center_key_pos = circ.find("\"center\"");
            if (center_key_pos != std::string::npos) {
                const std::size_t array_start = circ.find('[', center_key_pos);
                if (array_start != std::string::npos) {
                    const float center_x = static_cast<float>(atof(circ.c_str() + array_start + 1));
                    const std::size_t comma_pos = circ.find(',', array_start + 1);
                    const float center_y = comma_pos != std::string::npos
                                               ? static_cast<float>(atof(circ.c_str() + comma_pos + 1))
                                               : 500.0f;
                    if (center_x_fld) center_x_fld->setValue(center_x);
                    if (center_y_fld) center_y_fld->setValue(center_y);
                }
            }
        }
        if (radius_fld) radius_fld->setValue(jsonFloatVal(circ, "radius", 100.0f));
        if (npoints_fld) npoints_fld->setValue(jsonIntVal(circ, "n_receivers", 8));
        if (depth_fld) depth_fld->setValue(jsonFloatVal(circ, "depth", 10.0f));
    };

    const std::string srcsec = jsonObjectSection(json, "sources");
    if (!srcsec.empty()) {
        const bool iscirc = srcsec.find("\"circular\"") != std::string::npos;
        if (srcgeotypefld_) {
            srcgeotypefld_->setText(iscirc ? "circular" : "grid");
            updateGeometryTypeUI();
        }
        if (iscirc)
            loadCircular(srcsec, srccenterxfld_, srccenteryfld_, srcradiusfld_, srcnpointsfld_, srcdepthfld_);
        else
            loadGrid(srcsec, srcxfirstfld_, srcxlastfld_, srcxdeltafld_, srcyfirstfld_, srcylastfld_, srcydeltafld_, srczfirstfld_, srczlastfld_, srczdeltafld_);
    }

    const std::string rcvsec = jsonObjectSection(json, "receivers");
    if (!rcvsec.empty()) {
        const bool iscirc = rcvsec.find("\"circular\"") != std::string::npos;
        if (rcvgeotypefld_) {
            rcvgeotypefld_->setText(iscirc ? "circular" : "grid");
            updateGeometryTypeUI();
        }
        if (iscirc)
            loadCircular(rcvsec, rcvcenterxfld_, rcvcenteryfld_, rcvradiusfld_, rcvnpointsfld_, rcvdepthfld_);
        else
            loadGrid(rcvsec, rcvxfirstfld_, rcvxlastfld_, rcvxdeltafld_, rcvyfirstfld_, rcvylastfld_, rcvydeltafld_, rcvzfirstfld_, rcvzlastfld_, rcvzdeltafld_);
    }

    updateGeometrySummary();

    if (geostatuslbl_) {
        geostatuslbl_->setText(toUiString("Loaded parameters from existing src_rcv.json."));
        geostatuslbl_->setTextColor(OD::Color(0, 100, 180));
    }
}
