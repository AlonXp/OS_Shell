#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <memory>
#include <utility>
#include <vector>
#include <memory>
#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

#define PERROR(syscall) _perror(#syscall)

#define DO_SYS( syscall ) do { \
    if( (syscall) == -1 ) { \
        PERROR( syscall );     \
        return; \
    } \
} while( 0 )                   \

#define DO_SYS_WITH_EXIT( syscall ) do { \
    if( (syscall) == -1 ) { \
        PERROR( syscall );     \
        exit(0); \
    } \
} while( 0 )                             \

#define DO_SYS_WITH_NO_RETURN( syscall ) do { \
    if( (syscall) == -1 ) { \
        PERROR( syscall );     \
    } \
} while( 0 )                   \

void _perror(const char * syscall);
class Command {
protected:
    std::vector<std::string> param;
public:
    std::string cmd_line;
    Command() = default;
    explicit Command(const std::string &cmd_line);
    virtual ~Command() {};
    virtual void execute() = 0;
    std::vector<std::string>  getParams() const;
    void setParams(std::vector<std::string> new_paras);
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const std::string & cmd_line);
    virtual ~BuiltInCommand() {}
};

class PipeCommand : public Command {
public:
    PipeCommand (std::string & cmd_line);
    virtual ~PipeCommand() {}
    void execute() override;
};

class RedirectionCommand : public BuiltInCommand {
private:
public:
    explicit RedirectionCommand(std::string &cmd_line);
    virtual ~RedirectionCommand() {}
    void execute() override;
};

class JobsList;

class JobsList {
public:
    enum status {Finish, UNFinish, Stopped};
    class JobEntry {
        unsigned int job_id;
        unsigned int process_id;
        time_t timeIn;
        const char*  line;
        status t;
        int duration;
    public:
        JobEntry(unsigned int id, unsigned int pid, time_t time, const char *cmd, int duration = 0) :
                job_id(id), process_id(pid), timeIn(time), line(cmd), t(UNFinish), duration(duration) {}
        bool IsStopped(){
            return (t == Stopped);
        }
        bool IsFinished(){
            return (t==Finish);
        }
        void ContiniueRun(){
            t = UNFinish;
        }
        void StoppedJob(){
            t = Stopped;
        }
        void UpdateTime(){
            timeIn = time(nullptr);
        }
        void PrintStopped();
        void Print();
        void FinishJob(){
            t = Finish;
        }
        int GetJobId() const{
            return job_id;
        };
        int GetJobProcessId() const{
            return process_id;
        };
        std::string  GetCommandLine(){
            return line;
        }
        double ReamainTime();
        ~JobEntry() {
            delete line;
        }
    };
    std::vector<std::shared_ptr<JobEntry>> job_list;
public:
    JobsList()=  default;
    ~JobsList() {};
    void addJob(const char* cmd, int id, bool isStopped = false);
    bool ThisidExist(int id) {
        for(auto & i : job_list){
            if(i->GetJobId() == id)
                return true;}
        return false;
    }
    bool RunJob(int id);
    bool EmptyList() const{
        if(job_list.empty())
            return true;
        return false;
    }
    void printJobsList();
    void removeFinishedJobs();
    int getJobById(int jobId){
        int k = 0;
        for(auto & i : job_list){
            if(i->GetJobId() == jobId)
                k = i->GetJobProcessId();
        }
        return k;
    }
    void HandleLastJob();
    void HandNotLastJob(int jobId);
    std::shared_ptr<JobEntry> HandleStopJob(){
        std::shared_ptr<JobEntry> temp = nullptr;
        for(auto & i : job_list){
            if(i->IsStopped()){
                temp = i;
            }
        }
        return temp;
    }
    unsigned int Getsize() const{
        return job_list.size();
    }
    void FinishJob(int id){
        for(auto & i : job_list){
            if(i->GetJobProcessId() == id){
                i->FinishJob();
                return;
            }
    }}
    void alarmFirst(std::shared_ptr<JobsList> jobss);
    void PrintKilledJobs() const;
    void addtimedoutjob(int pid, int duration, const char* linek);
    static bool Compare(const std::shared_ptr<JobEntry>& first, const std::shared_ptr<JobEntry>& second);
    void RemoveJobById(int id);
    // TODO: Add extra methods or modify exisitng ones as needed
};
//TODO
class TimeOutCommand : public Command{
    std::shared_ptr<JobsList> job_liist;
    std::shared_ptr<JobsList> timed_out_list;
public:
    TimeOutCommand(const std::string &cmd_line, std::shared_ptr<JobsList> job_ls, std::shared_ptr<JobsList> alarm_ls) :
    Command(cmd_line), job_liist(std::move(job_ls)), timed_out_list(std::move(alarm_ls)) { };
    virtual ~TimeOutCommand() {};
    void execute() override;
};


class SmallShell {
private:
    std::string plastPwd;
    std::string to_print = "smash> ";
    std::shared_ptr<JobsList> job_list;
    std::shared_ptr<JobsList> timed_out;
    bool pipeChild = false;
    bool is_job_list_first_initialized = true;
    const char * orig_cmd_line;
    bool work = true;
    int pid;
    SmallShell();
public:
    std::shared_ptr<Command> CreateCommand(const std::string & cmd_line);
    SmallShell(SmallShell const&)      = delete; // disable copy ctor
    void operator=(SmallShell const&)  = delete; // disable = operator
    static SmallShell& getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        if (instance.is_job_list_first_initialized){
            instance.job_list = std::make_shared<JobsList>();;
            instance.timed_out = std::make_shared<JobsList>();;
            instance.is_job_list_first_initialized = false;
        }
        // Instantiated on first use.
        return instance;
    }
    std::string getPlastPwd() const;
    void setPlastPwd(char *Dir);
    bool getPipeChild() const;
    void changePipeChildState(bool flag);
    ~SmallShell();
    void executeCommand(const char* cmd_line);
    void makePipe(const std::string &cmd_line);
    std::shared_ptr<JobsList> getJobsList();
    void updateline(const std::string& newstring){
        to_print = newstring;
    }
    std::string line(){
        return to_print;
    }
    const char * originial() const{
        return orig_cmd_line;
    }
    std::shared_ptr<JobsList> getTimeOutList() {
        return timed_out;
    }
    void exits(){
        work = false;
    }
    bool IsRunning() const{
        return work;
    };
    int GetPid() const{
        return pid;
    }
    void UpdatePid(int id){
        pid = id;
    }
};
class ShowPidCommand : public BuiltInCommand {
    SmallShell &smash;
public:
    ShowPidCommand() = default;
    ShowPidCommand(const std::string & cmd_line, SmallShell &smash);
    virtual ~ShowPidCommand() {};
    void execute() override;
};
class ChangeDirCommand : public BuiltInCommand {
private:
    SmallShell &smash;
public:
    ChangeDirCommand(const std::string &cmd_line, SmallShell &smash);
    virtual ~ChangeDirCommand() {}
    void execute() override;
};
class KillCommand : public BuiltInCommand {
    SmallShell &smash;
public:
    KillCommand(const std::string & cmd_line, SmallShell &smash);
    virtual ~KillCommand() {}
    void execute() override;
};
class BackgroundCommand : public BuiltInCommand {
    SmallShell &smash;
public:
    BackgroundCommand(const std::string & cmd_line, SmallShell &smash);
    virtual ~BackgroundCommand() {}
    void execute() override;
};
class TailCommand : public BuiltInCommand {
public:
    TailCommand(const std::string & cmd_line);
    virtual ~TailCommand() {}
    void execute() override;
};
class JobsCommand : public BuiltInCommand {
    std::shared_ptr<JobsList> jobsi;
public:
    JobsCommand(const std::string& s,std::shared_ptr<JobsList> jobs) : BuiltInCommand(s), jobsi(std::move(jobs)) {};
    virtual ~JobsCommand() {}
    void execute() override{
        jobsi->printJobsList();
    }
};
class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const std::string& s) : BuiltInCommand(s) {};
    virtual ~GetCurrDirCommand() {}
    void execute() override;
};
class chprompt : public BuiltInCommand {
public:
    chprompt(const std::string& s, SmallShell &smashi) : BuiltInCommand(s){};
    virtual ~chprompt() {}
    void execute() override;
};
class ForegroundCommand : public BuiltInCommand {
    std::shared_ptr<JobsList> job_lst;
public:
    ForegroundCommand(const std::string& cmd_s, std::shared_ptr<JobsList> j) : BuiltInCommand(cmd_s), job_lst(std::move(j)){};
    virtual ~ForegroundCommand() {}
    void execute() override;
};
class QuitCommand : public BuiltInCommand {
    std::shared_ptr<JobsList> job_lst;
public:
    QuitCommand(const std::string& cmd_s, std::shared_ptr<JobsList> j) : BuiltInCommand(cmd_s), job_lst(std::move(j)){};
    virtual ~QuitCommand() {}
    void execute() override;
};
class TouchCommand : public BuiltInCommand {
public:
    TouchCommand(const std::string& cmd_s) : BuiltInCommand(cmd_s){};
    virtual ~TouchCommand() {}
    void execute() override;
};
class ExternalCommand : public Command {
    std::shared_ptr<JobsList> job_lst;
public:
    ExternalCommand(const std::string& cmd_s, std::shared_ptr<JobsList> j) : Command(cmd_s), job_lst(std::move(j)){};
    virtual ~ExternalCommand() {}
    void execute() override;
};
#endif //SMASH_COMMAND_H_
