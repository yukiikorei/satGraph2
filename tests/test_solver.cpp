#include <catch2/catch_test_macros.hpp>

#include "satgraf/solver.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

using namespace satgraf::solver;

namespace {

std::string write_mock_solver_script(const std::string& script_content) {
    static int counter = 0;
    std::string path = "/tmp/satgraf-test-mock-solver-" + std::to_string(::getpid()) + "-" + std::to_string(counter++);
    std::ofstream out(path);
    out << "#!/bin/bash\n" << script_content;
    out.close();
    ::chmod(path.c_str(), 0755);
    return path;
}

std::string write_test_cnf() {
    static int counter = 0;
    std::string path = "/tmp/satgraf-test-instance-" + std::to_string(::getpid()) + "-" + std::to_string(counter++) + ".cnf";
    std::ofstream out(path);
    out << "p cnf 3 2\n1 2 0\n-1 3 0\n";
    out.close();
    return path;
}

}

TEST_CASE("NamedFifo creates and cleans up", "[solver][fifo]") {
    std::string fifo_path = "/tmp/satgraf-test-fifo-" + std::to_string(::getpid()) + "-create";
    if (std::filesystem::exists(fifo_path)) { std::filesystem::remove(fifo_path); }

    REQUIRE_FALSE(std::filesystem::exists(fifo_path));
    {
        NamedFifo fifo(fifo_path);
        REQUIRE(std::filesystem::exists(fifo.path()));
        REQUIRE(fifo.path() == fifo_path);
    }
    REQUIRE_FALSE(std::filesystem::exists(fifo_path));
}

TEST_CASE("NamedFifo move transfers ownership", "[solver][fifo]") {
    std::string path1 = "/tmp/satgraf-test-fifo-" + std::to_string(::getpid()) + "-move1";
    std::string path2 = "/tmp/satgraf-test-fifo-" + std::to_string(::getpid()) + "-move2";
    if (std::filesystem::exists(path1)) { std::filesystem::remove(path1); }
    if (std::filesystem::exists(path2)) { std::filesystem::remove(path2); }

    NamedFifo a(path1);
    REQUIRE(std::filesystem::exists(path1));

    NamedFifo b(std::move(a));
    REQUIRE(b.path() == path1);
    REQUIRE(std::filesystem::exists(path1));

    b = NamedFifo(path2);
    REQUIRE_FALSE(std::filesystem::exists(path1));
    REQUIRE(std::filesystem::exists(path2));
}

TEST_CASE("NamedFifo release prevents cleanup", "[solver][fifo]") {
    std::string path = "/tmp/satgraf-test-fifo-" + std::to_string(::getpid()) + "-release";
    if (std::filesystem::exists(path)) { std::filesystem::remove(path); }

    {
        NamedFifo fifo(path);
        fifo.release();
    }
    REQUIRE(std::filesystem::exists(path));
    std::filesystem::remove(path);
}

TEST_CASE("ExternalSolver rejects missing solver binary", "[solver]") {
    ExternalSolver solver;
    REQUIRE_THROWS_AS(solver.start("/nonexistent/solver", "/tmp/test.cnf", "/tmp/pipe"),
                      std::runtime_error);
}

TEST_CASE("ExternalSolver detects SAT exit code 10", "[solver]") {
    auto cnf_path = write_test_cnf();
    auto solver_path = write_mock_solver_script("exit 10\n");

    std::string fifo_path = "/tmp/satgraf-test-fifo-" + std::to_string(::getpid()) + "-sat";
    if (std::filesystem::exists(fifo_path)) { std::filesystem::remove(fifo_path); }

    ExternalSolver solver;
    solver.set_fifo_path(fifo_path);
    REQUIRE(!solver.running());

    solver.start(solver_path, cnf_path, fifo_path);
    REQUIRE(solver.running());

    auto result = solver.wait_for_result(std::chrono::seconds(5));
    REQUIRE(result == SolverResult::SAT);
    REQUIRE(!solver.running());

    std::filesystem::remove(solver_path);
    std::filesystem::remove(cnf_path);
}

TEST_CASE("ExternalSolver detects UNSAT exit code 20", "[solver]") {
    auto cnf_path = write_test_cnf();
    auto solver_path = write_mock_solver_script("exit 20\n");

    std::string fifo_path = "/tmp/satgraf-test-fifo-" + std::to_string(::getpid()) + "-unsat";
    if (std::filesystem::exists(fifo_path)) { std::filesystem::remove(fifo_path); }

    ExternalSolver solver;
    solver.start(solver_path, cnf_path, fifo_path);

    auto result = solver.wait_for_result(std::chrono::seconds(5));
    REQUIRE(result == SolverResult::UNSAT);

    std::filesystem::remove(solver_path);
    std::filesystem::remove(cnf_path);
}

TEST_CASE("ExternalSolver detects crash", "[solver]") {
    auto cnf_path = write_test_cnf();
    auto solver_path = write_mock_solver_script("exit 1\n");

    std::string fifo_path = "/tmp/satgraf-test-fifo-" + std::to_string(::getpid()) + "-crash";
    if (std::filesystem::exists(fifo_path)) { std::filesystem::remove(fifo_path); }

    ExternalSolver solver;
    solver.start(solver_path, cnf_path, fifo_path);

    auto result = solver.wait_for_result(std::chrono::seconds(5));
    REQUIRE(result == SolverResult::UNKNOWN);

    std::filesystem::remove(solver_path);
    std::filesystem::remove(cnf_path);
}

TEST_CASE("ExternalSolver detects signal kill as crash", "[solver]") {
    auto cnf_path = write_test_cnf();
    // Use SIGABRT instead of SIGTERM so it doesn't propagate to test runner
    auto solver_path = write_mock_solver_script("kill -ABRT $$\n");

    std::string fifo_path = "/tmp/satgraf-test-fifo-" + std::to_string(::getpid()) + "-kill";
    if (std::filesystem::exists(fifo_path)) { std::filesystem::remove(fifo_path); }

    ExternalSolver solver;
    solver.start(solver_path, cnf_path, fifo_path);

    auto result = solver.wait_for_result(std::chrono::seconds(5));
    REQUIRE(result == SolverResult::CRASH);
    REQUIRE(!solver.running());

    std::filesystem::remove(solver_path);
    std::filesystem::remove(cnf_path);
}

TEST_CASE("ExternalSolver timeout works", "[solver]") {
    auto cnf_path = write_test_cnf();
    auto solver_path = write_mock_solver_script("sleep 60\n");

    std::string fifo_path = "/tmp/satgraf-test-fifo-" + std::to_string(::getpid()) + "-timeout";
    if (std::filesystem::exists(fifo_path)) { std::filesystem::remove(fifo_path); }

    ExternalSolver solver;
    solver.start(solver_path, cnf_path, fifo_path);

    auto result = solver.wait_for_result(std::chrono::milliseconds(200));
    REQUIRE(result == SolverResult::TIMEOUT);

    solver.cancel();

    std::filesystem::remove(solver_path);
    std::filesystem::remove(cnf_path);
}

TEST_CASE("FIFO IPC end-to-end with mock solver", "[solver][fifo][ipc]") {
    auto cnf_path = write_test_cnf();

    // Mock solver: write events to the pipe, then exit SAT
    // Uses background subshell to avoid blocking on FIFO open
    auto solver_path = write_mock_solver_script(
        R"(
PIPE_ARG=""
CNF_ARG=""
for arg in "$@"; do
  if [[ "$arg" == --pipe=* ]]; then
    PIPE_ARG="${arg#--pipe=}"
  else
    CNF_ARG="$arg"
  fi
done

# Open FIFO for writing (blocks until reader opens)
exec 3>"$PIPE_ARG"

# Write test events
echo "v d 1 0.5 1" >&3
echo "v p 0 0.3 2" >&3
echo "c + 1 -2 3 0" >&3
echo "! 1" >&3
echo "v d 1 0.8 3" >&3
echo "c - 1 -2 3 0" >&3

exec 3>&-

exit 10
)");

    std::string fifo_path = "/tmp/satgraf-test-fifo-" + std::to_string(::getpid()) + "-ipc";
    if (std::filesystem::exists(fifo_path)) { std::filesystem::remove(fifo_path); }

    NamedFifo fifo(fifo_path);

    ExternalSolver solver;
    solver.start(solver_path, cnf_path, fifo_path);

    // Open the FIFO for reading in a separate thread to avoid
    // deadlock (FIFO open blocks until both reader and writer are ready)
    int fd = -1;
    std::thread fifo_reader([&]() {
        fd = solver.open_fifo_for_read(std::chrono::seconds(5));
    });
    fifo_reader.join();
    REQUIRE(fd >= 0);

    // Read lines from the FIFO
    std::vector<std::string> lines;
    for (int i = 0; i < 6; i++) {
        auto line = solver.read_fifo_line(std::chrono::seconds(5));
        if (line.empty()) break;
        lines.push_back(line);
    }

    REQUIRE(lines.size() >= 3);
    REQUIRE(lines[0].substr(0, 2) == "v ");
    REQUIRE(lines[2].substr(0, 2) == "c ");

    // Check conflict line
    bool found_conflict = false;
    for (const auto& line : lines) {
        if (!line.empty() && line[0] == '!') {
            found_conflict = true;
            break;
        }
    }
    REQUIRE(found_conflict);

    auto result = solver.wait_for_result(std::chrono::seconds(5));
    REQUIRE(result == SolverResult::SAT);

    std::filesystem::remove(solver_path);
    std::filesystem::remove(cnf_path);
}

TEST_CASE("FIFO cleanup on solver crash", "[solver][fifo]") {
    auto cnf_path = write_test_cnf();
    auto solver_path = write_mock_solver_script("exit 1\n");

    std::string fifo_path = "/tmp/satgraf-test-fifo-" + std::to_string(::getpid()) + "-crashclean";
    {
        NamedFifo fifo(fifo_path);
        ExternalSolver solver;
        solver.start(solver_path, cnf_path, fifo_path);

        auto result = solver.wait_for_result(std::chrono::seconds(5));
        REQUIRE(result == SolverResult::UNKNOWN);
    }

    REQUIRE_FALSE(std::filesystem::exists(fifo_path));

    std::filesystem::remove(solver_path);
    std::filesystem::remove(cnf_path);
}

TEST_CASE("result_to_string covers all cases", "[solver]") {
    REQUIRE(result_to_string(SolverResult::SAT) == "SAT");
    REQUIRE(result_to_string(SolverResult::UNSAT) == "UNSAT");
    REQUIRE(result_to_string(SolverResult::UNKNOWN) == "UNKNOWN");
    REQUIRE(result_to_string(SolverResult::CRASH) == "CRASH");
    REQUIRE(result_to_string(SolverResult::TIMEOUT) == "TIMEOUT");
    REQUIRE(result_to_string(SolverResult::NOT_STARTED) == "NOT_STARTED");
}
