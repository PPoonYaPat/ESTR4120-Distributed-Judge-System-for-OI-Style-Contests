#include <bits/stdc++.h>
#include "../common/common.h"
#include "jsonProcess.h"
#include "nlohmann/json.hpp"
using namespace std;

using json = nlohmann::json;

string pad_number(int num) {
    stringstream ss;
    ss << setfill('0') << setw(2) << num;
    return ss.str();
}

// ============================================================================
// Read JSON
// ============================================================================

void read_json(string testcase_config_path, vector<Task> &taskData) {
    ifstream file(testcase_config_path);
    
    if (!file.is_open()) {
        cerr << "Error: Cannot open testcase.json" << endl;
        exit(1);
    }

    json config;
    file >> config;
    file.close();

    taskData.clear();
    for (auto& task_json : config["tasks"]) {
        Task task;
        task.task_id = task_json["task_id"];
        task.memory_limit = task_json["memory_limit"];
        task.time_limit = task_json["time_limit"];

        for (auto& subtask_json : task_json["subtasks"]) {
            Subtask subtask;
            subtask.task_id = task_json["task_id"];
            subtask.subtask_id = subtask_json["subtask_id"];
            subtask.mod = subtask_json["mod"];

            subtask.dependencies.clear();
            for (auto& dependency_json : subtask_json["dependencies"]) {
                subtask.dependencies.push_back(dependency_json);
            }

            for (auto& testcase_json : subtask_json["testcases"]) {
                Testcase testcase;
                testcase.input_path = testcase_json["input_path"];
                testcase.expected_output_path = testcase_json["expected_output_path"];
                subtask.testcase.push_back(testcase);
            }

            task.subtask.push_back(subtask);
        }

        taskData.push_back(task);
    }
};

void read_json_auto_detect(string testcase_config_path, vector<string> testcase_dirs, vector<Task> &taskData) {
    ifstream file(testcase_config_path);
    
    if (!file.is_open()) {
        cerr << "Error: Cannot open testcase.json" << endl;
        exit(1);
    }

    json config;
    file >> config;
    file.close();

    taskData.clear();
    int idx=0;
    for (auto& task_json : config["tasks"]) {
        Task task;
        task.task_id = task_json["task_id"];
        task.memory_limit = task_json["memory_limit"];
        task.time_limit = task_json["time_limit"];

        string testcase_dir = testcase_dirs[idx];
        map<int,int> cnt_subtask;

        for (auto& testcase_file : filesystem::directory_iterator(testcase_dir)) {
            if (!testcase_file.is_regular_file()) {
                continue;
            }

            string filename = testcase_file.path().filename().string();
            size_t underscore_pos = filename.find('_');
            if (underscore_pos == string::npos) continue;

            string subtask_str = filename.substr(0, underscore_pos);
            int subtask_idx = stoi(subtask_str);
            cnt_subtask[subtask_idx]++;
        }

        for (auto& subtask_json : task_json["subtasks"]) {
            Subtask subtask;
            subtask.task_id = task_json["task_id"];
            subtask.subtask_id = subtask_json["subtask_id"];
            subtask.mod = subtask_json["mod"];

            subtask.dependencies.clear();
            subtask.testcase.clear();
            for (auto& dependency_json : subtask_json["dependencies"]) {
                subtask.dependencies.push_back(dependency_json);
            }

            for (int i=1; i<=cnt_subtask[subtask.subtask_id]/2; ++i) {
                Testcase testcase;
                testcase.input_path = testcase_dir + "/" + pad_number(subtask.subtask_id) + "_" + pad_number(i) + ".in";
                testcase.expected_output_path = testcase_dir + "/" + pad_number(subtask.subtask_id) + "_" + pad_number(i) + ".out";
                subtask.testcase.push_back(testcase);
            }

            task.subtask.push_back(subtask);
        }

        taskData.push_back(task);
        ++idx;
    }
}

// ============================================================================
// initialize subtask dependency graph
// ============================================================================

void init_dependencies(vector<Task> &taskData, vector<vector<int>> &dependency_counts, vector<vector<vector<int>>> &dependency_edges) {
    dependency_counts.clear();
    dependency_edges.clear();

    for (auto& task : taskData) {
        int subtask_count = task.subtask.size();
        
        vector<int> dependency_count(subtask_count,0);
        vector<vector<int>> dependency_edge(subtask_count, vector<int>());

        for (auto& subtask : task.subtask) {
            for (auto& dependency : subtask.dependencies) {
                dependency_count[subtask.subtask_id]+=task.subtask[dependency].mod;
                dependency_edge[dependency].push_back(subtask.subtask_id);
            }
        }

        dependency_counts.push_back(dependency_count);
        dependency_edges.push_back(dependency_edge);
    }
};