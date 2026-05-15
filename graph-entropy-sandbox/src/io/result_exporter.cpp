#include "entropy/io/result_exporter.hpp"
#include <fstream>
#include <stdexcept>

namespace entropy::io {

void write_run_output(const std::string&    run_dir,
                      const nlohmann::json& manifest,
                      const ResultSet&      results) {
    {
        std::ofstream f(run_dir + "/manifest.json");
        if (!f) throw std::runtime_error("Cannot write manifest.json in " + run_dir);
        f << manifest.dump(2) << "\n";
    }
    results.write_json(run_dir + "/results.json");
    results.write_csv (run_dir + "/results.csv");
}

} // namespace entropy::io
