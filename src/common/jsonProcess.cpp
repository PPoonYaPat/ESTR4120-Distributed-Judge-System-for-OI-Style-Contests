#include <bits/stdc++.h>
#include "../common/common.h"
#include "jsonProcess.h"
#include "nlohmann/json.hpp"
using namespace std;

using json = nlohmann::json;

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