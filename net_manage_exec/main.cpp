#include <string>
#include <iostream>
#include <vector>
#include <exception>
#include <stdexcept>

#include <cstring>

#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using std::cerr;
using std::cout;
using std::endl;
using std::runtime_error;
using std::string;
using std::vector;

/*

添加配置网桥
添加虚拟网卡，不配置
创建

1. ip link add br-microc type bridge
2. ip address add 10.0.1.1/24 dev br-microc
3. ip link set br-microc up
4. iptables -t nat -A POSTROUTING -s 10.0.1.0/24 ! -d 10.0.1.0/24 -j MASQUERADE


------------------- some infomation -------------------

Chain POSTROUTING (policy ACCEPT)
target      prot opt source              destination
CNI-4       all  --  10.88.0.15           0.0.0.0/0
MASQUERADE  all  --  10.0.1.0/24         !10.0.1.0/24
MASQUERADE  all  --  0.0.0.0/0           !10.0.1.0/24

Chain CNI-4 (1 references)
target     prot opt source               destination
ACCEPT     all  --  0.0.0.0/0            10.88.0.0/16
MASQUERADE  all  --  0.0.0.0/0           !224.0.0.0/4


// bridge -> other
iptables -t nat -A POSTROUTING -s 10.0.1.0/24 ! -d 10.0.1.0/24 -j MASQUERADE
// other -> bridge
iptables -t nat -A POSTROUTING -d 10.0.1.0/24 -j ACCEPT

// create veth
ip link add veth233 type veth peer name veth322
ip link set veth233 master br-microc
ip address add 10.0.1.14/24 dev veth322
ip link set veth233 up
ip link set veth322 up

 */

const string bridgeName = "br-microc";
const string bridgeAddr = "10.0.1.1/24";
const string bridgeSubnetAddr = "10.0.1.0/24";

static const char ipPath[] = "/usr/sbin/ip";
static const char ipName[] = "ip";
static const char iptablesPath[] = "/usr/sbin/iptables";
static const char iptablesName[] = "iptables";

class IncorrectlyExitError : public runtime_error {
public:
    IncorrectlyExitError(const char *what) : runtime_error(what) {}
    ~IncorrectlyExitError() {}
};

string concat(const vector<string> &argv) {
    string res;
    for (auto &&x : argv)
        res.append(x).append(" ");
    return res;
};

// fork & exec in child, wait child and get return value
// args should contain filename, redirect stdout/stderr to /dev/null if noOut is true
// throw on error and when child not returned correctly (not WIFEXITED)
int fork_exec_wait(const string &filePath, const vector<string> &args, bool noOut = false) {
    int ret = 0;
    pid_t child = -1;
    child = fork();
    if (child == -1)
        throw runtime_error(string("Call to fork failed: ") + strerror(errno));
    else if (child == 0) {
        // child
        if (noOut) {
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
        }
        const size_t sz = args.size();
        char *argv[sz + 1];
        for (size_t i = 0; i < sz; i++)
            argv[i] = strdup(args[i].c_str());
        argv[sz] = nullptr;
        ret = execv(filePath.c_str(), argv);
        if (ret == -1)
            exit(1);
    } else {
        int statLoc = -1;
        ret = waitpid(child, &statLoc, 0);
        if (ret == -1)
            throw runtime_error(string("Call to waitpid failed: ") + strerror(errno));
        else if (WIFEXITED(statLoc))
            return WEXITSTATUS(statLoc);
        else
            throw IncorrectlyExitError("Child returned incorrectly");
    }
    return 0;
}

// configure nat if not exist, throw on error
void confBridgeNat() {
    vector<string> checkArgv({iptablesName, "-t", "nat", "-C", "POSTROUTING", "-s",
                              bridgeSubnetAddr, "!", "-d", bridgeSubnetAddr, "-j", "MASQUERADE"});
    vector<string> addArgv({iptablesName, "-t", "nat", "-A", "POSTROUTING", "-s", bridgeSubnetAddr,
                            "!", "-d", bridgeSubnetAddr, "-j", "MASQUERADE"});
    int retCode = fork_exec_wait(iptablesPath, checkArgv, true);
    if (retCode == 0) {
        return;
    } else {
        retCode = fork_exec_wait(iptablesPath, addArgv, true);
        if (retCode != 0)
            throw runtime_error("Add iptables rule failed: " + concat(addArgv));
    }
}

// throw on error
void deleteBridgeNat() {
    vector<string> delArgv({iptablesName, "-t", "nat", "-D", "POSTROUTING", "-s", bridgeSubnetAddr,
                            "!", "-d", bridgeSubnetAddr, "-j", "MASQUERADE"});
    int retCode = fork_exec_wait(iptablesPath, delArgv);
    if (retCode != 0)
        throw runtime_error("Delete iptables rule failed: " + concat(delArgv));
}

// throw on error
void deleteBridgeDev() {
    vector<string> argv = {ipName, "link", "delete", bridgeName};
    int ret = fork_exec_wait(ipPath, argv);
    if (ret != 0)
        throw runtime_error("Delete bridge failed on exec: " + concat(argv));
}

// throw on error
bool checkBridgeExits() {
    int ret = fork_exec_wait(ipPath, {ipName, "link", "show", bridgeName}, true);
    return ret == 0;
}

// throw on error
void createBridgeDev() {
    vector<vector<string>> argvs = {
        {ipName, "link", "add", bridgeName, "type", "bridge"},
        {ipName, "address", "add", bridgeAddr, "dev", bridgeName},
        {ipName, "link", "set", bridgeName, "up"},
    };
    for (auto &&argv : argvs) {
        int retCode = fork_exec_wait(ipPath, argv, false);
        if (retCode != 0)
            throw runtime_error("Bridge create failed on exec: " + concat(argv));
    }
}

// create bridge and config nat (via iptable)
// no throw
void createBridge() {
    try {
        if (!checkBridgeExits()) {
            cout << "Creating bridge" << endl;
            createBridgeDev();
            confBridgeNat();
            cout << "Created bridge" << endl;
        }
    } catch (const std::exception &e) {
        cerr << "Create bridge failed: " << e.what() << endl;
        try {
            if (checkBridgeExits()) {
                cout << "Deleting bridge" << endl;
                deleteBridgeDev();
                deleteBridgeNat();
            }
        } catch (const std::exception &e) {
            cerr << "Delete bridge failed: " << e.what() << endl;
        }
    }
}

int main(int argc, char const *argv[]) {
    createBridge();
    return 0;
}
