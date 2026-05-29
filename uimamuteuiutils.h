#pragma once

#include "bufstring.h"
#include "bufstringset.h"
#include "file.h"
#include "filepath.h"
#include "uigeninput.h"
#include "uilabel.h"
#include "uitabbar.h"
#include "uitabstack.h"

#include <string>

class uiHiddenTabStack : public uiTabStack {
  public:
    uiHiddenTabStack(uiParent* p, const char* nm) : uiTabStack(p, nm) {
        if (tabbar_)
            tabbar_->display(false, true);
    }
};

namespace MamuteUI {

inline void normalizeDecimalSeparator(uiGenInput* fld) {
    if (!fld) return;
    const char* txt = fld->text();
    if (!txt || !*txt) return;
    std::string s(txt);
    bool changed = false;
    for (char& ch : s) {
        if (ch == ',') {
            ch = '.';
            changed = true;
        }
    }
    if (changed) fld->setText(s.c_str());
}

inline uiLabel* createFieldMessageLabel(uiParent* parent, uiParent* fld,
                                        int widthinchar = 18) {
    uiLabel* lbl = new uiLabel(parent, toUiString(" "));
    lbl->setPrefWidthInChar(widthinchar);
    lbl->setPrefHeightInChar(1);
    lbl->setTextColor(OD::Color::Red());
    if (fld)
        lbl->attach(alignedBelow, fld);
    return lbl;
}

inline void collectRunOutputFiles(const BufferString& projdir,
                                  BufferStringSet& outfiles) {
    outfiles.setEmpty();

    FilePath datadir(projdir);
    datadir.add("data");
    if (!File::exists(datadir.fullPath()))
        return;

    BufferStringSet entries;
    if (!File::listDir(datadir.fullPath(), File::DirListType::FilesInDir, entries))
        return;

    for (int i = 0; i < entries.size(); i++) {
        const BufferString entry = entries.get(i);
        const std::string name = entry.buf();
        if (name.rfind("dobs_", 0) == 0 && name.size() > 4 &&
            name.rfind(".bin") == name.size() - 4) {
            FilePath fp(datadir.fullPath());
            fp.add(entry);
            outfiles.addIfNew(fp.fullPath());
        }
    }
}

inline bool clearOutputFiles(const BufferStringSet& files) {
    for (int i = 0; i < files.size(); i++) {
        const BufferString path = files.get(i);
        if (!File::exists(path))
            continue;
        if (!File::remove(path))
            return false;
    }
    return true;
}

inline void collectBuildOutputFiles(const BufferString& projdir,
                                    bool includeSource,
                                    bool includeVelocity,
                                    bool includeQp,
                                    BufferStringSet& outfiles) {
    outfiles.setEmpty();

    const auto addIfExists = [&](const char* subdir, const char* fname) {
        FilePath fp(projdir);
        fp.add(subdir).add(fname);
        if (File::exists(fp.fullPath()))
            outfiles.addIfNew(fp.fullPath());
    };

    if (includeSource)
        addIfExists("data", "source.bin");

    if (includeVelocity) {
        addIfExists("data", "velocity.bin");
        addIfExists("data", "velocity_model.bin");
    }

    if (includeQp) {
        addIfExists("data", "qp.bin");
        addIfExists("data", "qp_model.bin");
    }
}

inline bool hasProjectVelocityModel(const BufferString& projdir) {
    FilePath fp(projdir);
    fp.add("data").add("velocity_model.bin");
    return File::exists(fp.fullPath());
}

inline bool hasProjectGeometryFiles(const BufferString& projdir) {
    FilePath srcfp(projdir);
    srcfp.add("coords").add("src_coord.bin");
    if (!File::exists(srcfp.fullPath()))
        return false;

    FilePath coordsdir(projdir);
    coordsdir.add("coords");
    if (!File::exists(coordsdir.fullPath()))
        return false;

    BufferStringSet entries;
    if (!File::listDir(coordsdir.fullPath(), File::DirListType::FilesInDir, entries))
        return false;

    for (int i = 0; i < entries.size(); i++) {
        const std::string filename = entries.get(i).buf();
        if (filename.rfind("rcv_coord_", 0) == 0 && filename.size() > 4 && filename.rfind(".bin") == filename.size() - 4) {
            return true;
        }
    }

    return false;
}

inline std::string jsonStringVal(const std::string& json, const std::string& key) {
    const std::string search = "\"" + key + "\"";
    const std::size_t kpos = json.find(search);
    if (kpos == std::string::npos) return {};
    const std::size_t col = json.find(':', kpos);
    if (col == std::string::npos) return {};
    const std::size_t q1 = json.find('"', col + 1);
    if (q1 == std::string::npos) return {};
    const std::size_t q2 = json.find('"', q1 + 1);
    if (q2 == std::string::npos) return {};
    return json.substr(q1 + 1, q2 - q1 - 1);
}

inline float jsonFloatVal(const std::string& json, const std::string& key, float fallback) {
    const std::string search = "\"" + key + "\"";
    const std::size_t kpos = json.find(search);
    if (kpos == std::string::npos) return fallback;
    const std::size_t col = json.find(':', kpos);
    if (col == std::string::npos) return fallback;
    std::size_t vs = col + 1;
    while (vs < json.size() && (json[vs] == ' ' || json[vs] == '\t' || json[vs] == '\n' || json[vs] == '\r')) ++vs;
    if (vs >= json.size()) return fallback;
    const float v = static_cast<float>(atof(json.c_str() + vs));
    return v;
}

inline int jsonIntVal(const std::string& json, const std::string& key, int fallback) {
    const float v = jsonFloatVal(json, key, static_cast<float>(fallback));
    return static_cast<int>(v);
}

inline std::string jsonObjectSection(const std::string& json, const std::string& key) {
    const std::string search = "\"" + key + "\"";
    const std::size_t kpos = json.find(search);
    if (kpos == std::string::npos) return {};
    const std::size_t brace = json.find('{', json.find(':', kpos));
    if (brace == std::string::npos) return {};
    int depth = 0;
    for (std::size_t i = brace; i < json.size(); ++i) {
        if (json[i] == '{')
            ++depth;
        else if (json[i] == '}') {
            --depth;
            if (depth == 0) return json.substr(brace, i - brace + 1);
        }
    }
    return {};
}

}
