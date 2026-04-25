#include <bits/stdc++.h>
using namespace std;

enum Status { ACC, WA, RE, TLE };

struct Submission {
    int problem; // 0-based index
    Status status;
    int time;
};

struct ProblemState {
    // Persistent across entire contest
    bool solved = false;
    int accept_time = -1; // time of first AC in entire history
    int wrong_before_accept = 0; // X for +x display and penalty
    int wrong_total = 0; // total wrong attempts counted (excludes pending frozen ones)

    // Current freeze window (only valid when frozen_active == true)
    vector<Submission> frozen_subs; // submissions after current FREEZE for problems unsolved at FREEZE
};

struct Team {
    string name;
    vector<ProblemState> probs; // size = M

    // Visible metrics (exclude current frozen submissions)
    int solved_visible = 0;
    long long penalty_visible = 0;
    vector<int> solve_times_visible_desc; // sorted descending

    // Helper: set of frozen problems (indices) that currently have frozen_subs not empty
    // We will track by checking frozen_subs.size() > 0
    struct Last {
        int problem = -1;
        Status status = WA;
        int time = -1;
        bool has = false;
    };
    Last last_any;
    array<Last,4> last_by_status{};
    vector<Last> last_by_problem; // size M
    vector<array<Last,4>> last_by_problem_status; // size M
};

int M = 0; // number of problems
bool started = false;
bool frozen_active = false;
int duration_time = 0;

unordered_map<string,int> team_id;
vector<Team> teams;

vector<int> last_flush_order; // team indices in ranking order after last FLUSH/SCROLL flush
bool has_flushed = false;

static inline Status parse_status(const string& s){
    if(s == "Accepted") return ACC;
    if(s == "Wrong_Answer") return WA;
    if(s == "Runtime_Error") return RE;
    // default
    return TLE;
}

static inline string status_str(Status st){
    switch(st){
        case ACC: return "Accepted";
        case WA: return "Wrong_Answer";
        case RE: return "Runtime_Error";
        case TLE: return "Time_Limit_Exceed";
    }
    return "";
}

// Ranking comparator only uses visible metrics
struct RankCmp {
    bool operator()(int a, int b) const {
        const Team &A = teams[a], &B = teams[b];
        if (A.solved_visible != B.solved_visible)
            return A.solved_visible > B.solved_visible; // more solved is better (smaller key)
        if (A.penalty_visible != B.penalty_visible)
            return A.penalty_visible < B.penalty_visible; // less penalty better
        // Compare solve times: smaller maximum time better, then next smaller, etc.
        const auto &VA = A.solve_times_visible_desc;
        const auto &VB = B.solve_times_visible_desc;
        // Both have same length because solved_visible equal
        for (size_t i = 0; i < VA.size(); ++i) {
            // They are sorted descending; smaller maximum means VA.back()? No, we stored descending, so maximum is VA[0]
            // We want to compare lexicographically on descending vectors preferring smaller values
            if (VA[i] != VB[i]) return VA[i] < VB[i];
        }
        // tie-break by name lexicographically ascending
        return A.name < B.name;
    }
};

// Maintain global current ranking based on visible metrics
static set<int, RankCmp> rank_set;

static inline void insert_solve_time_desc(vector<int>& v, int t){
    // keep v sorted descending; size <= 26
    auto it = lower_bound(v.begin(), v.end(), t, greater<int>());
    v.insert(it, t);
}

static inline void apply_accept_visible(Team& T, int accept_time, int x_wrong){
    T.solved_visible += 1;
    T.penalty_visible += 20LL * x_wrong + accept_time;
    insert_solve_time_desc(T.solve_times_visible_desc, accept_time);
}

static inline void add_team(const string& name){
    if (started){
        cout << "[Error]Add failed: competition has started.\n";
        return;
    }
    if (team_id.count(name)){
        cout << "[Error]Add failed: duplicated team name.\n";
        return;
    }
    Team T; T.name = name; T.probs.clear();
    teams.push_back(move(T));
    int id = (int)teams.size()-1;
    team_id[name] = id;
    cout << "[Info]Add successfully.\n";
}

static inline void start_comp(int dur, int m){
    if (started){
        cout << "[Error]Start failed: competition has started.\n";
        return;
    }
    started = true;
    duration_time = dur; M = m;
    for (auto &t : teams){
        t.probs.assign(M, ProblemState());
        t.solved_visible = 0; t.penalty_visible = 0; t.solve_times_visible_desc.clear();
        t.last_any = Team::Last();
        for (int i=0;i<4;++i) t.last_by_status[i] = Team::Last();
        t.last_by_problem.assign(M, Team::Last());
        t.last_by_problem_status.assign(M, {});
        for (int p=0;p<M;++p){ for (int i=0;i<4;++i) t.last_by_problem_status[p][i] = Team::Last(); }
    }
    // initialize ranking
    rank_set.clear();
    for (int i = 0; i < (int)teams.size(); ++i) rank_set.insert(i);
    cout << "[Info]Competition starts.\n";
}

static inline void handle_submit(char problem_char, const string& team_name, const string& status_str_in, int t){
    int pid = problem_char - 'A';
    int tid = team_id[team_name];
    Status st = parse_status(status_str_in);

    // Record submission for queries
    Submission sub{pid, st, t};
    // We'll maintain a per-team submissions vector for queries
    // To save memory, we can store in a global map vector; use a separate vector
    // For simplicity, store in a global per-team vector
    // We'll create it outside: submissions_per_team
}

// We'll declare global submissions storage now
static vector<vector<Submission>> team_submissions;

static inline void ensure_submission_storage(){
    if ((int)team_submissions.size() != (int)teams.size()) team_submissions.assign(teams.size(), {});
}

static inline void submit_update(const Submission& sub, int tid){
    Team &T = teams[tid];
    ProblemState &P = T.probs[sub.problem];
    // Update latest submission caches
    T.last_any = {sub.problem, sub.status, sub.time, true};
    T.last_by_status[sub.status] = {sub.problem, sub.status, sub.time, true};
    if ((int)T.last_by_problem.size() == M){
        T.last_by_problem[sub.problem] = {sub.problem, sub.status, sub.time, true};
    }
    if ((int)T.last_by_problem_status.size() == M){
        T.last_by_problem_status[sub.problem][sub.status] = {sub.problem, sub.status, sub.time, true};
    }
    // If already solved in final history, ignore for scoring; still record for query
    if (P.solved){
        return;
    }
    if (!frozen_active){
        // Normal time
        if (sub.status == ACC){
            // update ranking around metric change
            auto it = rank_set.find(tid); if (it != rank_set.end()) rank_set.erase(it);
            P.solved = true;
            P.accept_time = sub.time;
            P.wrong_before_accept = P.wrong_total; // wrong_total counts wrongs so far
            apply_accept_visible(T, P.accept_time, P.wrong_before_accept);
            rank_set.insert(tid);
        } else {
            // wrong attempt counts now
            P.wrong_total++;
        }
        // Update rank_set for this team if solved occurred
    } else {
        // Frozen active: only problems unsolved at freeze are affected
        // If unsolved at freeze, submissions to it become frozen; capture in P.frozen_subs
        // If it was solved before freeze, we would have P.solved == true; handled earlier
        P.frozen_subs.push_back(sub);
        // Do not change visible metrics yet
    }
}

static inline void do_flush(){
    last_flush_order.clear(); last_flush_order.reserve(teams.size());
    for (int tid : rank_set) last_flush_order.push_back(tid);
    has_flushed = true;
    cout << "[Info]Flush scoreboard.\n";
}

static inline bool team_exists(const string& name){ return team_id.count(name) != 0; }

static inline void do_freeze(){
    if (frozen_active){
        cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
        return;
    }
    frozen_active = true;
    cout << "[Info]Freeze scoreboard.\n";
}

static inline void print_scoreboard(const vector<int>& order){
    // Prepare ranking map
    vector<int> rank_idx(teams.size());
    for (size_t i = 0; i < order.size(); ++i) rank_idx[order[i]] = (int)i + 1;
    for (int tid : order){
        Team &T = teams[tid];
        cout << T.name << ' ' << rank_idx[tid] << ' ' << T.solved_visible << ' ' << T.penalty_visible;
        for (int p = 0; p < M; ++p){
            const ProblemState &P = T.probs[p];
            cout << ' ';
            if (!frozen_active){
                if (P.solved){
                    if (P.wrong_before_accept == 0) cout << '+';
                    else cout << '+' << P.wrong_before_accept;
                } else {
                    if (P.wrong_total == 0) cout << '.';
                    else cout << '-' << P.wrong_total;
                }
            } else {
                // Frozen display rules
                if (P.solved){
                    if (P.wrong_before_accept == 0) cout << '+';
                    else cout << '+' << P.wrong_before_accept;
                } else if (!P.frozen_subs.empty()){
                    int x = P.wrong_total; // wrongs before freeze (since wrong_total not updated during freeze)
                    int y = (int)P.frozen_subs.size();
                    if (x == 0) cout << "0/" << y;
                    else cout << '-' << x << '/' << y;
                } else {
                    if (P.wrong_total == 0) cout << '.';
                    else cout << '-' << P.wrong_total;
                }
            }
        }
        cout << "\n";
    }
}

// No global rank recomputation; we will use a local set in SCROLL

static inline void do_scroll(){
    if (!frozen_active){
        cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
        return;
    }
    cout << "[Info]Scroll scoreboard.\n";
    // Build local ranking structure from current global ranking and print pre-scroll scoreboard
    set<int, RankCmp> rank_set_local = rank_set;
    {
        vector<int> pre_order; pre_order.reserve(teams.size());
        for (int tid : rank_set_local) pre_order.push_back(tid);
        print_scoreboard(pre_order);
    }

    // Build set of teams with frozen problems
    struct FrozenCmp {
        bool operator()(int a, int b) const {
            return RankCmp{}(a,b);
        }
    };
    set<int, FrozenCmp> with_frozen;
    auto has_frozen = [&](int tid)->bool{
        const Team &T = teams[tid];
        for (int p = 0; p < M; ++p){
            if (!T.probs[p].solved && !T.probs[p].frozen_subs.empty()) return true;
        }
        return false;
    };
    for (int tid : rank_set_local){
        if (has_frozen(tid)) with_frozen.insert(tid);
    }

    while (!with_frozen.empty()){
        // pick lowest-ranked: rbegin()
        int tid = *with_frozen.rbegin();
        Team &T = teams[tid];
        // find smallest problem index among frozen ones
        int sel = -1;
        for (int p = 0; p < M; ++p){
            if (!T.probs[p].solved && !T.probs[p].frozen_subs.empty()) { sel = p; break; }
        }
        if (sel == -1){
            // should not happen
            with_frozen.erase(tid);
            continue;
        }

        // Save neighbor below before change
        int neighbor_below_old = -1;
        {
            auto it = rank_set_local.find(tid);
            auto itn = it; ++itn;
            if (itn != rank_set_local.end()) neighbor_below_old = *itn;
        }

        // Unfreeze selected problem
        ProblemState &P = T.probs[sel];
        bool changed_visible = false;
        // Process frozen submissions in order
        bool solved_now = false;
        int wrong_before_accept_after = 0;
        int accept_time = -1;
        for (const auto &s : P.frozen_subs){
            if (P.solved) break; // If became solved earlier in the same cycle (unlikely), but guard
            if (s.status == ACC){
                solved_now = true;
                accept_time = s.time;
                break;
            } else {
                wrong_before_accept_after++;
            }
        }
        if (solved_now){
            P.solved = true;
            P.accept_time = accept_time;
            P.wrong_before_accept = P.wrong_total + wrong_before_accept_after;
            // Update visible metrics
            apply_accept_visible(T, P.accept_time, P.wrong_before_accept);
            changed_visible = true;
        } else {
            // No AC in frozen subs: all are wrong attempts
            P.wrong_total += (int)P.frozen_subs.size();
        }
        // Clear frozen submissions for this problem
        P.frozen_subs.clear();

        // Update local ranking position if changed
        if (changed_visible){
            auto it = rank_set_local.find(tid);
            if (it != rank_set_local.end()) rank_set_local.erase(it);
            rank_set_local.insert(tid);
        }

        // Determine ranking change event
        bool ranking_changed = false;
        int team2 = -1;
        {
            auto it = rank_set_local.find(tid);
            auto itn = it; ++itn; // neighbor below (lower ranked)
            int neighbor_below_new = -1;
            if (itn != rank_set_local.end()) neighbor_below_new = *itn;
            if (neighbor_below_new != neighbor_below_old) {
                ranking_changed = true;
                team2 = neighbor_below_new;
            }
        }
        if (ranking_changed){
            cout << teams[tid].name << ' ' << (team2==-1? string("-") : teams[team2].name) << ' ' << teams[tid].solved_visible << ' ' << teams[tid].penalty_visible << "\n";
        }

        // Update with_frozen membership: remove tid, then if still has frozen problems, reinsert (position may have changed)
        with_frozen.erase(tid);
        if (has_frozen(tid)) with_frozen.insert(tid);
    }

    // After scrolling ends, frozen state lifted
    frozen_active = false;

    // Print scoreboard after scrolling
    vector<int> post_order; post_order.reserve(teams.size());
    for (int tid : rank_set_local) post_order.push_back(tid);
    print_scoreboard(post_order);

    // Update global ranking and last flush order to this latest order
    rank_set = rank_set_local;
    last_flush_order = post_order;
    has_flushed = true;
}

static inline void query_ranking(const string& team_name){
    if (!team_exists(team_name)){
        cout << "[Error]Query ranking failed: cannot find the team.\n";
        return;
    }
    cout << "[Info]Complete query ranking.\n";
    if (frozen_active){
        cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
    }
    int tid = team_id[team_name];
    int ranknum = -1;
    if (has_flushed){
        for (size_t i = 0; i < last_flush_order.size(); ++i){
            if (last_flush_order[i] == tid){ ranknum = (int)i + 1; break; }
        }
    } else {
        // before first flush, rank by lex order of names
        vector<int> ids(teams.size());
        iota(ids.begin(), ids.end(), 0);
        sort(ids.begin(), ids.end(), [&](int a, int b){ return teams[a].name < teams[b].name; });
        for (size_t i = 0; i < ids.size(); ++i){ if (ids[i]==tid){ ranknum=(int)i+1; break; } }
    }
    cout << team_name << " NOW AT RANKING " << ranknum << "\n";
}

static inline void query_submission(const string& team_name, const string& prob_filter, const string& status_filter){
    if (!team_exists(team_name)){
        cout << "[Error]Query submission failed: cannot find the team.\n";
        return;
    }
    ensure_submission_storage();
    int tid = team_id[team_name];
    int want_p = -1; // -1 means ALL
    if (prob_filter != "ALL") want_p = prob_filter[0]-'A';
    Status want_s; bool any_status = false;
    if (status_filter == "ALL") any_status = true;
    else want_s = parse_status(status_filter);

    bool found = false; Submission last;
    const Team &T = teams[tid];
    if (want_p == -1 && any_status){
        if (T.last_any.has){ last = {T.last_any.problem, T.last_any.status, T.last_any.time}; found = true; }
    } else if (want_p == -1 && !any_status){
        const auto &L = T.last_by_status[want_s];
        if (L.has){ last = {L.problem, L.status, L.time}; found = true; }
    } else if (want_p != -1 && any_status){
        if (want_p < M && want_p >= 0){
            const auto &L = T.last_by_problem[want_p];
            if (L.has){ last = {L.problem, L.status, L.time}; found = true; }
        }
    } else {
        if (want_p < M && want_p >= 0){
            const auto &L = T.last_by_problem_status[want_p][want_s];
            if (L.has){ last = {L.problem, L.status, L.time}; found = true; }
        }
    }
    cout << "[Info]Complete query submission.\n";
    if (!found){
        cout << "Cannot find any submission.\n";
    } else {
        cout << teams[tid].name << ' ' << char('A'+last.problem) << ' ' << status_str(last.status) << ' ' << last.time << "\n";
    }
}

int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string cmd;
    while (cin >> cmd){
        if (cmd == "ADDTEAM"){
            string name; cin >> name;
            add_team(name);
        } else if (cmd == "START"){
            string tmp; int dur, prob;
            cin >> tmp; // DURATION
            cin >> dur;
            cin >> tmp; // PROBLEM
            cin >> prob;
            start_comp(dur, prob);
        } else if (cmd == "SUBMIT"){
            string prob_token, tmp, team, status; int t;
            cin >> prob_token; // problem like "A"
            cin >> tmp; // BY
            cin >> team;
            cin >> tmp; // WITH
            cin >> status;
            cin >> tmp; // AT
            cin >> t;

            int tid = team_id[team];
            ensure_submission_storage();
            Submission s{prob_token[0] - 'A', parse_status(status), t};
            team_submissions[tid].push_back(s);
            submit_update(s, tid);
        } else if (cmd == "FLUSH"){
            do_flush();
        } else if (cmd == "FREEZE"){
            do_freeze();
        } else if (cmd == "SCROLL"){
            do_scroll();
        } else if (cmd == "QUERY_RANKING"){
            string name; cin >> name;
            query_ranking(name);
        } else if (cmd == "QUERY_SUBMISSION"){
            string team, where, problem_eq, and_s, status_eq;
            cin >> team;
            cin >> where; // WHERE
            cin >> problem_eq; // PROBLEM=...
            cin >> and_s; // AND
            cin >> status_eq; // STATUS=...
            string prob_filter = problem_eq.substr(problem_eq.find('=')+1);
            string status_filter = status_eq.substr(status_eq.find('=')+1);
            query_submission(team, prob_filter, status_filter);
        } else if (cmd == "END"){
            cout << "[Info]Competition ends.\n";
            break;
        } else {
            // Unknown token: consume rest of line to avoid infinite loop
            string rest; getline(cin, rest);
        }
    }
    return 0;
}
