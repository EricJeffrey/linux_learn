
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mount.h>

#include <cstring>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include <stdexcept>

using namespace std;

// throw on error
int mkdirIfNotExist(const string &dirPath) {
    if (dirPath.empty())
        throw runtime_error("empty path is not allowed");
    struct stat st = {0};
    if (stat(dirPath.c_str(), &st) == -1) {
        const int tmpErrno = errno;
        if (tmpErrno != ENOENT)
            throw runtime_error(string("call to stat failed: ") + strerror(tmpErrno));
        // create it now
        if (mkdir(dirPath.c_str(), 0755) == -1) {
            const int tmpErrno = errno;
            throw runtime_error(string("call to mkdir failed: ") + strerror(tmpErrno));
        }
        return 0;
    }
    if (!S_ISDIR(st.st_mode))
        throw runtime_error("");
    return 0;
}

// throw on error
int mountOverlayFs(const string &dst, vector<string> lowerDirs, const string &upperDir,
                   const string &workDir) {
    if (dst.empty() || upperDir.empty() || lowerDirs.empty() || workDir.empty())
        throw runtime_error("empty dir is not allowed");
    string lowerDir;
    bool fir = true;
    for (auto &&tmpDir : lowerDirs) {
        if (!tmpDir.empty()) {
            if (!fir)
                lowerDir += ":";
            lowerDir += tmpDir;
            fir = false;
        }
    }
    if (lowerDir.empty())
        throw runtime_error("empty lower dir is not allowed");
    const string sep(",");
    string optionStr =
        "lowerdir=" + lowerDir + sep + "upperdir=" + upperDir + sep + "workdir=" + workDir;
    const ulong flags = MS_RELATIME | MS_NODEV;
    if (mount("overlay", dst.c_str(), "overlay", flags, optionStr.c_str()) == -1) {
        const int tmpErrno = errno;
        throw runtime_error(string("call to mount failed: ") + strerror(tmpErrno));
    }
    return 0;
}

int main(int argc, char const *argv[]) {
    vector<string> dirPathList({"./lower1", "./lower2", "./upper", "./merged", "./work"});
    try {
        for (auto &&path : dirPathList)
            mkdirIfNotExist(path);
        mountOverlayFs("./merged", {"./lower1", "./lower2"}, "./upper", "./work");
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    cout << "press any key to continue...";
    char ch;
    cin >> ch;
    if (umount("./merged") == -1) {
        const int tmpErrno = errno;
        throw runtime_error(string("call to umount failed: ") + strerror(tmpErrno));
    }
    return 0;
}
