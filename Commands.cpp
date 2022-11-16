#include <unistd.h>
#include <string.h>
#include <iostream>
#include <utility>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <time.h>
#include <utime.h>
#include <memory>
#include <algorithm>
#include <cassert>
#include <fcntl.h>
#include <stack>
#include "signals.h"


using namespace std;
#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif



void _perror(const char * syscall){
    string _syscall = string (syscall);
    unsigned int first = _syscall.find('=');
    unsigned int last = _syscall.find('(') - 1;
    if (first != string::npos){
        first += 2;
        _syscall = _syscall.substr(first, last - first + 1);
    }
    else
        _syscall = _syscall.substr(0, last + 1);
    string temp = "smash error: ";
    temp +=  _syscall;
    temp += " failed";
    perror(temp.c_str());
}

bool containsOnlyNums(const string & arg, bool is_first_arg_of_kill_cmd);
const std::string WHITESPACE = " \n\r\t\f\v";

int findOperandIndexInVec(vector<string> &parameters);
string _ltrim(const std::string& s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for(std::string s; iss >> s; ) {
        args[i] = (char*)malloc(s.length()+1);
        memset(args[i], 0, s.length()+1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;

    }
    return i;

    FUNC_EXIT()
}
vector<string> _LineToParams(const string& cmd_line){
    vector<string> param;
    string cmd_without_tabs = _trim(string(cmd_line));
    replace(cmd_without_tabs.begin(), cmd_without_tabs.end(), '\t', ' ');
    std::istringstream temp(cmd_without_tabs);
    for(string s; temp >> s; ) {
        param.push_back(s);
    }
    if(!param.empty()) {
        param.erase(param.begin());
    }
    return param;
}

bool _isBackgroundComamnd(const string& cmd_line) {
    const string& str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
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
Command::Command(const std::string &cmd_line) : cmd_line(cmd_line) {
    param = _LineToParams(cmd_line);
}
ShowPidCommand::ShowPidCommand(const std::string &cmd_line, SmallShell &smash) : BuiltInCommand(cmd_line), smash(smash) {}

void ShowPidCommand::execute() {
    if (smash.getPipeChild())
        cout << "smash pid is " << getppid() << endl;
    else
        cout << "smash pid is " << getpid() << endl;
}
ChangeDirCommand::ChangeDirCommand(const std::string &cmd_line, SmallShell &smash)
        : BuiltInCommand(cmd_line),smash(smash){}

void ChangeDirCommand::execute() {
    if (param.size() > 1){
        cerr << "smash error: cd: too many arguments" << endl;
        return;
    }
    char *curDir = get_current_dir_name();
    if (curDir == nullptr) {
        perror("smash error: get_current_dir_name failed");
        return;
    }
    int y;
    if (param.at(0) == "-"){
        if(smash.getPlastPwd().empty())
            cerr << "smash error: cd: OLDPWD not set" << endl;
        else{
            DO_SYS(y = chdir(smash.getPlastPwd().c_str()));
            smash.setPlastPwd(curDir);
        }
    } else{
        DO_SYS(y = chdir(param.at(0).c_str()));
        smash.setPlastPwd(curDir);
    }
    free(curDir);
}
int findOperandIndexInVec(vector<string> &parameters){
    for(unsigned int i = 0  ;  i < parameters.size() ; i++){
        if (parameters.at(i) == ">" || parameters.at(i) == ">>" ||parameters.at(i) == "|" || parameters.at(i) == "|&"){
            return (int)i;
        }
    }
    return -1;
}
TailCommand::TailCommand(const std::string &cmd_line) : BuiltInCommand(cmd_line){}
void TailCommand::execute() {
    int n;
    if (param.size() > 2 || param.empty() ||
    (param.size() == 2 && (param.at(0).at(0) != '-' || !containsOnlyNums(param.at(0), true)))){
        cerr << "smash error: tail: invalid arguments"<<endl;
        return;
    }
    n = param.size() == 1 ? 10 : abs(stoi(param.at(0)));
    int index = param.size() == 1 ? 0 : 1;
    int fd, read_result;
    off_t pos;
    char charBuf;
    bool does_it_end_with_new_line = false;
    std::stack<char> temp_to_print;
    DO_SYS(fd =  open(param.at(index).c_str(), O_RDONLY | O_APPEND));
    DO_SYS(pos = lseek(fd, 0 ,SEEK_END));//check if empty
    if (pos == 0 || n == 0)
        return;
    DO_SYS(pos = lseek(fd, -1 ,SEEK_END));
    DO_SYS( read_result = read(fd, &charBuf, 1));
    does_it_end_with_new_line = charBuf == '\n';
    int count =  does_it_end_with_new_line ? 0 : 1;
    int  i = 2;
    while (pos >= 0){
        if (i > 2)
            DO_SYS( read_result = read(fd, &charBuf, 1));
        assert(read_result == 1);
        if (charBuf == '\n'){
            if (count++ == n)
                break;
        }
        temp_to_print.push(charBuf);
        if (pos == 0)
            break;
        DO_SYS(pos = lseek(fd, -i ,SEEK_END));
        i++;
    }
    string to_print;
    while (!temp_to_print.empty()){
        to_print += temp_to_print.top();
        temp_to_print.pop();
    }
    cout << to_print;
    int res;
    DO_SYS(res = close(fd));
}
KillCommand::KillCommand(const std::string &cmd_line, SmallShell &smash) : BuiltInCommand(cmd_line), smash(smash){}
bool containsOnlyNums(const string & arg, bool is_first_arg_of_kill_cmd){
    bool first_of_first = is_first_arg_of_kill_cmd && arg.at(0)  == '-';
    for(auto i : arg){
        if (first_of_first){
            first_of_first = false;
            continue;
        }
        if (i > '9' || i < '0')
            return false;
    }
    return true;
}
void KillCommand::execute() {
    if (param.size() != 2 || param.at(0).at(0) != '-' ||
        !containsOnlyNums(param.at(0), true)||
        !containsOnlyNums(param.at(1), true)||
        stoi(param.at(0).substr(1)) > 32){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    if (!smash.getJobsList()->ThisidExist(stoi(param.at(1))))
        cerr << "smash error: kill: job-id "<< stoi(param.at(1)) << " does not exist" << endl;
    else{
        int stat;
        pid_t jobPid = smash.getJobsList()->getJobById(stoi(param.at(1)));
        DO_SYS(stat = kill(jobPid, abs(stoi(param.at(0)))));
        if(stat == 0)
            cout << "signal number " << abs(stoi(param.at(0))) << " was sent to pid " << jobPid << endl;
    }//what if -9 is sent? is the above statement printed?
}
BackgroundCommand::BackgroundCommand(const std::string &cmd_line, SmallShell &smash) : BuiltInCommand(cmd_line),
                                                                                       smash(smash) {}
void BackgroundCommand::execute() {
    shared_ptr<JobsList::JobEntry> next_stopped_job = smash.getJobsList()->HandleStopJob();
    if (param.size() > 1 || (!param.empty() && !containsOnlyNums(param.at(0), true))){
        cerr << "smash error: bg: invalid arguments" << endl;
        return;
    }
    if (!param.empty()){
        int job_id = stoi(param.at(0));
        if (!smash.getJobsList()->ThisidExist(job_id))
            cerr << "smash error: bg: job-id " << job_id << " does not exist" << endl;
        else if (!smash.getJobsList()->RunJob(job_id))
            cerr << "smash error: bg: job-id " << job_id << " is already running in the background" << endl;
    }else if (smash.getJobsList()->EmptyList() || !next_stopped_job)
        cerr << "smash error: bg: there is no stopped jobs to resume" << endl;
    else{
        pid_t next_stopped = next_stopped_job->GetJobProcessId();
        std::cout << next_stopped_job->GetCommandLine() << " : " << next_stopped << std::endl;
        int res;
        DO_SYS(res = kill(next_stopped, SIGCONT));
        next_stopped_job->ContiniueRun();
    }

}
void JobsList::JobEntry::PrintStopped() {
    std::cout << " (stopped)";
}
std::vector<std::string> Command::getParams() const {
    return param;
}

void Command::setParams(std::vector<std::string> new_paras) {
    param = std::move(new_paras);
}
BuiltInCommand::BuiltInCommand(const std::string &cmd_line){
    string _cmd_line = cmd_line;
    _removeBackgroundSign(const_cast<char *>(_cmd_line.c_str()));
    this->cmd_line = _cmd_line;
    param = _LineToParams(_cmd_line);
}
void JobsList::JobEntry::Print() {
    std::cout <<  "[" << job_id << "]" << " " << string(line) << " : " <<
              process_id << " " << difftime(time(nullptr),timeIn) << " secs";
}

void JobsList::printJobsList() {
    this->removeFinishedJobs();
    for(auto & i : job_list){
        if(i->IsStopped()){
            i->Print();
            i->PrintStopped();
            std::cout << "" << std::endl;
        } else{
            i->Print();
            std::cout << "" << std::endl;
        }
    }
}
double JobsList::JobEntry::ReamainTime() {
    double tt = difftime(time(nullptr),timeIn);
    return (duration - tt);
}

SmallShell::SmallShell() {
}

SmallShell::~SmallShell() {
// TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/

char** _LineToParams2(const string& cmd_line){
    char** arr = new char*[4];
    string s = "/bin/bash";
    arr[0] = new char[10];
    strcpy(arr[0],s.data());
    string ss = "-c";
    arr[1] = new char [3];
    strcpy(arr[1],ss.data());
    arr[2] = new char [cmd_line.size() + 1];
    strcpy(arr[2],cmd_line.data());
    arr[3] = (char*)NULL;
    return arr;
}
void chprompt::execute() {
    SmallShell& shl = SmallShell::getInstance();
    string temp = cmd_line.substr(8, cmd_line.length());
    temp = _trim(temp);
    string s = temp.substr(0, temp.find_first_of(WHITESPACE));
    if(s.empty())
        shl.updateline("smash> ");
    else {
        s.append(">");
        s.append(" ");
        shl.updateline(s);
    }
}
shared_ptr<Command> SmallShell::CreateCommand(const string& cmd_line){
    job_list->removeFinishedJobs();
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    if (firstWord == "chprompt")
        return make_shared<chprompt>(cmd_s, getInstance());
    if (firstWord == "pwd")
        return make_shared<GetCurrDirCommand>(cmd_s);
    if (firstWord == "jobs")
        return make_shared<JobsCommand>(cmd_s, job_list);
    if(firstWord == "fg")
        return make_shared<ForegroundCommand> (cmd_s, job_list);
    if(firstWord == "quit")
        return make_shared<QuitCommand>(cmd_s,job_list);
    if(firstWord == "touch")
        return make_shared<TouchCommand>(cmd_s);
    if (firstWord == "showpid")
        return make_shared<ShowPidCommand> (cmd_s, getInstance());
    if(firstWord == "cd")
        return make_shared<ChangeDirCommand>(cmd_s, getInstance());
    if (firstWord == "tail")
        return make_shared<TailCommand>(cmd_s);
    if(firstWord == "kill")
        return make_shared<KillCommand>(cmd_s, getInstance());
    if (firstWord == "bg")
        return make_shared<BackgroundCommand>(cmd_s, getInstance());
    if(firstWord == "timeout")
        return make_shared<TimeOutCommand>(cmd_s, job_list, timed_out);
    return make_shared<ExternalCommand>(cmd_s,job_list);
}

void JobsList::addtimedoutjob(int pid, int duration, const char* linek) {
    shared_ptr<JobEntry> new_job(new JobEntry(1,pid,time(nullptr),linek,duration));
    job_list.push_back(new_job);
    sort(job_list.begin(),job_list.end(),Compare);
    alarm(job_list[0]->ReamainTime());
}
void TimeOutCommand::execute() {
    int res;
    string without = cmd_line.substr(7,cmd_line.size());
    without = _trim(string(without));
    if(without.empty() || without == "&"){
        cerr << "smash error: timeout: invalid arguments" << endl;
        return;
    }
    string first = without.substr(0, without.find_first_of(" \n"));
    if(!containsOnlyNums(first, false)){
        cerr << "smash error: timeout: invalid arguments" << endl;
        return;
    }
    without = without.substr(first.size(),cmd_line.size());
    without = _trim(string(without));
    if(without.empty()|| without == "&"){
        cerr << "smash error: timeout: invalid arguments" << endl;
        return;
    }
    if(!_isBackgroundComamnd(cmd_line)) {
        int x;
        DO_SYS(x = fork());
        if (x == 0) {
            DO_SYS(res = setpgrp());
            char **a = _LineToParams2(without);
            DO_SYS(res = execv(a[0], a));
        } else {
            int status;
            shared_ptr<JobsList> timee = timed_out_list;
            SmallShell& tempi = SmallShell::getInstance();
            string a = tempi.originial();
            char* knew1 = new char[a.size() + 1];
            strcpy(knew1,tempi.originial());
            timee->addtimedoutjob(x,stoi(first),knew1);
            tempi.UpdatePid(x);
            waitpid(x, &status, WUNTRACED);
            if(WIFSTOPPED(status)){
                char* knew = new char[a.size() + 1];
                strcpy(knew,tempi.originial());
                job_liist->removeFinishedJobs();
                job_liist->addJob(knew,x,true);
            }
        }
    }
    else{
        int y;
        DO_SYS(y = fork());
        if(y == 0) {
            DO_SYS(res = setpgrp());
            without = without.substr(0, without.size() - 1);
            char **a = _LineToParams2(without);
            DO_SYS(res = execv(a[0], a));
        } else{
            SmallShell& tempi = SmallShell::getInstance();
            string a = tempi.originial();
            char* knew = new char[a.size() + 1];
            strcpy(knew,tempi.originial());
            char* knew1 = new char[a.size() + 1];
            strcpy(knew1,tempi.originial());
            job_liist->removeFinishedJobs();
            job_liist->addJob(knew,y);
            shared_ptr<JobsList> timee = timed_out_list;
            timee->addtimedoutjob(y,stoi(first),knew1);
        }
    }
}
bool JobsList::Compare(const std::shared_ptr<JobEntry>& first, const std::shared_ptr<JobEntry>& second) {
        return ((first->ReamainTime()) < (second->ReamainTime()));
}
void ExternalCommand::execute() {
    int res;
    if(!_isBackgroundComamnd(cmd_line)) {
        int x;
        DO_SYS(x = fork());
        if (x == 0) {
            DO_SYS(res = setpgrp());
            char **a = _LineToParams2(cmd_line);
            DO_SYS(res = execv(a[0], a));
        } else {
            int status;
            SmallShell& shl = SmallShell::getInstance();
            shl.UpdatePid(x);
            waitpid(x, &status, WUNTRACED);
            if(WIFSTOPPED(status)){
                string a = shl.originial();
                char* knew = new char[a.size() + 1];
                strcpy(knew,shl.originial());
                job_lst->removeFinishedJobs();
                job_lst->addJob(knew,x,true);
            }
        }
    }
    else{
        int y;
        DO_SYS(y = fork());
        if(y == 0) {
            DO_SYS(res = setpgrp());
            string cutstring = cmd_line.substr(0, cmd_line.size() - 1);
            cutstring.append("\0");
            char **a = _LineToParams2(cutstring);
//            std::cout << string(a[2]) << std::endl;
            DO_SYS(res = execv(a[0], a));
        } else{
            job_lst->removeFinishedJobs();
            SmallShell& tempi = SmallShell::getInstance();
            string a = tempi.originial();
            char* knew = new char[a.size() + 1];
            strcpy(knew,tempi.originial());
            job_lst->addJob(knew,y);
        }
    }
}
string makeSpace(const char * cmd_line){
    string s = string (cmd_line);
    size_t pos = s.find_first_of("|&>");
    if(pos == string::npos)
        return s;
    int size_of_sign = ((pos + 1 < s.size()) && (s.at(pos + 1) == '>' || s.at(pos + 1) == '&'))
                       ? 2 : 1;
    string firstCmd = s.substr(0,pos);
    string secondCmd = s.substr(pos, size_of_sign);
    string third = s.substr(pos + size_of_sign);
    return firstCmd + ' ' + secondCmd + ' ' + third;
}

void SmallShell::executeCommand(const char *cmd_line) {
    orig_cmd_line = cmd_line;
    string temp = makeSpace(cmd_line);
    shared_ptr<Command> cmd = CreateCommand(temp);
    assert(cmd != nullptr);
    if (typeid(*cmd) == typeid(ExternalCommand)){
        cmd = CreateCommand(orig_cmd_line);
    }
    if (temp.find('|') != string::npos){
        makePipe(orig_cmd_line);
        return;
    }
    if (temp.find('>') != string::npos){
        shared_ptr<RedirectionCommand> redirect = make_shared<RedirectionCommand>(temp);
        redirect->execute();
        return;
    }
    cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}
std::string SmallShell::getPlastPwd() const{return plastPwd;}
void SmallShell::setPlastPwd(char *Dir) { plastPwd = Dir;}
bool SmallShell::getPipeChild() const {return pipeChild;}
void SmallShell::changePipeChildState(bool flag) {pipeChild = flag;}
std::shared_ptr<JobsList> SmallShell::getJobsList() {return job_list;}

// RedirectionCommand class
RedirectionCommand::RedirectionCommand(string &cmd_line) : BuiltInCommand(cmd_line) {}
void RedirectionCommand::execute() {
    //TODO check if valid num of arg or no args after >
    size_t pos = cmd_line.find('>');
    assert(pos != string::npos);
    string left = cmd_line.substr(0,pos);
    string right = cmd_line.substr(pos + 1);
    if (cmd_line.at(pos+1) == '>'){
        right = cmd_line.substr(pos+2);
    }
    right = _trim(right);
    unsigned int buf = findOperandIndexInVec(param);///
    int outputFD, res;
    if (access(right.c_str(), F_OK) == 0 && param.at(buf) == ">>")// file exist and ">>" op
        DO_SYS(outputFD = open(right.c_str(),O_RDWR | O_APPEND));
    else{
        DO_SYS(outputFD = open(right.c_str(), O_RDWR | O_CREAT | O_TRUNC,
                               S_IWUSR | S_IRUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH));
    }
    shared_ptr<Command> cmd = SmallShell::getInstance().CreateCommand(left);
    int screenFD;
    DO_SYS(screenFD = dup(1));
    DO_SYS(res = dup2(outputFD, 1));
    DO_SYS(res = close(outputFD));
    cmd->execute();
    DO_SYS(res = dup2(screenFD, 1));
    DO_SYS(res = close(screenFD));

}
//RedirectionCommand end
void SmallShell::makePipe(const string&cmd_line) {
    int myPipe[2];
    int myPipeTarget = 1;
    size_t pos = cmd_line.find('|');
    assert(pos != string::npos);
    string firstCmd = cmd_line.substr(0,pos);
    string secondCmd = cmd_line.substr(pos + 1);
    if (cmd_line.at(pos+1) == '&'){
        myPipeTarget = 2;
        secondCmd = cmd_line.substr(pos+2);
    }
    shared_ptr<Command> cmd1 = CreateCommand(firstCmd);
    shared_ptr<Command> cmd2 = CreateCommand(secondCmd);
    if (cmd2 == nullptr || cmd1 == nullptr)//need to check what should do if arguments are illegal
        return;
    int res;
    DO_SYS_WITH_NO_RETURN(res = pipe(myPipe));
    pid_t first;
    DO_SYS_WITH_NO_RETURN(first = fork());
    if (first == 0){//handle first son - (cmd1)
        setpgrp();//TODO  returns -1 on err or is it syscall?
        DO_SYS_WITH_EXIT(res = close(myPipe[0]));
        DO_SYS_WITH_EXIT(res = dup2(myPipe[1], myPipeTarget));
        DO_SYS_WITH_EXIT(res = close(myPipe[1]));
        pipeChild = true;// still not sure if it shouldn't be right after fork
        cmd1->execute();
        pipeChild = false;
        exit(0);
    }
    pid_t second;
    DO_SYS_WITH_NO_RETURN(second = fork());
    if (second == 0){// handle second son - (cmd2)
        setpgrp();
        DO_SYS_WITH_EXIT(res = close(myPipe[1]));
        DO_SYS_WITH_EXIT(res = dup2(myPipe[0], 0));
        DO_SYS_WITH_EXIT(res = close(myPipe[0]));
        pipeChild = true;// still not sure if it shouldn't be right after fork
        cmd2->execute();
        pipeChild = false;
        exit(0);
    }
    DO_SYS(res = close(myPipe[0]));
    DO_SYS(res = close(myPipe[1]));
    waitpid(first, nullptr, WUNTRACED);
    waitpid(second, nullptr, WUNTRACED);
}
void JobsList::addJob(const char* cmd, int id, bool isStopped) {
    int new_id = 1;
    if(!job_list.empty()){
        shared_ptr<JobEntry> temp = job_list[job_list.size()-1];
        new_id = temp->GetJobId() + 1;
    }
    shared_ptr<JobEntry> temp (new JobEntry(new_id, id, time(nullptr), cmd));
    if(isStopped)
        temp->StoppedJob();
    job_list.push_back(temp);
}
void JobsList::removeFinishedJobs(){
    int status = 0;
    for(auto & i : job_list){
        int check = waitpid(i->GetJobProcessId(), &status,WNOHANG);
        if(check == i->GetJobProcessId()) {
            i->FinishJob();
        }
    }
    job_list.erase(std::remove_if(job_list.begin(), job_list.end(), [](const std::shared_ptr<JobEntry>& a)
    {return a->IsFinished();}),job_list.end());
}
void JobsList::RemoveJobById(int id){
    int k = 0;
    for(auto & i : job_list){
        if(i->GetJobProcessId() == id){
            job_list.erase(job_list.begin() + k);
        }
        k++;
    }
};

void JobsList::HandleLastJob() {
    int id = job_list[job_list.size()-1]->GetJobProcessId();
    std::cout << job_list[job_list.size()-1]->GetCommandLine() << " : " << id << std::endl;
    int status = 0, res;
    SmallShell& shl = SmallShell::getInstance();
    shl.UpdatePid(id);
    DO_SYS(res = kill(id,SIGCONT));
    waitpid(id, &status, WUNTRACED);
    if(WIFSTOPPED(status)){
        job_list[job_list.size()-1]->UpdateTime();
        job_list[job_list.size()-1]->StoppedJob();
    } else{
        job_list.pop_back();
    }
}
void JobsList::HandNotLastJob(int jobId){
    int j = 0, status, res;
    for(auto & i : job_list){
        if(i->GetJobId() == jobId){
            SmallShell& shl = SmallShell::getInstance();
            shl.UpdatePid(i->GetJobProcessId());
            std::cout << job_list[j]->GetCommandLine() << " : " << shl.GetPid() << std::endl;
            DO_SYS(res = kill(shl.GetPid(),SIGCONT));
            waitpid(shl.GetPid(), &status, WUNTRACED);
            if(WIFSTOPPED(status)){
                job_list[j]->UpdateTime();
                job_list[j]->StoppedJob();
            } else{
                job_list.erase(job_list.begin()+j);
            }
            break;
        }
        j++;
    }
}
bool JobsList::RunJob(int id) {
    int res;
    for(auto & i : job_list){
        if(i->GetJobId() == id){
            if(i ->IsStopped()){
                std::cout << i->GetCommandLine() << " : " << i->GetJobProcessId() << std::endl;
                DO_SYS_WITH_NO_RETURN(res = kill(i->GetJobProcessId(), SIGCONT));
                i->ContiniueRun();
                return true;
            }
            else{
                return false;
            }
        }
    }
    return false;
}
void JobsList::PrintKilledJobs() const {
    int res;
    for(auto & i : job_list){
        std::cout << i->GetJobProcessId() << ": " << i->GetCommandLine() << std::endl;
        DO_SYS(res = kill(i->GetJobProcessId(),SIGKILL));
    }
}
void JobsList::alarmFirst(std::shared_ptr<JobsList> jobss) {
    int status;
    if(job_list.empty()){
        std::cout << "XXXXXXXXX" << std::endl;
        return;
    }
    shared_ptr<JobEntry> first = job_list[0];
    int check = waitpid(first->GetJobProcessId(), &status,WNOHANG);
    if(check == 0) {
        std::cout << "smash: " << first->GetCommandLine() << " timed out!" << std::endl;
        kill(first->GetJobProcessId(), SIGKILL);
    }
    if(check == 0 || check == first->GetJobProcessId()){
        jobss->FinishJob(first->GetJobProcessId());
    }
    job_list.erase(job_list.begin());
    if(job_list.empty()){
        return;
    }
    shared_ptr<JobEntry> sec = job_list[0];
    alarm(sec->ReamainTime());
}
void ForegroundCommand::execute() {
    string temp = cmd_line.substr(2, cmd_line.length());
    temp = _trim(string(temp));
    if(temp.empty()){
        if(job_lst->EmptyList())
            cerr<< "smash error: fg: jobs list is empty" << endl;
        else
            job_lst->HandleLastJob();
        return;
    }
    if(temp[0] == '-') {
        string temp1 = temp.substr(1, temp.size());
        for (char i : temp1) {
            if (i < '0' || i > '9') {
                cerr << "smash error: fg: invalid arguments" << endl;
                return;
            }
        }
    } else{
        for (char i : temp) {
            if (i < '0' || i > '9') {
                cerr << "smash error: fg: invalid arguments" << endl;
                return;
            }
        }}
    int id = stoi(temp);
    if(!job_lst->ThisidExist(id)){
        cerr<< "smash error: fg: job-id " << id <<  " does not exist" << endl;
        return;
    }
    job_lst->HandNotLastJob(id);
}
void QuitCommand::execute() {
    string temp = cmd_line.substr(4, cmd_line.length());
    temp = _trim(string(temp));
    string helper2 = temp.substr(0, temp.find_first_of(" \n"));
    if(helper2 == "kill"){
        std::cout << "smash: sending SIGKILL signal to " << job_lst->Getsize() << " jobs:" << std::endl;
        job_lst->PrintKilledJobs();
    }
    SmallShell& shl = SmallShell::getInstance();
    shl.exits();
}
void TouchCommand::execute() {
    string temp = cmd_line.substr(5, cmd_line.length());
    temp = _trim(string(temp));
    string path = temp.substr(0, temp.find_first_of(" \n"));
//      std::cout << path << std::endl;
    if(path.empty()){
        cerr<< "smash error: touch: invalid arguments" << endl;
        return;
    }
    string helper2 = temp.substr(path.size(), cmd_line.length());
    helper2 = _trim(string(helper2));
    string stamp = helper2.substr(0, helper2.find_first_of(" \n"));
//      std::cout << stamp << std::endl;
    stamp = _trim(string(stamp));
    if(stamp.empty()){
        cerr<< "smash error: touch: invalid arguments" << endl;
        return;
    }
    string remain = helper2.substr(stamp.size(), cmd_line.length());
//      std::cout << remain << std::endl;
    remain = _trim(string(remain));
    if(!remain.empty()){
        cerr<< "smash error: touch: invalid arguments" << endl;
        return;
    }
    stamp = _trim(string(stamp));
    const char* a = path.data();
    struct tm new_time;
    string sec = stamp.substr(0, stamp.find_first_of(':'));
    new_time.tm_sec = stoi(sec);
    stamp = stamp.substr(sec.size()+1, stamp.size());
    string min = stamp.substr(0, stamp.find_first_of(':'));
    new_time.tm_min = stoi(min);
    stamp = stamp.substr(min.size()+1, stamp.size());
    string hour = stamp.substr(0, stamp.find_first_of(':'));
    new_time.tm_hour = stoi(hour);
    stamp = stamp.substr(hour.size()+1, stamp.size());
    string day = stamp.substr(0, stamp.find_first_of(':'));
    new_time.tm_mday = stoi(day);
    stamp = stamp.substr(day.size()+1, stamp.size());
    string mon = stamp.substr(0, stamp.find_first_of(':'));
    new_time.tm_mon = stoi(mon) - 1;
    stamp = stamp.substr(mon.size()+1, stamp.size());
    string year = stamp.substr(0, stamp.find_first_of(" \n"));
    new_time.tm_year = stoi(year) - 1900;
    time_t t = mktime(&new_time);
    struct utimbuf ubuf;
    ubuf.actime = t;
    ubuf.modtime = t;
    int res;
    DO_SYS(res = utime(a, &ubuf));
}
void GetCurrDirCommand::execute() {
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) == nullptr){
        cerr << "getcwd() error" << endl;
        return;
    }
    else
        std::cout<< std::string(cwd) << std::endl;
}