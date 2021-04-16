#include "fat.h"
#include <unistd.h>
#include <sys/wait.h>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>

namespace {

const std::string prompt = "> ";

struct Command {
    std::string name;
    std::function<void(const std::vector<std::string> &)> func;
    int expected_args;
};

void show_status(std::string description, bool result) {
    if (result) {
        std::cout << description << ": ";
        std::cout << "returned true (successful)" << std::endl;
    } else {
        std::cerr << description << ": ";
        std::cerr << "returned false (failed)" << std::endl;
    }
}

bool check_integer(const std::string &what, const std::string &arg, int *result) {
    char *ptr;
    *result = static_cast<int>(std::strtol(arg.c_str(), &ptr, 10));
    if (*ptr != 0) {
        std::cerr << what << ": '" << arg << "' is not an integer" << std::endl;
        return false;
    } else {
        return true;
    }
}

void do_mount(const std::vector<std::string> &args) {
    show_status("mounting " + args[0], fat_mount(args[0]));
}

void do_open(const std::vector<std::string> &args) {
    int result = fat_open(args[0]);
    if (result < 0) {
        std::cerr << "opening " << args[0] << ": returned " << result << " (failed)" << std::endl;
    } else {
        std::cout << "opening " << args[0] << ": returned fd " << result << std::endl;
    }
}

void do_close(const std::vector<std::string> &args) {
    int fd;
    if (!check_integer("close", args[0], &fd)) return;
    show_status("closing fd " + std::to_string(fd), fat_close(fd));
}

bool normal_directory_entry(const AnyDirEntry & entry) {
    if (entry.dir.DIR_Attr & DirEntryAttributes::VOLUME_ID)
        return false;
    if ((entry.dir.DIR_Attr & DirEntryAttributes::LONG_NAME_MASK) == DirEntryAttributes::LONG_NAME)
        return false;
    if (entry.dir.DIR_Name[0] == 0xE5)
        return false;
    return true;
}

void do_lsdir(const std::vector<std::string> &args) {
    std::vector<AnyDirEntry> entries = fat_readdir(args[0]);
    if (entries.size() == 0) {
        std::cerr << "lsdir " << args[0] << ": returned empty list (failed?)" << std::endl;
        return;
    }
    int normal_entries = 0;
    int skipped_entries = 0;
    bool found_null_entry = false;
    for (const AnyDirEntry &entry : entries) {
        if (entry.dir.DIR_Name[0] == 0x00) {
            found_null_entry = true;
            break;
        }
        if (!normal_directory_entry(entry)) {
            ++skipped_entries;
            continue;
        }
        ++normal_entries;
    }
    std::cout << std::dec << args[0] << ": found " << normal_entries << " normal directory entries";
    std::cout << " (ignoring " << skipped_entries << " deleted or long directory entries";
    if (found_null_entry) {
        std::cout << " and " << (entries.size() - skipped_entries - normal_entries - 1)
                  << " entries after the first entry with 0x00)" << std::endl;
    } else {
        std::cout << ")" << std::endl;
    }
    std::cout << std::setw(12) << "name" << " " << std::setw(10) << "size" << " " << std::setw(16) << "type" << " "
              << std::setw(15) << "first cluster" << std::endl;
    for (const AnyDirEntry &entry : entries) {
        if (entry.dir.DIR_Name[0] == 0x00) break;
        if (!normal_directory_entry(entry)) continue;
        std::string file_name = std::string(entry.dir.DIR_Name, entry.dir.DIR_Name + 8);
        while (file_name.back() == ' ') file_name.pop_back();
        if (entry.dir.DIR_Name[8] != ' ') {
            file_name += '.';
            file_name += std::string(entry.dir.DIR_Name + 8, entry.dir.DIR_Name + 11);
            while (file_name.back() == ' ') file_name.pop_back();
        }
        unsigned first_cluster = entry.dir.DIR_FstClusLO | (entry.dir.DIR_FstClusHI << 16);
        if (entry.dir.DIR_Attr & DirEntryAttributes::DIRECTORY) {
            std::cout << std::setw(12) << file_name << " " << std::setw(10) << "--" << " " << std::setw(16) << "directory";
        } else {
            std::cout << std::setw(12) << file_name << " " << std::setw(10) << entry.dir.DIR_FileSize
                      << " " << std::setw(16) << "regular file";
        }
        std::ios_base::fmtflags saved_fmt_flags(std::cout.flags());
        std::cout << " " << std::setw(15) << std::hex << std::showbase << first_cluster << std::endl;
        std::cout.flags(saved_fmt_flags);
    }
}

void do_pread(const std::vector<std::string> &args) {
    int fd, count, offset;
    if (!check_integer("pread fd", args[0], &fd)) return;
    if (!check_integer("pread count", args[1], &count)) return;
    if (!check_integer("pread offset", args[2], &offset)) return;
    std::vector<char> buffer(count <= 0 ? 1 : count);
    int rv = fat_pread(fd, static_cast<void*>(&buffer[0]), count, offset);
    if (rv < 0) {
        std::cerr << "pread: returned -1 (error)" << std::endl;
        return;
    }
    buffer.resize(rv);
    std::cout << "pread from fd " << fd << ", offset " << offset << ", count " << count << ": returned " << rv << " (bytes read)"
              << std::endl;
    if (rv > count) {
        std::cerr << "WARNING: more bytes read than requested!" << std::endl;
    }
    bool all_text = true;
    for (char c : buffer) {
        if (!std::isprint(c) && !std::isspace(c)) all_text = false;
    }

    if (all_text) {
        std::cout << "---contents read---\n";
        std::cout << std::string(buffer.begin(), buffer.end());
        std::cout << "---end of contents---\n";
    } else {
        std::ios_base::fmtflags saved_fmt_flags(std::cout.flags());
        std::cout << "result buffer contains non-printable characters, displaying as hexadecimal\n";
        std::cout << std::setw(13)  << std::left << "start byte#" << std::right << " values as hexadecimal\n";
        const unsigned per_line = 32;
        for (unsigned i = 0; i < buffer.size(); i += per_line) {
            std::cout << std::hex << std::showbase << std::left << std::setw(13) << i << std::noshowbase << std::right;
            for (unsigned j = i; j < i + per_line && j < buffer.size(); ++j) {
                std::cout << " " <<std::hex << std::setw(2) << std::setfill('0') << (static_cast<int>(buffer[j]) & 0xFF) << std::setfill(' ');
            }
            std::cout << "\n";
        }
        std::cout << "---end of contents---\n";
        std::cout.flags(saved_fmt_flags);
    }
}

void do_preadandsave(const std::vector<std::string> &args) {
    int fd, count, offset;
    if (!check_integer("preadandsave fd", args[0], &fd)) return;
    if (!check_integer("preadandsave size", args[1], &count)) return;
    if (!check_integer("preadandsave offset", args[2], &offset)) return;
    std::string output_file = args[3];
    std::vector<char> buffer(count <= 0 ? 1 : count);
    int rv = fat_pread(fd, static_cast<void*>(&buffer[0]), count, offset);
    if (rv < 0) {
        std::cerr << "preadandsave: pread returned -1 (error)" << std::endl;
        return;
    }
    buffer.resize(rv);
    std::cout << "pread from fd " << fd << ", offset " << offset << ", count " << count << ": returned " << rv << " (bytes read)"
              << std::endl;
    if (rv > count) {
        std::cerr << "WARNING: more bytes read than requested!" << std::endl;
    }
    std::cout << "writing contents to " << output_file << std::endl;
    std::ofstream out(output_file);
    out.write(&buffer[0], buffer.size());
    out.close();
}

void do_help(const std::vector<std::string> &args) {
    std::cout << \
"fat_shell commands:\n\
   mount FILENAME\n\
     Call fat_mount() to mount a filesystem image.\n\
   lsdir PATH\n\
     Call fat_readdir() on PATH and display the results in a human-readable way.\n\
     Directory entries which do not appear to represent regular files or directories\n\
     will be skipped and long file name information will be ignored.\n\
     If a directory entry that indicates end-of-directory is encountered, then no\n\
     directory entries present in the result of fat_readdir() afterwards will be output\n\
     even if they are directory entries that represent normal files.\n\
   open PATH\n\
     Call fat_open() on the specified path and print out the file descriptor returned\n\
   pread FD COUNT OFFSET\n\
     Call fat_pread() to read COUNT bytes starting at offset byte OFFSET from\n\
     file descriptor FD.\n\
     Then displays the result, as text if the result consistents entirely of\n\
     printable ASCII characters; otherwise as hexadecimal.\n\
   preadandsave FD COUNT OFFSET OUTPUT\n\
     Call fat_pread() to read COUNT bytes starting at offset byte OFFSET from\n\
     file descriptor FD.\n\
     Then write the result to a new file (outside the disk image) named\n\
     OUTPUT.\n\
   close FD\n\
     Call fat_close() on file descriptor FD. Output whether it returns success\n\
   exit\n\
     Quit\n\
   OPERATION | SHELL-COMMAND\n\
     Run the OPERATION command, but sends its output through the shell command\n\
     SHELL-COMMAND.\n\
        Example:  lsdir / | less\n\
            to run less to paginate lsdir's output\n\
   !SHELL-COMMAND\n\
     Run SHELL_COMMAND in the underlying OS's shell\n\
";
}

Command commands[] = {
    { "mount", do_mount, 1 },
    { "lsdir", do_lsdir, 1 },
    { "open", do_open, 1 },
    { "close", do_close, 1 },
    { "pread", do_pread, 3 },
    { "preadandsave", do_preadandsave, 4 },
    { "help", do_help, -1 },
};

void run_command(const std::string &line) {
    std::vector<std::string> args;
    if (line[0] == '!') {
        std::string shell_command = line.substr(1);
        if (std::system(shell_command.c_str()) != 0) {
            std::cerr << "shell command returned non-zero exit status\n";
        }
        return;
    }
    std::string operation = line;
    int saved_stdout_fd = -1;
    pid_t pipe_pid = (pid_t) -1;
    if (line.find('|') != std::string::npos) {
        std::cout.flush();
        operation = line.substr(0, line.find('|'));
        std::string shell_command = line.substr(line.find('|') + 1);
        int pipe_fds[2];
        if (pipe(pipe_fds) < 0) {
            std::perror("pipe");
            return;
        }
        pipe_pid = fork();
        if (pipe_pid == 0) { // child process
            close(pipe_fds[1]);
            dup2(pipe_fds[0], STDIN_FILENO);
            close(pipe_fds[0]);
            const char *args[] = { "/bin/sh", "-c", shell_command.c_str(), NULL };
            execv("/bin/sh", (char *const*) args);
            std::perror("execv");
            std::_Exit(1);
        } else {
            saved_stdout_fd = dup(STDOUT_FILENO);
            dup2(pipe_fds[1], STDOUT_FILENO);
            close(pipe_fds[0]);
            close(pipe_fds[1]);
        }
    }
    std::stringstream ss(operation);
    std::string command, token;
    ss >> command;

    if (!ss) return;  // empty command line

    while (ss >> token) {
        args.push_back(token);
    }

    if (command == "exit") {
        std::exit(0);
    }

    Command const *found_command = nullptr;
    for (Command const &c : commands) {
        if (c.name == command) {
            found_command = &c;
            break;
        }
    }
    
    if (!found_command) {
        std::cerr << command << ": command not found" << std::endl;
    } else if (found_command->expected_args != -1 && found_command->expected_args != (int) args.size()) {
        std::cerr << command << ": expected " << found_command->expected_args
                  << " arguments, but found " << args.size() << std::endl;
    } else {
        found_command->func(args);
    }

    if (pipe_pid != (pid_t) -1) {
        dup2(saved_stdout_fd, STDOUT_FILENO);
        close(saved_stdout_fd);
        waitpid(pipe_pid, nullptr, 0);
    }
}

}   // unnamed namespace


int main(void) {
    std::string line;
    std::cout << prompt;
    while (std::getline(std::cin, line)) {
        run_command(line);
        std::cout << prompt;
    }
}
