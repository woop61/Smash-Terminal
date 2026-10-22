// Ver: 04-11-2025
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_
#define MAX_SIZE 256

#include <vector>
#include <map>
#include <list>
#include <unistd.h>
#include <string>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

struct USBDevice {
    int devnum;
    std::string vendor_id;
    std::string product_id;
    std::string manufacturer;
    std::string product_name;
    std::string max_power;

    bool operator<(const USBDevice& other) const {
        return devnum < other.devnum;
    }
};

//https://man7.org/linux/man-pages/man0/dirent.h.0p.html
struct direntInfo{
    long long d_info;
    off_t d_off;
    unsigned short d_rec;
    unsigned char d_type;
    char d_name[];
};

class Command {
protected:
    int pid;
    bool backRound;
    const char* cmdLine;
    std::vector<std::string> cmdSegs;
    std::string aliasChar;
    std::string fileDirectedExt;

public:
    Command(const char *cmd_line, std::string aliasComs = "", std::string fileDirectedExt = "");

    virtual ~Command();

    virtual void execute() = 0;

    //virtual void prepare();
    //virtual void cleanup();

    int getProcPid();
    void setProcPid(int pid);
    std::string printCmd();

    std::string printAliasCmd();
    std::string getAliasCommand();
    bool isAlias();
    void setAliasCommand(std::string cmd);

    std::string getPath();
    void setPath(std::string path);

};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line);

    virtual ~BuiltInCommand();
};

class ExternalCommand : public Command {
public:
    std::vector<std::string> argc;
    ExternalCommand(const char *cmd_line, std::string cmd_s, bool isBackground);

    virtual ~ExternalCommand() {
    }

    void execute() override;
};


class RedirectionCommand : public Command {
    std::string relPath;
public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() {
    }

    void execute() override;
};

class PipeCommand : public Command {

public:
    PipeCommand(const char *cmd_line): Command(cmd_line){};

    virtual ~PipeCommand() {
    }

    void execute() override;
};

class DiskUsageCommand : public Command {
public:
    DiskUsageCommand(const char *cmd_line);

    virtual ~DiskUsageCommand() {
    }

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char *cmd_line);

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};

class USBInfoCommand : public Command {
    // TODO: Add your data members **BONUS: 10 Points**
public:
    USBInfoCommand(const char *cmd_line);

    virtual ~USBInfoCommand() {
    }

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    ChangeDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line){};

    virtual ~ChangeDirCommand() {
    }

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line){};

    virtual ~GetCurrDirCommand() {
    }

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line){};

    virtual ~ShowPidCommand() {
    }

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
public:
    JobsList* jobs;
    QuitCommand(const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand() {
    }

    void execute() override;
};

class JobsList {
public:
    class JobEntry {
    public:
        Command* cmd;
        bool isStopped;
        int jobId;
        int pid;
        std::string command;
        JobEntry(Command* cmd,bool isStopped,int jobId,int pid,std::string command):cmd(cmd),isStopped(isStopped),jobId(jobId),pid(pid),command(command){};
        
        ~JobEntry()=default;
        std::string getDetails();
        
    };

    std::list<JobEntry*> list;
public:
    JobsList(): list(){};

    ~JobsList();

    void addJob(Command *cmd, int curPid, bool isStopped = false);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob();

    JobEntry *getLastStoppedJob(int *jobId);

    void printJobsForQuit();
};

class JobsCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    JobsCommand(const char *cmd_line, JobsList *jobs);

    virtual ~JobsCommand() {
    }

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    KillCommand(const char *cmd_line, JobsList *jobs);

    virtual ~KillCommand() {
    }

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);

    virtual ~ForegroundCommand() {
    }

    void execute() override;
};

class AliasCommand : public BuiltInCommand {
public:
    AliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line){};

    virtual ~AliasCommand() {
    }

    void execute() override;
};

class UnAliasCommand : public BuiltInCommand {
public:
    UnAliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line){};

    virtual ~UnAliasCommand() {
    }

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand {
public:
    UnSetEnvCommand(const char *cmd_line);

    virtual ~UnSetEnvCommand() {
    }

    void execute() override;
};

class SysInfoCommand : public BuiltInCommand {
public:
    SysInfoCommand(const char *cmd_line);

    virtual ~SysInfoCommand() {
    }

    void execute() override;
};

class changeSmashPrompt: public BuiltInCommand {
public:

    std::string newPrompt;

    changeSmashPrompt(const char *cmd_line):BuiltInCommand(cmd_line){};

    virtual ~changeSmashPrompt() {}

    void execute() override;
};

class SmallShell {
private:
    std::string prompt;
    int pid;
    std::string workingDir;
    std::string prevWorkingDir;
    JobsList *job;
    std::map <std::string, std::string> alias;
    std::vector <std::string> aliasInOrder;
    int curFrontPid;
    std::streambuf *coutDirect;
    std::vector <std::string> cmdString;
    int inDup;
    int outDup;
    int errDup;
    int watchFrontPid;
    int watchBackPid;

    SmallShell();

public:
    static const int stdIn = STDIN_FILENO;
    static const int stdOut = STDOUT_FILENO;
    static const int stdErr = STDERR_FILENO;

    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance();
    void setPrompt(std::string newPrompt);
    std::string getPrompt();
    int getShellPid();
    std::string getPWD();
    std::string getPrevPWD();
    void changePWD(std::string newPWD);
    JobsList* getJobs();
    std::string getAlias(std::string name);
    void setAlias(std::string name,std::string source);
    void removeAlias(std::string name);
    void getAllAlias(std::vector<std::string>& vec);
    void bringProcessToFront(int pid);
    void leaveProcess();
    void createCommandVector();
    bool validCommand(std::string name);
    int getOriginalOut();
    void runWatchCommand();
    int getWatchInForegroundPid();
    void setWatchInForegroundPid(int pid);
    int getBackRoundWatch();
    void setbackGroundWatch(int back);

    void smashControlCSigHandler();

    ~SmallShell();

    void executeCommand(const char *cmd_line, std::string extPath = "", std::string aliasCmd = "");
};


#endif //SMASH_COMMAND_H_
