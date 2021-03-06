#include "helpers.hpp"
#include "common.hpp"
#include "util/path.hpp"
#include "util/log.hpp"
#include "util/unix.hpp"

extern "C" {
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <linux/loop.h>
}

static void HelperError(TFile &err, const std::string &text, TError error) {
    L_WRN("{}: {}", text, error);
    err.WriteAll(fmt::format("{}: {}", text, error));
    _exit(EXIT_FAILURE);
}

TError RunCommand(const std::vector<std::string> &command,
                  const TFile &dir, const TFile &in, const TFile &out,
                  const TCapabilities &caps) {
    TCgroup memcg = MemorySubsystem.Cgroup(PORTO_HELPERS_CGROUP);
    TError error;
    TFile err;
    TTask task;
    TPath path = dir.RealPath();

    if (!command.size())
        return TError("External command is empty");

    error = err.CreateUnnamed("/tmp", O_APPEND);
    if (error)
        return error;

    std::string cmdline;

    for (auto &arg : command)
        cmdline += arg + " ";

    L_ACT("Call helper: {} in {}", cmdline, path);

    error = task.Fork();
    if (error)
        return error;

    if (task.Pid) {
        error = task.Wait();
        if (error) {
            std::string text;
            TError error2 = err.ReadEnds(text, TError::MAX - 1024);
            if (error2)
                text = "Cannot read stderr: " + error2.ToString();
            error = TError(error, "helper: {} stderr: {}", cmdline, text);
        }
        return error;
    }

    error = memcg.Attach(GetPid());
    if (error)
        HelperError(err, "Cannot attach to helper cgroup: {}", error);

    SetDieOnParentExit(SIGKILL);

    if (!in) {
        TFile in;
        error = in.Open("/dev/null", O_RDONLY);
        if (error)
            HelperError(err, "open stdin", error);
        if (dup2(in.Fd, STDIN_FILENO) != STDIN_FILENO)
            HelperError(err, "stdin", TError::System("dup2"));
    } else {
        if (dup2(in.Fd, STDIN_FILENO) != STDIN_FILENO)
            HelperError(err, "stdin", TError::System("dup2"));
    }

    if (dup2(out ? out.Fd : err.Fd, STDOUT_FILENO) != STDOUT_FILENO)
        HelperError(err, "stdout", TError::System("dup2"));

    if (dup2(err.Fd, STDERR_FILENO) != STDERR_FILENO)
        HelperError(err, "stderr", TError::System("dup2"));

    TPath root("/");
    TPath dot(".");

    if (dir && !path.IsRoot()) {
        /* Unshare and remount everything except CWD Read-Only */
        error = dir.Chdir();
        if (error)
            HelperError(err, "chdir", error);

        if (unshare(CLONE_NEWNS))
            HelperError(err, "newns", TError::System("unshare"));

        error = root.Remount(MS_PRIVATE | MS_REC);
        if (error)
            HelperError(err, "remont", error);

        error = root.Remount(MS_BIND | MS_REC | MS_RDONLY);
        if (error)
            HelperError(err, "remont", error);

        error = dot.Bind(dot, MS_REC);
        if (error)
            HelperError(err, "bind", error);

        error = TPath("../" + path.BaseName()).Chdir();
        if (error)
            HelperError(err, "chdir bind", error);

        error = dot.Remount(MS_BIND | MS_REC | MS_ALLOW_WRITE);
        if (error)
            HelperError(err, "remount bind", error);
    } else {
        error =root.Chdir();
        if (error)
            HelperError(err, "root chdir", error);
    }

    error = caps.ApplyLimit();
    if (error)
        HelperError(err, "caps", error);

    TFile::CloseAll({STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO});

    const char **argv = (const char **)malloc(sizeof(*argv) * (command.size() + 1));
    for (size_t i = 0; i < command.size(); i++)
        argv[i] = command[i].c_str();
    argv[command.size()] = nullptr;

    execvp(argv[0], (char **)argv);

    err.SetFd = STDERR_FILENO;
    HelperError(err, fmt::format("Cannot execute {}", argv[0]), TError::System("exec"));
}

TError CopyRecursive(const TPath &src, const TPath &dst) {
    TError error;
    TFile dir;

    error = dir.OpenDir(dst);
    if (error)
        return error;

    return RunCommand({ "cp", "--archive", "--force",
                        "--one-file-system", "--no-target-directory",
                        src.ToString(), "." }, dir);
}

TError ClearRecursive(const TPath &path) {
    TError error;
    TFile dir;

    error = dir.OpenDir(path);
    if (error)
        return error;

    return RunCommand({ "find", ".", "-xdev", "-mindepth", "1", "-delete"}, dir);
}

TError RemoveRecursive(const TPath &path) {
    TError error;
    TFile dir;

    error = dir.OpenDir(path.NormalPath().DirName());
    if (error)
        return error;

    return RunCommand({"rm", "-rf", "--one-file-system", "--", path.ToString()}, dir);
}
