#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <sstream>
#include <dirent.h>
#include <cstring>
#include <cerrno>
#include <pwd.h>
#include <grp.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <regex>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;
    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h

void SmallShell::createCommandVector()
{
    cmdString.push_back("chprompt");
    cmdString.push_back("showpid");
    cmdString.push_back("pwd");
    cmdString.push_back("cd");
    cmdString.push_back("jobs");
    cmdString.push_back("fg");
    cmdString.push_back("quit");
    cmdString.push_back("kill");
    cmdString.push_back("alias");
    cmdString.push_back("unalias");
    cmdString.push_back("sysinfo");
    cmdString.push_back("du");
    cmdString.push_back("whoami");
    cmdString.push_back("usbinfo");
}

void createVectorForLine(const char* cmd_line,std::vector<std::string>& line)
{
    std::string temp=cmd_line;
    std::string toAdd;
    stringstream stringLine(temp);
    while (getline(stringLine, toAdd, ' ')) {
        if(!toAdd.empty())
        {
            line.push_back(toAdd);
        }
    }
}

bool specialCommand(const char* cmd_line)
{
    std::string temp = string(cmd_line);
    size_t first_pipe = temp.find('|');
    size_t last_pipe = temp.find_last_of('|');
    bool multiple_pipes = (first_pipe != std::string::npos && first_pipe != last_pipe);

    if(temp.find('*') != std::string::npos || temp.find('?') != std::string::npos || multiple_pipes)
    {
        return true;
    }
    return false;
}

std::string SmallShell::getPWD()
{
    char place[COMMAND_MAX_LENGTH];
    getcwd(place, COMMAND_MAX_LENGTH);
    return std::string(place);
}

SmallShell::SmallShell() :
prompt ("smash"),
pid(getpid()),
workingDir(getPWD()),
prevWorkingDir(""),
job(new JobsList),
alias(),
aliasInOrder(),
curFrontPid(-1),
coutDirect(std::cout.rdbuf()),
cmdString(),
inDup(dup(stdIn)),
outDup(dup(stdOut)),
errDup(dup(stdErr))
{}
SmallShell &SmallShell::getInstance() // make SmallShell singleton
{
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
}

SmallShell::~SmallShell() {
    delete job;
    std::cout.rdbuf(coutDirect);
    dup2(stdOut, 1);
}

int SmallShell::getOriginalOut()
{
    return outDup;
}

bool validLetters(std::string name)
{
    for(auto c : name){
        if(!((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '_' ))
        {
            return false;
        }
    }
    return true;
}

bool SmallShell::validCommand(std::string name)
{
    for(auto cmd : cmdString){
        if(name == cmd)
        {
            return true;
        }
    }
    return false;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
int SmallShell::getShellPid()
{
    return pid;
}
Command *SmallShell::CreateCommand(const char *cmd_line) {
    std::string cmd_s = _trim(string(cmd_line));
    if(cmd_s.empty()) return nullptr;
    
    std::string firstWord = cmd_s.substr(0, cmd_s.find_first_of(WHITESPACE));
    std::string alias_expansion = getAlias(firstWord);
    if (!alias_expansion.empty()) {
        std::string remainder = (cmd_s.size() > firstWord.size()) ? cmd_s.substr(firstWord.size()) : "";
        cmd_s = alias_expansion + remainder;
        firstWord = cmd_s.substr(0, cmd_s.find_first_of(WHITESPACE));
    }

    bool isBackround = _isBackgroundComamnd(cmd_line);
    std::string clean_cmd_s = cmd_s;
    if (!clean_cmd_s.empty() && clean_cmd_s.back() == '&') {
        clean_cmd_s.pop_back();
        clean_cmd_s = _trim(clean_cmd_s);
    }

    size_t first_pipe = clean_cmd_s.find('|');
    size_t last_pipe = clean_cmd_s.find_last_of('|');
    bool multiple_pipes = (first_pipe != std::string::npos && first_pipe != last_pipe);

    if (firstWord == "alias")
       return new AliasCommand(cmd_line);
    else if (firstWord == "unalias")
        return new UnAliasCommand(cmd_line);

    else if (clean_cmd_s.find("|") != std::string::npos && !multiple_pipes){
        char* safe_line = (char*)malloc(cmd_s.length() + 1);
        strcpy(safe_line, cmd_s.c_str());
        return new PipeCommand(safe_line);
    }
    else if (clean_cmd_s.find(">") != std::string::npos){
        char* safe_line = (char*)malloc(cmd_s.length() + 1);
        strcpy(safe_line, cmd_s.c_str());
        return new RedirectionCommand(safe_line);
    }

    else if (firstWord == "unsetenv")
        return new UnSetEnvCommand(cmd_line);
    else if (firstWord == "chprompt")
        return new changeSmashPrompt(cmd_line);
    else if (firstWord == "showpid")
        return new ShowPidCommand(cmd_line);
    else if (firstWord == "pwd")
        return new GetCurrDirCommand(cmd_line);
    else if (firstWord == "cd")
        return new ChangeDirCommand(cmd_line);
    else if (firstWord == "jobs")
        return new JobsCommand(cmd_line, SmallShell::getInstance().getJobs());
    else if (firstWord == "fg")
        return new ForegroundCommand(cmd_line, SmallShell::getInstance().getJobs());
    else if (firstWord == "quit")
        return new QuitCommand(cmd_line, SmallShell::getInstance().getJobs());
    else if (firstWord == "kill")
        return new KillCommand(cmd_line, SmallShell::getInstance().getJobs());
    else if (firstWord == "sysinfo")
        return new SysInfoCommand(cmd_line);
    else if (firstWord == "du")
        return new DiskUsageCommand(cmd_line);
    else if (firstWord == "usbinfo")
        return new USBInfoCommand(cmd_line);
    else if (firstWord == "whoami")
        return new WhoAmICommand(cmd_line);
    else
        return new ExternalCommand(cmd_line, clean_cmd_s, isBackround);
    return nullptr;
}
int SmallShell::getWatchInForegroundPid()
{
    return watchFrontPid;
}
void SmallShell::setWatchInForegroundPid(int pid)
{
    watchFrontPid = pid;
}
int SmallShell::getBackRoundWatch()
{
    return watchBackPid;
}
void SmallShell::setbackGroundWatch(int back)
{
    watchBackPid=back;
}
void SmallShell::executeCommand(const char *cmd_line, std::string extPath, std::string aliasCmd) {
    job->removeFinishedJobs();
    
    if (extPath == "") dup2(stdOut, 1);

    Command* cmd = CreateCommand(cmd_line);
    if (!cmd) return;

    cmd->setPath(extPath);
    cmd->setAliasCommand(aliasCmd);
    cmd->execute();
    delete cmd;
}
void SmallShell::setPrompt(std::string newPrompt){
    prompt = newPrompt;
};
std::string SmallShell::getPrompt(){
    return prompt;
};
std::string SmallShell::getPrevPWD(){
    return prevWorkingDir;

};
void SmallShell::changePWD(std::string newPWD){
    auto cur = workingDir;
    workingDir = newPWD;
    prevWorkingDir = cur;
};
JobsList* SmallShell::getJobs(){
    return job;
};
std::string SmallShell::getAlias(std::string name){
    if (alias.find(name) == alias.end()) return "";
    return alias[name];
};
void SmallShell::setAlias(std::string name,std::string source){
    alias[name] = source;
    aliasInOrder.push_back(name);
};
void SmallShell::removeAlias(std::string name){
    alias.erase(name);
    aliasInOrder.erase(std::find(aliasInOrder.begin(), aliasInOrder.end(), name));
};
void SmallShell::getAllAlias(std::vector<std::string>& vec){
    for (auto element : aliasInOrder){
        vec.push_back((element + "='" + alias[element] + "'"));
    }
}
void SmallShell::bringProcessToFront(int pid){
    curFrontPid = pid;
};
void SmallShell::leaveProcess(){
    curFrontPid = -1;
};
void SmallShell::smashControlCSigHandler(){
    if (curFrontPid == -1) return;

    int pidToKill = curFrontPid;
    curFrontPid = -1;

    if (kill(pidToKill, SIGKILL) == 0) {
        const char prefix[] = "smash: process ";
        const char suffix[] = " was killed\n";
        write(STDOUT_FILENO, prefix, sizeof(prefix) - 1);
        char buf[16];
        int i = 0;
        int tmp = pidToKill;
        if (tmp == 0) { buf[i++] = '0'; }
        else { while (tmp > 0) { buf[i++] = '0' + (tmp % 10); tmp /= 10; } }
        for (int l = 0, r = i - 1; l < r; l++, r--) { char t = buf[l]; buf[l] = buf[r]; buf[r] = t; }
        write(STDOUT_FILENO, buf, i);
        write(STDOUT_FILENO, suffix, sizeof(suffix) - 1);
    }
}


JobsList::~JobsList(){
    for (auto entry : list){
        if (entry) {
            if (entry->cmd) {
                delete entry->cmd;
            }
            delete entry;
        }
    }
}
void JobsList::addJob(Command *cmd, int pidCurr,bool isStopped)
{
    removeFinishedJobs();
    int jobId = 1;
    if(!list.empty()) jobId = 1 + list.back()->jobId;

    std::string cmdString = cmd->printCmd();
    if(cmd->isAlias())
    {
        cmdString = cmd->getAliasCommand();
    }
    list.push_back(new JobEntry(cmd, isStopped, jobId, pidCurr, cmdString));
}
void JobsList::printJobsList()
{
    removeFinishedJobs();
    for(auto jobEntry : list)
    {
        std::cout << "[" << jobEntry->jobId << "] " << jobEntry->command << std::endl;
    }
}
void JobsList::killAllJobs()
{
    for(auto jobEntry : list)
    {
        if (jobEntry){
            std::cout << jobEntry->pid << ": " << jobEntry->command << std::endl;
            kill(jobEntry->pid, SIGKILL);
        }
    }
}
bool isFinished(JobsList::JobEntry* job)
{
    if(!job) return true;
    int status;
    pid_t res = waitpid(job->pid, &status, WNOHANG);
    // res > 0: process finished and reaped
    // res == -1 with ECHILD: process no longer exists
    // res == 0: process still running - keep in list
    if (res > 0) return true;
    if (res == -1 && errno == ECHILD) return true;
    return false;
}
void JobsList::removeFinishedJobs()
{
    list.remove_if(isFinished);
}
JobsList::JobEntry *JobsList::getJobById(int jobId)
{
    for(auto listIt = list.begin(); listIt != list.end(); ++listIt)
    {
        if((*listIt)->jobId==jobId)
        {
            return *listIt;
        }
    }
    return nullptr;
}
void JobsList::removeJobById(int jobId)
{
    auto it = list.begin();
    while (it != list.end()) {
        if ((*it)->jobId == jobId) {
            JobEntry* entry = *it;
            it = list.erase(it);
            delete entry;
            return;
        } else {
            ++it;
        }
    }
}
JobsList::JobEntry *JobsList::getLastJob()
{
    if(list.empty()) return nullptr;
    return list.back();
}
void JobsList::printJobsForQuit()
{
    removeFinishedJobs();
    std::cout << "smash: sending SIGKILL signal to " << list.size() << " jobs:" << std::endl;
    for(auto listIt = list.begin(); listIt != list.end(); ++listIt)
    {
        auto job = *listIt;
        std::cout << job->pid << ": " << job->command << std::endl;
    }
}

Command::Command(const char *cmd_line, std::string aliasCommand, std::string fileDirectedForExternal):
pid(getpid()),
backRound(false),
cmdLine(cmd_line),
cmdSegs(),
fileDirectedExt("")
{
    createVectorForLine(cmd_line, cmdSegs);
    if(_isBackgroundComamnd(cmd_line)) backRound = true;
}
Command::~Command(){}
std::string Command::printCmd()
{
    return cmdLine;
}
int Command::getProcPid()
{
    return pid;
}
void Command::setProcPid(int pid)
{
    this->pid = pid;
}
bool Command::isAlias()
{
    return aliasChar != "";
}
std::string Command::getAliasCommand()
{
    return aliasChar;
}
void Command::setAliasCommand(std::string command)
{
    aliasChar = command;
}
void Command::setPath(std::string path)
{
    fileDirectedExt = path;
}
std::string Command::getPath()
{
    return fileDirectedExt;
}

BuiltInCommand::BuiltInCommand(const char *cmd_line):
Command(cmd_line)
{}
BuiltInCommand::~BuiltInCommand()
{}

void changeSmashPrompt::execute()
{
    if(cmdSegs.size() > 1)
    {
        std::string prompt = cmdSegs[1];
        if (!prompt.empty() && prompt.back() == '&') {
            prompt.pop_back();
        }

        if(prompt.empty())
        {
            newPrompt = "smash";
            return;
        }
        newPrompt = prompt;
    }
    else
    {
        newPrompt = "smash";
    }
    SmallShell::getInstance().setPrompt(newPrompt);
}

void ShowPidCommand::execute()
{
    std::cout << "smash pid is " << SmallShell::getInstance().getShellPid() << std::endl;
}

void GetCurrDirCommand::execute()
{
    std::cout << SmallShell::getInstance().getPWD() << std::endl;
}

void ChangeDirCommand::execute()
{
    if(cmdSegs.size() > 2)
    {
        if(cmdSegs[cmdSegs.size() - 1] == "&")
        {
            cmdSegs.pop_back();
        }
    }
    if(cmdSegs.size() == 1)
    {
        SmallShell::getInstance().changePWD(SmallShell::getInstance().getPWD());
        return;
    }
    if(cmdSegs.size() == 2)
    {
        std::string path = cmdSegs[1];
        if (!path.empty() && path.back() == '&') {
            path.pop_back();
        }
        if(path == "-")
        {
            if(SmallShell::getInstance().getPrevPWD() == "")
            {
                std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
            }
            else
            {
                if(chdir(SmallShell::getInstance().getPrevPWD().c_str()) != -1)
                {
                    SmallShell::getInstance().changePWD(SmallShell::getInstance().getPrevPWD());
                }
                else
                {
                    perror("smash error: chdir failed");
                }
            }
        }
        else
        {
            if(chdir(path.c_str()) == -1)
            {
                perror("smash error: chdir failed");
            }
            else
            {
                SmallShell::getInstance().changePWD(path);
            }
        }
    }
    else
    {
        std::cerr << "smash error: cd: too many arguments" << std::endl;
    }
}

JobsCommand::JobsCommand(const char *cmd_line, JobsList *jobs):
BuiltInCommand(cmd_line),
jobs(jobs)
{}
void JobsCommand::execute()
{
    jobs->printJobsList();
}

ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs):
BuiltInCommand(cmd_line),
jobs(jobs)
{}
void ForegroundCommand::execute()
{
    {
        int jobId = 0;
        if(cmdSegs.size() == 1)
        {
            if(jobs->list.empty())
            {
                std::cerr << "smash error: fg: jobs list is empty" << std::endl;
                return;
            }
            else
            {
                jobId=jobs->getLastJob()->jobId;
            }
        }
        else
        {
            if(cmdSegs.size() == 2)
            {
                try
                {
                    jobId = stoi(cmdSegs[1]);
                }
                catch(const std::exception& e)
                {
                    std::cerr << "smash error: fg: invalid arguments" << std::endl;
                    return;
                }
                if(jobId < 0)
                {
                    std::cerr << "smash error: fg: invalid arguments" << std::endl;
                    return;
                }

            }
            else
            {
                std::cerr << "smash error: fg: invalid arguments" << std::endl;
                return;
            }
        }
        JobsList::JobEntry* job = jobs->getJobById(jobId);
        if(!job)
        {
            std::cerr << "smash error: fg: job-id " << jobId << " does not exist" << std::endl;
            return;
        }
        std::string line = job->command;
        std::cout << job->command << " " << job->pid << std::endl;
        int pid = job->pid;
        jobs->removeJobById(jobId);
        SmallShell::getInstance().bringProcessToFront(pid);

        if(SmallShell::getInstance().getBackRoundWatch() == pid)
        {
            SmallShell::getInstance().setbackGroundWatch(-1);
            if (!line.empty() && line.back() == '&') {
                line.pop_back();
            }
            kill(pid,SIGKILL);
            SmallShell::getInstance().executeCommand(line.c_str());
        }
        else
        {
            while (waitpid(pid, NULL, 0) == -1 && errno == EINTR) {}

            signal(SIGTTOU, SIG_IGN);
            tcsetpgrp(STDIN_FILENO, getpgrp()); 
            signal(SIGTTOU, SIG_DFL);

        }
        SmallShell::getInstance().leaveProcess();
    }
}

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs):
BuiltInCommand(cmd_line),
jobs(jobs)
{}
void QuitCommand::execute()
{
    if (cmdSegs.size() >= 2 && cmdSegs[1] == "kill") {
        jobs->removeFinishedJobs();
        std::cout << "smash: sending SIGKILL signal to " << jobs->list.size() << " jobs:" << std::endl;
        jobs->killAllJobs();
    }
    exit(0);
}

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs):
BuiltInCommand(cmd_line),
jobs(jobs)
{}
void KillCommand::execute()
{
    if(cmdSegs.size() != 3) std::cerr << "smash error: kill: invalid arguments" << std::endl;
    else
    {
        if((cmdSegs[1])[0] != '-')
        {
            std::cerr << "smash error: kill: invalid arguments" << std::endl;
        }
        else
        {
            try
            {
                int signal = stoi(cmdSegs[1].substr(1, cmdSegs[1].size() - 1));
                int jobId = stoi(cmdSegs[2]);
                auto job = jobs->getJobById(jobId);
                if(!job)
                {
                    std::cerr << "smash error: kill: job-id " << jobId << " does not exist" << std::endl;
                    return;
                }
                int pid = job->pid;
                std::cout << "signal number " << signal << " was sent to pid " << pid << std::endl;
                if(kill(pid,signal) != 0) perror("smash error: kill failed");
            }
            catch(const std::exception& e)
            {
                std::cerr << "smash error: kill: invalid arguments" << std::endl;
                return;
            }
        }
    }
}

void AliasCommand::execute()
{
    if(cmdSegs.size() == 1)
    {
        std::vector<std::string> alias;
        SmallShell::getInstance().getAllAlias(alias);
        for(auto str : alias)
        {
            std::cout << str << std::endl;
        }
        return;
    }
    std::string fullString = "";
    for(auto str : cmdSegs)
    {
        fullString += str + " ";
    }
    if(!fullString.empty())
    {
        fullString = fullString.substr(0, fullString.size() - 1);
    }
    if(!fullString.empty() && this->backRound)
    {
        fullString = fullString.substr(0, fullString.size() - 1);
    }
    int first = -1;
    int last = -1;
    std::regex expression("^alias [a-zA-Z0-9_]+='[^']*'$");
    if(!std::regex_match(fullString, expression))
    {
        std::cerr << "smash error: alias: invalid alias format" << std::endl;
        return;
    }
    std::string temp = fullString.substr(cmdSegs[0].size() + 1, fullString.size() - cmdSegs[0].size() - 1);
    int size = temp.size();
    for(int i = 0; i < size; i++)
    {
        if(temp[i] == '\'')
            if (first == -1) {
                first = i;
            }
            else
            {
                if(last == -1) last = i;
                else
                {
                    std::cerr << "smash error: alias: invalid alias format" << std::endl;
                    return;
                }
            }
    }
    std::string alias = temp.substr(0, first - 1);
    std::string command = temp.substr(first + 1, last - first - 1);
    if(SmallShell::getInstance().getAlias(alias).compare("")!=0)
    {
        std::cerr << "smash error: alias: " << alias << " already exists or is a reserved command"  << std::endl;
        return;
    }
    if(SmallShell::getInstance().validCommand(alias))
    {
        std::cerr << "smash error: alias: " << alias << " already exists or is a reserved command" << std::endl;
        return;
    }
    SmallShell::getInstance().setAlias(alias, command);
}

void UnAliasCommand::execute()
{
    if(cmdSegs.size() == 1)
    {
        std::cerr << "smash error: unalias: not enough arguments" << std::endl;
    }
    else
    {
        int size = cmdSegs.size();
        for(int i = 1; i < size; i++)
        {
            if(SmallShell::getInstance().getAlias(cmdSegs[i]).compare("")==0)
            {
                std::cerr << "smash error: unalias: " << cmdSegs[i] << " alias does not exist" << std::endl;
                return;
            }
            SmallShell::getInstance().removeAlias(cmdSegs[i]);
        }
    }
}

ExternalCommand::ExternalCommand(const char *cmd_line,std::string cmd_s,bool isBackround):
Command(cmd_line),
argc()
{
    backRound = isBackround;
    std::istringstream iss(cmd_s);
    std::string token;

    while (iss >> token) {
        argc.push_back(token);
    }
}
bool withDirection(std::string line)
{
    if(line.empty())
    {
        return false;
    }
    if(line[0] == '/')
    {
        return true;
    }
    if(line.size() == 1)
    {
        return false;
    }
    if(line[0] == '.' && line[1] == '/')
    {
        return true;
    }
    if(line.size() == 2)
    {
        return false;
    }
    return (line[0] == '.' && line[1] == '.' && line[2] == '/');
}
void ExternalCommand::execute()
{
    pid_t pid = fork();
    if(pid < 0)
    {
        perror("smash error: fork failed");
        return;
    }
    if(pid == 0) //Child
    {
        setpgrp();
        if (!backRound) {
            tcsetpgrp(STDIN_FILENO, getpgrp());
        }

        if(specialCommand(cmdLine)){
            const char* bash_path = "/bin/bash";
            const char* bash_args[] = {"bash", "-c", cmdLine, nullptr};
            execv(bash_path, (char* const*)bash_args);
            perror("smash error: execv failed");
            exit(1);
        }
        std::vector<char*> args;
        for (const auto &arg : argc) args.push_back(const_cast<char*>(arg.c_str()));
        args.push_back(nullptr);

        execvp(args[0], args.data());
        perror("smash error: execvp failed");
        exit(1);
    }
    else //Parent
    {
        this->setProcPid(pid);
        if(backRound)
        {
            JobsList* jobs = SmallShell::getInstance().getJobs();
            jobs->addJob(this,pid);
        }
        else
        {
            SmallShell::getInstance().bringProcessToFront(pid);

            int status;
            while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}

            signal(SIGTTOU, SIG_IGN);
            tcsetpgrp(STDIN_FILENO, getpgrp()); 
            signal(SIGTTOU, SIG_DFL);

            SmallShell::getInstance().leaveProcess();
            return;
        }
    }
}
bool isDirectoryOrFile(const char *pathToElement, const char *element,bool directory)
{
    struct stat statStruct;
    std::string full_path = std::string(pathToElement) + "/" + element;
    if (stat(full_path.c_str(), &statStruct) == -1)
    {
        return false;
    }
    if(directory)
    {
        return S_ISDIR(statStruct.st_mode);
    }
    return S_ISREG(statStruct.st_mode);
}
void printSortedPrintingLine(std::string path,std::vector<std::string>&all,std::vector<std::string>&files,std::vector<std::string>&directories)
{
    for (const auto &elem : all)
    {
        if (isDirectoryOrFile(path.c_str(), elem.c_str(),true))
            directories.push_back(elem);
        else
            if(isDirectoryOrFile(path.c_str(), elem.c_str(),false))
                files.push_back(elem);
    }

    //files first
    std::sort(files.begin(),files.end());
    for (std::string file:files)
        if(!file.empty() && file[0]!='.')
                std::cout << "file: " << file  << std::endl;

    //directories second
    std::sort(directories.begin(),directories.end());
    for (std::string directory:directories)
        if(!directory.empty() && directory[0]!='.')
                std::cout << "directory: " << directory  << std::endl;

}

RedirectionCommand::RedirectionCommand(const char *cmd_line):
Command(cmd_line)
{
    relPath = cmdSegs[cmdSegs.size() - 1];
}
void RedirectionCommand::execute()
{
    std::string line = cmdLine;
    size_t redirPos = line.find(">>");
    bool append = true;
    size_t redirLen = 2;

    if (redirPos == std::string::npos) {
        append = false;
        redirPos = line.find(">");
        redirLen = 1;
    }

    if (redirPos == std::string::npos) {
        return;
    }

    std::string realCommand = _trim(line.substr(0, redirPos));
    std::string finalPath = _trim(line.substr(redirPos + redirLen));

    if (!finalPath.empty() && finalPath.back() == '&') {
        finalPath.pop_back();
        finalPath = _trim(finalPath);
    }

    int fd;
    if (append)
    {
        fd = open(finalPath.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
    }
    else
    {
        fd = open(finalPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    }

    if(fd == -1)
    {
        perror("smash error: open failed");
        return;
    }

    int old_stdout = dup(STDOUT_FILENO);
    if (old_stdout == -1) {
        perror("smash error: dup failed");
        close(fd);
        return;
    }

    if (dup2(fd, STDOUT_FILENO) == -1) {
        perror("smash error: dup2 failed");
        close(fd);
        close(old_stdout);
        return;
    }
    close(fd);
    
    this->cmdSegs.clear();
    createVectorForLine(realCommand.c_str(), this->cmdSegs);

    SmallShell &smash = SmallShell::getInstance();
    smash.executeCommand(realCommand.c_str(), "intercepted_routing");

    dup2(old_stdout, STDOUT_FILENO);
    close(old_stdout);
}

void PipeCommand::execute()
{
    std::string line = cmdLine;
    size_t pipePos = line.find("|&");
    bool apperSymbol = false; 
    size_t pipeLen = 1;

    if (pipePos != std::string::npos) {
        apperSymbol = true;
        pipeLen = 2;
    } else {
        pipePos = line.find("|");
    }

    if (pipePos == std::string::npos) {
        std::cerr << "smash error: pipe error" << std::endl;
        return;
    }

    std::string firstCommand = line.substr(0, pipePos);
    std::string secondCommand = line.substr(pipePos + pipeLen);

    int pipeSides[2];
    if (pipe(pipeSides) == -1)
    {
        perror("smash error: pipe failed");
        return;
    }

    pid_t p1 = fork();
    if (p1 == 0) {
        setpgrp();
        if (apperSymbol) {
            dup2(pipeSides[1], STDERR_FILENO);
        } else {
            dup2(pipeSides[1], STDOUT_FILENO);
        }
        close(pipeSides[0]);
        close(pipeSides[1]);

        SmallShell::getInstance().executeCommand(firstCommand.c_str());
        exit(0);
    }

    pid_t p2 = fork();
    if (p2 == 0) {
        setpgrp();
        dup2(pipeSides[0], STDIN_FILENO);
        close(pipeSides[0]);
        close(pipeSides[1]);

        SmallShell::getInstance().executeCommand(secondCommand.c_str());
        exit(0);
    }

    close(pipeSides[0]);
    close(pipeSides[1]);

    while (waitpid(p1, nullptr, 0) == -1 && errno == EINTR);
    while (waitpid(p2, nullptr, 0) == -1 && errno == EINTR);
}

SysInfoCommand::SysInfoCommand(const char *cmd_line) :
BuiltInCommand(cmd_line)
{}
void SysInfoCommand::execute() {
    struct utsname uts;
    if (uname(&uts) == -1) {
        perror("smash error: uname failed");
        return;
    }

    struct sysinfo info;
    if (sysinfo(&info) == -1) {
        perror("smash error: sysinfo failed");
        return;
    }

    time_t cur_time = time(nullptr);
    time_t boot_timestamp = cur_time - info.uptime;

    struct tm *bt = localtime(&boot_timestamp);

    char boot_time_str[30];
    strftime(boot_time_str, sizeof(boot_time_str), "%Y-%m-%d %H:%M:%S", bt);

    std::cout << "System: "         << uts.sysname      << std::endl;
    std::cout << "Hostname: "       << uts.nodename     << std::endl;
    std::cout << "Kernel: "         << uts.release      << std::endl;
    std::cout << "Architecture: "   << uts.machine      << std::endl;
    std::cout << "Boot Time: "      << boot_time_str    << std::endl;
}

WhoAmICommand::WhoAmICommand(const char *cmd_line) :
Command(cmd_line)
{}
void WhoAmICommand::execute() {
    uid_t uid = getuid();
    gid_t gid = getgid();

    int fd = open("/etc/passwd", O_RDONLY);
    if (fd == -1) {
        perror("smash error: open failed");
        return;
    }

    char ch;
    std::string current_line = "";
    std::string username = "";
    std::string home_dir = "";

    while (read(fd, &ch, 1) > 0) {
        if (ch == '\n') {
            std::stringstream ss(current_line);
            std::string item;
            std::vector<std::string> tokens;
            while (std::getline(ss, item, ':')) {
                tokens.push_back(item);
            }
            //            [0]     [1]    [2] [3]  [4]    [5]
            // tokens: username:password:UID:GID:gecos:home_dir:shell
            if (tokens.size() >= 6) {
                if (tokens[2] == std::to_string(uid)) {
                    username = tokens[0];
                    home_dir = tokens[5];
                    break;
                }
            }
            current_line = "";
        } else {
            current_line += ch;
        }
    }
    close(fd);

    if (!username.empty()) {
        std::cout << username   << std::endl;
        std::cout << uid        << std::endl;
        std::cout << gid        << std::endl;
        std::cout << home_dir   << std::endl;
    }
}

static long long calculate_dir_space(const std::string &path) {
    long long total_size = 0;

    // system struct (https://man7.org/linux/man-pages/man2/stat.2.html)
    struct stat st;
    if (lstat(path.c_str(), &st) == -1) {
        return 0;
    }

    // https://man7.org/linux/man-pages/man7/inode.7.html
    if (S_ISLNK(st.st_mode)) {
        return 0;
    }
    total_size += st.st_blocks * 512;

    if (S_ISDIR(st.st_mode)) {
        int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
        if (fd != -1) {
            char buf[4096];
            int nread;
            
            // https://man7.org/linux/man-pages/man2/getdents.2.html
            while ((nread = syscall(SYS_getdents64, fd, buf, sizeof(buf))) > 0) {
                for (int bpos = 0; bpos < nread;) {
                    struct direntInfo *d = (struct direntInfo *) (buf + bpos);
                    std::string name(d->d_name);

                    if (name != "." && name != "..") {
                        std::string next_path = path + "/" + name;
                        total_size += calculate_dir_space(next_path);
                    }
                    bpos += d->d_rec;
                }
            }
            close(fd);
        }
    }
    return total_size;
}
DiskUsageCommand::DiskUsageCommand(const char *cmd_line) :
Command(cmd_line)
{}
void DiskUsageCommand::execute() {
    if (cmdSegs.size() > 2) {
        std::cerr << "smash error: du: too many arguments" << std::endl;
        return;
    }

    std::string path = (cmdSegs.size() == 2) ? _trim(cmdSegs[1]) : ".";
    if (!path.empty() && path.back() == '&') {
        path.pop_back();
        path = _trim(path);
    }

    long long size_in_bytes = calculate_dir_space(path);

    long long size_in_kb = size_in_bytes / 1024;
    if (size_in_bytes % 1024 != 0) {
        size_in_kb += 1;
    }

    std::cout << "Total disk usage: " << size_in_kb << " KB" << std::endl;
}

UnSetEnvCommand::UnSetEnvCommand(const char *cmd_line) :
BuiltInCommand(cmd_line)
{}
void UnSetEnvCommand::execute() {
    if (cmdSegs.size() < 2) {
        std::cerr << "smash error: unsetenv: not enough arguments" << std::endl; 
        return;
    }

    extern char **environ;

    for (size_t i = 1; i < cmdSegs.size(); ++i) {
        std::string var_name = cmdSegs[i];
        if (!var_name.empty() && var_name.back() == '&') {
            var_name.pop_back();
        }

        int fd = open("/proc/self/environ", O_RDONLY);
        if (fd == -1) {
            perror("smash error: open failed");
            return;
        }

        char buf[4096];
        ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
        close(fd);

        if (bytes < 0) {
            perror("smash error: read failed");
            return;
        }

        bool exists = false;
        ssize_t start = 0;
        for (ssize_t j = 0; j < bytes; ++j) {
            if (buf[j] == '\0') {
                std::string entry(&buf[start]);
                if (entry.find(var_name + "=") == 0) {
                    exists = true;
                    break;
                }
                start = j + 1;
            }
        }

        if (!exists) {
            std::cerr << "smash error: unsetenv: " << var_name << " does not exist" << std::endl;
            return;
        }

        for (char **env = environ; *env != nullptr; ++env) {
            std::string current_entry(*env);
            if (current_entry.find(var_name + "=") == 0) {
                for (char **next = env; *next != nullptr; ++next) {
                    *next = *(next + 1);
                }
                break;
            }
        }
    }
}

USBInfoCommand::USBInfoCommand(const char *cmd_line) :
Command(cmd_line)
{}
static std::string read_usb_file(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) return "";

    char buf[MAX_SIZE];
    ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (bytes <= 0) return "";
    buf[bytes] = '\0';

    std::string line(buf);
    size_t pos = line.find('\n');
    if (pos != std::string::npos) {
        line = line.substr(0, pos);
    }
    return _trim(line);
}
void USBInfoCommand::execute() {
    int dir_fd = open("/sys/bus/usb/devices", O_RDONLY | O_DIRECTORY);
    if (dir_fd == -1) {
        std::cerr << "smash error: usbinfo: no USB devices found" << std::endl;
        return;
    }

    std::vector<USBDevice> devices;
    char buf[4096];
    int nread;
    // https://man7.org/linux/man-pages/man2/getdents.2.html
    while ((nread = syscall(SYS_getdents64, dir_fd, buf, sizeof(buf))) > 0) {
        for (int bpos = 0; bpos < nread;) {
            struct direntInfo *d = (struct direntInfo *) (buf + bpos);

            std::string name(d->d_name);
            bpos += d->d_rec;

            if (name == "." || name == "..") continue;

            std::string base_path = "/sys/bus/usb/devices/" + name;

            std::string devnum_str = read_usb_file(base_path + "/devnum");
            std::string vendor = read_usb_file(base_path + "/idVendor");
            std::string product = read_usb_file(base_path + "/idProduct");

            if (devnum_str.empty() || vendor.empty() || product.empty()) continue;

            int devnum = 0;
            try {
                devnum = std::stoi(devnum_str);
            } catch (...) {
                continue;
            }

            if (vendor == "1d6b") continue;

            std::string manufacturer = read_usb_file(base_path + "/manufacturer");
            if (manufacturer.empty()) manufacturer = "N/A";

            std::string product_name = read_usb_file(base_path + "/product");
            if (product_name.empty()) product_name = "N/A";

            std::string max_power = read_usb_file(base_path + "/bMaxPower");
            if (max_power.empty()) {
                max_power = "N/A";
            } else {
                max_power.erase(std::remove(max_power.begin(), max_power.end(), ' '), max_power.end());
            }

            devices.push_back({devnum, vendor, product, manufacturer, product_name, max_power});
        }
    }
    close(dir_fd);

    if (devices.empty()) {
        std::cerr << "smash error: usbinfo: no USB devices found" << std::endl;
        return;
    }

    std::sort(devices.begin(), devices.end());

    for (const auto& d : devices) {
        std::cout << "Device " << d.devnum
                  << ": ID " << d.vendor_id << ":" << d.product_id
                  << " " << d.manufacturer
                  << " " << d.product_name
                  << " MaxPower: " << d.max_power << std::endl;
    }
}
