#include <cstdio>
#include <iostream>
#include <vector>
#include <fstream>

#include "timetable.hh"
#include "raptor.hh"
#include "logging.hh"
#include "file_util.hh"

namespace {
    void usage_exit (const char *argv[]) {
        std::cerr << "Usage: " << argv[0]
                  << " -query-file=queries-unif.csv [-min-change-time=60]"
                     " [-nq=1000] [-beg=0] [-end=t_max] [-o=raptor.csv]"
                     " gtfs_dir"
                  << std::endl;
        exit(1);
    }

    std::vector<std::string> get_args(int argc, const char *argv[]) {
        std::vector<std::string> a;
        for (int i = 0; i < argc; ++i) {
            std::string s(argv[i]);
            if (s[0] == '-') continue; // option
            a.push_back(s);
        }
        return a;
    }

    bool has_opt(int argc, const char *argv[], std::string opt) {
        assert(opt[0] == '-');
        for (int i = 0; i < argc; ++i) {
            std::string s(argv[i]);
            if (s == opt) return true;
        }
        return false;
    }

    std::string get_opt(int argc, const char *argv[],
                        std::string opt_prefix, std::string dft) {
        size_t len = opt_prefix.size();
        assert(opt_prefix[0] == '-'
               && (opt_prefix[len-1] == '=' || opt_prefix[len-1] == ':'));
        for (int i = 0; i < argc; ++i) {
            std::string s(argv[i]);
            if (s.size() >= len && s.substr(0, len) == opt_prefix) {
                return s.substr(len);
            }
        }
        return dft;
    }

    int get_int_opt(int argc, const char *argv[],
                    std::string opt_prefix, int dft) {
        return std::stoi(get_opt(argc, argv, opt_prefix, std::to_string(dft)));
    }

    using pset = pareto_rev<int>;

    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();

    void log(const std::string &str) {
        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> dur = end-start;
        std::cerr << dur.count() << "s: " << str << std::endl;
    }
}

int
main(int argc, const char *argv[])
{
    logging main_log("--");

    // ------------------------ usage -------------------------
    std::vector<std::string> args = get_args(argc, argv);
    if (args.size() != 2) {
        usage_exit(argv);
    }
    std::string dir{args[1]};
    std::cerr <<"--------------- "<< dir <<" ---------------------\n";
    dir += "/";

    // ------------------------ time -------------------------
    main_log.cerr() << "start\n";
    double t = main_log.lap();

    //std::cerr <<"first rands: "<< rand() <<" "<< rand() <<" "<< rand() <<"\n";


    // ------------------------- load timetable ----------------------
    // timetable ttbl{dir+"stop_times.csv.gz", dir+"transfers.csv.gz", true};

    // timetable ttbl{args[1], args[2],
    //                dir+"calendar.txt", dir+"calendar_dates.txt",
    //                dir+"trips.txt", dir+"stop_times.txt", dir+"transfers.txt", true};

    timetable ttbl{dir+"stop_times.csv.gz",
                   dir+"walk_and_transfer_inhubs.gr.gz",
                   dir+"walk_and_transfer_outhubs.gr.gz",
                   dir+"transfers.csv.gz", true};

    //dir+"walking_and_transfers.gr", t_from, t_to};
    std::cerr << ttbl.n_r <<" routes, "<< ttbl.n_st <<" sations, "
              << ttbl.n_s <<" stops\n";
    main_log.cerr(t) << "timetable\n";
    t = main_log.lap();
    //exit(0);

    // --------------- earliest arrival time through Raptor and CSA ---------
    raptor rpt(ttbl);
    main_log.cerr(t) << "raptor initialized\n";
    t = main_log.lap();

    timetable rev_ttbl(ttbl);
    rev_ttbl.check();
    rev_ttbl.reverse_time();
    raptor rev_rpt(rev_ttbl);
    main_log.cerr(t) << "rev raptor initialized\n";
    t = main_log.lap();

    const int chg=std::stoi(get_opt(argc, argv, "-min-change-time=", "60"));

    uint64_t sum = 0, n_ok = 0;


    // ------------------ read queries -------------------
    int n_q = 0;
    std::vector<std::tuple<int, int, int> > queries;
    if (get_opt(argc, argv, "-query-file=", "queries-unif.csv") != "") {
        auto rows = read_csv
            (dir + get_opt(argc, argv, "-query-file=", "queries-unif.csv"), 3,
             "source", "destination", "departure_time");
        //"source", "target", "time");
        int n_q_max = get_int_opt(argc, argv, "-nq=", 10000);
        for (auto r : rows) {
            if (n_q >= n_q_max) break;
            if (has_opt(argc, argv, "-1") && n_q >= 1) break;
            if (has_opt(argc, argv, "-10") && n_q >= 10) break;
            if (has_opt(argc, argv, "-100") && n_q >= 100) break;
            int src = ttbl.id_to_station[r[0]];
            int dst = ttbl.id_to_station[r[1]];
            int t = std::stoi(r[2]);
            queries.push_back(std::make_tuple(src, dst, t));
            ++n_q;
        }
        main_log.cerr(t) << n_q << " queries\n";
        t = main_log.lap();
    }
    // */

    auto cout_avg_time = [n_q,&t,&main_log](std::string s) {
                             std::cout << s <<"_avg_time "
                                       << (main_log.lap() - t) * 1000.0 / n_q <<"\n";
                         };
    t = main_log.lap();

    std::string path(get_opt(argc, argv, "-o=", "raptor.csv"));
    std::ofstream out(path);
    if (out.bad()) {
        perror(path.c_str());
        exit(EXIT_FAILURE);
    }
    out << "query,arrival,departure" << std::endl;

    // go profile Raptor
    sum = 0, n_ok = 0;
    int t_beg = get_int_opt(argc, argv, "-beg=", 0),
        t_end = get_int_opt(argc, argv, "-end=", ttbl.t_max);
    for (size_t i = 0; i < queries.size(); ++i) {
        auto &q(queries[i]);
        int src = std::get<0>(q);
        int dst = std::get<1>(q);
        int t = std::get<2>(q);
        if (t < t_beg) continue;
        main_log.cerr(t) <<src<<" "<<dst<<" ["<<t_beg<<","<<t_end<<") : ";
        pset prof = rpt.profile(rev_rpt, src, dst, t_beg, t_end,
                                false, true, chg);
        auto f = [&out, i]() { out << std::to_string(i) << ","; };
        prof.rprint(f, out);
        int ntrips = prof.size();
        std::cout << ntrips <<"\n";
        sum += ntrips;
        ++n_ok;
    }
    out.close();
    cout_avg_time("prRaptor");
    main_log.cerr(t) << n_q << " Profile Raptor queries done, avg_ntrips = "
                     << (sum / n_ok)
                     << "  "<< n_ok <<"/"<< queries.size() <<" ok\n";
    t = main_log.lap();


    // ------------------------ end -------------------------
    main_log.cerr() << "end "<< dir <<"\n";

}
