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
#include <sys/socket.h>
#include <arpa/inet.h>

using std::cerr;
using std::cout;
using std::endl;
using std::runtime_error;
using std::string;
using std::vector;

/*

网络相关函数

    添加、删除、检查网桥设备
    配置网桥NAT
    添加、删除虚拟网卡veth
    将虚拟网卡添加到net命名空间
    添加默认路由 ip route ad
    添加端口映射 iptables

1. ip link add br-microc type bridge
2. ip address add 10.0.1.1/24 dev br-microc
3. ip link set br-microc up
4. iptables -t nat -A POSTROUTING -s 10.0.1.0/24 ! -d 10.0.1.0/24 -j MASQUERADE
5. iptables -t nat -A PREROUTING ! -i docker0 -p tcp -m tcp --dport 7880 -j DNAT --to-destination

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

// concatenate a vector<string> with space
string concat(const vector<string> &argv) {
    string res;
    for (auto &&x : argv)
        res.append(x).append(" ");
    return res;
};

// fork & exec in child, wait child and get return value
// args should contain filename, close stdout/stderr if noOut is true [default]
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

// throw on error
void addDefGatewayRoute(const string &gatewayIp, const string &devName) {
    vector<string> addRouteArgv(
        {ipName, "route", "add", "default", "via", gatewayIp, "dev", devName});
    int retCode = fork_exec_wait(ipPath, addRouteArgv);
    if (retCode != 0)
        throw runtime_error("Add route failed on exec: " + concat(addRouteArgv));
}

// throw on error
void addPortMap(int hostPort, const string &destIp, int destPort, const string &proto = "tcp") {
    // iptables -A nat ! -i br-microc -p tcp -m tcp --dport 7880 -j DNAT --to-destination
    // 172.17.0.4:8080
    using std::to_string;
    vector<string> portMapArgv({iptablesName, "-t", "nat", "-A", "PREROUTING", "!", "-i",
                                bridgeName, "-p", proto, "-m", proto, "--dport",
                                to_string(hostPort), "-j", "DNAT", "--to-destination",
                                destIp + ":" + to_string(destPort)});
    int retCode = fork_exec_wait(iptablesPath, portMapArgv);
    if (retCode != 0)
        throw runtime_error("Add port mapping failed on exec: " + concat(portMapArgv));
}

// throw on error
void createVethPeer(const string &vethName1, const string &vethName2) {
    vector<string> addVethArgv(
        {ipName, "link", "add", vethName1, "type", "veth", "peer", "name", vethName2});
    int retCode = fork_exec_wait(ipPath, addVethArgv);
    if (retCode != 0)
        throw runtime_error("Add veth peer failed on exec: " + concat(addVethArgv));
}

// throw on error
void addVethToNetns(const string &vethName, pid_t pid) {
    vector<string> addToNsArgv({ipName, "link", "set", vethName, "netns", std::to_string(pid)});
    int retCode = fork_exec_wait(ipPath, addToNsArgv);
    if (retCode != 0)
        throw runtime_error("Add veth to netns failed on exec: " + concat(addToNsArgv));
}

// throw on error
void addVethToBridge(const string &vethName) {
    vector<string> addVeth2BrArgv({ipName, "link", "set", vethName, "master", bridgeName});
    int retCode = fork_exec_wait(ipPath, addVeth2BrArgv);
    if (retCode != 0)
        throw runtime_error("Add veth to bridge failed on exec: " + concat(addVeth2BrArgv));
}

// throw on error
void confVethIp(const string &vethName, const string &ipAddr, int ipPreLen) {
    // should be called inside netns
    vector<vector<string>> argvs({
        {ipName, "address", "add", ipAddr + "/" + std::to_string(ipPreLen), "dev", vethName},
        {ipName, "link", "set", vethName, "up"},
    });
    for (auto &&argv : argvs) {
        int retCode = fork_exec_wait(ipPath, argv);
        if (retCode != 0)
            throw runtime_error("Config veth ip failed on exec: " + concat(argv));
    }
}

// throw on error
void confVethUp(const string &vethName) {
    vector<string> argv = {ipName, "link", "set", vethName, "up"};
    if (fork_exec_wait(ipPath, argv) != 0)
        throw runtime_error("Config veth up failed on exec: " + concat(argv));
}

// configure nat if not exist, throw on error
void confBridgeNat() {
    vector<string> checkArgv({iptablesName, "-t", "nat", "-C", "POSTROUTING", "-s",
                              bridgeSubnetAddr, "!", "-d", bridgeSubnetAddr, "-j", "MASQUERADE"});
    vector<string> addArgv({iptablesName, "-t", "nat", "-A", "POSTROUTING", "-s", bridgeSubnetAddr,
                            "!", "-d", bridgeSubnetAddr, "-j", "MASQUERADE"});
    int retCode = fork_exec_wait(iptablesPath, checkArgv);
    if (retCode == 0) {
        return;
    } else {
        retCode = fork_exec_wait(iptablesPath, addArgv);
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
    return fork_exec_wait(ipPath, {ipName, "link", "show", bridgeName}) == 0;
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
bool createBridgeUpIP() {
    try {
        if (!checkBridgeExits()) {
            cout << "Creating bridge" << endl;
            createBridgeDev();
            confBridgeNat();
            cout << "Created bridge" << endl;
        }
        return true;
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
    return false;
}

/*

--------------------------------------------------------

. create bridge
. conf nat & make bridge up
. create veth peer
. add veth to bridge
. create netns
. add veth to netns
. conf veth up & ip -- inside netns
. add default gateway -- inside netns
. add port mapping
. listen on port 2333

 */

const string veth1 = "veth2222", veth2 = "veth3333";
const string veth1IpAddr = "10.0.1.12";
const int port = 2333, hostPort = 12333;

static const size_t stackSize = 1024 * 1024;
static char childStack[stackSize] = {};
int childProc(void *arg) {
    try {
        sleep(1);
        // int parentId = -1;
        // if (arg != nullptr)
        //     parentId = *(int *)arg;
        confVethIp(veth1, veth1IpAddr, 24);
        cerr << "child: configured Veth1 ip" << endl;
        cerr << "--------child:ip link show--------" << endl;
        fork_exec_wait(ipPath, {ipName, "link", "show"});
        cerr << "--------child:ip addr show--------" << endl;
        fork_exec_wait(ipPath, {ipName, "addr", "show"});
        cerr << "----------------------------------" << endl;
        addDefGatewayRoute(bridgeAddr.substr(0, bridgeAddr.find('/')), veth1);
        cerr << "child: added default gateway" << endl;
        auto errExit = []() {
            cerr << "child: ERROR:" << strerror(errno) << endl;
            exit(1);
        };
        int sd = socket(AF_INET, SOCK_STREAM, 0);
        cerr << "child: created socket" << endl;
        if (sd == -1)
            errExit();
        int opt = 1;
        if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1)
            errExit();
        cerr << "child: socket reuse set" << endl;
        sockaddr_in addr;
        addr.sin_family = AF_INET, addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(sd, (sockaddr *)&addr, sizeof(addr)) == -1)
            errExit();
        cerr << "child: socket binded" << endl;
        if (listen(sd, 1024) == -1)
            errExit();
        cerr << "child: server started on port 2333" << endl;
        while (true) {
            sockaddr_in addr;
            socklen_t len;
            int conn = accept(sd, (sockaddr *)&addr, &len);
            cerr << "child: connect established" << endl;
            const int bufSz = 1024;
            char buf[bufSz] = {};
            read(conn, buf, bufSz);
            char outputBuf[] = "HTTP/1.1 200\r\n\
Content-Type: application/json\r\n\
Content-Length: 29\r\n\
Date: Thu, 13 Aug 2020 08:02:00 GMT\r\n\
Keep-Alive: timeout=60\r\n\
Connection: keep-alive\r\n\r\n\
<strong>[HELLO WORLD]</strong>\r\n\r\n";
            write(conn, outputBuf, strlen(outputBuf));
            close(conn);
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << endl;
    }
    return 0;
}

void work() {
    try {
        if (!createBridgeUpIP())
            return;
        cerr << "bridge created & configured" << endl;
        createVethPeer(veth1, veth2);
        cerr << "veth peer created" << endl;
        addVethToBridge(veth2);
        cerr << "veth2 added to bridge" << endl;
        confVethUp(veth2);
        cerr << "veth2 set to up" << endl;
        int parentId = getpid();
        int childId =
            clone(&childProc, childStack + stackSize - 1, CLONE_NEWNET | SIGCHLD, &parentId);
        if (childId == -1) {
            int tmpErrno = errno;
            cerr << "clone failed: " << strerror(tmpErrno) << endl;
            return;
        }
        cerr << "child created" << endl;
        addVethToNetns(veth1, childId);
        cerr << "veth added to netns" << endl;
        addPortMap(hostPort, veth1IpAddr, port);
        cerr << "port mapping add" << endl;
        int childStat = 0;
        if (wait(&childStat) == -1) {
            int tmpErrno = errno;
            cerr << "wait failed: " << strerror(tmpErrno) << endl;
            return;
        }
        cerr << "child existed" << endl;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
    }
    return;
}

#define MOVE_EXCEPT(A)                                                                             \
    {                                                                                              \
        try {                                                                                      \
            A;                                                                                     \
        } catch (const std::exception &e) {                                                        \
        }                                                                                          \
    }
void clear() {
    using std::to_string;

    MOVE_EXCEPT(deleteBridgeDev());
    MOVE_EXCEPT(deleteBridgeNat());
    MOVE_EXCEPT(fork_exec_wait(iptablesPath, {iptablesName, "-t", "nat", "-D", "PREROUTING", "!",
                                              "-i", bridgeName, "-p", "tcp", "-m", "tcp", "--dport",
                                              to_string(hostPort), "-j", "DNAT", "--to-destination",
                                              veth1IpAddr + ":" + to_string(port)}));
}

int main(int argc, char const *argv[]) {
    auto outUsage = []() { cerr << "usage: [clear | work | test]" << endl; };
    const string clearCmd = "clear", workCmd = "work", testCmd = "test";
    if (argc > 1) {
        if (argv[1] == workCmd)
            work();
        else if (argv[1] == clearCmd) {
            clear();
        } else if (argv[1] == testCmd) {
            // do test here
            createVethPeer(veth1, veth2);
        } else {
            outUsage();
        }
    } else {
        outUsage();
    }
    return 0;
}
